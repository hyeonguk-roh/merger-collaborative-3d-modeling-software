#pragma once
#include <string>
#include <thread>
#include <asio.hpp>
#include <unordered_map>
#include <mutex>
#include <rtc/rtc.hpp>
#include "render.h"
#include "lock_queue.h"
#include "scene_serialization.h"
#include "scene.h"
#include <atomic>

#define SERVER_PORT 1234
#define SERVER_PORT_STR "1234"
#define BUF_SIZE 2048
#define HEADROOM_SIZE 128
#define DEFAULT_PAYLOAD_SIZE 1024

namespace Network
{
    using MessageBuffer = std::shared_ptr<std::vector<uint8_t>>;

    enum class MessageType : uint8_t
    {
        NONE,
        CONNECT,
        SET_NUM,
        DISCONNECT,
        TRANSLATE,
        ROTATE,
        SCALE,
        ADD,
        REMOVE,
        REPARENT,
        ACKNOWL,
        NACKNOWL,
        SEND_NODE_HIERARCHY,
        NET_ERR
    };

    struct Message
    {
        MessageType type;
        std::array<uint8_t, 1024> data;      // Looking back this should definitely be a pointer to a vector of bytes or something like that
    };

    struct MessageHeader
    {
        uint16_t type;  // type of message
        uint16_t size;  // size of payload
    };

    struct BaseHeader
    {
        uint16_t flags;   // bitfield that defines what headers are added to the packet
        uint16_t size;    // total packet size
    };

    struct MessageBuffer2
    {
        std::vector<uint8_t> storage;   // preallocate upon creation with size of DEFAULT_PAYLOAD_SIZE + HEADROOM
        uint32_t start;
        uint32_t payload_size;
        uint32_t payload_end;

        void writeU32(uint32_t u);
        void writeFloat(float f);
        void writeI32(int32_t i);
        void writeU8(uint8_t u);
        void writeI8(int8_t i);
        void writeU16(uint16_t u);
        void writeI16(int16_t i);
        void writeBytes(uint8_t* bytes, uint32_t size);
    };

    // [BaseHeader][OptionalHeaders][MessageHeader][Payload]
    MessageBuffer2 CreateMessageBuffer(uint16_t type, uint32_t flags);
    bool PrependMessageBuffer(const MessageBuffer2& msg);   // this is generally only used internally

    inline MessageBuffer CreateMessageBuffer(MessageType type)
    {
        std::shared_ptr<std::vector<uint8_t>> message = std::make_shared<std::vector<uint8_t>>();
        message->reserve(sizeof(Network::MessageType));
        Network::WriteBytes(*message, &type, sizeof(Network::MessageType));
        return message;
    }

    inline MessageBuffer CreateSceneSerializationMessage(const Scene& scene, MessageType type)
    {
        MessageBuffer buffer = CreateMessageBuffer(type);
        SerializeSceneToBuffer(scene, *buffer);
        return buffer;
    }

    inline bool ReadU32At(const uint8_t* data, size_t len, size_t offset, uint32_t& out)
    {
        if (!data || offset + sizeof(uint32_t) > len) return false;
        uint32_t v;
        std::memcpy(&v, data + offset, sizeof(uint32_t));
        out = ntohl(v);
        return true;
    }

    inline MessageType ReadMessageType(const std::array<uint8_t, 2048>& message)
    {
        return static_cast<MessageType>(message[0]);
    }

    inline uint32_t ReadMessageNum(const std::array<uint8_t, 2048>& message)
    {
        uint32_t num;
        std::memcpy(&num, message.data() + sizeof(MessageType), sizeof(uint32_t));
        return ntohl(num);
    }

    inline std::string EndpointToString(const asio::ip::udp::endpoint& endpoint)
    {
        return endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
    }
    
    class Client
    {
        private:
            std::thread client_thread;
            asio::io_context io_context;
            asio::ip::udp::socket socket {io_context};
            asio::ip::udp::resolver resolver {io_context};
            std::mutex endpoint_mutex;
            asio::ip::udp::endpoint server_endpoint;
            volatile bool is_running = false;
            asio::ip::udp::endpoint receive_endpoint;
            std::array<uint8_t, BUF_SIZE> receive_buffer;
            asio::steady_timer timer {io_context};
            LockQueue<Message>* message_queue = nullptr;
            asio::strand<asio::io_context::executor_type> strand {asio::make_strand(io_context)};

            std::shared_ptr<std::vector<uint8_t>> last_sent_packet;
            uint32_t next_client_acknowl_num = 1;
            uint32_t server_current = 0;

            std::vector<uint8_t> scene_packets;
            std::vector<uint8_t> hierarchy_packets;
            uint32_t expected_scene_size = 0;
            uint32_t expected_hierarchy_size = 0;

            size_t last_num_bytes_received = 0;

            std::shared_ptr<rtc::WebSocket> sig_ws;
            std::shared_ptr<rtc::PeerConnection> pc;
            std::shared_ptr<rtc::DataChannel> dc;
            std::string signaling_url;
            int client_id = -1;

            void run();
            void handle_receive(const asio::error_code& error, std::size_t size);
            void handle_scene_recv();
            void handle_add();
            void handle_remove();
            void handle_translate();
            void handle_rotate();
            void handle_scale();
            void handle_reparent();
            void handle_node_hierarchy();
            void send_acknowl();
            void send_nacknowl();

        public:
            void init(LockQueue<Message>& message_queue);
            bool deinit();

            bool connect(const std::string& ip, const std::string& port);
            void send(const std::shared_ptr<std::vector<uint8_t>> buffer);
    };


    class Server{

        private:
            volatile bool is_running = false;
            std::thread server_thread;
            asio::io_context io_context;
            asio::ip::udp::socket socket {io_context};
            std::array<uint8_t, BUF_SIZE> buffer;
            asio::ip::udp::endpoint remote_endpoint;
            std::unordered_map<std::string, asio::ip::udp::endpoint> client_endpoints;
            std::unordered_map<int, uint32_t> client_acknowl_counters_id;
            std::unordered_map<int, uint32_t> client_expected_acknowl_num_id;
            std::unordered_map<int, std::vector<uint8_t>> client_last_packet_id;
            asio::steady_timer timer {io_context};
            std::array<uint8_t, BUF_SIZE> keep_alive_buffer;

            std::shared_ptr<rtc::WebSocket> sig_ws;
            std::unordered_map<int, std::shared_ptr<rtc::PeerConnection>> pcs;
            std::unordered_map<int, std::shared_ptr<rtc::DataChannel>> dcs;
            std::mutex peers_mutex;
            std::string signaling_url;

            Scene* scene = nullptr;

            std::vector<uint8_t> hierarchy_packets;
            uint32_t expected_hierarchy_size = 0;

            std::atomic<uint32_t> cur_object_id = 0;
            uint32_t global_server_acknowl_num = 0;
            uint32_t received;
            std::vector<uint8_t> sync_packet;
            Network::MessageType type;

            void run();
            void handle_receive(std::size_t size);
            void handle_add(std::size_t size);
            void send_acknowl(int clientId);
            void send_nacknowl(int clientId);
            void handle_connect(int clientId);
            void handle_disconnect(int clientId);
            void handle_receive_from(int from_client_id, const uint8_t* data, size_t len);
            void handle_node_hierarchy(int clientId);
            void broadcast_from(int from_client_id, const uint8_t* data, size_t len);

            void broadcast(const std::array<uint8_t, BUF_SIZE>& message, std::size_t size);

        public:
            bool init(Scene& scene);
            void deinit();
            void setSignaling(const std::string& ip, const std::string& port);
            void send_keep_alives();
    };

    class Signaling
    {
        private:
            struct ConnState {
                bool roleSet = false;
                bool isServer = false;
                int myId = -1;
            };

            std::thread signaling_thread;
            std::string bind_ip = "0.0.0.0";
            uint16_t bind_port = 8000;
            std::unordered_map<int, std::shared_ptr<rtc::WebSocket>> clients;
            std::mutex signaling_mutex;
            std::shared_ptr<rtc::WebSocket> appServer;
            bool is_running = false;
            std::unique_ptr<rtc::WebSocketServer> ws_server;
            int nextId = 1;

            void run();

        public:
            bool init();
            void deinit();
    };
}