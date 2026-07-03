#include "scene.h"
#include "render.h"
#include <iostream>

static void WalkSceneDrawables(Scene &scene, SceneID node_id,
                               const glm::mat4 &parent_transform,
                               std::vector<SceneDrawable> &drawables,
                               bool rebuild_world_matrix = false) {
  SceneNode &node = scene.nodes[node_id];
  if (node.local_transform.needs_rebuild) 
  {
    Render::RebuildTransformMatrix(node.local_transform);
    rebuild_world_matrix = true;
  }

  // Only rebuild the world transform if necessary
  if (rebuild_world_matrix)
  {
    node.world_matrix = parent_transform * node.local_transform.matrix;
  }

  // Will definitely want to cache this so we don't need to recalculate this
  // every frame
  node.world_matrix = parent_transform * node.local_transform.matrix;

  if (node.mesh)
    drawables.push_back(SceneDrawable{
        .id = node_id, .transform = node.world_matrix, .mesh = *node.mesh});

  for (int i = 0; i < node.children.size(); i++) {
    WalkSceneDrawables(scene, node.children[i], node.world_matrix, drawables);
  }
}

static void WalkSceneDeinit(Scene &scene, SceneID node_id) {
  SceneNode &node = scene.nodes[node_id];
  if (node.mesh)
    Render::DestroyMesh(*node.mesh);
  // if (node.light) Render::DestroyLight(*node.light);
  // if (node.camera) Render::DestroyCamera(*node.camera);

  for (int i = 0; i < node.children.size(); i++) {
    WalkSceneDeinit(scene, node.children[i]);
  }
}

void RebuildScene(Scene &scene) {
  for (SceneNode &node : scene.nodes) {
    node.local_transform.needs_rebuild = true;
    if (node.mesh)
      node.mesh->needs_rebuild = true;
  }
}

void ReparentNode(Scene &scene, SceneID node_id, SceneID new_parent_id) {
  if (scene.root == node_id)
    return; // Don't allow reparenting of root node for now

  SceneNode &node = scene.nodes[node_id];

  // Preserve world position: save current world matrix
  glm::mat4 old_world = node.world_matrix;

  // Reparent
  std::erase(scene.nodes[node.parent_id].children, node_id);
  scene.nodes[new_parent_id].children.push_back(node_id);
  node.parent_id = new_parent_id;

  // Compute new local transform so that new_parent_world * new_local = old_world
  glm::mat4 new_parent_world = scene.nodes[new_parent_id].world_matrix;
  glm::mat4 new_local = glm::inverse(new_parent_world) * old_world;

  // Decompose into translation / rotation / scale
  node.local_transform.translation = glm::vec3(new_local[3]);

  glm::vec3 scale;
  scale.x = glm::length(glm::vec3(new_local[0]));
  scale.y = glm::length(glm::vec3(new_local[1]));
  scale.z = glm::length(glm::vec3(new_local[2]));
  node.local_transform.scale = scale;

  glm::mat3 rot_mat = glm::mat3(
      glm::vec3(new_local[0]) / scale.x,
      glm::vec3(new_local[1]) / scale.y,
      glm::vec3(new_local[2]) / scale.z);
  node.local_transform.rotation = glm::quat_cast(rot_mat);
  node.local_transform.needs_rebuild = true;
}

Scene InitScene() {
  Scene scene = {};
  scene.nodes.push_back(SceneNode{}); // root node
  scene.root = 0;
  return scene;
}

void DeinitScene(Scene &scene) {
  WalkSceneDeinit(scene, scene.root);
  scene = {};
}

SceneID CreateNode(Scene &scene) { return CreateNode(scene, scene.root); }

SceneID CreateNode(Scene &scene, SceneID parent) {
  if (parent < 0 || parent > scene.nodes.size() - 1)
    return UINT32_MAX;

  SceneID id = scene.nodes.size();
  scene.nodes.push_back(SceneNode{
      .name = "Node",
      .parent_id = 0,
  });
  scene.nodes[parent].children.push_back(id);
  return id;
}

void AppendSceneToScene(Scene& dst_scene, Scene& src_scene, SceneID parent_id)
{
  // Offset to add to each node in src_scene
  uint32_t offset = dst_scene.nodes.size();

  // Go through the src scene's nodes, appending them to the dst_scene, updating ids along the way
  for (int i = 0; i < src_scene.nodes.size(); i++)
  {
    dst_scene.nodes.push_back(std::move(src_scene.nodes[i]));
    SceneID node_id = offset + i;
    SceneNode& node = dst_scene.nodes[node_id];

    // Update all node child ids by offset
    for (int child = 0; child < node.children.size(); child++)
    {
      node.children[child] += offset;
    }

    // Change parent of root to given parent_id and append it to parent_id's children
    if (i == src_scene.root) 
    {
      node.parent_id = parent_id;
      dst_scene.nodes[parent_id].children.push_back(node_id);
    }
    else
    {
      node.parent_id += offset;
    }
  }
}

void NodeAttachMesh(Scene &scene, SceneID node,
                    std::unique_ptr<Render::Mesh> mesh) {
  if (node < 0 || node > scene.nodes.size() - 1)
    return;

  scene.nodes[node].mesh = std::move(mesh);
}

std::vector<SceneDrawable> CollectSceneDrawables(Scene &scene) {
  std::vector<SceneDrawable> drawables = {};
  WalkSceneDrawables(scene, scene.root, glm::identity<glm::mat4>(), drawables);

  return drawables;
}

void NodeDelete(Scene &scene, SceneID node_id) {
  if (node_id == 0 || node_id >= scene.nodes.size())
    return;
  SceneNode &node = scene.nodes[node_id];

  if (node.parent_id != UINT32_MAX && node.parent_id < scene.nodes.size()) {
    std::erase(scene.nodes[node.parent_id].children, node_id);
    node.parent_id = UINT32_MAX;
  }

  WalkSceneDeinit(scene, node_id);
  node.mesh.reset();
  node.children.clear();
}

void NodeTranslate(Scene &scene, SceneID node_id, const glm::vec3 &delta) {
  if (node_id < 0 || node_id > scene.nodes.size() - 1)
    return;
  SceneNode &node = scene.nodes[node_id];
  node.local_transform.translation += delta;
  node.local_transform.needs_rebuild = true;
}

void NodeRotate(Scene &scene, SceneID node_id, const glm::quat &delta) {
  if (node_id < 0 || node_id > scene.nodes.size() - 1)
    return;
  SceneNode &node = scene.nodes[node_id];
  node.local_transform.rotation = delta * node.local_transform.rotation;
  node.local_transform.needs_rebuild = true;
}

void NodeScale(Scene &scene, SceneID node_id, const glm::vec3 &delta) {
  if (node_id < 0 || node_id > scene.nodes.size() - 1)
    return;
  SceneNode &node = scene.nodes[node_id];
  node.local_transform.scale *= delta;
  const float min_scale = 0.01f;
  if (std::abs(node.local_transform.scale.x) < min_scale)
    node.local_transform.scale.x =
        (node.local_transform.scale.x < 0.0f) ? -min_scale : min_scale;
  if (std::abs(node.local_transform.scale.y) < min_scale)
    node.local_transform.scale.y =
        (node.local_transform.scale.y < 0.0f) ? -min_scale : min_scale;
  if (std::abs(node.local_transform.scale.z) < min_scale)
    node.local_transform.scale.z =
        (node.local_transform.scale.z < 0.0f) ? -min_scale : min_scale;
  node.local_transform.needs_rebuild = true;
}

void NodeSetTranslation(Scene &scene, SceneID node_id,
                        const glm::vec3 &translation) {
  if (node_id < 0 || node_id > scene.nodes.size() - 1)
    return;
  SceneNode &node = scene.nodes[node_id];
  node.local_transform.translation = translation;
  node.local_transform.needs_rebuild = true;
}

void NodeSetRotation(Scene &scene, SceneID node_id, const glm::quat &rotation) {
  if (node_id < 0 || node_id > scene.nodes.size() - 1)
    return;
  SceneNode &node = scene.nodes[node_id];
  node.local_transform.rotation = rotation;
  node.local_transform.needs_rebuild = true;
}

void NodeSetScale(Scene &scene, SceneID node_id, const glm::vec3 &scale) {
  if (node_id < 0 || node_id > scene.nodes.size() - 1)
    return;
  SceneNode &node = scene.nodes[node_id];
  node.local_transform.scale = scale;
  node.local_transform.needs_rebuild = true;
}