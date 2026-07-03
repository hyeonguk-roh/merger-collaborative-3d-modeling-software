#include "scene.h"
#include "render.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>

static Assimp::Importer assimp_importer;

static std::optional<Render::Mesh> LoadAssimpMesh(aiMesh* mesh)
{
    Render::Mesh out_mesh;
        
    // Extract faces
    for (int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        if (face.mNumIndices != 3 && face.mNumIndices != 4) return {};

        Render::Face out_face;
        out_face.vert_count = face.mNumIndices;

        for (int j = 0; j < face.mNumIndices; j++)
        {
            out_face.vertex_ids[j] = face.mIndices[j];
        }

        out_mesh.faces.push_back(out_face);
    }

    // Extract Vertices
    for (int i = 0; i < mesh->mNumVertices; i++)
    {
        Render::Vertex vertex;
        aiVector3D position = mesh->mVertices[i];
        vertex.position = glm::vec3(position.x, position.y, position.z);

        // Don't need to load normals since we compute them ourselves later on
        // if (mesh->HasNormals())
        // {
        //     aiVector3D normal = mesh->mNormals[i];
        //     vertex.normal = glm::vec3(normal.x, normal.y, normal.z);
        // }

        out_mesh.vertices.push_back(vertex);
    }

    Render::RebuildMesh(out_mesh);

    return out_mesh;
}

std::optional<Render::Mesh> LoadMeshFromFile(const std::string& path)
{
    const aiScene* scene = assimp_importer.ReadFile(path, aiProcess_JoinIdenticalVertices | aiProcess_DropNormals);
    if (scene == nullptr) return {};

    std::optional<Render::Mesh> mesh = LoadAssimpMesh(scene->mMeshes[0]);
    assimp_importer.FreeScene();

    return mesh;
}

static bool LoadSceneNodeAssimp(Scene& scene, const aiNode* node, SceneID parent, const aiScene* assimp_scene)
{
    SceneID id = CreateNode(scene, parent);

    // Load node mesh/meshes if they exist
    // FIXME: For now we are just loading the first mesh of the node but will want to load all of them eventually
    if (node->mNumMeshes > 0)
    {
        std::optional<Render::Mesh> mesh = LoadAssimpMesh(assimp_scene->mMeshes[node->mMeshes[0]]);
        if (!mesh)
        {
            std::cout << "Failed to load mesh" << std::endl;
            return false;
        }
        NodeAttachMesh(scene, id, std::make_unique<Render::Mesh>(std::move(mesh.value())));
    }

    // Set node transform
    aiVector3D scale;
    aiQuaternion rotation;
    aiVector3D translation;
    node->mTransformation.Decompose(scale, rotation, translation);

    NodeSetScale(scene, id, glm::vec3(scale.x, scale.y, scale.z));
    NodeSetRotation(scene, id, glm::quat(rotation.w, rotation.x, rotation.y, rotation.z));
    NodeSetTranslation(scene, id, glm::vec3(translation.x, translation.y, translation.z));
    

    for (int i = 0; i < node->mNumChildren; i++)
    {
        if (!LoadSceneNodeAssimp(scene, node->mChildren[i], id, assimp_scene)) return false;
    }

    return true;
}

std::optional<Scene> LoadNodeHierarchyFromFile(const std::string& path)
{
    const aiScene* assimp_scene = assimp_importer.ReadFile(path, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_DropNormals);
    if (assimp_scene == nullptr) 
    {
        std::cerr << "ASSIMP: Failed to load scene into memory" << std::endl;
        std::cerr << assimp_importer.GetErrorString() << std::endl;
        return {};
    }

    Scene scene = InitScene();

    // Recursively load all meshes and apply transforms
    if (!LoadSceneNodeAssimp(scene, assimp_scene->mRootNode, 0, assimp_scene))
    {
        std::cerr << "Failed to load scene nodes" << std::endl;
        return {};
    }
    
    assimp_importer.FreeScene();

    return scene;
}