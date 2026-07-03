#pragma once
#include <memory>
#include <vector>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <memory>
#include <string>
#include "render.h"

// struct Transform
// {
//     glm::vec3 translation = glm::vec3(0.0f);
//     glm::vec3 scale = glm::vec3(1.0f);
//     glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
// };

typedef uint32_t SceneID;
#define UNDEFINED_NODE UINT32_MAX

// Switch Everything from using pointers to using ids
struct SceneNode
{
    std::string name;
    SceneID parent_id = UINT32_MAX;
    Render::Transform local_transform = {};
    glm::mat4 world_matrix = glm::mat4(1.0f);
    std::vector<SceneID> children = {};

    std::unique_ptr<Render::Mesh> mesh = nullptr;
    // std::unique_ptr<Render::Light> light = nullptr;
    // std::unique_ptr<Render::Camera> camera = nullptr;
};

struct Scene
{
    SceneID root;
    std::vector<SceneNode> nodes;
};

struct SceneDrawable
{
    SceneID id;
    glm::mat4 transform = glm::mat4(1.0f);
    Render::Mesh& mesh;
};

Scene InitScene();
void DeinitScene(Scene& scene);

SceneID CreateNode(Scene& scene);
SceneID CreateNode(Scene& scene, SceneID parent_id);

// Appends src_scene to dst_scene with parent_id as parent of src_scene's root (recursively appends all it's nodes)
void AppendSceneToScene(Scene& dst_scene, Scene& src_scene, SceneID parent_id);

std::vector<SceneDrawable> CollectSceneDrawables(Scene& scene);

void ReparentNode(Scene& scene, SceneID node_id, SceneID new_parent_id);

void NodeAttachMesh(Scene& scene, SceneID node, std::unique_ptr<Render::Mesh> mesh);
// void NodeAttachLight(std::vector<SceneNode>& scene, SceneID node, std::unique_ptr<Render::Light> light);
// void NodeAttachCamera(std::vector<SceneNode>& scene, SceneID node, std::unique_ptr<Render::Camera> camera);

void NodeDelete(Scene& scene, SceneID node_id);

void RebuildScene(Scene& scene);

std::optional<Render::Mesh> LoadMeshFromFile(const std::string& path);
std::optional<Scene> LoadNodeHierarchyFromFile(const std::string& path);

void NodeTranslate(Scene& scene, SceneID node_id, const glm::vec3& delta);
void NodeRotate(Scene& scene, SceneID node_id, const glm::quat& delta);
void NodeScale(Scene& scene, SceneID node_id, const glm::vec3& delta);
void NodeSetTranslation(Scene& scene, SceneID node_id, const glm::vec3& translation);
void NodeSetRotation(Scene& scene, SceneID node_id, const glm::quat& rotation);
void NodeSetScale(Scene& scene, SceneID node_id, const glm::vec3& scale);