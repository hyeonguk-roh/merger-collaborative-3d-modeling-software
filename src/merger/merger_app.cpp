#include "merger.h"
#include "network.h"
#include "render.h"
#include "scene.h"
#include <iostream>

namespace Merger {
App::App() {}

bool App::init_window(uint32_t width, uint32_t height, const char *title) {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  window_handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
  // Might want to print error here
  if (!window_handle)
    return false;
  const GLFWvidmode *vidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());
  glfwSwapInterval(vidmode->refreshRate);
  glfwMakeContextCurrent(window_handle);
  return true;
}

bool App::init(uint32_t width, uint32_t height, const char *title,
               bool start_server) {
  // Init window
  if (!init_window(width, height, title)) {
    std::cerr << "Failed to initialize window" << std::endl;
    return false;
  }

  // Init renderer
  Render::InitContext(window_handle);
  ImGuiFileDialog::Instance()->OpenDialog("LoadModelKey", "File Browser", ".glb, .fbx");
  
  glPointSize(10.0f);

  glGenVertexArrays(1, &grid_vao);

  main_rt = Render::CreateRenderTarget();
  Render::RenderTargetSetColorAttachment(
      main_rt, Render::CreateTexture(
                   width, height, Render::InternalFormat::RGBA_UBYTE,
                   Render::ExternalFormat::RGBA, Render::UploadType::UBYTE));
  Render::RenderTargetSetDepthAttachment(main_rt, width, height);
  if (!Render::VerifyRenderTarget(main_rt)) {
    std::cerr << "Failed to create main framebuffer" << std::endl;
    return false;
  }

  id_rt = Render::CreateRenderTarget();
  Render::RenderTargetSetColorAttachment(
      id_rt, Render::CreateTexture(
                 width, height, Render::InternalFormat::RGB_UINT32,
                 Render::ExternalFormat::RGB_INT, Render::UploadType::UINT));
  Render::RenderTargetSetDepthAttachment(id_rt, width, height);
  if (!Render::VerifyRenderTarget(id_rt)) {
    std::cerr << "Failed to create id framebuffer" << std::endl;
    return false;
  }

  if (!Render::LoadShader(&shader, "../shader/default.vert",
                          "../shader/default.frag")) {
    std::cerr << "Failed to load shader" << std::endl;
    return false;
  }

  if (!Render::LoadShader(&id_shader, "../shader/id_picking.vert",
                          "../shader/id_picking.frag")) {
    std::cerr << "Failed to load shader" << std::endl;
    return false;
  }

  if (!Render::LoadShader(&vertex_view_shader, "../shader/vertex_view.vert",
                          "../shader/vertex_view.frag")) {
    std::cerr << "Failed to load shader" << std::endl;
    return false;
  }

  if (!Render::LoadShader(&vertex_view_shader, "../shader/vertex_view.vert",
                          "../shader/vertex_view.frag")) {
    std::cerr << "Failed to load shader" << std::endl;
    return false;
  }

  if (!Render::LoadShader(&grid_shader, "../shader/grid.vert",
                          "../shader/grid.frag")) {
    std::cerr << "Failed to load shader" << std::endl;
    return false;
  }

  if (!Render::LoadShader(&solid_shader, "../shader/solid_color.vert", "../shader/solid_color.frag"))
  {
    std::cerr << "Failed to load shader" << std::endl;
    return false;
  }

  // camera.set_orthographic(0, width, 0, height, -10.0f, 10.0f);
  camera.set_perspective(glm::radians(45.0f), (float)width / (float)height,
                         0.1f, 100.0f);
  
  const double theta_radians = glm::radians(theta);
  const double phi_radians = glm::radians(phi);
  cam_forward = glm::normalize(glm::vec3(sin(theta_radians) * cos(phi_radians),
                                        sin(phi_radians),
                                        -cos(theta_radians) * cos(phi_radians)));
  camera.set_position(arcball_lookat - cam_forward * (float)cam_radius);
  camera.set_lookat(arcball_lookat);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  mode = ToolMode::PLACE;
  scene = InitScene();
  if (start_server){
    signaling.init();
  }
  server.setSignaling("127.0.0.1", "8000");
  if (start_server)
    server.init(scene);
  client.init(messages);

  // Send connect message
  {
    if (!client.connect("127.0.0.1", "8000")) {
      std::cout << "Local auto-connect failed\n";
    } else {
      std::shared_ptr<std::vector<uint8_t>> connect_message =
          std::make_shared<std::vector<uint8_t>>();
      connect_message->reserve(sizeof(Network::MessageType));
      Network::MessageType type = Network::MessageType::CONNECT;
      Network::WriteBytes(*connect_message, &type,
                          sizeof(Network::MessageType));
      client.send(connect_message);
    }
  }

  return true;
}

void App::run() {
  double previous_mouse_x, previous_mouse_y;
  glfwGetCursorPos(window_handle, &previous_mouse_x, &previous_mouse_y);

  while (!glfwWindowShouldClose(window_handle)) {
    glfwPollEvents();

    while (!messages.empty()) {
      Network::Message msg = messages.pop();
      switch (msg.type) {
      case Network::MessageType::ADD:
        handle_add_message(msg);
        break;
      case Network::MessageType::REMOVE:
        handle_remove_message(msg);
        break;
      case Network::MessageType::TRANSLATE:
        handle_translate_message(msg);
        break;
      case Network::MessageType::CONNECT:
        handle_connect_message(msg);
        break;
      case Network::MessageType::ROTATE:
        handle_rotate_message(msg);
        break;
      case Network::MessageType::SCALE:
        handle_scale_message(msg);
        break;
      case Network::MessageType::REPARENT:
        handle_reparent_message(msg);
        break;
      case Network::MessageType::SEND_NODE_HIERARCHY:
        handle_hierarchy_receive(msg);
        break;
      default:
        break;
      };
    }

    double current_mouse_x, current_mouse_y;
    glfwGetCursorPos(window_handle, &current_mouse_x, &current_mouse_y);
    mouse_delta = glm::vec2(current_mouse_x - previous_mouse_x,
                            -(current_mouse_y - previous_mouse_y));
    previous_mouse_x = current_mouse_x;
    previous_mouse_y = current_mouse_y;

    // Do input management
    // Do network stuff
    // Do render stuff
    // Create GUI
    create_gui_layout();
    ImGuizmo::BeginFrame();

    // Define GUI stuff
    build_toolbar();

    // ImGui::Begin("File Browser");
    // ImGui::End();

    build_scene_view();

    // Main workspace GUI and rendering tied together
    // Should fix this
    ImGui::Begin("Workspace");
    ImVec2 size = ImGui::GetContentRegionAvail();
    if ((size.x != workspace_size.x || size.y != workspace_size.y) &&
        glfwGetMouseButton(window_handle, GLFW_MOUSE_BUTTON_LEFT) ==
            GLFW_RELEASE) {
      workspace_size = glm::vec2(size.x, size.y);

      // TEMP: START
      // renderer.resize_viewport(workspace_size);
      Render::ResizeRenderTarget(main_rt, workspace_size);
      Render::ResizeRenderTarget(id_rt, workspace_size);

      // camera.set_orthographic(0.0f, workspace_size.x, 0.0f, workspace_size.y,
      // -10.0f, 10.0f);
      camera.set_perspective(glm::radians(45.0f),
                             (float)workspace_size.x / (float)workspace_size.y,
                             0.1f, 100.0f);
      // TEMP: END
    }

    const double theta_radians = glm::radians(theta);
    const double phi_radians = glm::radians(phi);

    // Negate the Z component because angles of theta = 0 and phi = 0 should return a vector of (0, 0, -1)
    cam_forward = glm::normalize(glm::vec3(sin(theta_radians) * cos(phi_radians),
                                             sin(phi_radians),
                                             -cos(theta_radians) * cos(phi_radians)));

    if (cam_type == CameraType::FIRST_PERSON)
    {
      glm::vec3 cam_position = camera.get_position();
      if (ImGui::IsKeyDown(ImGuiKey_W))
      {
        cam_position += cam_forward * CAM_SPEED;
      }

      if (ImGui::IsKeyDown(ImGuiKey_S))
      {
        cam_position -= cam_forward * CAM_SPEED;
      }

      if (ImGui::IsKeyDown(ImGuiKey_A))
      {
        cam_position -= glm::normalize(glm::cross(cam_forward, glm::vec3(0.0f, 1.0f, 0.0))) * CAM_SPEED;
      }

      if (ImGui::IsKeyDown(ImGuiKey_D))
      {
        cam_position += glm::normalize(glm::cross(cam_forward, glm::vec3(0.0f, 1.0f, 0.0f))) * CAM_SPEED;
      }

      if (ImGui::IsKeyDown(ImGuiKey_Space))
      {
        cam_position += glm::vec3(0.0f, 1.0f, 0.0f) * CAM_SPEED;
      }

      if (ImGui::IsKeyDown(ImGuiKey_LeftShift))
      {
        cam_position -= glm::vec3(0.0f, 1.0f, 0.0f) * CAM_SPEED;
      }

      camera.set_position(cam_position);
      camera.set_lookat(cam_position + cam_forward);
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.MouseWheel != 0.0 && cam_type == CameraType::ARCBALL && ImGui::IsWindowHovered())
    {
      cam_radius -= 0.5 * io.MouseWheel;
      camera.set_position(arcball_lookat - cam_forward * (float)cam_radius);
      camera.set_lookat(arcball_lookat);
    }
    
    // Handle camera movement (only when hovering over workspace)
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) 
    {
      theta += mouse_delta.x;
      phi += mouse_delta.y;
      if (phi > 89.9f)
        phi = 89.9;
      else if (phi < -89.9f)
        phi = -89.9;

      if (cam_type == CameraType::ARCBALL)
      {
        camera.set_position(arcball_lookat - cam_forward * (float)cam_radius);
        camera.set_lookat(arcball_lookat);
      }
      else
      {
        camera.set_lookat(camera.get_position() + cam_forward);
      }
    }
    // Check mouse in correct bounds
    ImVec2 min = ImGui::GetWindowContentRegionMin();
    ImVec2 max = ImGui::GetWindowContentRegionMax();
    ImVec2 window_pos = ImGui::GetWindowPos();
    min.x += window_pos.x;
    min.y += window_pos.y;
    max.x += window_pos.x;
    max.y += window_pos.y;
    ImVec2 mouse_pos = ImGui::GetMousePos();
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(min.x, min.y, size.x, size.y);
    ImGuizmo::SetOrthographic(false);
    glm::vec2 mouse_canvas_pos = glm::vec2(
        mouse_pos.x - min.x, workspace_size.y - (mouse_pos.y - min.y));
    
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_A, false) && ImGui::GetIO().KeyShift && mouse_pos.x >= min.x && mouse_pos.x <= max.x && mouse_pos.y >= min.y && mouse_pos.y <= max.y && !ImGuizmo::IsOver())
    {
        ImGui::OpenPopup("WorkspaceContextMenu");
    }

    if (ImGui::BeginPopup("WorkspaceContextMenu"))
    {
        
        if (ImGui::MenuItem("Add Triangle"))
        {
            primitive = Render::Primitive::TRIANGLE;
            handle_place_click(glm::vec3(0.0f, 0.0f, 0.0f));
            
        }

        if (ImGui::MenuItem("Add Square"))
        {
            primitive = Render::Primitive::SQUARE;
            handle_place_click(glm::vec3(0.0f, 0.0f, 0.0f));
            
        }

        if (ImGui::MenuItem("Add Pyramid"))
        {
            primitive = Render::Primitive::PYRAMID;
            handle_place_click(glm::vec3(0.0f, 0.0f, 0.0f));
            
        }

        if (ImGui::MenuItem("Add Cube"))
        {
            primitive = Render::Primitive::CUBE;
            handle_place_click(glm::vec3(0.0f, 0.0f, 0.0f));
            
        }

        if (ImGui::MenuItem("Add Monkey Model"))
        {
            primitive = Render::Primitive::MONKEY;
            handle_place_click(glm::vec3(0.0f, 0.0f, 0.0f));
            
        }

        ImGui::EndPopup();
      }


    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mouse_pos.x >= min.x &&
        mouse_pos.x <= max.x && mouse_pos.y >= min.y && mouse_pos.y <= max.y &&
        !ImGuizmo::IsOver()) {
      switch (mode) {
      case ToolMode::PLACE:
        // TODO: CHANGE THIS TO ALWAYS PLACE OBJECTS AT THE ORIGIN
        handle_place_click(glm::vec3(0.0f, 0.0f, 0.0f));
        break;
      case ToolMode::SELECT:
        handle_select_click(mouse_canvas_pos);
        break;
      case ToolMode::GRAB:
      case ToolMode::ROTATE:
      case ToolMode::SCALE:
        mode = ToolMode::SELECT;
        break;
      default:
        break;
      };
    }

    // ImGui::Image(renderer.get_texture(), size, ImVec2(0, 1), ImVec2(1, 0));
    ImGui::Image(main_rt.color_attachment.id, size, ImVec2(0, 1), ImVec2(1, 0));
    ImGui::End();

    // TODO: ALSO NEED TO ADD THIS LOGIC TO THE GUI BUTTON CONTROLS AS WELL
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_G, false) &&
        !selected_meshes.empty()) {
      mode = ToolMode::GRAB;
    }
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_R, false) &&
        !selected_meshes.empty()) {
      mode = ToolMode::ROTATE;
    }
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_S, false) &&
        !selected_meshes.empty()) {
      mode = ToolMode::SCALE;
    }

    if (!selected_meshes.empty() && selected_mesh[0] >= scene.nodes.size()) {
      selected_meshes.clear();
      selected_mesh = glm::uvec3(UNSELECTED_MESH, UNSELECTED_MESH, UNSELECTED_MESH);
      if (mode == ToolMode::GRAB || mode == ToolMode::ROTATE || mode == ToolMode::SCALE)
        mode = ToolMode::SELECT;
    }

    if (!selected_meshes.empty() &&
        (mode == ToolMode::GRAB || mode == ToolMode::ROTATE ||
         mode == ToolMode::SCALE)) {
      ImGuizmo::OPERATION current_gizmo_op = ImGuizmo::TRANSLATE;
      if (mode == ToolMode::GRAB)
        current_gizmo_op = ImGuizmo::TRANSLATE;
      else if (mode == ToolMode::ROTATE)
        current_gizmo_op = ImGuizmo::ROTATE;
      else if (mode == ToolMode::SCALE)
        current_gizmo_op = ImGuizmo::SCALE;

      glm::mat4 view_matrix = camera.get_view();
      glm::mat4 projection_matrix = camera.get_projection();

      SceneNode &selected_node = scene.nodes[selected_mesh[0]];
      if (selected_node.local_transform.needs_rebuild)
        Render::RebuildTransformMatrix(selected_node.local_transform);

      glm::mat4 &world_matrix = selected_node.world_matrix;

      glm::vec3 local_center(0.0f);
      if (select_granularity == SelectionGranularity::VERTEX &&
          selected_node.mesh && selected_mesh[2] < selected_node.mesh->vertices.size()) {
        local_center = selected_node.mesh->vertices[selected_mesh[2]].position;
      } else if (select_granularity == SelectionGranularity::FACE &&
                 selected_node.mesh && selected_mesh[1] < selected_node.mesh->faces.size()) {
        auto &face = selected_node.mesh->faces[selected_mesh[1]];
        for (int i = 0; i < face.vert_count; ++i) {
          if (face.vertex_ids[i] < selected_node.mesh->vertices.size()) {
            local_center +=
                selected_node.mesh->vertices[face.vertex_ids[i]].position;
          }
        }
        local_center /= (float)face.vert_count;
      }

      static glm::mat4 gizmo_cache = glm::mat4(1.0f);
      static bool was_using_gizmo = false;

      if (!was_using_gizmo) {
        if (select_granularity == SelectionGranularity::OBJECT) {
          gizmo_cache = world_matrix;
        } else {
          glm::vec3 world_pos =
              glm::vec3(world_matrix * glm::vec4(local_center, 1.0f));
          gizmo_cache = glm::translate(glm::mat4(1.0f), world_pos);
        }
      }

      glm::mat4 gizmo_matrix = gizmo_cache;

      glm::mat4 model_matrix_copy = gizmo_matrix;
      glm::mat4 delta_matrix = glm::mat4(1.0f);

      ImGuizmo::Manipulate(
          glm::value_ptr(view_matrix), glm::value_ptr(projection_matrix),
          current_gizmo_op, current_gizmo_mode,
          glm::value_ptr(model_matrix_copy), glm::value_ptr(delta_matrix));

      gizmo_cache = model_matrix_copy;
      was_using_gizmo = ImGuizmo::IsUsing();

      if (ImGuizmo::IsUsing()) {
        glm::vec3 translation_delta, rotation_delta_euler, scale_delta;
        ImGuizmo::DecomposeMatrixToComponents(
            glm::value_ptr(delta_matrix), glm::value_ptr(translation_delta),
            glm::value_ptr(rotation_delta_euler), glm::value_ptr(scale_delta));

        if (select_granularity != SelectionGranularity::OBJECT) {
          glm::mat3 inv_model_rot_scale = glm::inverse(glm::mat3(world_matrix));
          translation_delta = inv_model_rot_scale * translation_delta;
        }

        if (current_gizmo_op == ImGuizmo::TRANSLATE) {
          if (glm::length(translation_delta) > 0.0001f) {
            for (std::size_t i = 0; i < selected_meshes.size(); i++) {
              auto message = std::make_shared<std::vector<uint8_t>>();
              Network::MessageType type = Network::MessageType::TRANSLATE;
              Network::WriteBytes(*message, &type, sizeof(Network::MessageType));
              Network::WriteBytes(*message, &select_granularity,
                                sizeof(SelectionGranularity));
              Network::WriteU32(*message, selected_meshes[i][0]);
              if (select_granularity == SelectionGranularity::VERTEX)
                Network::WriteU32(*message, selected_meshes[i][2]);
              else if (select_granularity == SelectionGranularity::FACE)
                Network::WriteU32(*message, selected_meshes[i][1]);
              Network::WriteF32(*message, translation_delta.x);
              Network::WriteF32(*message, translation_delta.y);
              Network::WriteF32(*message, translation_delta.z);
              client.send(message);
            }
          }
        } else if (current_gizmo_op == ImGuizmo::ROTATE) {
          if (glm::length(rotation_delta_euler) > 0.0001f) {
            glm::quat rotation_step =
                glm::quat(glm::radians(rotation_delta_euler));
            for (std::size_t i = 0; i < selected_meshes.size(); i++) {
              auto message = std::make_shared<std::vector<uint8_t>>();
              Network::MessageType type = Network::MessageType::ROTATE;
              Network::WriteBytes(*message, &type, sizeof(Network::MessageType));
              Network::WriteBytes(*message, &select_granularity,
                                  sizeof(SelectionGranularity));
              Network::WriteU32(*message, selected_meshes[i][0]);
              if (select_granularity == SelectionGranularity::VERTEX)
                Network::WriteU32(*message, selected_meshes[i][2]);
              else if (select_granularity == SelectionGranularity::FACE)
                Network::WriteU32(*message, selected_meshes[i][1]);
              Network::WriteBytes(*message, &rotation_step, sizeof(glm::quat));
              client.send(message);
            }
          }
        } else if (current_gizmo_op == ImGuizmo::SCALE) {
          if (glm::length(scale_delta - glm::vec3(1.0f)) > 0.0001f) {
            for (std::size_t i = 0; i < selected_meshes.size(); i++) {
              auto message = std::make_shared<std::vector<uint8_t>>();
              Network::MessageType type = Network::MessageType::SCALE;
              Network::WriteBytes(*message, &type, sizeof(Network::MessageType));
              Network::WriteBytes(*message, &select_granularity,
                                  sizeof(SelectionGranularity));
              Network::WriteU32(*message, selected_meshes[i][0]);
              if (select_granularity == SelectionGranularity::VERTEX)
                Network::WriteU32(*message, selected_meshes[i][2]);
              else if (select_granularity == SelectionGranularity::FACE)
                Network::WriteU32(*message, selected_meshes[i][1]);
              Network::WriteF32(*message, scale_delta.x);
              Network::WriteF32(*message, scale_delta.y);
              Network::WriteF32(*message, scale_delta.z);
              client.send(message);
            }
          }
        }
      }
    }

    build_node_menu();

    // Note: This can actually come anywhere in the frame so long as the imgui
    // window has a reference to the texture TEMP: START
    float main_clear_color[] = {0.15f, 0.15f, 0.15f, 1.0f};
    Render::BeginRender(main_rt, main_clear_color);

    std::vector<SceneDrawable> drawables = CollectSceneDrawables(scene);

    for (SceneDrawable &drawable : drawables) 
    {
      glUseProgram(shader);

      glUniform3fv(glGetUniformLocation(shader, "light_pos"), 1, glm::value_ptr(camera.get_position()));
      glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(camera.get_projection()));
      glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, glm::value_ptr(camera.get_view()));
      if (select_granularity == SelectionGranularity::OBJECT) 
      {
        bool is_selected_mesh = is_node_or_descendant_selected(drawable.id);
        glUniform1i(glGetUniformLocation(shader, "selected"), is_selected_mesh);
      }
      else
      {
        glUniform1i(glGetUniformLocation(shader, "selected"), 0);
      }

      if (drawable.mesh.needs_rebuild) Render::RebuildMesh(drawable.mesh);

      // Will need to have logic to rebuild transform if necessary (i.e. if
      // needs_rebuild == true)
      glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(drawable.transform));
      glBindVertexArray(drawable.mesh.draw_data.vao);
      glDrawElements(GL_TRIANGLES, drawable.mesh.draw_data.triangle_count * 3, GL_UNSIGNED_INT, nullptr);

      if (select_granularity == SelectionGranularity::VERTEX) 
      {
        glUseProgram(vertex_view_shader);
        glUniformMatrix4fv(glGetUniformLocation(vertex_view_shader, "projection"), 1, GL_FALSE, glm::value_ptr(camera.get_projection()));
        glUniformMatrix4fv(glGetUniformLocation(vertex_view_shader, "view"), 1, GL_FALSE, glm::value_ptr(camera.get_view()));
        glUniformMatrix4fv(glGetUniformLocation(vertex_view_shader, "model"), 1, GL_FALSE, glm::value_ptr(drawable.transform));
        glBindVertexArray(drawable.mesh.draw_data.vao);
        
        uint32_t selected_uniform_location = glGetUniformLocation(vertex_view_shader, "selected");

        // Draw all vertices at first to get them on the screen
        glUniform1ui(selected_uniform_location, 0);
        glDrawArrays(GL_POINTS, 0, drawable.mesh.draw_data.triangle_count * 3);

        // Now render all the selected vertices of this object in magenta
        // Set depth func to <= so the vertex renders on top of the already rendered vertices
        glDepthFunc(GL_LEQUAL);
        glUniform1ui(selected_uniform_location, 1);
        for (uint32_t cpu_vertex_id : selected_vertices[drawable.id])
        {
          uint32_t gpu_vertex_id = drawable.mesh.draw_data.gpu_vertex_map[cpu_vertex_id][0];
          glDrawArrays(GL_POINTS, gpu_vertex_id, 1);
        }

        glDepthFunc(GL_LESS);
      } 
      else if (select_granularity == SelectionGranularity::FACE) 
      {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -1.0f);

        for (std::size_t i = 0; i < selected_meshes.size(); i++) 
        {
          if (drawable.id == selected_meshes[i][0]) 
          {
            glUniform1i(glGetUniformLocation(shader, "selected"), 1);

            GLuint face_index = selected_meshes[i][1];
            size_t index_offset = 0;

            for (size_t i = 0; i < face_index; i++) 
            {
              if (drawable.mesh.faces[i].vert_count == 3)
                index_offset += 3;
              else if (drawable.mesh.faces[i].vert_count == 4)
                index_offset += 6;
            }

            if (drawable.mesh.faces[face_index].vert_count == 3) 
            {
              glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, (void *)(index_offset * sizeof(GLuint)));
            } 
            else if (drawable.mesh.faces[face_index].vert_count == 4) 
            {
              glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void *)(index_offset * sizeof(GLuint)));
            }
          }
        }

        // Draw wireframe mesh over current mesh
        glUseProgram(solid_shader);
        glUniformMatrix4fv(glGetUniformLocation(solid_shader, "projection"), 1, GL_FALSE, glm::value_ptr(camera.get_projection()));
        glUniformMatrix4fv(glGetUniformLocation(solid_shader, "view"), 1, GL_FALSE, glm::value_ptr(camera.get_view()));
        glUniformMatrix4fv(glGetUniformLocation(solid_shader, "model"), 1, GL_FALSE, glm::value_ptr(drawable.transform));
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawElements(GL_TRIANGLES, drawable.mesh.draw_data.triangle_count * 3, GL_UNSIGNED_INT, nullptr);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_POLYGON_OFFSET_FILL);
      }

      glBindVertexArray(0);
    }

    // Draw Grid
    glUseProgram(grid_shader);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(grid_vao);
    glUniformMatrix4fv(glGetUniformLocation(grid_shader, "projection"), 1,
                       GL_FALSE, glm::value_ptr(camera.get_projection()));
    glUniformMatrix4fv(glGetUniformLocation(grid_shader, "view"), 1, GL_FALSE,
                       glm::value_ptr(camera.get_view()));
    glUniform3fv(glGetUniformLocation(grid_shader, "cam_world_pos"), 1,
                 glm::value_ptr(camera.get_position()));
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // renderer.present();
    // TEMP: END

    if (ImGuiFileDialog::Instance()->Display("LoadModelKey"))
    {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
            std::optional<Scene> load_scene = LoadNodeHierarchyFromFile(path.c_str());
            if (load_scene)
            {
                Network::MessageBuffer hierarchy_message = Network::CreateSceneSerializationMessage(load_scene.value(), Network::MessageType::SEND_NODE_HIERARCHY);
                client.send(hierarchy_message);
            }
        }

        // ImGuiFileDialog::Instance()->Close();
    }

    present_gui();
  }
}

void App::handle_place_click(const glm::vec3 &create_pos) {
  std::shared_ptr<std::vector<uint8_t>> message =
      std::make_shared<std::vector<uint8_t>>();
  message->reserve(2 * sizeof(uint8_t));
  Network::MessageType type = Network::MessageType::ADD;
  Network::WriteBytes(*message, &type, sizeof(Network::MessageType));
  Network::WriteBytes(*message, &primitive, sizeof(Render::Primitive));
  Network::WriteF32(*message, create_pos.x);
  Network::WriteF32(*message, create_pos.y);
  Network::WriteF32(*message, create_pos.z);
  Network::WriteU32(*message, 0); // Parent ID
  client.send(message);
  mode = ToolMode::SELECT;
}

void App::handle_reparent_message(const Network::Message& msg)
{
    uint8_t* ptr = (uint8_t*)msg.data.data();
    SceneID parent = *(uint32_t*)ptr;
    ptr += sizeof(SceneID);
    SceneID child = *(uint32_t*)ptr;
    ReparentNode(scene, child, parent);
}

void App::handle_add_message(const Network::Message &msg) {
  const uint8_t *ptr = msg.data.data();

  ptr += sizeof(Render::Primitive);

  glm::vec3 create_pos = *(glm::vec3 *)ptr;
  ptr += sizeof(glm::vec3);

  SceneID parent_id = *(SceneID *)ptr;
  ptr += sizeof(SceneID);

  uint32_t object_id = *(uint32_t *)ptr;

  // Primitive type is at first byte in the data array
  switch (static_cast<Render::Primitive>(msg.data[0])) {
  case Render::Primitive::TRIANGLE: {
    SceneID id = CreateNode(scene, parent_id);
    NodeAttachMesh(scene, id,
                   std::make_unique<Render::Mesh>(Render::GenerateTriangle(
                       create_pos, glm::vec2(1.0f), object_id)));
  } break;
  case Render::Primitive::SQUARE: {
    SceneID id = CreateNode(scene, parent_id);
    NodeAttachMesh(scene, id,
                   std::make_unique<Render::Mesh>(Render::GenerateSquare(
                       create_pos, glm::vec2(1.0f), object_id)));
  } break;
  case Render::Primitive::PYRAMID: {
    SceneID id = CreateNode(scene, parent_id);
    NodeAttachMesh(scene, id,
                   std::make_unique<Render::Mesh>(Render::GeneratePyramid(
                       create_pos, glm::vec2(1.0f), 1.0f, object_id)));
  } break;
  case Render::Primitive::CUBE: {
    SceneID id = CreateNode(scene, parent_id);
    NodeAttachMesh(scene, id,
                   std::make_unique<Render::Mesh>(Render::GenerateCube(
                       create_pos, glm::vec3(1.0f), object_id)));
  } break;
  case Render::Primitive::MONKEY: {
      std::optional<Scene> monkey_scene = LoadNodeHierarchyFromFile("../model.glb");
      AppendSceneToScene(scene, monkey_scene.value(), scene.root);
  } break;
  }
}

void App::handle_select_click(const glm::vec2 &mouse_pos) {
  unsigned int clear_val[] = {UNSELECTED_MESH, UNSELECTED_MESH, UNSELECTED_MESH,
                              UNSELECTED_MESH};
  Render::BeginRender(id_rt, clear_val);

  std::vector<SceneDrawable> drawables = CollectSceneDrawables(scene);

  glUseProgram(id_shader);
  glUniformMatrix4fv(glGetUniformLocation(id_shader, "projection"), 1, GL_FALSE,
                     glm::value_ptr(camera.get_projection()));
  glUniformMatrix4fv(glGetUniformLocation(id_shader, "view"), 1, GL_FALSE,
                     glm::value_ptr(camera.get_view()));

  for (SceneDrawable &drawable : drawables) {
    glUniform1ui(glGetUniformLocation(id_shader, "model_id"), drawable.id);
    glUniformMatrix4fv(glGetUniformLocation(id_shader, "model"), 1, GL_FALSE,
                       glm::value_ptr(drawable.transform));
    glBindVertexArray(drawable.mesh.pick_data.vao);

    switch (select_granularity) {
    case SelectionGranularity::OBJECT:
    case SelectionGranularity::FACE:
      glDrawArrays(GL_TRIANGLES, 0, drawable.mesh.pick_data.vertex_count);
      break;
    case SelectionGranularity::VERTEX:
      glDrawArrays(GL_POINTS, 0, drawable.mesh.pick_data.vertex_count);
      break;
    default:
      break;
    }
  }

  unsigned int channels[3] = {};
  glReadPixels(mouse_pos.x, mouse_pos.y, 1, 1, GL_RGB_INTEGER, GL_UNSIGNED_INT,
               &channels);
  // std::cout << "Selected Object: " << channels[0] << std::endl;
  // std::cout << "Selected Face: " << channels[1] << std::endl;
  // std::cout << "Selected Vertex: " << channels[2] << std::endl;
  selected_mesh = glm::uvec3(channels[0], channels[1], channels[2]);
  if (selected_mesh[0] == UNSELECTED_MESH) {
    selected_meshes.clear();
    selected_vertices.clear();
    return;
  }
  if (!ImGui::IsKeyDown(ImGuiKey_LeftShift) && !ImGui::IsKeyDown(ImGuiKey_RightShift)) {
    selected_meshes.clear();
    selected_vertices.clear();
  } 
  selected_meshes.push_back(selected_mesh);
  for (std::size_t i = 0; i < selected_vertices[selected_mesh[0]].size(); i++) if (selected_vertices[selected_mesh[0]][i] == selected_mesh[2]) return;
  selected_vertices[selected_mesh[0]].push_back(selected_mesh[2]);
}

void App::handle_remove_message(const Network::Message &msg) {
  uint8_t *data_ptr = (uint8_t *)msg.data.data();
  SelectionGranularity granularity = *(SelectionGranularity *)data_ptr;
  data_ptr += sizeof(SelectionGranularity);

  uint32_t object_id = *(uint32_t *)data_ptr;
  data_ptr += sizeof(uint32_t);

  uint32_t optional_id = UNSELECTED_MESH;
  if (granularity == SelectionGranularity::VERTEX ||
      granularity == SelectionGranularity::FACE) {
    optional_id = *(uint32_t *)data_ptr;
  }

  if (granularity == SelectionGranularity::OBJECT)
    NodeDelete(scene, object_id);
  else if (granularity == SelectionGranularity::VERTEX &&
           object_id < scene.nodes.size() && scene.nodes[object_id].mesh != nullptr && optional_id < scene.nodes[object_id].mesh->vertices.size())
    Render::VertexDelete(*scene.nodes[object_id].mesh, optional_id);
  else if (granularity == SelectionGranularity::FACE &&
           object_id < scene.nodes.size() && scene.nodes[object_id].mesh != nullptr && optional_id < scene.nodes[object_id].mesh->faces.size())
    Render::FaceDelete(*scene.nodes[object_id].mesh, optional_id);

  // Clear selections entirely to prevent out-of-bounds access when vectors shift
  selected_mesh = glm::uvec3(UNSELECTED_MESH, UNSELECTED_MESH, UNSELECTED_MESH);
  selected_meshes.clear();
  selected_vertices.clear();

  // If the object becomes completely empty, delete it
  if (object_id < scene.nodes.size() && scene.nodes[object_id].mesh != nullptr) {
      if (scene.nodes[object_id].mesh->vertices.empty() || scene.nodes[object_id].mesh->faces.empty()) {
          NodeDelete(scene, object_id);
      }
  }
}

void App::handle_translate_message(const Network::Message &msg) {
  uint8_t *data_ptr = (uint8_t *)msg.data.data();
  SelectionGranularity granularity = *(SelectionGranularity *)data_ptr;
  data_ptr += sizeof(SelectionGranularity);

  uint32_t object_id = UNSELECTED_MESH;
  object_id = *(uint32_t *)data_ptr;
  data_ptr += sizeof(uint32_t);

  uint32_t optional_id = UNSELECTED_MESH;
  if (granularity == SelectionGranularity::VERTEX ||
      granularity == SelectionGranularity::FACE) {
    optional_id = *(uint32_t *)data_ptr;
    data_ptr += sizeof(uint32_t);
  }

  glm::vec3 delta = glm::vec3(0.0f, 0.0f, 0.0f);
  delta = *(glm::vec3 *)data_ptr;
  data_ptr += sizeof(glm::vec3);

  // NOTE: delta is in world space at this point
  // (maybe we can do the conversion to model space before it is sent over the
  // network and assume the space of delta changes depending on granularity?)

  // Translate correct granularity
  // The code for vertex and face movements is pretty inefficient right now but
  // it might just have to be that way We should definitely try to look for a
  // better way than this though...
  if (granularity == SelectionGranularity::OBJECT)
    NodeTranslate(scene, object_id, delta);
  else if (granularity == SelectionGranularity::VERTEX &&
           object_id < scene.nodes.size() && scene.nodes[object_id].mesh != nullptr && optional_id < scene.nodes[object_id].mesh->vertices.size())
    Render::VertexTranslate(*scene.nodes[object_id].mesh, optional_id, delta);
  else if (granularity == SelectionGranularity::FACE &&
           object_id < scene.nodes.size() && scene.nodes[object_id].mesh != nullptr && optional_id < scene.nodes[object_id].mesh->faces.size())
    Render::FaceTranslate(*scene.nodes[object_id].mesh, optional_id, delta);
}

void App::handle_connect_message(const Network::Message &msg) {
  Scene *scene_ptr = (Scene *)msg.data.data();
  RebuildScene(*scene_ptr);
  scene = std::move(*scene_ptr);
}

void App::handle_rotate_message(const Network::Message &msg) {
  uint8_t *ptr = (uint8_t *)msg.data.data();
  SelectionGranularity gran = *(SelectionGranularity *)ptr;
  ptr += sizeof(SelectionGranularity);
  uint32_t obj_id = *(uint32_t *)ptr;
  ptr += sizeof(uint32_t);

  if (obj_id >= scene.nodes.size()) return;

  if (gran == SelectionGranularity::OBJECT) {
    glm::quat rot = *(glm::quat *)ptr;
    NodeRotate(scene, obj_id, rot);
  } else if (gran == SelectionGranularity::FACE) {
    uint32_t face_id = *(uint32_t *)ptr;
    ptr += sizeof(uint32_t);
    glm::quat rot = *(glm::quat *)ptr;
    if (scene.nodes[obj_id].mesh != nullptr && face_id < scene.nodes[obj_id].mesh->faces.size())
      Render::FaceRotate(*scene.nodes[obj_id].mesh, face_id, rot);
  }
}

void App::handle_scale_message(const Network::Message &msg) {
  uint8_t *ptr = (uint8_t *)msg.data.data();
  SelectionGranularity gran = *(SelectionGranularity *)ptr;
  ptr += sizeof(SelectionGranularity);
  uint32_t obj_id = *(uint32_t *)ptr;
  ptr += sizeof(uint32_t);

  if (obj_id >= scene.nodes.size()) return;

  uint32_t optional_id = 0;
  if (gran != SelectionGranularity::OBJECT) {
    optional_id = *(uint32_t *)ptr;
    ptr += sizeof(uint32_t);
  }

  glm::vec3 scale = *(glm::vec3 *)ptr;
  if (gran == SelectionGranularity::OBJECT)
    NodeScale(scene, obj_id, scale);
  else if (gran == SelectionGranularity::VERTEX &&
           scene.nodes[obj_id].mesh != nullptr && optional_id < scene.nodes[obj_id].mesh->vertices.size())
    Render::VertexScale(*scene.nodes[obj_id].mesh, optional_id, scale);
  else if (gran == SelectionGranularity::FACE &&
           scene.nodes[obj_id].mesh != nullptr && optional_id < scene.nodes[obj_id].mesh->faces.size())
    Render::FaceScale(*scene.nodes[obj_id].mesh, optional_id, scale);
}
void App::handle_hierarchy_receive(const Network::Message& msg)
{
  Scene *scene_ptr = (Scene *)msg.data.data();
  AppendSceneToScene(scene, *scene_ptr, scene.root);
}

void App::deinit() {
  // Deinit network
  client.deinit();
  server.deinit();
  signaling.deinit();

  DeinitScene(scene);
  scene = {};

  // Deinit renderer
  // Destroy render targets...
  // Destroy all buffers...
  Render::DestroyRenderTarget(main_rt);
  Render::DestroyRenderTarget(id_rt);
  Render::DestroyContext();

  // Deinit window
  Render::DestroyWindow(window_handle);
}
} // namespace Merger