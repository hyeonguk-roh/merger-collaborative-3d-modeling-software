#include "merger.h"
#include <algorithm>
#include <iostream>
#include <cstring>
#include "network.h"
#include "scene.h"

static const ImGuiTreeNodeFlags TREE_NODE_OPTIONS = ImGuiTreeNodeFlags_DrawLinesFull 
                                            | ImGuiTreeNodeFlags_OpenOnArrow 
                                            | ImGuiTreeNodeFlags_OpenOnDoubleClick 
                                            | ImGuiTreeNodeFlags_DefaultOpen
                                            | ImGuiTreeNodeFlags_SpanAvailWidth;

namespace Merger
{
    void App::collect_descendants(SceneID id, std::vector<glm::uvec3>& out)
    {
        out.push_back(glm::uvec3(id, 0, 0));
        const SceneNode& node = scene.nodes[id];
        for (SceneID child : node.children)
        {
            collect_descendants(child, out);
        }
    }

    bool App::is_node_or_descendant_selected(SceneID id)
    {
        // Check if this node is directly selected
        for (const auto& sel : selected_meshes)
            if (sel[0] == id) return true;

        // Walk up the parent chain to see if any ancestor is selected
        SceneID current = scene.nodes[id].parent_id;
        while (current != UINT32_MAX && current < scene.nodes.size())
        {
            for (const auto& sel : selected_meshes)
                if (sel[0] == current) return true;
            current = scene.nodes[current].parent_id;
        }
        return false;
    }

    void App::walk_gui_scene_nodes(SceneID id)
    {
        const SceneNode& node = scene.nodes[id];

        ImGuiTreeNodeFlags options = TREE_NODE_OPTIONS;
        if (node.children.size() == 0) options |= ImGuiTreeNodeFlags_Leaf;

        // Check if this node should be highlighted (selected or descendant of selected)
        bool is_highlighted = is_node_or_descendant_selected(id);
        if (is_highlighted) options |= ImGuiTreeNodeFlags_Selected;

        // Build label: use node name + ###id for unique ImGui ID
        std::string display_name = node.name.empty() ? "Node" : node.name;
        std::string label = display_name + "###node_" + std::to_string(id);

        // If this node is being renamed, show an InputText replacing the label
        if (renaming_node == id)
        {
            // Capture where the label text starts before rendering the tree node
            float label_start_x = ImGui::GetCursorPosX() + ImGui::GetTreeNodeToLabelSpacing();

            // Remove SpanAvailWidth so the empty tree node doesn't fill the row,
            // add AllowOverlap so the InputText can share the same line
            ImGuiTreeNodeFlags rename_options = (options & ~ImGuiTreeNodeFlags_SpanAvailWidth) | ImGuiTreeNodeFlags_AllowOverlap;
            bool tree_open = ImGui::TreeNodeEx(("###node_" + std::to_string(id)).c_str(), rename_options);

            // Position InputText flush where the label text would normally appear
            ImGui::SameLine(label_start_x);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (!rename_focus_set)
            {
                ImGui::SetKeyboardFocusHere();
                rename_focus_set = true;
            }

            // Strip frame padding so the InputText matches the label text size exactly
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            bool committed = ImGui::InputText("##rename_input", rename_buffer, sizeof(rename_buffer),
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            ImGui::PopStyleVar(2);

            // Commit on Enter or when clicking away after editing
            if (committed || ImGui::IsItemDeactivatedAfterEdit())
            {
                if (std::strlen(rename_buffer) > 0)
                    scene.nodes[id].name = rename_buffer;
                renaming_node = UINT32_MAX;
                rename_focus_set = false;
            }
            // Cancel on Escape
            else if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                renaming_node = UINT32_MAX;
                rename_focus_set = false;
            }
            // Cancel when clicking away (item was active then lost focus)
            else if (ImGui::IsItemDeactivated())
            {
                renaming_node = UINT32_MAX;
                rename_focus_set = false;
            }

            if (tree_open)
            {
                for (SceneID child : node.children)
                    walk_gui_scene_nodes(child);
                ImGui::TreePop();
            }
            return;
        }

        if (ImGui::TreeNodeEx(label.c_str(), options))
        {
            // Left click: select only this node (children are highlighted visually but not in selected_meshes)
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                if (!ImGui::IsKeyDown(ImGuiKey_LeftShift) && !ImGui::IsKeyDown(ImGuiKey_RightShift))
                {
                    selected_meshes.clear();
                    selected_vertices.clear();
                }

                selected_mesh = glm::uvec3(id, 0, 0);
                selected_meshes.push_back(glm::uvec3(id, 0, 0));
            }

            // Right-click context menu
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            {
                selected_mesh = glm::uvec3(id, 0, 0);
                if (!is_highlighted)
                {
                    selected_meshes.clear();
                    selected_vertices.clear();
                    selected_meshes.push_back(glm::uvec3(id, 0, 0));
                }
                ImGui::OpenPopup("##node_context");
            }

            if (ImGui::BeginPopup("##node_context"))
            {
                if (ImGui::MenuItem("Rename"))
                {
                    renaming_node = id;
                    std::strncpy(rename_buffer, scene.nodes[id].name.c_str(), sizeof(rename_buffer) - 1);
                    rename_buffer[sizeof(rename_buffer) - 1] = '\0';
                    rename_focus_set = false;
                }
                ImGui::EndPopup();
            }

            // Allow this node to be dragged
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                ImGui::SetDragDropPayload("NODE_DRAG_PAYLOAD", &id, sizeof(id));
                ImGui::Text("Reparent Node");
                ImGui::EndDragDropSource();
            }

            // If target dragged to this node, reparent received node to this current node
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("NODE_DRAG_PAYLOAD"))
                {
                    SceneID new_child_id = *(SceneID*)payload->Data;

                    std::shared_ptr<std::vector<uint8_t>> connect_message = std::make_shared<std::vector<uint8_t>>();
                    connect_message->reserve(sizeof(Network::MessageType));
                    Network::MessageType type = Network::MessageType::REPARENT;
                    Network::WriteBytes(*connect_message, &type, sizeof(Network::MessageType));
                    Network::WriteU32(*connect_message, id);
                    Network::WriteU32(*connect_message, new_child_id);
                    client.send(connect_message);
                }
                ImGui::EndDragDropTarget();
            }

            for (SceneID child : node.children)
            {
                walk_gui_scene_nodes(child);
            }
            ImGui::TreePop();
        }
    }

    void App::create_gui_layout()
    {
        static bool init_loop = true;
        static bool style_initialized = false;

        if (!style_initialized)
        {
            setup_gui_style();
            style_initialized = true;
        }

        static bool mode_initialized = false;

        if (!mode_initialized)
        {
            mode = ToolMode::SELECT;
            mode_initialized = true;
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuiID main_dockspace = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        ImGuiID toolbar_id, workspace_id, filebrowser_id, objectmenu_id, scene_graph_id;
        if (init_loop)
        {
            init_loop = false;
            ImGui::DockBuilderRemoveNode(main_dockspace);
            ImGui::DockBuilderAddNode(main_dockspace);

            ImGuiID left_id, right_id, middle_id;
            ImGui::DockBuilderSplitNode(main_dockspace, ImGuiDir_Left, 0.20f, &left_id, &right_id);
            ImGui::DockBuilderSplitNode(right_id, ImGuiDir_Left, 0.85f, &middle_id, &scene_graph_id);
            ImGui::DockBuilderSplitNode(left_id, ImGuiDir_Up, 0.60f, &toolbar_id, &objectmenu_id);
            ImGui::DockBuilderSplitNode(middle_id, ImGuiDir_Down, 0.30f, &filebrowser_id, &workspace_id);
            ImGui::DockBuilderDockWindow("Workspace", workspace_id);
            ImGui::DockBuilderDockWindow("Toolbar", toolbar_id);
            ImGui::DockBuilderDockWindow("Node Menu", objectmenu_id);
            ImGui::DockBuilderDockWindow("File Browser##LoadModelKey", filebrowser_id);
            ImGui::DockBuilderDockWindow("Scene", scene_graph_id);

            ImGui::DockBuilderFinish(main_dockspace);
        }
    }

    void App::setup_gui_style()
    {
        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowPadding = ImVec2(10.0f, 10.0f);
        style.FramePadding = ImVec2(8.0f, 6.0f);
        style.ItemSpacing = ImVec2(8.0f, 8.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);

        style.WindowRounding = 5.0f;
        style.FrameRounding = 5.0f;
        style.PopupRounding = 5.0f;
        style.GrabRounding = 5.0f;
        style.ScrollbarRounding = 5.0f;
        
        // Main background
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.11f, 0.13f, 1.0f);

        // Borders
        style.Colors[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.30f, 0.5f);

        // Headers 
        style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.35f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.35f, 0.45f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.25f, 0.30f, 0.40f, 1.0f);

        // Title bar
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.15f, 1.0f);

        // Text
        style.Colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.95f, 1.0f);

        ImGui::GetIO().FontGlobalScale = 1.30f;
    }


    static bool ButtonWithShadow(const char* label, ImVec2 buttonSize, ImVec4 color)
    {
        
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        
        bool pressed = ImGui::Button(label, buttonSize);

        ImVec2 tl = ImGui::GetItemRectMin();
        ImVec2 br = ImGui::GetItemRectMax();
        ImVec2 size = ImGui::GetItemRectSize();

        float k = 0.3f;

        ImVec2 tl_middle(tl.x, tl.y + size.y * (1.0f - k));
        ImVec2 br_middle(br.x, tl.y + size.y * k);

        ImVec4 col_darker(0.0f, 0.0f, 0.0f, 0.20f);
        ImVec4 col_interm(0.0f, 0.0f, 0.0f, 0.10f);
        ImVec4 col_transp(0.0f, 0.0f, 0.0f, 0.0f);

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilledMultiColor(
            tl,
            br_middle,
            ImGui::GetColorU32(col_interm),
            ImGui::GetColorU32(col_interm),
            ImGui::GetColorU32(col_transp),
            ImGui::GetColorU32(col_transp)
        );

        drawList->AddRectFilledMultiColor(
            tl_middle,
            br,
            ImGui::GetColorU32(col_transp),
            ImGui::GetColorU32(col_transp),
            ImGui::GetColorU32(col_darker),
            ImGui::GetColorU32(col_darker)
        );

        ImGui::PopStyleColor();
        return pressed;
    }

    void App::shapes_section()
    {
        ImVec2 btn = ImVec2(120.0f, 28.0f);
        ImVec4 color = ImVec4(0.24f, 0.44f, 0.70f, 1.0f);

        ImGui::SeparatorText("Shapes");

        if (ButtonWithShadow("Triangle", btn, color))
        {
            primitive = Render::Primitive::TRIANGLE;
            mode = ToolMode::PLACE;
        } 

        ImGui::SameLine();

        if (ButtonWithShadow("Square", btn, color))
        {
            primitive = Render::Primitive::SQUARE;
            mode = ToolMode::PLACE;
        }

        if (ButtonWithShadow("Pyramid", btn, color))
        {
            primitive = Render::Primitive::PYRAMID;
            mode = ToolMode::PLACE;
        }

        ImGui::SameLine();

        if (ButtonWithShadow("Cube", btn, color))
        {
            primitive = Render::Primitive::CUBE;
            mode = ToolMode::PLACE;
        }

        if (ButtonWithShadow("Monkey Model", btn, color))
        {
            primitive = Render::Primitive::MONKEY;
            mode = ToolMode::PLACE;
        }

        // if (ButtonWithShadow("Load Model", btn, color))
        // {
        //     std::optional<Scene> model_scene = LoadNodeHierarchyFromFile("../assets/commodore_amiga_500__computer.glb");
        //     if (model_scene)
        //     {
        //         Network::MessageBuffer scene_message = Network::CreateSceneSerializationMessage(model_scene.value(), Network::MessageType::SEND_NODE_HIERARCHY);
        //         // client.send(scene_message);
        //         Network::MessageBuffer message = Network::CreateMessageBuffer(Network::MessageType::SEND_NODE_HIERARCHY);
        //         client.send(message);
        //         AppendSceneToScene(this->scene, model_scene.value(), this->scene.root);
        //     }
        //     else
        //     {
        //         std::cerr << "Failed to load scene" << std::endl;
        //     }
        //     mode = ToolMode::PLACE;
        // }
    }

    void App::camera_section()
    {
        ImGui::SeparatorText("Camera");

        ImVec2 btn = ImVec2(120.0f, 28.0f);
        ImVec4 color = ImVec4(0.24f, 0.44f, 0.70f, 1.0f);

        if (ButtonWithShadow("Arcball", btn, color))
        {
            cam_type = CameraType::ARCBALL;
            arcball_lookat = camera.get_position() + cam_forward * (float)cam_radius;
        }

        ImGui::SameLine();

        if (ButtonWithShadow("First-Person", ImVec2(140.0f, 28.0f), color))
        {
            cam_type = CameraType::FIRST_PERSON;
        }
    }

    void App::selection_section()
    {
        ImGui::SeparatorText("Selection");

        ImVec2 mode_btn = ImVec2(120.0f, 28.0f);
        ImVec4 color = ImVec4(0.24f, 0.44f, 0.70f, 1.0f);
        if (ButtonWithShadow("Select", mode_btn, color))
        { 
            mode = ToolMode::SELECT;
        }

        ImGui::SameLine();

        if (ButtonWithShadow("Object Mode", mode_btn, color))
        {
            select_granularity = SelectionGranularity::OBJECT;
            selected_mesh = { UNSELECTED_MESH, UNSELECTED_MESH, UNSELECTED_MESH };
        }


        if (ButtonWithShadow("Face Mode", mode_btn, color))
        {
            select_granularity = SelectionGranularity::FACE;
            selected_mesh = { UNSELECTED_MESH, UNSELECTED_MESH, UNSELECTED_MESH };
        }

        ImGui::SameLine();

        if (ButtonWithShadow("Vertex Mode", mode_btn, color))
        {
            select_granularity = SelectionGranularity::VERTEX;
            selected_mesh = { UNSELECTED_MESH, UNSELECTED_MESH, UNSELECTED_MESH };
        }

        bool delete_key_pressed = !ImGui::GetIO().WantTextInput && (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false));
        if (ButtonWithShadow("Delete", mode_btn, color) || delete_key_pressed)
        {
            if (!selected_meshes.empty())
            {
                std::sort(selected_meshes.begin(), selected_meshes.end(), [this](const glm::uvec3& a, const glm::uvec3& b) {
                    if (a[0] != b[0]) return a[0] > b[0];
                    if (select_granularity == SelectionGranularity::FACE) return a[1] > b[1];
                    if (select_granularity == SelectionGranularity::VERTEX) return a[2] > b[2];
                    return false;
                });

                for (std::size_t i = 0; i < selected_meshes.size(); i++) {
                    auto message = std::make_shared<std::vector<uint8_t>>();
                    Network::MessageType type = Network::MessageType::REMOVE;
                    Network::WriteBytes(*message, &type, sizeof(Network::MessageType));
                    Network::WriteBytes(*message, &select_granularity, sizeof(SelectionGranularity));
                    Network::WriteU32(*message, selected_meshes[i][0]);

                    if (select_granularity == SelectionGranularity::VERTEX)
                        Network::WriteU32(*message, selected_meshes[i][2]);
                    else if (select_granularity == SelectionGranularity::FACE)
                        Network::WriteU32(*message, selected_meshes[i][1]);

                    client.send(message);
                }
            }
        }

    }

    void App::transform_section()
    {
       
        ImGui::SeparatorText("Transform");

        ImVec2 btn = ImVec2(80.0f, 28.0f);
        ImVec4 color = ImVec4(0.24f, 0.44f, 0.70f, 1.0f);

        if (ButtonWithShadow("Grab", btn, color))
        {
            mode = ToolMode::GRAB;
            // TODO: PUT PREVIOUS WORLD POSITION VECTOR IN HERE TOO
        }

        ImGui::SameLine();

        if (ButtonWithShadow("Scale", btn, color))
        {
            mode = ToolMode::SCALE;
        }

        ImGui::SameLine();

        if (ButtonWithShadow("Rotate", btn, color))
        {
            mode = ToolMode::ROTATE;
        }


    }

    void App::network_section()
    {
        ImGui::SeparatorText("Network");

        ImVec2 connect_btn = ImVec2(180.0f, 28.0f);

        ImVec4 color = ImVec4(0.24f, 0.44f, 0.70f, 1.0f);

        if (ButtonWithShadow("Connect to Server", connect_btn, color))
        {  
            ImGui::OpenPopup("Connect to Server");
        } 

        if (ImGui::BeginPopupModal("Connect to Server", NULL, ImGuiWindowFlags_AlwaysAutoResize)) 
        {
            static char ipTextField[32] = "";    
            static char portTextField[32] = "";  

            ImGui::InputText("IP Address", ipTextField, IM_ARRAYSIZE(ipTextField));
            ImGui::InputText("Port", portTextField, IM_ARRAYSIZE(portTextField));

            if (ButtonWithShadow("Connect", ImVec2(90.0f, 28.0f), color))
            {
                selected_mesh = { UNSELECTED_MESH, UNSELECTED_MESH, UNSELECTED_MESH };
                mode = ToolMode::SELECT;
                previous_world_position = glm::vec3(0.0f);
                DeinitScene(scene);
                scene = InitScene();
                server.deinit();
                client.deinit();
                server.setSignaling(ipTextField, portTextField);
                server.init(scene);
                client.init(messages);
                if (!client.connect(ipTextField, portTextField))
                {
                    std::cout << "Connection Failed" << std::endl;
                    exit(EXIT_FAILURE);
                }
                
                // Send connect message
                {
                    std::shared_ptr<std::vector<uint8_t>> connect_message = std::make_shared<std::vector<uint8_t>>();
                    connect_message->reserve(sizeof(Network::MessageType));
                    Network::MessageType type = Network::MessageType::CONNECT;
                    Network::WriteBytes(*connect_message, &type, sizeof(Network::MessageType));
                    client.send(connect_message);
                }
            }

            ImGui::SameLine();

            if (ButtonWithShadow("Close", ImVec2(90.0f, 28.0f), color))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }


        if (ButtonWithShadow("Disconnect", connect_btn, color))
        {
            // Send disconnect message
            {
                std::shared_ptr<std::vector<uint8_t>> disconnect_message = std::make_shared<std::vector<uint8_t>>();
                disconnect_message->reserve(sizeof(Network::MessageType));
                Network::MessageType type = Network::MessageType::DISCONNECT;
                Network::WriteBytes(*disconnect_message, &type, sizeof(Network::MessageType));
                client.send(disconnect_message);
            }

            DeinitScene(scene);
            scene = InitScene();
            selected_mesh = { UNSELECTED_MESH, UNSELECTED_MESH, UNSELECTED_MESH };
            mode = ToolMode::SELECT;
            previous_world_position = glm::vec3(0.0f);
            client.deinit();
            server.deinit();
            server.setSignaling("127.0.0.1", "8000");
            server.init(scene);
            client.init(messages);
            if (!client.connect("127.0.0.1","8000"))
            {
                std::cout << "Reconnection to localhost Failed" << std::endl;
                return;
            }

            // Send connect message
            {
                std::shared_ptr<std::vector<uint8_t>> connect_message = std::make_shared<std::vector<uint8_t>>();
                connect_message->reserve(sizeof(Network::MessageType));
                Network::MessageType type = Network::MessageType::CONNECT;
                Network::WriteBytes(*connect_message, &type, sizeof(Network::MessageType));
                client.send(connect_message);
            }
        }
    }

    void App::status_section()
    {
        ImGui::SeparatorText("Status");

        if (mode == ToolMode::SELECT)
        {
            ImGui::Text("Mode: Select");
        }
        else if (mode == ToolMode::PLACE)
        {
            ImGui::Text("Mode: Place");
        }
        else if (mode == ToolMode::GRAB)
        {
            ImGui::Text("Mode: Grab");
        }
        else if (mode == ToolMode::SCALE)
        {
            ImGui::Text("Mode: Scale");
        }
        else if (mode == ToolMode::ROTATE)
        {
            ImGui::Text("Mode: Rotate");
        }

        ImGui::Text("Select Granularity: %d", select_granularity);
    }

    void App::build_toolbar()
    {
        ImGui::Begin("Toolbar");

        shapes_section();
        selection_section();
        transform_section();
        camera_section();
        network_section();
        status_section();

        ImGui::End();
    }

    void App::build_scene_view()
    {
        ImGui::Begin("Scene");

        walk_gui_scene_nodes(scene.root);

        // F2 or Enter to rename the selected node (only when not already in a text input)
        if (selected_mesh[0] != UNSELECTED_MESH && renaming_node == UINT32_MAX && !ImGui::GetIO().WantTextInput)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_F2, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false))
            {
                renaming_node = selected_mesh[0];
                std::strncpy(rename_buffer, scene.nodes[renaming_node].name.c_str(), sizeof(rename_buffer) - 1);
                rename_buffer[sizeof(rename_buffer) - 1] = '\0';
                rename_focus_set = false;
            }
        }

        ImGui::End();
    }

    void App::build_node_menu()
    {
        ImGui::Begin("Node Menu");

        // Display stats of selected object
        if (selected_mesh[0] != UNSELECTED_MESH)
        {

            SceneNode& node = scene.nodes[selected_mesh[0]];

            std::string name = node.name + " (ID: " + std::to_string(selected_mesh[0]) + ")";

            ImGui::Text("%s", name.c_str());

            const ImGuiTreeNodeFlags tree_options = ImGuiTreeNodeFlags_DefaultOpen;

            if (ImGui::TreeNodeEx("Transform", tree_options))
            {

                if (ImGui::DragFloat3("Translation", (float*)&node.local_transform.translation[0]))
                {
                    node.local_transform.needs_rebuild = true;
                }

                // FIXME: This is not really correct right now... Kind of messed up...
                if (ImGui::DragFloat4("Rotation", (float*)&node.local_transform.rotation[0], 0.005f))
                {
                    node.local_transform.needs_rebuild = true;
                }

                if (ImGui::DragFloat3("Scale", (float*)&node.local_transform.scale[0], 0.05f))
                {
                    node.local_transform.needs_rebuild = true;
                }

                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Mesh", tree_options))
            {
                if (node.mesh)
                {
                    ImGui::Text("%p", node.mesh.get());
                }
                else
                {
                    ImGui::Text("None");
                }
                ImGui::TreePop();
            }

            if (select_granularity == SelectionGranularity::VERTEX) {
              ImGui::Text("Vertex: %d", selected_mesh[2]);
            }

        }

        ImGui::End();
    }


    void App::present_gui()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.15, 0.15, 0.15, 1.0);
        int width, height;
        glfwGetFramebufferSize(window_handle, &width, &height);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Present to screen
        glfwSwapBuffers(window_handle);
    }
}

