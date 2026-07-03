#pragma once
#include <cstdint>
#include <vector>
#include "render.h"   
#include "scene.h"

#ifdef _WIN32
    #include <Winsock2.h>
    #pragma comment(lib, "Ws2_32.lib") 
#else
    #include <arpa/inet.h>
#endif

namespace Network
{
   
    using ByteBuffer = std::vector<uint8_t>;
     
    ByteBuffer SerializeMesh(const Render:: Mesh& m);
    bool DeserializeMesh(const uint8_t* data, size_t len, Render:: Mesh& out_mesh);
    ByteBuffer SerializeScene(const Scene& scene);
    void SerializeSceneToBuffer(const Scene& scene, ByteBuffer& buffer);
    bool DeserializeScene(const uint8_t* data, size_t len, Scene& out_scene);

    void WriteBytes(ByteBuffer& buf, const void* src, size_t size);
    bool ReadBytes(const uint8_t*& ptr, const uint8_t* end, void* dst, size_t size);
    void WriteU32(ByteBuffer& buf, uint32_t v);
    bool ReadU32(const uint8_t*& ptr, const uint8_t* end, uint32_t& v);
    void WriteF32(ByteBuffer& buf, float v);
    bool ReadF32(const uint8_t*& ptr, const uint8_t* end, float& v);
}
