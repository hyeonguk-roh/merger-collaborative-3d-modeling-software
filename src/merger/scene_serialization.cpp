#include <cstdint>
#include <vector>
#include <cstring>
#include <glad/glad.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iostream>
#include "scene_serialization.h"

namespace Network {

    using Render::Mesh;
    using Render::Transform;
    using Render::Vertex;
    using Render::Face;

    void WriteBytes(ByteBuffer& buf, const void* src, size_t size)
    {
        size_t pos = buf.size();
        buf.resize(pos + size);
        std::memcpy(buf.data() + pos, src, size);
    }

    bool ReadBytes(const uint8_t*& ptr, const uint8_t* end, void* dst, size_t size)
    {
        if (ptr + size > end){ 
            return false;
        }

        // Only copy when dst != null (allows us to eat bytes we don't need to save)
        if (dst) std::memcpy(dst, ptr, size);
        ptr += size;
        return true;
    }

    void WriteU32(ByteBuffer& buf, uint32_t v)
    {
        uint32_t net = htonl(v); 
        WriteBytes(buf, &net, sizeof(net));
    }

    bool ReadU32(const uint8_t*& ptr, const uint8_t* end, uint32_t& v)
    {
        uint32_t net = 0;
        if (!ReadBytes(ptr, end, &net, sizeof(net))){
            return false;
        }

        v = ntohl(net);                               
        return true;
    }

    void WriteF32(ByteBuffer& buf, float v)
    {
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));         
        uint32_t net = htonl(bits);                   
        WriteBytes(buf, &net, sizeof(net));
    }

    bool ReadF32(const uint8_t*& ptr, const uint8_t* end, float& v)
    {
        uint32_t net = 0;
        if (!ReadBytes(ptr, end, &net, sizeof(net))){
            return false;
        }

        uint32_t host = ntohl(net);                   
        std::memcpy(&v, &host, sizeof(v));            
        return true;
    }

    static void write_transform(ByteBuffer& buf, const Transform& t)
    {
    
        WriteF32(buf, t.translation.x);
        WriteF32(buf, t.translation.y);
        WriteF32(buf, t.translation.z);

    
        WriteF32(buf, t.rotation.w);
        WriteF32(buf, t.rotation.x);
        WriteF32(buf, t.rotation.y);
        WriteF32(buf, t.rotation.z);

    
        WriteF32(buf, t.scale.x);
        WriteF32(buf, t.scale.y);
        WriteF32(buf, t.scale.z);
    }

    static bool read_transform(const uint8_t*& ptr, const uint8_t* end, Transform& t)
    {
        bool ok =
            ReadF32(ptr, end, t.translation.x) &&
            ReadF32(ptr, end, t.translation.y) &&
            ReadF32(ptr, end, t.translation.z) &&
            ReadF32(ptr, end, t.rotation.w)    &&
            ReadF32(ptr, end, t.rotation.x)    &&
            ReadF32(ptr, end, t.rotation.y)    &&
            ReadF32(ptr, end, t.rotation.z)    &&
            ReadF32(ptr, end, t.scale.x)       &&
            ReadF32(ptr, end, t.scale.y)       &&
            ReadF32(ptr, end, t.scale.z);

        if (ok) {
            t.needs_rebuild = true; 
        }
        return ok;
    }

    static void write_vertex(ByteBuffer& buf, const Vertex& v)
    {
        WriteF32(buf, v.position.x);
        WriteF32(buf, v.position.y);
        WriteF32(buf, v.position.z);
        WriteF32(buf, v.normal.x);
        WriteF32(buf, v.normal.y);
        WriteF32(buf, v.normal.z);
    }

    static bool read_vertex(const uint8_t*& ptr, const uint8_t* end, Vertex& v)
    {
        return ReadF32(ptr, end, v.position.x) 
            && ReadF32(ptr, end, v.position.y) 
            && ReadF32(ptr, end, v.position.z)
            && ReadF32(ptr, end, v.normal.x)
            && ReadF32(ptr, end, v.normal.y)
            && ReadF32(ptr, end, v.normal.z);
    }

    static void write_face(ByteBuffer& buf, const Face& f)
    {
        WriteU32(buf, f.vert_count);
        for (uint32_t i = 0; i < 4; ++i) {
            WriteU32(buf, f.vertex_ids[i]);
        }
    }

    static bool read_face(const uint8_t*& ptr, const uint8_t* end, Face& f)
    {
        if (!ReadU32(ptr, end, f.vert_count)){
            return false;
        }

        if (f.vert_count > 4){
            return false; 
        }

        for (uint32_t i = 0; i < 4; ++i) {
            if (!ReadU32(ptr, end, f.vertex_ids[i])){
                return false;
            }
        }
        return true;
    }

    ByteBuffer SerializeMesh(const Mesh& m)
    {
        ByteBuffer buf;

        uint32_t vcount = static_cast<uint32_t>(m.vertices.size());
        WriteU32(buf, vcount);
        for (const auto& v : m.vertices) {
            write_vertex(buf, v);
        }

        uint32_t fcount = static_cast<uint32_t>(m.faces.size());
        WriteU32(buf, fcount);
        for (const auto& f : m.faces) {
            write_face(buf, f);
        }

        return buf;
    }

    bool DeserializeMesh(const uint8_t* data, size_t len, Mesh& out_mesh)
    {
        const uint8_t* ptr = data;
        const uint8_t* end = data + len;

        out_mesh = {}; 

        
        uint32_t vcount = 0;
        if (!ReadU32(ptr, end, vcount)) {
            std::cout << " Failed to read vertex count\n";
            return false;
        }

        out_mesh.vertices.resize(vcount);
        for (uint32_t i = 0; i < vcount; ++i) {
            if (!read_vertex(ptr, end, out_mesh.vertices[i])) {
                std::cout << "Failed reading vertex " << i << "\n";
                return false;
            }
        }

        
        uint32_t fcount = 0;
        if (!ReadU32(ptr, end, fcount)) {
            std::cout << "Failed to read face count\n";
            return false;
        }

        out_mesh.faces.resize(fcount);
        for (uint32_t i = 0; i < fcount; ++i) {
            if (!read_face(ptr, end, out_mesh.faces[i])) {
                std::cout << "Failed reading face " << i << "\n";
                return false;
            }
        }
        
        out_mesh.needs_rebuild = true;
        return true;
    }

    static ByteBuffer SerializeNode(const SceneNode& node)
    {
        ByteBuffer buffer;
        WriteU32(buffer, node.parent_id);
        write_transform(buffer, node.local_transform);
        if (node.mesh)
        {
            uint8_t has_mesh = 1;
            WriteBytes(buffer, &has_mesh, sizeof(uint8_t));
            ByteBuffer mbuf = SerializeMesh(*node.mesh);
            uint32_t mlen = static_cast<uint32_t>(mbuf.size());
            WriteU32(buffer, mlen);
            buffer.insert(buffer.end(), mbuf.begin(), mbuf.end());
        }
        else
        {
            uint8_t has_mesh = 0;
            WriteBytes(buffer, &has_mesh, sizeof(uint8_t));
        }

        WriteU32(buffer, static_cast<uint32_t>(node.children.size()));
        for (uint32_t child : node.children)
        {
            WriteU32(buffer, child);
        }

        return buffer;
    }

    void SerializeSceneToBuffer(const Scene& scene, std::vector<uint8_t>& buffer)
    {
        WriteU32(buffer, scene.root);

        uint32_t node_count = static_cast<uint32_t>(scene.nodes.size());
        WriteU32(buffer, node_count);

        for (const SceneNode& node : scene.nodes)
        {
            ByteBuffer node_buf = SerializeNode(node);
            WriteU32(buffer, static_cast<uint32_t>(node_buf.size()));
            buffer.insert(buffer.end(), node_buf.begin(), node_buf.end());
        }
    }


    ByteBuffer SerializeScene(const Scene& scene)
    {
        ByteBuffer buf;
        SerializeSceneToBuffer(scene, buf);
        return buf;
    }

    static bool DeserializeNode(const uint8_t* data, size_t len, SceneNode& out_node)
    {
        const uint8_t* ptr = data;
        const uint8_t* end = data + len;

        out_node = {};

        uint32_t parent_id;
        if (!ReadU32(ptr, end, parent_id))
        {
            std::cout << "Failed to read parent_id" << std::endl;
            return false;
        }
        out_node.parent_id = parent_id;

        if (!read_transform(ptr, end, out_node.local_transform))
        {
            std::cout << "Failed to read transform" << std::endl;
            return false;
        }

        uint8_t has_mesh;
        if (!ReadBytes(ptr, end, &has_mesh, sizeof(uint8_t)))
        {
            std::cout << "Failed to read has_mesh" << std::endl;
            return false;
        }

        if (has_mesh)
        {
            uint32_t mesh_len;
            if (!ReadU32(ptr, end, mesh_len))
            {
                std::cout << "Failed to read mesh_len" << std::endl;
                return false;
            }

            out_node.mesh = std::make_unique<Mesh>();
            if (!DeserializeMesh(ptr, mesh_len, *out_node.mesh))
            {
                std::cout << "Failed to deserialize mesh" << std::endl;
                return false;
            }
            ptr += mesh_len;
        }

        uint32_t children_count;
        if (!ReadU32(ptr, end, children_count))
        {
            std::cout << "Failed to read children_count" << std::endl;
            return false;
        }
        
        out_node.children.reserve(children_count);
        for (int i = 0; i < children_count; i++)
        {
            uint32_t child_id;
            if (!ReadU32(ptr, end, child_id))
            {
                std::cout << "Failed to read child_id" << std::endl;
                return false;
            }
            out_node.children.push_back(child_id);
        }

        return true;
    }

    bool DeserializeScene(const uint8_t* data, size_t len, Scene& out_scene)
    {
        const uint8_t* ptr = data;
        const uint8_t* end = data + len;
        out_scene = {};

        uint32_t root_id;
        if (!ReadU32(ptr, end, root_id))
        {
            std::cout << "Failed to read root_id" << std::endl;
            return false;
        }
        out_scene.root = root_id;

        uint32_t node_count = 0;
        if (!ReadU32(ptr, end, node_count))
        {
            std::cout << "Failed to read node_count" << std::endl;
            return false;
        }

        out_scene.nodes.reserve(node_count);

        for (int i = 0; i < node_count; i++)
        {
            uint32_t node_len = 0;
            if (!ReadU32(ptr, end, node_len))
            {
                std::cout << "Failed to read node_length" << std::endl;
                return false;
            }

            SceneNode node;
            if (!DeserializeNode(ptr, node_len, node))
            {
                std::cout << "Failed to deserialize node" << std::endl;
                return false;
            }

            out_scene.nodes.push_back(std::move(node));
            ptr += node_len;
        }

        return true;
    }
} 
