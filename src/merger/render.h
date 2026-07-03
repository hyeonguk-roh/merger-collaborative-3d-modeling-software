#pragma once
#include <cstdint>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

#include <string>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <optional>

namespace Render
{

    // Will eventually hold more stuff like normals, uvs, etc.
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
    };

    // Will just hold id and position
    struct PickingVertex
    {
        glm::vec3 position;
        uint32_t face_id;
        uint32_t vertex_id;
    };

    enum class BufferUsage
    {
        STATIC,
        DYNAMIC
    };

    enum class BufferType
    {
        VERTEX,
        INDEX
    };

    // Format of incoming data
    // How data is stored in CPU
    enum class ExternalFormat
    {
        RGB,
        RGBA,
        RGB_INT,
        RGBA_INT
    };

    enum class UploadType
    {
        UBYTE,
        FLOAT,
        UINT
    };

    // Format of texture on GPU
    // How data is stored on GPU
    enum class InternalFormat
    {
        RGB_UBYTE,
        RGBA_UBYTE,
        RGB_32F,
        RGBA_32F,
        RGB_UINT32,
        RGBA_UINT32
    };

    enum class Primitive : uint8_t
    {
        TRIANGLE,
        SQUARE,
        CUBE,
        PYRAMID,
        MONKEY
    };
    struct Buffer
    {
        unsigned int id;
        unsigned int alloc_size;  // How much space is actually allocated for the buffer
        unsigned int cur_size;        // How much space is being used in the buffer
        BufferType type;
        BufferUsage usage;
        // Define constructor, destructor, and move operators. Delete copy operators
        void append(void* data, unsigned int size, bool* rellocated = nullptr);
        void write(void* data, unsigned int size, unsigned int offset);
    };

    // Assume texture2D for now
    struct Texture
    {
        unsigned int id;
        unsigned int width, height;
        InternalFormat internal_format;
        ExternalFormat external_format;
        UploadType upload_type;

        // Not safe to copy opengl handles because you would just copy the handles
        // Might want to make it so a deep copy is made but for now we'll just delete them
        // Texture(const Texture& other) = delete;
        // Texture& operator=(const Texture& other) = delete;
        // Would want to implement these if going RAII route
        // Texture();
        // Texture(Texture&& other) noexcept;
        // Texture& operator=(Texture&& other) noexcept;
        // ~Texture();
    };

    struct Transform
    {
        glm::vec3 translation = glm::vec3(0.0f);
        glm::vec3 scale = glm::vec3(1.0f);
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::mat4 matrix = glm::mat4(1.0f);
        bool needs_rebuild = false;
    };

    // Faces don't necessarily have to be quads, they could also be triangles
    struct Face
    {
        // Vertices given in model space
        uint32_t vertex_ids[4];              // This holds the four ids of the vertices that make up this face.
        uint32_t vert_count;                 // To get the actual vertex data should be able to do vertices[face.vertex_ids[i]]; (gets the data of the ith vertex of face)
    };

    // NOTE: DO NOT INSTANTIATE THIS CLASS BY ITSELF. ONLY DO SO THROUGH MESH GENERATING FUNCTIONS
    struct Mesh
    {
        std::vector<Face> faces;             // The "id" of the face is just its index in this array
        std::vector<Vertex> vertices;        // The "id" of the vertex is just its index in this array

        struct GPUDrawData
        {
            uint32_t vao = UINT32_MAX;
            Buffer vertex_buffer;
            Buffer index_buffer;
            uint32_t triangle_count;
            std::vector<std::vector<uint32_t>> gpu_vertex_map;   // input: CPU side vertex index, output: array of indices into GPU Buffer that correspond to CPU vertex
        };

        struct GPUPickingData
        {
            uint32_t vao = UINT32_MAX;
            Buffer vertex_buffer;
            uint32_t vertex_count;
        };

        // Holds indexed draw data
        GPUDrawData draw_data;

        // Holds non-indexed draw data for picking
        // Right now it will always be in vram but can optimize this later on.
        GPUPickingData pick_data;

        bool needs_rebuild = false;
    };

    void RebuildMesh(Mesh& m);

    // void RebuildMeshGPUData(Mesh& m);
    void RebuildTransformMatrix(Transform& t);

    void VertexScale(Mesh& m, uint32_t vert_id, const glm::vec3& scale_factor);
    void FaceRotate(Mesh& m, uint32_t face_id, const glm::quat& rotation);
    void FaceScale(Mesh& m, uint32_t face_id, const glm::vec3& scale);

    void VertexTranslate(Mesh& m, uint32_t vert_id, const glm::vec3& delta);
    void FaceTranslate(Mesh& m, uint32_t face_id, const glm::vec3& delta);

    void FaceDelete(Mesh& m, uint32_t face_id);
    void VertexDelete(Mesh& m, uint32_t vert_id);

    glm::vec3 TranslationFromTransformMatrix(const glm::mat4& matrix);
    glm::vec3 GetArcballVector(const glm::vec2& ndc);

    class Camera
    {
        private:
            glm::vec3 position = glm::vec3(0.0f, 0.0f, 1.0f);
            glm::vec3 lookat = glm::vec3(0.0f);
            glm::mat4 projection = glm::mat4(1.0f);
            glm::mat4 view = glm::mat4(1.0f);
            glm::mat4 inverse_projection_view = glm::mat4(1.0f);
            bool needs_rebuild = true;

            void rebuild_camera();

        public:
            void set_position(const glm::vec3& pos);
            void set_lookat(const glm::vec3& at);
            void set_perspective(float fov, float aspect_ratio, float near, float far);
            void set_orthographic(float left, float right, float bottom, float top, float near, float far);

            const glm::mat4& get_projection() const;
            const glm::mat4& get_view();
            const glm::vec3& get_position() const;
            const glm::vec3& get_lookat() const;
            const glm::mat4& get_inverse_projection_view();
            const glm::vec3 get_forward() const;
    };

    struct RenderTarget
    {
        unsigned int framebuffer;
        Texture color_attachment;
        unsigned int depth_buffer;
        glm::uvec2 viewport_size;
    };

    RenderTarget CreateRenderTarget();
    void DestroyRenderTarget(RenderTarget& rt);
    bool VerifyRenderTarget(const RenderTarget& rt);
    void RenderTargetSetColorAttachment(RenderTarget& rt, Texture&& tex);                       // TODO: Currently a memory leak if this is called twice
    void RenderTargetSetDepthAttachment(RenderTarget& rt, uint32_t width, uint32_t height);     // TODO: Currently a memory leak if this is called twice
    bool ResizeRenderTarget(RenderTarget& rt, const glm::uvec2 size);


    void BeginRender(const RenderTarget& rt, void* clear_color);
    // void EndRender();

    // void Draw(const Mesh& m, Camera& cam);

    Mesh GenerateTriangle(const glm::vec3& center, const glm::vec2& size, uint32_t id);
    Mesh GenerateSquare(const glm::vec3& center, const glm::vec2& size, uint32_t id);
    Mesh GenerateCube(const glm::vec3& center, const glm::vec3& extents, uint32_t id);
    Mesh GeneratePyramid(const glm::vec3& center, const glm::vec2& base_size, float height, uint32_t id);
    Mesh GenerateUVSphere(const glm::vec3& center, float radius);
    Mesh GenerateIcoSphere(const glm::vec3& center, float radius, uint32_t subdivisions);

    // Very basic implementation of file loading (doesn't take into account multiple meshes right now which will have to be fixed once we get the scene graph done)
    // std::optional<Mesh> LoadMeshFromFile(const std::string& path);
    // std::optional<Mesh> LoadSceneNodeFromFile(const std::string& path);
    void DestroyMesh(Mesh& m);
    void DrawMesh(Mesh& mesh, unsigned int shader, Camera& camera, const glm::mat4& transform);

    glm::vec3 NDCToWorldPosition(Camera& camera, const glm::vec2& ndc, const glm::vec3& plane_point, const glm::vec3& plane_normal);

    void InitContext(GLFWwindow* window);
    void DestroyContext();
    GLFWwindow* InitWindow(uint32_t width, uint32_t height, const char* title);
    void DestroyWindow(GLFWwindow* window);
    Texture CreateTexture(uint32_t width, uint32_t height, InternalFormat int_format, ExternalFormat ext_format, UploadType type, void* data = nullptr);
    void DestroyTexture(Texture& id);
    bool LoadShader(unsigned int* shader, const std::string& vertex_path, const std::string& fragment_path);
    void DestroyShader(unsigned int id);
    Buffer CreateBuffer(void* data, unsigned int size, BufferType type, BufferUsage usage);
    Buffer CreateBuffer(unsigned int capacity, BufferType type, BufferUsage usage);
    bool CopyBuffer(Buffer& src, unsigned int src_offset, Buffer& dst, unsigned int dst_offset, unsigned int size);
    // Write size bytes from data to buf at offset. Should only be used with buffers created with DYNAMIC usage.
    // Returns if the write was successful or not
    // bool BufferWrite(Buffer buf, void* data, unsigned int size, unsigned int offset);
    
    void DestroyBuffer(Buffer& buf);
}