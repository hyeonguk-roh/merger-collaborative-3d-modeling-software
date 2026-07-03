#include "render.h"
#include "scene.h"
#include <GLFW/glfw3.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <fstream>
#include <glad/glad.h>
#include <iostream>
#include <sstream>

// Need to add point light with attenuation that follows the camera's position

namespace Render {
// Goes through faces and vertices and flattens it into a single array of
// vertices then creates the necessary gpu buffers.
static Mesh::GPUDrawData LoadGPUDataIndexed(const std::vector<Vertex>& vertices, const std::vector<Face>& faces)
{
  std::vector<glm::uvec3> triangles;
  std::vector<Vertex> expanded_vertices;

  // For mapping between CPU vertices and GPU vertices
  std::vector<std::vector<uint32_t>> vertex_mapping(vertices.size());

  uint32_t vert_index = 0;
  for (int i = 0; i < faces.size(); i++)
  {
    const Face& face = faces[i];
    assert(face.vert_count == 3 || face.vert_count == 4);

    glm::vec3 dir1 = vertices[face.vertex_ids[1]].position - vertices[face.vertex_ids[0]].position;
    glm::vec3 dir2 = vertices[face.vertex_ids[2]].position - vertices[face.vertex_ids[0]].position;
    glm::vec3 norm = glm::normalize(glm::cross(dir1, dir2));

    // Push back a vertex per index in the face
    for (int j = 0; j < face.vert_count; j++)
    {
      expanded_vertices.push_back(Vertex{ .position = vertices[face.vertex_ids[j]].position, .normal = norm});
      vertex_mapping[face.vertex_ids[j]].push_back(expanded_vertices.size() - 1);
    }

    // Add the triangles to the index buffer
    triangles.push_back({vert_index, vert_index + 1, vert_index + 2});

    if (face.vert_count == 4)
    {
      triangles.push_back({vert_index + 2, vert_index + 3, vert_index});
    }

    vert_index += face.vert_count;
  }

  // Create buffers
  Buffer vertex_buffer = CreateBuffer((void *)expanded_vertices.data(), expanded_vertices.size() * sizeof(Vertex), BufferType::VERTEX, BufferUsage::STATIC);
  Buffer index_buffer = CreateBuffer((void *)triangles.data(), triangles.size() * sizeof(glm::uvec3), BufferType::INDEX, BufferUsage::STATIC);

  // Assign vao and attributes
  unsigned int vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer.id);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, position));
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer.id);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  // return gpu data
  return {
          .vao = vao,
          .vertex_buffer = vertex_buffer,
          .index_buffer = index_buffer,
          .triangle_count = static_cast<uint32_t>(triangles.size()),
          .gpu_vertex_map = vertex_mapping
        };
}

static Mesh::GPUPickingData LoadGPUPickingData(const std::vector<Vertex> &vertices, const std::vector<Face> &faces) 
{
  std::vector<PickingVertex> picking_vertices;
  for (uint32_t i = 0; i < faces.size(); i++) 
  {
    const Face &f = faces[i];
    assert(f.vert_count == 3 || f.vert_count == 4);

    picking_vertices.push_back({.position = vertices[f.vertex_ids[0]].position, .face_id = i, .vertex_id = f.vertex_ids[0]});
    picking_vertices.push_back({.position = vertices[f.vertex_ids[1]].position, .face_id = i, .vertex_id = f.vertex_ids[1]});
    picking_vertices.push_back({.position = vertices[f.vertex_ids[2]].position, .face_id = i, .vertex_id = f.vertex_ids[2]});

    // If a quad, push back both triangles
    if (f.vert_count == 4) 
    {
      picking_vertices.push_back({.position = vertices[f.vertex_ids[2]].position, .face_id = i, .vertex_id = f.vertex_ids[2]});
      picking_vertices.push_back({.position = vertices[f.vertex_ids[3]].position, .face_id = i, .vertex_id = f.vertex_ids[3]});
      picking_vertices.push_back({.position = vertices[f.vertex_ids[0]].position, .face_id = i, .vertex_id = f.vertex_ids[0]});
    }
  }

  // Generate gpu buffers
  Buffer vertex_buffer = CreateBuffer(picking_vertices.data(), picking_vertices.size() * sizeof(PickingVertex), BufferType::VERTEX, BufferUsage::STATIC);

  // Bind vao and attributes
  uint32_t vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer.id);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PickingVertex), (void *)offsetof(PickingVertex, position));
  glEnableVertexAttribArray(0);
  glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(PickingVertex), (void *)offsetof(PickingVertex, face_id));
  glEnableVertexAttribArray(1);
  glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(PickingVertex), (void *)offsetof(PickingVertex, vertex_id));
  glEnableVertexAttribArray(2);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  return {
          .vao = vao,
          .vertex_buffer = vertex_buffer,
          .vertex_count = static_cast<uint32_t>(picking_vertices.size())
        };
}
static void DestroyGPUDrawData(Mesh::GPUDrawData &data) 
{
  glDeleteVertexArrays(1, &data.vao);
  data.vao = UINT32_MAX;
  DestroyBuffer(data.vertex_buffer);
  DestroyBuffer(data.index_buffer);
  data.triangle_count = 0;
}

static void DestroyGPUPickingData(Mesh::GPUPickingData &data) 
{
  glDeleteVertexArrays(1, &data.vao);
  data.vao = UINT32_MAX;
  DestroyBuffer(data.vertex_buffer);
  data.vertex_count = 0;
}

void DestroyMesh(Mesh &m) 
{
  DestroyGPUDrawData(m.draw_data);
  DestroyGPUPickingData(m.pick_data);
  m.faces = {};
  m.vertices = {};
}

GLFWwindow* InitWindow(uint32_t width, uint32_t height, const char *title) 
{
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  GLFWwindow *window = glfwCreateWindow(width, height, title, nullptr, nullptr);
  const GLFWvidmode *vidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());
  glfwSwapInterval(vidmode->refreshRate);
  glfwMakeContextCurrent(window);

  return window;
}

void DestroyWindow(GLFWwindow *window) 
{
  glfwDestroyWindow(window);
  glfwTerminate();
}

void InitContext(GLFWwindow *window) 
{
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) 
  {
    std::cout << "Failed to load opengl" << std::endl;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();
}

void DestroyContext()
{
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

Texture CreateTexture(uint32_t width, uint32_t height, InternalFormat int_format, ExternalFormat ext_format, UploadType type, void *data) {
  GLint ext_form = (ext_format == ExternalFormat::RGB) ? GL_RGB : GL_RGBA;

  switch (ext_format) 
  {
    case ExternalFormat::RGB:
      ext_form = GL_RGB;
      break;
    case ExternalFormat::RGBA:
      ext_form = GL_RGBA;
      break;
    case ExternalFormat::RGB_INT:
      ext_form = GL_RGB_INTEGER;
      break;
    case ExternalFormat::RGBA_INT:
      ext_form = GL_RGBA_INTEGER;
      break;
    default:
      ext_form = GL_RGBA;
      break;
  }

  GLint int_form = GL_RGBA;
  switch (int_format) 
  {
    case InternalFormat::RGB_UBYTE:
      int_form = GL_RGB;
      break;
    case InternalFormat::RGBA_UBYTE:
      int_form = GL_RGBA;
      break;
    case InternalFormat::RGB_32F:
      int_form = GL_RGB32F;
      break;
    case InternalFormat::RGBA_32F:
      int_form = GL_RGBA32F;
      break;
    case InternalFormat::RGB_UINT32:
      int_form = GL_RGB32UI;
      break;
    case InternalFormat::RGBA_UINT32:
      int_form = GL_RGBA32UI;
      break;
    default:
      int_form = GL_RGBA;
      break;
  }

  GLint upload_type = GL_UNSIGNED_BYTE;
  switch (type) 
  {
    case UploadType::UBYTE:
      upload_type = GL_UNSIGNED_BYTE;
      break;
    case UploadType::FLOAT:
      upload_type = GL_FLOAT;
      break;
    case UploadType::UINT:
      upload_type = GL_UNSIGNED_INT;
      break;
    default:
      upload_type = GL_UNSIGNED_BYTE;
      break;
  }

  unsigned int id;
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);
  glTexImage2D(GL_TEXTURE_2D, 0, int_form, width, height, 0, ext_form, upload_type, data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  return {.id = id,
          .width = width,
          .height = height,
          .internal_format = int_format,
          .external_format = ext_format,
          .upload_type = type};
}

void DestroyTexture(Texture &t) 
{
  glDeleteTextures(1, &t.id);
  t.id = 0;
  t.width = 0;
  t.height = 0;
}
static bool LoadShaderModule(unsigned int *module, std::string path, GLenum shader_type) 
{
  int success = 0;
  char err_log[512];

  std::ifstream file(path);
  if (!file.is_open()) 
  {
    std::cerr << "Failed to load file: " << path << std::endl;
    return false;
  }

  std::ostringstream stream;
  stream << file.rdbuf();
  std::string source = stream.str();
  std::erase(source, '\r');
  file.close();

  unsigned int shader = glCreateShader(shader_type);
  const char *const source_ptr = source.c_str();
  glShaderSource(shader, 1, &source_ptr, nullptr);
  glCompileShader(shader);
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

  if (!success) 
  {
    glGetShaderInfoLog(shader, 512, nullptr, err_log);
    switch (shader_type) 
    {
      case GL_VERTEX_SHADER:
        std::cout << "Vertex Shader: " << std::endl;
        break;
      case GL_FRAGMENT_SHADER:
        std::cout << "Fragment Shader: " << std::endl;
        break;
      default:
        break;
    }

    std::cout << err_log << std::endl;
    return false;
  }

  *module = shader;
  return true;
}

// Might want to replace bool with std::optional<unsigned int> to use newer c++
// features
bool LoadShader(unsigned int *shader, const std::string &vertex_path, const std::string &fragment_path) 
{
  int success = 0;
  char err_log[512];

  unsigned int vertex_shader;
  if (!LoadShaderModule(&vertex_shader, vertex_path, GL_VERTEX_SHADER)) 
  {
    std::cerr << "Failed to Load Vertex Shader" << std::endl;
    return false;
  }

  unsigned int fragment_shader;
  if (!LoadShaderModule(&fragment_shader, fragment_path, GL_FRAGMENT_SHADER)) 
  {
    std::cerr << "Failed to Load Fragment Shader" << std::endl;
    return false;
  }

  unsigned int program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);
  glGetProgramiv(program, GL_LINK_STATUS, &success);

  if (!success) 
  {
    glGetProgramInfoLog(program, 512, nullptr, err_log);
    std::cout << "Program Linker:" << std::endl;
    std::cout << err_log << std::endl;
    return false;
  }

  glDetachShader(program, vertex_shader);
  glDetachShader(program, fragment_shader);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  *shader = program;
  return true;
}

void DestroyShader(unsigned int id) 
{ 
  glDeleteProgram(id); 
}

Buffer CreateBuffer(void *data, unsigned int size, BufferType type, BufferUsage usage) 
{
  unsigned int buffer;
  glGenBuffers(1, &buffer);

  GLenum buffer_type = (type == BufferType::VERTEX) ? GL_ARRAY_BUFFER : GL_ELEMENT_ARRAY_BUFFER;

  glBindBuffer(buffer_type, buffer);

  GLenum buffer_usage = (usage == BufferUsage::STATIC) ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;
  glBufferData(buffer_type, size, data, buffer_usage);

  return Buffer{.id = buffer,
                .alloc_size = size,
                .cur_size = size,
                .type = type,
                .usage = usage};
}

Buffer CreateBuffer(unsigned int capacity, BufferType type, BufferUsage usage) 
{
  unsigned int buffer;
  glGenBuffers(1, &buffer);

  GLenum buffer_type = (type == BufferType::VERTEX) ? GL_ARRAY_BUFFER : GL_ELEMENT_ARRAY_BUFFER;
  glBindBuffer(buffer_type, buffer);

  GLenum buffer_usage = (usage == BufferUsage::STATIC) ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;
  glBufferData(buffer_type, capacity, nullptr, buffer_usage);

  return Buffer{.id = buffer,
                .alloc_size = capacity,
                .cur_size = 0,
                .type = type,
                .usage = usage};
}

/*bool BufferWrite(Buffer buf, void* data, unsigned int size, unsigned int
offset)
{
    if (offset + size > buf.capacity) return false;
    GLenum buffer_type = (buf.type == VERTEX) ? GL_ARRAY_BUFFER :
GL_ELEMENT_ARRAY_BUFFER; glBindBuffer(buffer_type, buf.id);
    glBufferSubData(buffer_type, offset, size, data);
    return true;
}*/
bool CopyBuffer(Buffer &src, unsigned int src_offset, Buffer &dst, unsigned int dst_offset, unsigned int size) 
{
  if (src_offset + size > src.alloc_size || dst_offset + size > dst.alloc_size) return false;

  glBindBuffer(GL_COPY_READ_BUFFER, src.id);
  glBindBuffer(GL_COPY_WRITE_BUFFER, dst.id);
  void *read_buffer =
      glMapBufferRange(GL_COPY_READ_BUFFER, src_offset, size, GL_MAP_READ_BIT);
  void *write_buffer = glMapBufferRange(GL_COPY_WRITE_BUFFER, dst_offset, size,
                                        GL_MAP_WRITE_BIT);
  memcpy(write_buffer, read_buffer, size);

  if (!glUnmapBuffer(GL_COPY_READ_BUFFER)) assert(false);
  if (!glUnmapBuffer(GL_COPY_WRITE_BUFFER)) assert(false);

  return true;
}
void Buffer::append(void *data, unsigned int size, bool *reallocated) 
{
  assert(usage != Render::BufferUsage::STATIC);
  bool did_realloc = false;

  // Grow buffer if needed
  if (cur_size + size > alloc_size) 
  {
    unsigned int new_size = alloc_size * 2;
    Render::Buffer new_buffer = Render::CreateBuffer(new_size, type, usage);

    if (!CopyBuffer(*this, 0, new_buffer, 0, cur_size)) assert(false);

    new_buffer.cur_size = cur_size;
    Render::DestroyBuffer(*this);

    *this = new_buffer;
    did_realloc = true;
  }

  glBindBuffer(GL_ARRAY_BUFFER, id);
  glBufferSubData(GL_ARRAY_BUFFER, cur_size, size, data);
  cur_size += size;

  // Tell user we reallocated buffer so we can set up pointers again
  if (reallocated != nullptr) *reallocated = did_realloc;
}

void Buffer::write(void *data, unsigned int size, unsigned int offset) 
{
  assert(usage != Render::BufferUsage::STATIC);
  std::cerr << "Not Implemented" << std::endl;
  assert(false);
}

void DestroyBuffer(Buffer &buf) 
{
  glDeleteBuffers(1, &buf.id);
  buf.id = UINT32_MAX;
}

Mesh GenerateTriangle(const glm::vec3 &center, const glm::vec2 &size, uint32_t id) 
{
  glm::vec2 half_size = size / 2.0f;

  Mesh mesh = {
    .faces = {
      {.vertex_ids = {0, 1, 2}, .vert_count = 3}
    },
    .vertices = {
      {.position = {-half_size.x, -half_size.y, 0.0f}},
      {.position = {half_size.x, -half_size.y, 0.0f}},
      {.position = {0.0f, half_size.y, 0.0f}}
    }
  };

  mesh.draw_data = LoadGPUDataIndexed(mesh.vertices, mesh.faces);
  mesh.pick_data = LoadGPUPickingData(mesh.vertices, mesh.faces);

  return mesh;
}

Mesh GenerateSquare(const glm::vec3 &center, const glm::vec2 &size, uint32_t id) 
{
  glm::vec2 half_size = size / 2.0f;

  Mesh mesh = {
    .faces = {
      {.vertex_ids = {0, 1, 2, 3}, .vert_count = 4}
    },
    .vertices = {
      {.position = {-half_size.x, -half_size.y, 0.0f}},
      {.position = {half_size.x, -half_size.y, 0.0f}},
      {.position = {half_size.x, half_size.y, 0.0f}},
      {.position = {-half_size.x, half_size.y, 0.0f}}
    }
  };

  mesh.draw_data = LoadGPUDataIndexed(mesh.vertices, mesh.faces);
  mesh.pick_data = LoadGPUPickingData(mesh.vertices, mesh.faces);

  return mesh;
}

Mesh GenerateCube(const glm::vec3 &center, const glm::vec3 &extents, uint32_t id) 
{
  glm::vec3 half_size = extents / 2.0f;

  Mesh mesh = {
    .faces = {
      {.vertex_ids = {0, 1, 2, 3}, .vert_count = 4},
      {.vertex_ids = {4, 5, 6, 7}, .vert_count = 4},
      {.vertex_ids = {1, 4, 7, 2}, .vert_count = 4},
      {.vertex_ids = {5, 0, 3, 6}, .vert_count = 4},
      {.vertex_ids = {3, 2, 7, 6}, .vert_count = 4},
      {.vertex_ids = {5, 4, 1, 0}, .vert_count = 4}
    },
    .vertices = {
      {.position = {-half_size.x, -half_size.y, half_size.z}},
      {.position = {half_size.x, -half_size.y, half_size.z}},
      {.position = {half_size.x, half_size.y, half_size.z}},
      {.position = {-half_size.x, half_size.y, half_size.z}},

      {.position = {half_size.x, -half_size.y, -half_size.z}},
      {.position = {-half_size.x, -half_size.y, -half_size.z}},
      {.position = {-half_size.x, half_size.y, -half_size.z}},
      {.position = {half_size.x, half_size.y, -half_size.z}},
    }
  };

  mesh.draw_data = LoadGPUDataIndexed(mesh.vertices, mesh.faces);
  mesh.pick_data = LoadGPUPickingData(mesh.vertices, mesh.faces);

  return mesh;
}

Mesh GeneratePyramid(const glm::vec3 &center, const glm::vec2 &base_size, float height, uint32_t id) 
{
  glm::vec2 half_size = base_size / 2.0f;
  Mesh mesh = {
    .faces = {
      {.vertex_ids = {0, 1, 2, 3}, .vert_count = 4},
      {.vertex_ids = {4, 3, 2}, .vert_count = 3},
      {.vertex_ids = {4, 0, 3}, .vert_count = 3},
      {.vertex_ids = {4, 2, 1}, .vert_count = 3},
      {.vertex_ids = {4, 1, 0}, .vert_count = 3}
    },
    .vertices = {
      {.position = {-half_size.x, 0.0f, -half_size.y}},
      {.position = {half_size.x, 0.0f, -half_size.y}},
      {.position = {half_size.x, 0.0f, half_size.y}},
      {.position = {-half_size.x, 0.0f, half_size.y}},
      {.position = {0.0f, height, 0.0f}, .normal = {0.0f, 0.0f, 0.0f}},
    }
  };

  // Generate normals for the 4 top faces
  for (int i = 1; i < mesh.faces.size(); i++)
  {
    const Face& face = mesh.faces[i];
    const glm::vec3 a = mesh.vertices[face.vertex_ids[1]].position - mesh.vertices[face.vertex_ids[0]].position;
    const glm::vec3 b = mesh.vertices[face.vertex_ids[2]].position - mesh.vertices[face.vertex_ids[0]].position;
    glm::vec3 norm = glm::cross(a, b);
    for (int j = 0; j < face.vert_count; j++)
    {
      mesh.vertices[face.vertex_ids[j]].normal = norm;
    }
  }

  mesh.draw_data = LoadGPUDataIndexed(mesh.vertices, mesh.faces);
  mesh.pick_data = LoadGPUPickingData(mesh.vertices, mesh.faces);

  return mesh;
}

void RebuildMesh(Mesh &m) 
{
  DestroyGPUDrawData(m.draw_data);
  DestroyGPUPickingData(m.pick_data);
  m.draw_data = LoadGPUDataIndexed(m.vertices, m.faces);
  m.pick_data = LoadGPUPickingData(m.vertices, m.faces);
  m.needs_rebuild = false;
}

void RebuildTransformMatrix(Transform &t) 
{
  t.rotation = glm::normalize(t.rotation);
  t.matrix = glm::translate(glm::mat4(1.0f), t.translation);
  t.matrix = t.matrix * glm::toMat4(t.rotation);
  t.matrix = glm::scale(t.matrix, t.scale); // Might want to scale before rotation
  t.needs_rebuild = false;
}

glm::vec3 TranslationFromTransformMatrix(const glm::mat4 &matrix) 
{
  return glm::vec3(matrix[3]);
}

RenderTarget CreateRenderTarget() 
{
  unsigned int framebuffer;
  glGenFramebuffers(1, &framebuffer);
  return {.framebuffer = framebuffer, .color_attachment = {}};
}

void DestroyRenderTarget(RenderTarget &rt) 
{
  glDeleteFramebuffers(1, &rt.framebuffer);
  DestroyTexture(rt.color_attachment);
}

bool VerifyRenderTarget(const RenderTarget &rt) 
{
  glBindFramebuffer(GL_FRAMEBUFFER, rt.framebuffer);
  return (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
}


void RenderTargetSetColorAttachment(RenderTarget &rt, Texture &&tex) {
  glBindFramebuffer(GL_FRAMEBUFFER, rt.framebuffer);
  glBindTexture(GL_TEXTURE_2D, tex.id);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.id, 0);
  rt.color_attachment = tex;
}

void RenderTargetSetDepthAttachment(RenderTarget &rt, uint32_t width, uint32_t height)
{
  glGenRenderbuffers(1, &rt.depth_buffer);
  glBindRenderbuffer(GL_RENDERBUFFER, rt.depth_buffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

  glBindFramebuffer(GL_FRAMEBUFFER, rt.framebuffer);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rt.depth_buffer);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

bool ResizeRenderTarget(RenderTarget &rt, const glm::uvec2 size) 
{
  Texture tex = CreateTexture( size.x, size.y, rt.color_attachment.internal_format, rt.color_attachment.external_format, rt.color_attachment.upload_type);
  DestroyTexture(rt.color_attachment);
  rt.color_attachment = tex;

  glDeleteRenderbuffers(1, &rt.depth_buffer);
  glGenRenderbuffers(1, &rt.depth_buffer);
  glBindRenderbuffer(GL_RENDERBUFFER, rt.depth_buffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size.x, size.y);

  glBindFramebuffer(GL_FRAMEBUFFER, rt.framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.color_attachment.id, 0);
  glFramebufferRenderbuffer(GL_RENDERBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rt.depth_buffer);
  rt.viewport_size = size;

  return true;
}

// Clear value must correspond to the correct number of components per pixel to
// work
void BeginRender(const RenderTarget &rt, void *clear_val) 
{
  glBindFramebuffer(GL_FRAMEBUFFER, rt.framebuffer);
  glViewport(0, 0, rt.viewport_size.x, rt.viewport_size.y);
  glEnable(GL_DEPTH_TEST);

  // If uint framebuffer, clear using special command
  if (rt.color_attachment.upload_type == UploadType::UINT) 
  {
    glClearBufferuiv(GL_COLOR, 0, (GLuint *)clear_val);
  } 
  else 
  {
    // float* clear_color = (float*)clear_val;
    // glClearColor(clear_color[0], clear_color[1], clear_color[2],
    // clear_color[3]);
    glClearBufferfv(GL_COLOR, 0, (GLfloat *)clear_val);
    // glClear(GL_COLOR_BUFFER_BIT);
  }

  glClear(GL_DEPTH_BUFFER_BIT);
}

void VertexTranslate(Mesh &m, uint32_t vert_id, const glm::vec3 &delta) 
{
  m.vertices[vert_id].position += delta;
  m.needs_rebuild = true;
}

void FaceTranslate(Mesh &m, uint32_t face_id, const glm::vec3 &delta) 
{
  const Face &face = m.faces[face_id];
  for (int i = 0; i < face.vert_count; i++) 
  {
    m.vertices[face.vertex_ids[i]].position += delta;
  }
  m.needs_rebuild = true;
}

void FaceDelete(Mesh &m, uint32_t face_id) {
  if (face_id < m.faces.size()) {
    const Face &face = m.faces[face_id];
    for (int i = face.vert_count - 1; i >= 0; --i) {
      bool vert_still_needed = false;
      for (int j = 0; j < m.faces.size(); j++) {
        for (int k = 0; k < m.faces[j].vert_count; ++k) {
          if (j != face_id && m.faces[j].vertex_ids[k] == face.vertex_ids[i]) {
            vert_still_needed = true;
            break;
          }
        }
        if (vert_still_needed) break;
      }
      if (!vert_still_needed) {
        for (int j = 0; j < m.faces.size(); j++) {
          for (int k = 0; k < m.faces[j].vert_count; ++k) {
            if (m.faces[j].vertex_ids[k] > face.vertex_ids[i]) {
              m.faces[j].vertex_ids[k]--;
            }
          }
        }
        m.vertices.erase(m.vertices.begin() + face.vertex_ids[i]);
      }
    }
    m.faces.erase(m.faces.begin() + face_id);
    m.needs_rebuild = true;
  }
}

void VertexDelete(Mesh &m, uint32_t vertex_id) 
{
  if (vertex_id < m.vertices.size()) 
  {
    for (int i = (int)m.faces.size() - 1; i >= 0; --i) 
    {
      bool has_vert = false;
      for (int j = 0; j < m.faces[i].vert_count; ++j) 
      {
        if (m.faces[i].vertex_ids[j] == vertex_id) 
        {
          has_vert = true;
          break;
        }
      }

      if (has_vert) 
      {
        m.faces.erase(m.faces.begin() + i);
      }
    }

    // Shift down all remaining vertex indices that point after the deleted vertex
    for (int i = 0; i < m.faces.size(); ++i) {
      for (int j = 0; j < m.faces[i].vert_count; ++j) {
        if (m.faces[i].vertex_ids[j] > vertex_id) {
          m.faces[i].vertex_ids[j]--;
        }
      }
    }

    m.vertices.erase(m.vertices.begin() + vertex_id);
    m.needs_rebuild = true;
  }
}

void VertexScale(Mesh &m, uint32_t vert_id, const glm::vec3 &scale_factor) {}

void FaceRotate(Mesh &m, uint32_t face_id, const glm::quat &rotation) 
{
  const Face &face = m.faces[face_id];
  // Calculate face center for local rotation
  glm::vec3 center(0.0f);
  for (int i = 0; i < face.vert_count; i++)
    center += m.vertices[face.vertex_ids[i]].position;
  center /= (float)face.vert_count;

  for (int i = 0; i < face.vert_count; i++) 
  {
    glm::vec3 &pos = m.vertices[face.vertex_ids[i]].position;
    pos = center + rotation * (pos - center);
  }
  m.needs_rebuild = true;
}

void FaceScale(Mesh &m, uint32_t face_id, const glm::vec3 &scale) 
{
  const Face &face = m.faces[face_id];
  glm::vec3 center(0.0f);

  for (int i = 0; i < face.vert_count; i++)
    center += m.vertices[face.vertex_ids[i]].position;
  center /= (float)face.vert_count;

  glm::vec3 safe_scale = scale;
  const float MIN_SCALE_MULT = 0.001f;

  if (std::abs(safe_scale.x) < MIN_SCALE_MULT)
    safe_scale.x = (safe_scale.x < 0) ? -MIN_SCALE_MULT : MIN_SCALE_MULT;

  if (std::abs(safe_scale.y) < MIN_SCALE_MULT)
    safe_scale.y = (safe_scale.y < 0) ? -MIN_SCALE_MULT : MIN_SCALE_MULT;

  if (std::abs(safe_scale.z) < MIN_SCALE_MULT)
    safe_scale.z = (safe_scale.z < 0) ? -MIN_SCALE_MULT : MIN_SCALE_MULT;

  for (int i = 0; i < face.vert_count; i++) 
  {
    glm::vec3 &pos = m.vertices[face.vertex_ids[i]].position;
    glm::vec3 diff = pos - center;
    glm::vec3 new_diff = safe_scale * diff;
    if (glm::length(new_diff) < 0.01f) 
    {
      new_diff = (glm::length(diff) > 0.001f) ? glm::normalize(diff) * 0.01f : new_diff;
    }
    pos = center + new_diff;
  }
  m.needs_rebuild = true;
}

glm::vec3 GetArcballVector(const glm::vec2 &ndc) 
{
  glm::vec3 p = glm::vec3(ndc.x, ndc.y, 0.0f);
  float squared_mag = p.x * p.x + p.y * p.y;

  if (squared_mag <= 1.0f) 
  {
    p.z = sqrt(1.0f - squared_mag);
  } 
  else 
  {
    p = glm::normalize(p);
  }
  return p;
}

void Camera::set_position(const glm::vec3 &pos) 
{
  position = pos;
  needs_rebuild = true;
}

void Camera::set_lookat(const glm::vec3 &at) 
{
  lookat = at;
  needs_rebuild = true;
}

void Camera::set_perspective(float fov, float aspect_ratio, float near, float far) 
{
  projection = glm::perspective(fov, aspect_ratio, near, far);
}

void Camera::set_orthographic(float left, float right, float bottom, float top, float near, float far) 
{
  projection = glm::ortho(left, right, bottom, top, near, far);
}

const glm::mat4 &Camera::get_projection() const
{ 
  return projection; 
}

const glm::mat4 &Camera::get_view() 
{
  if (needs_rebuild) rebuild_camera();
  return view;
}

const glm::vec3 &Camera::get_position() const
{ 
  return position; 
}

const glm::vec3 &Camera::get_lookat() const
{ 
  return lookat; 
}

void Camera::rebuild_camera() 
{
  view = glm::lookAt(position, lookat, glm::vec3(0.0f, 1.0f, 0.0f));
  inverse_projection_view = glm::inverse(projection * view);

  needs_rebuild = false;
}

const glm::mat4 &Camera::get_inverse_projection_view() 
{
  if (needs_rebuild) rebuild_camera();
  return inverse_projection_view;
}

const glm::vec3 Camera::get_forward() const 
{ 
  return position - lookat; 
}

// Takes NDC and converts it to a ray then calculates the world position of the
// intersection of that ray with the given plane.
glm::vec3 NDCToWorldPosition(Camera &camera, const glm::vec2 &ndc, const glm::vec3 &plane_point, const glm::vec3 &plane_normal) 
{
  // Get world coordinates
  glm::vec4 world_near_pos = camera.get_inverse_projection_view() * glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
  glm::vec4 world_far_pos = camera.get_inverse_projection_view() * glm::vec4(ndc.x, ndc.y, 1.0f, 1.0f);

  // Account for perspective division
  world_near_pos /= world_near_pos.w;
  world_far_pos /= world_far_pos.w;

  // Final world space ray
  glm::vec3 origin = glm::vec3(world_near_pos);
  glm::vec3 direction = glm::vec3(world_far_pos) - origin;

  // Drag Plane: Normal = camera.get_forward(), P = selected_mesh.position
  // Intersection point given by:
  float intersection_t = glm::dot(plane_normal, plane_point - origin) / glm::dot(plane_normal, direction);

  return origin + intersection_t * direction;
}

void DrawMesh(Mesh &mesh, unsigned int shader, Camera &camera, const glm::mat4 &transform) 
{
  glUseProgram(shader);
  glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(camera.get_projection()));
  glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, glm::value_ptr(camera.get_view()));

  if (mesh.needs_rebuild) RebuildMesh(mesh);

  glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(transform));
  glBindVertexArray(mesh.draw_data.vao);
  glDrawElements(GL_TRIANGLES, mesh.draw_data.triangle_count * 3, GL_UNSIGNED_INT, nullptr);
}

// bool Renderer::init(uint32_t width, uint32_t height, GLFWwindow* window)
// {
//     InitContext(window);
//     main_rt = CreateRenderTarget();
//     id_rt = CreateRenderTarget();
//     return true;
// }
// void Renderer::deinit()
// {
//     DestroyRenderTarget(main_rt);
//     DestroyRenderTarget(id_rt);
//     DestroyContext();
// }
} // namespace Render