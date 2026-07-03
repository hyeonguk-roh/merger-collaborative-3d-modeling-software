#include <iostream>
#include <string>
#include <asio.hpp>
#include <thread>
#include <chrono>
#include <array>
#include <cstring>
#include <rtc/rtc.hpp>
#include "network.h"
#include "scene_serialization.h"
#include "merger_types.h"




static const std::string TURN_IP   = "3.151.102.45";
static const std::string TURN_PORT = "3478";
static const std::string TURN_USER = "merger";
static const std::string TURN_PASS = "mergerpass";

namespace Network
{
    static rtc::Configuration make_turn_only_cfg()
    {
        rtc::Configuration cfg;
        rtc::IceServer turn("turn:" + TURN_IP + ":" + TURN_PORT + "?transport=udp");
        turn.username = TURN_USER;
        turn.password = TURN_PASS;

        cfg.iceServers.push_back(turn);
        cfg.iceTransportPolicy = rtc::TransportPolicy::Relay;

        return cfg;
    }

    void Client::init(LockQueue<Message>& message_queue) 
    {
        if (is_running) return;
        is_running = true;
        this->message_queue = &message_queue;
        client_thread = std::thread(&Client::run, this);  
    }

    void Client::handle_scene_recv()
    {
        const uint8_t* message_data = receive_buffer.data();
        message_data++;

        uint32_t length;
        std::memcpy(&length, message_data, sizeof(uint32_t));
        message_data += sizeof(uint32_t);
        length = ntohl(length);

        if (expected_scene_size == 0) expected_scene_size = length;

        size_t scene_packet_size = last_num_bytes_received - sizeof(uint8_t) - sizeof(uint32_t);
        scene_packets.insert(scene_packets.end(), message_data, message_data + scene_packet_size);

        if (scene_packets.size() < expected_scene_size) return;

        Scene scene;
        if(!Network::DeserializeScene(scene_packets.data(), expected_scene_size, scene)) std::cout << "Failed to load scene" << std::endl;

        Message msg = { .type = MessageType::CONNECT };
        *(Scene*)msg.data.data() = std::move(scene);
        message_queue->push(msg);

        scene_packets.clear();
        expected_scene_size = 0;
    }

    void Client::handle_node_hierarchy()
    {
        const uint8_t* message_data = receive_buffer.data();
        message_data++;

        uint32_t length;
        std::memcpy(&length, message_data, sizeof(uint32_t));
        message_data += sizeof(uint32_t);
        length = ntohl(length);

        if (expected_hierarchy_size == 0) expected_hierarchy_size = length;

        size_t hierarchy_packet_size = last_num_bytes_received - sizeof(uint8_t) - sizeof(uint32_t);
        hierarchy_packets.insert(hierarchy_packets.end(), message_data, message_data + hierarchy_packet_size);

        if (hierarchy_packets.size() < expected_hierarchy_size) return;

        Scene hierarchy;
        if(!Network::DeserializeScene(hierarchy_packets.data(), expected_hierarchy_size, hierarchy)) std::cout << "Failed to load hierarchy" << std::endl;

        Message msg = { .type = MessageType::SEND_NODE_HIERARCHY };
        *(Scene*)msg.data.data() = std::move(hierarchy);
        message_queue->push(msg);

        hierarchy_packets.clear();
        expected_hierarchy_size = 0;
    }

    void Client::handle_add()
    {
        const uint8_t* message_data = receive_buffer.data();
        const uint8_t* end = message_data + (receive_buffer.size() * sizeof(uint8_t));

        // Eat message byte
        ReadBytes(message_data, end, nullptr, sizeof(MessageType));

        Render::Primitive primitive_type = Render::Primitive::TRIANGLE;
        ReadBytes(message_data, end, (uint8_t*)&primitive_type, sizeof(Render::Primitive));

        glm::vec3 create_pos = glm::vec3(0.0, 0.0, 0.0);
        ReadF32(message_data, end, create_pos.x);
        ReadF32(message_data, end, create_pos.y);
        ReadF32(message_data, end, create_pos.z);

        uint32_t parent_id = 0;
        ReadU32(message_data, end, parent_id);

        uint32_t object_id = 0;
        ReadU32(message_data, end, object_id);

        Message msg = { .type = MessageType::ADD };
        uint8_t* ptr = msg.data.data();
        std::memcpy(ptr, &primitive_type, sizeof(Render::Primitive));
        ptr += sizeof(Render::Primitive);
        std::memcpy(ptr, &create_pos, sizeof(glm::vec3));
        ptr += sizeof(glm::vec3);
        std::memcpy(ptr, &parent_id, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        std::memcpy(ptr, &object_id, sizeof(uint32_t));
        ptr += sizeof(uint32_t);

        message_queue->push(msg);
    }

    void Client::handle_remove()
    {
        const uint8_t* message_data = receive_buffer.data();
        const uint8_t* end = message_data + (receive_buffer.size()) * sizeof(uint8_t);

        // Eat message byte
        ReadBytes(message_data, end, nullptr, sizeof(MessageType));
        SelectionGranularity granularity = SelectionGranularity::OBJECT;
        ReadBytes(message_data, end, &granularity, sizeof(SelectionGranularity));

        uint32_t object_id = UINT32_MAX;
        ReadU32(message_data, end, object_id);

        uint32_t optional_id = UINT32_MAX;
        if (granularity == SelectionGranularity::VERTEX || granularity == SelectionGranularity::FACE)
        {
            ReadU32(message_data, end, optional_id);
        }

        Message msg = { .type = MessageType::REMOVE };
        
        uint8_t* msg_ptr = msg.data.data();
        std::memcpy(msg_ptr, &granularity, sizeof(SelectionGranularity));
        msg_ptr += sizeof(SelectionGranularity);
        std::memcpy(msg_ptr, &object_id, sizeof(uint32_t));
        msg_ptr += sizeof(uint32_t);
        if (granularity == SelectionGranularity::VERTEX || granularity == SelectionGranularity::FACE)
        {
            std::memcpy(msg_ptr, &optional_id, sizeof(uint32_t));
        }

        message_queue->push(msg);
    }

    void Client::handle_translate()
    {
        const uint8_t* message_data = receive_buffer.data();
        const uint8_t* end = message_data + (receive_buffer.size()) * sizeof(uint8_t);

        // Eat message byte
        ReadBytes(message_data, end, nullptr, sizeof(MessageType));
        SelectionGranularity granularity = SelectionGranularity::OBJECT;
        ReadBytes(message_data, end, &granularity, sizeof(SelectionGranularity));

        uint32_t object_id = UINT32_MAX;    // Unselected Mesh
        ReadU32(message_data, end, object_id);

        uint32_t optional_id = UINT32_MAX;              // If this exists, this is either the vertex id or the face id depending on granularity
        if (granularity == SelectionGranularity::VERTEX || granularity == SelectionGranularity::FACE)
        {
            ReadU32(message_data, end, optional_id);
        }

        glm::vec3 delta = glm::vec3(0.0f, 0.0f, 0.0f);
        ReadF32(message_data, end, delta.x);
        ReadF32(message_data, end, delta.y);
        ReadF32(message_data, end, delta.z);

        Message msg = { .type = MessageType::TRANSLATE };
        
        uint8_t* msg_ptr = msg.data.data();
        std::memcpy(msg_ptr, &granularity, sizeof(SelectionGranularity));
        msg_ptr += sizeof(SelectionGranularity);
        std::memcpy(msg_ptr, &object_id, sizeof(uint32_t));
        msg_ptr += sizeof(uint32_t);
        if (granularity == SelectionGranularity::VERTEX || granularity == SelectionGranularity::FACE)
        {
            std::memcpy(msg_ptr, &optional_id, sizeof(uint32_t));
            msg_ptr += sizeof(uint32_t);
        }
        std::memcpy(msg_ptr, &delta, sizeof(glm::vec3));
        msg_ptr += sizeof(glm::vec3);

        message_queue->push(msg);
    }

    void Client::handle_rotate()
    {
        const uint8_t* message_data = receive_buffer.data();
        const uint8_t* end = message_data + receive_buffer.size();

        ReadBytes(message_data, end, nullptr, sizeof(MessageType)); // Eat type
        SelectionGranularity granularity;
        ReadBytes(message_data, end, &granularity, sizeof(SelectionGranularity));

        uint32_t object_id;
        ReadU32(message_data, end, object_id);

        Message msg = { .type = MessageType::ROTATE };
        uint8_t* msg_ptr = msg.data.data();
        std::memcpy(msg_ptr, &granularity, sizeof(SelectionGranularity));
        msg_ptr += sizeof(SelectionGranularity);
        std::memcpy(msg_ptr, &object_id, sizeof(uint32_t));
        msg_ptr += sizeof(uint32_t);

        if (granularity != SelectionGranularity::OBJECT)
        {
            uint32_t optional_id;
            ReadU32(message_data, end, optional_id);
            std::memcpy(msg_ptr, &optional_id, sizeof(uint32_t));
            msg_ptr += sizeof(uint32_t);
        }

        glm::quat rotation;
        ReadBytes(message_data, end, &rotation, sizeof(glm::quat));
        std::memcpy(msg_ptr, &rotation, sizeof(glm::quat));

        message_queue->push(msg);
    }

    void Client::handle_scale()
    {
        const uint8_t* message_data = receive_buffer.data();
        const uint8_t* end = message_data + receive_buffer.size();

        ReadBytes(message_data, end, nullptr, sizeof(MessageType)); // Eat type
        SelectionGranularity granularity;
        ReadBytes(message_data, end, &granularity, sizeof(SelectionGranularity));

        uint32_t object_id;
        ReadU32(message_data, end, object_id);

        Message msg = { .type = MessageType::SCALE };
        uint8_t* msg_ptr = msg.data.data();
        std::memcpy(msg_ptr, &granularity, sizeof(SelectionGranularity));
        msg_ptr += sizeof(SelectionGranularity);
        std::memcpy(msg_ptr, &object_id, sizeof(uint32_t));
        msg_ptr += sizeof(uint32_t);

        if (granularity != SelectionGranularity::OBJECT)
        {
            uint32_t optional_id;
            ReadU32(message_data, end, optional_id);
            std::memcpy(msg_ptr, &optional_id, sizeof(uint32_t));
            msg_ptr += sizeof(uint32_t);
        }

        glm::vec3 scale;
        ReadF32(message_data, end, scale.x);
        ReadF32(message_data, end, scale.y);
        ReadF32(message_data, end, scale.z);
        std::memcpy(msg_ptr, &scale, sizeof(glm::vec3));

        message_queue->push(msg);
    }

    void Client::handle_reparent()
    {
        const uint8_t* message_data = receive_buffer.data();
        const uint8_t* end = message_data + receive_buffer.size();
        ReadBytes(message_data, end, nullptr, sizeof(MessageType)); // Eat type

        // Get New Parent ID
        SceneID parent = UINT32_MAX;
        ReadU32(message_data, end, parent);
        SceneID child = UINT32_MAX;
        ReadU32(message_data, end, child);

        Message msg = { .type = MessageType::REPARENT };
        uint8_t* msg_ptr = msg.data.data();
        std::memcpy(msg_ptr, &parent, sizeof(SceneID));
        msg_ptr += sizeof(SceneID);
        std::memcpy(msg_ptr, &child, sizeof(SceneID));

        message_queue->push(msg);
    }

    void Client::handle_receive(const asio::error_code& error, std::size_t size)
    {
        if (error)
        {
            std::cout << "[client] receive error: " << error.message() << std::endl;
            return;
        }

        last_num_bytes_received = size;

        // Do stuff outlined by message type
        switch (ReadMessageType(receive_buffer))
        {
            case MessageType::CONNECT:
                send_acknowl();
                handle_scene_recv();
                break;

            case MessageType::SET_NUM:
                server_current = ReadMessageNum(receive_buffer);
                next_client_acknowl_num = server_current + 1;
                break;

            case MessageType::TRANSLATE:
                send_acknowl();
                handle_translate();
                break;

            case MessageType::ADD:
                send_acknowl();
                handle_add();
                break;

            case MessageType::REMOVE:
                send_acknowl();
                handle_remove();
                break;

            case MessageType::ROTATE:
                send_acknowl();
                handle_rotate();
                break;

            case MessageType::SCALE:
                send_acknowl();
                handle_scale();
                break;

            case MessageType::REPARENT:
                send_acknowl();
                handle_reparent();
                break;

            case MessageType::ACKNOWL:
                if (server_current == ReadMessageNum(receive_buffer))
                    server_current += 2;
                break;
            case MessageType::NACKNOWL:
                if (last_sent_packet)
                    send(last_sent_packet); 
                break;

            case MessageType::SEND_NODE_HIERARCHY:
                send_acknowl();
                handle_node_hierarchy();
                break;
            default:
                break;
        }
    }

    void Client::send_acknowl()
    {
        std::shared_ptr<std::vector<uint8_t>> acknowl_message = std::make_shared<std::vector<uint8_t>>();
        acknowl_message->reserve(sizeof(Network::MessageType) + sizeof(uint32_t));
        Network::MessageType type = Network::MessageType::ACKNOWL;
        Network::WriteBytes(*acknowl_message, &type, sizeof(Network::MessageType));
        Network::WriteU32(*acknowl_message, next_client_acknowl_num);
        send(acknowl_message);
        next_client_acknowl_num += 2;
    }


    void Client::send_nacknowl()
    {
        std::shared_ptr<std::vector<uint8_t>> nacknowl_message = std::make_shared<std::vector<uint8_t>>();
        nacknowl_message->reserve(sizeof(Network::MessageType));
        Network::MessageType type = Network::MessageType::NACKNOWL;
        Network::WriteBytes(*nacknowl_message, &type, sizeof(Network::MessageType));
        send(nacknowl_message);
    }
         


    void Client::run() 
    {
        try
        {
            while (is_running)
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        catch (const std::exception& e) 
        {
            std::cerr << "Client exception: " << e.what() << std::endl;
        }
    }


    bool Client::connect(const std::string& ip, const std::string& port)
    {
        
        signaling_url = "ws://" + ip + ":" + port;
        //std::cout << "[client] signaling: " << signaling_url << "\n";

        sig_ws = std::make_shared<rtc::WebSocket>();

        sig_ws->onOpen([this]() {
            //std::cout << "[client] signaling open\n";
            sig_ws->send("ROLE CLIENT");
        });

        sig_ws->onClosed([]() {
            //std::cout << "[client] signaling closed\n";
        });

        sig_ws->onError([](std::string err) {
            std::cout << "[client] signaling error: " << err << "\n";
        });

        sig_ws->onMessage([this](rtc::message_variant msg) {
            if (!std::holds_alternative<std::string>(msg)){
                return;
            }

            std::string str = std::get<std::string>(msg);

            if (str.rfind("ID ", 0) == 0)
            {
                client_id = std::stoi(str.substr(3));

                pc = std::make_shared<rtc::PeerConnection>(make_turn_only_cfg());

                pc->onStateChange([](rtc::PeerConnection::State st) {
                    //std::cout << "[client] pc state=" << (int)st << "\n";
                });

                pc->onLocalDescription([this](rtc::Description desc) {
                    if (desc.typeString() == "offer") {
                        sig_ws->send(std::string("OFFER\n") + std::string(desc));
                    }
                });

                pc->onLocalCandidate([this](rtc::Candidate cand) {
                    sig_ws->send(std::string("CAND\n") + std::string(cand));
                });

                // Create data channel
                dc = pc->createDataChannel("merger");

                dc->onOpen([]() {
                    //std::cout << "[client] dc open\n";
                });

                dc->onClosed([]() {
                   // std::cout << "[client] dc closed\n";
                });

                dc->onError([](std::string err) {
                    std::cout << "[client] dc error: " << err << "\n";
                });

                dc->onMessage([this](rtc::message_variant mv) {
                    if (!std::holds_alternative<rtc::binary>(mv)){
                        return;
                    }

                    const rtc::binary& bytes = std::get<rtc::binary>(mv);

                    size_t n = bytes.size();
                    if (n > receive_buffer.size()) {
                        std::cout << "[client] packet too big: " << n << "\n";
                        return;
                    }

                    std::memcpy(receive_buffer.data(), bytes.data(), n);
                    if (n < receive_buffer.size())
                        std::memset(receive_buffer.data() + n, 0, receive_buffer.size() - n);
                    handle_receive(asio::error_code{}, n);
                });

                pc->setLocalDescription(rtc::Description::Type::Offer);
                return;
            }

            if (str.rfind("ANSWER\n", 0) == 0)
            {
                std::string sdp = str.substr(7);
                //std::cout << "[client] got ANSWER\n";
                if (pc) pc->setRemoteDescription(rtc::Description(sdp, "answer"));
                return;
            }

            if (str.rfind("CAND\n", 0) == 0)
            {
                std::string cand = str.substr(5);
                if (pc) pc->addRemoteCandidate(rtc::Candidate(cand));
                return;
            }
        });

        sig_ws->open(signaling_url);
        return true;
    }

    void Client::send(const std::shared_ptr<std::vector<uint8_t>> buffer)
    {
        if (!buffer || buffer->empty()) return;

        Network::MessageType type = static_cast<Network::MessageType>((*buffer)[0]);
        if (type != Network::MessageType::ACKNOWL && type != Network::MessageType::NACKNOWL) last_sent_packet = buffer;


        if (!dc || !dc->isOpen()) {
            //std::cout << "[client] dc not open yet\n";
            return;
        }

        if (type == Network::MessageType::SEND_NODE_HIERARCHY) {
            const uint8_t* buffer_data = buffer->data();
            size_t buffer_size = buffer->size();
            const uint8_t* scene_data = buffer_data + sizeof(uint8_t);
            size_t scene_size = buffer_size - sizeof(uint8_t);

            std::vector<uint8_t> packet;
            size_t offset = 0;
            size_t packet_len;

            bool has_hierarchy_size = true;

            while (offset < scene_size) {
                packet.clear();
                if (scene_size - offset < 1000) packet_len = scene_size - offset; else packet_len = 1000;

                packet.reserve(sizeof(uint8_t) + sizeof(uint32_t) + packet_len);
                packet.push_back(static_cast<uint8_t>(MessageType::SEND_NODE_HIERARCHY));

                uint32_t len;
                if (has_hierarchy_size) len = static_cast<uint32_t>(scene_size); else len = static_cast<uint32_t>(packet_len);
                len = htonl(len);
                uint8_t* len_bytes = reinterpret_cast<uint8_t*>(&len);
                packet.insert(packet.end(), len_bytes, len_bytes + sizeof(uint32_t));

                packet.insert(packet.end(), scene_data + offset, scene_data + offset + packet_len);

                dc->send(reinterpret_cast<const std::byte*>(packet.data()), packet.size());

                offset += packet_len;
                has_hierarchy_size = false;
            }
        } else dc->send(reinterpret_cast<const std::byte*>(buffer->data()), buffer->size());
    }

    bool Client::deinit()
    {
        if (!is_running) return true;
        is_running = false;

        try {
            if (sig_ws)
                sig_ws->close();
        }
        catch (const std::exception& e)
        {
            std::cout << "[client] signaling close failed: "
                    << e.what() << std::endl;
        }
        dc.reset();
        pc.reset();
        sig_ws.reset();
        
        if (client_thread.joinable()) client_thread.join();
        return true;
    }
}

