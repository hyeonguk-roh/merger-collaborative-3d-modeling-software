#pragma once
#include <cstdint>
#include <glm/glm.hpp>
// Temp for now
#include "lock_queue.h"
#include "merger_types.h"
#include "network.h"
#include "render.h"
#include "scene.h"
// clang-format off
#include "imgui.h"
#include "imgui_internal.h"
#include "ImGuizmo.h"
#include "ImGuiFileDialog.h"

// clang-format on
// Forward declare class so we don't need to include glfw here
struct GLFWwindow;
namespace Merger {
class App {
private:
  GLFWwindow *window_handle;

  // Renderer stuff that will be contained in renderer class
  Render::RenderTarget main_rt;
  Render::RenderTarget id_rt;
  
  // Camera info
  Render::Camera camera;
  double cam_radius = 3.0;
  double theta = 0.0;
  double phi = -15.0;
  enum CameraType
  {
    ARCBALL,
    FIRST_PERSON
  };
  CameraType cam_type = CameraType::ARCBALL;
  glm::vec3 arcball_lookat = glm::vec3(0.0f, 0.0f, 0.0f);
  glm::vec3 cam_forward = glm::vec3(0.0f, 0.0f, -1.0f); 
  const float CAM_SPEED = 0.05f;

  unsigned int grid_vao;
  unsigned int shader, id_shader, vertex_view_shader, grid_shader, solid_shader;

  glm::vec2 workspace_size = glm::vec2(0.0f);

  bool init_window(uint32_t width, uint32_t height, const char *title);
  void create_gui_layout();
  void build_toolbar();
  void shapes_section();
  void selection_section();
  void transform_section();
  void network_section();
  void status_section();
  void camera_section();
  void setup_gui_style();
  void build_scene_view();
  void build_node_menu();
  void walk_gui_scene_nodes(SceneID id);
  void collect_descendants(SceneID id, std::vector<glm::uvec3>& out);
  bool is_node_or_descendant_selected(SceneID id);
  void present_gui();
  void handle_place_click(const glm::vec3 &mouse_pos);
  void handle_select_click(const glm::vec2 &mouse_pos);
  void handle_add_message(const Network::Message &msg);
  void handle_remove_message(const Network::Message &msg);
  void handle_translate_message(const Network::Message &msg);
  void handle_connect_message(const Network::Message &msg);
  void handle_rotate_message(const Network::Message &msg);
  void handle_scale_message(const Network::Message &msg);
  void handle_reparent_message(const Network::Message& msg);
  void handle_hierarchy_receive(const Network::Message& msg);
  

  glm::vec3 arcball_start_vector = glm::vec3(0.0f);

  glm::vec2 mouse_delta;

  ToolMode mode = ToolMode::PLACE;
  Render::Primitive primitive = Render::Primitive::TRIANGLE;

  SelectionGranularity select_granularity = SelectionGranularity::OBJECT;
  ImGuizmo::MODE current_gizmo_mode = ImGuizmo::LOCAL;

  static const uint32_t UNSELECTED_MESH = UINT32_MAX;

  // (objectID, faceID, vertexID)
  glm::uvec3 selected_mesh =
      glm::uvec3(UNSELECTED_MESH, UNSELECTED_MESH, UNSELECTED_MESH);
  std::vector<glm::uvec3> selected_meshes;
  std::unordered_map<uint32_t, std::vector<uint32_t>> selected_vertices;

  // Rename state
  SceneID renaming_node = UINT32_MAX;
  char rename_buffer[256] = {};
  bool rename_focus_set = false;
  Scene scene;

  // previous world position used when in grab mode
  glm::vec3 previous_world_position = glm::vec3(0.0f);

  Network::Server server;
  Network::Client client;
  Network::Signaling signaling;
  LockQueue<Network::Message> messages;

public:
  App();
  bool init(uint32_t width, uint32_t height, const char *title,
            bool start_server = true);
  void run();
  void deinit();
};
}; // namespace Merger