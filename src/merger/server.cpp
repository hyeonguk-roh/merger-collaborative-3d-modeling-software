#include <iostream>
#include <string>
#include <asio.hpp>
#include "network.h"
#include <array>
#include <functional>
#include <memory>
#include <vector>
#include "scene_serialization.h"  
#include <unordered_map>
#include <mutex>
#include <thread>
#include <chrono>
#include <cstring>
#include <rtc/rtc.hpp>


namespace Network
{
    static const std::string TURN_IP   = "3.151.102.45";
    static const std::string TURN_PORT = "3478";
    static const std::string TURN_USER = "merger";
    static const std::string TURN_PASS = "mergerpass";

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
    void Server::setSignaling(const std::string& ip, const std::string& port)
    {
        signaling_url = "ws://" + ip + ":" + port;
    }

    bool Server::init(Scene& scene)
    {
        if (is_running) return true;

        buffer = {};
        keep_alive_buffer = {};        
        this->scene = &scene;
        cur_object_id = 0;
        is_running = true;
        client_acknowl_counters_id.clear();
        client_expected_acknowl_num_id.clear();
        client_last_packet_id.clear();

        
        if (signaling_url.empty())
            signaling_url = "ws://127.0.0.1:8000";
        sig_ws = std::make_shared<rtc::WebSocket>();

        sig_ws->onOpen([this]() {
            //std::cout << "[server] signaling open: " << signaling_url << "\n";
            sig_ws->send("ROLE SERVER");
        });

        sig_ws->onClosed([this]() {
            //std::cout << "[server] signaling closed\n";
        });

        sig_ws->onError([](std::string err) {
            std::cout << "[server] signaling error: " << err << "\n";
        });

        sig_ws->onMessage([this](rtc::message_variant msg) {
            if (!std::holds_alternative<std::string>(msg)){
                return;
            }

            const std::string s = std::get<std::string>(msg);

            if (s.rfind("ERR ", 0) == 0)
            {
                // std::cout << "[server] signaling rejected ROLE SERVER: " << s << "\n";
                // std::cout << "[server] This instance will NOT be host relay. Shutting down Server::init().\n";

                is_running = false;

                try {
                    if (sig_ws)
                        sig_ws->close();
                    }
                catch (const std::exception& e)
                {
                    std::cout << "failed: "
                    << e.what() << std::endl;
                }

                return;
            }

            if (s.rfind("OFFER ", 0) == 0)
            {
                std::size_t nl = s.find('\n');
                if (nl == std::string::npos) return;

                int clientId = -1;
                try {
                    clientId = std::stoi(s.substr(6, nl - 6));
                } catch (const std::exception& e) { return; }

                std::string offerSdp = s.substr(nl + 1);
                //std::cout << "[server] got OFFER from client " << clientId << "\n";

                std::shared_ptr<rtc::PeerConnection> pc;

                {
                    std::lock_guard<std::mutex> lock(peers_mutex);

                    auto it = pcs.find(clientId);
                    if (it == pcs.end())
                    {
                        pc = std::make_shared<rtc::PeerConnection>(make_turn_only_cfg());
                        pcs[clientId] = pc;

                        pc->onStateChange([clientId](rtc::PeerConnection::State st) {
                           // std::cout << "[server] pc(" << clientId << ") state=" << (int)st << "\n";
                        });

                        pc->onLocalDescription([this, clientId](rtc::Description desc) {
                            if (desc.typeString() == "answer")
                            {
                                //std::cout << "[server] send ANSWER to client " << clientId << "\n";
                                sig_ws->send("ANSWER " + std::to_string(clientId) + "\n" + std::string(desc));
                            }
                        });

                        pc->onLocalCandidate([this, clientId](rtc::Candidate cand) {
                            sig_ws->send("CAND " + std::to_string(clientId) + "\n" + std::string(cand));
                        });

                        pc->onDataChannel([this, clientId](std::shared_ptr<rtc::DataChannel> dc) {
                           // std::cout << "[server] dc received from client " << clientId << "\n";

                            {
                                std::lock_guard<std::mutex> lock(peers_mutex);
                                dcs[clientId] = dc;
                            }

                            dc->onOpen([this, clientId, dc]() {
                                //std::cout << "[server] dc(" << clientId << ") open\n";

                                {
                                    std::lock_guard<std::mutex> lock(peers_mutex);
                                    client_acknowl_counters_id[clientId] = 0;      
                                    client_expected_acknowl_num_id[clientId] = 1;  
                                }

                                sync_packet.clear();
                                Network::MessageType t = MessageType::SET_NUM;
                                Network::WriteBytes(sync_packet, &t, sizeof(t));
                                Network::WriteU32(sync_packet, 0); 

                                {
                                    std::lock_guard<std::mutex> lock(peers_mutex);
                                    client_last_packet_id[clientId] = sync_packet;
                                }

                                dc->send(reinterpret_cast<const std::byte*>(sync_packet.data()), sync_packet.size());
                                send_acknowl(clientId); 
                                handle_connect(clientId);
                            });

                            dc->onClosed([this, clientId]() {
                               // std::cout << "[server] dc(" << clientId << ") closed\n";
                                std::lock_guard<std::mutex> lock(peers_mutex);
                                dcs.erase(clientId);
                                pcs.erase(clientId);
                            });

                            dc->onError([clientId](std::string err) {
                                std::cout << "[server] dc(" << clientId << ") error: " << err << "\n";
                            });

                            
                            dc->onMessage([this, clientId](rtc::message_variant mv) {
                                if (!std::holds_alternative<rtc::binary>(mv)) return;
                                const auto& b = std::get<rtc::binary>(mv);
                                if (b.empty()) return;

                                handle_receive_from(
                                    clientId,
                                    reinterpret_cast<const uint8_t*>(b.data()),
                                    b.size()
                                );
                            });
                        });
                    }
                    else
                    {
                        pc = it->second;
                    }
                }

                pc->setRemoteDescription(rtc::Description(offerSdp, "offer"));
                pc->setLocalDescription(rtc::Description::Type::Answer);
                return;
            }

            if (s.rfind("CAND ", 0) == 0)
            {
                auto nl = s.find('\n');
                if (nl == std::string::npos) return;

                int clientId = -1;
                try {
                    clientId = std::stoi(s.substr(5, nl - 5));
                } 
                catch (const std::exception& e) {
                    return;
                }

                std::string cand = s.substr(nl + 1);

                std::lock_guard<std::mutex> lock(peers_mutex);
                auto it = pcs.find(clientId);
                if (it != pcs.end())
                {
                    it->second->addRemoteCandidate(rtc::Candidate(cand));
                }
                return;
            }

        });

        sig_ws->open(signaling_url);

        server_thread = std::thread(&Server::run, this);
        return true;
    }

    void Server::send_acknowl(int clientId)
    {
        std::shared_ptr<rtc::DataChannel> dc;
        uint32_t num = 0;

        {
            std::lock_guard<std::mutex> lock(peers_mutex);
            auto itDc = dcs.find(clientId);
            if (itDc == dcs.end() || !itDc->second || !itDc->second->isOpen())
                return;
            dc = itDc->second;

            num = client_acknowl_counters_id[clientId];
            client_acknowl_counters_id[clientId] += 2;
        }

        std::vector<uint8_t> packet;
        Network::MessageType type = MessageType::ACKNOWL;
        Network::WriteBytes(packet, &type, sizeof(type));
        Network::WriteU32(packet, num);

        
        dc->send(reinterpret_cast<const std::byte*>(packet.data()), packet.size());
    }

    void Server::send_nacknowl(int clientId)
    {
        std::shared_ptr<rtc::DataChannel> dc;

        {
            std::lock_guard<std::mutex> lock(peers_mutex);
            auto itDc = dcs.find(clientId);
            if (itDc == dcs.end() || !itDc->second || !itDc->second->isOpen())
                return;
            dc = itDc->second;
        }

        std::vector<uint8_t> packet;
        Network::MessageType type = MessageType::NACKNOWL;
        Network::WriteBytes(packet, &type, sizeof(type));

        dc->send(reinterpret_cast<const std::byte*>(packet.data()), packet.size());
    }

    

    void Server::handle_connect(int clientId)
    {
        if (!scene) return;

        std::shared_ptr<rtc::DataChannel> dc;
        {
            std::lock_guard<std::mutex> lock(peers_mutex);
            auto it = dcs.find(clientId);
            if (it != dcs.end()) dc = it->second;
        }

        if (!dc || !dc->isOpen()) {
            std::cout << "[server] cannot send scene to client " << clientId << "\n";
            return;
        }

        std::vector<uint8_t> scene_bytes = SerializeScene(*scene);
        std::vector<uint8_t> packet;
        size_t offset = 0;
        size_t packet_len;

        bool has_scene_size = true;

        while (offset < scene_bytes.size()) {
            packet.clear();
            if (scene_bytes.size() - offset < 1000) packet_len = scene_bytes.size() - offset; else packet_len = 1000;

            // Message Type (uint8_t) + Length (uint32_t) + packet_len
            packet.reserve(sizeof(uint8_t) + sizeof(uint32_t) + packet_len);
            packet.push_back(static_cast<uint8_t>(MessageType::CONNECT));

            uint32_t len;
            if (has_scene_size) len = static_cast<uint32_t>(scene_bytes.size()); else len = static_cast<uint32_t>(packet_len);
            len = htonl(len);
            uint8_t* len_bytes = reinterpret_cast<uint8_t*>(&len);
            packet.insert(packet.end(), len_bytes, len_bytes + sizeof(uint32_t));

            packet.insert(packet.end(), scene_bytes.begin() + offset, scene_bytes.begin() + offset + packet_len);

            dc->send(reinterpret_cast<const std::byte*>(packet.data()), packet.size());

            offset += packet_len;
            has_scene_size = false;
        }

    }

    void Server::handle_disconnect(int clientId)
    {
        std::lock_guard<std::mutex> lock(peers_mutex);
        std::cout << "[server] client " << clientId << " disconnected\n";
        dcs.erase(clientId);
        pcs.erase(clientId);
        client_acknowl_counters_id.erase(clientId);
        client_expected_acknowl_num_id.erase(clientId);
        client_last_packet_id.erase(clientId);
    }
    void Server::broadcast(const std::array<uint8_t, BUF_SIZE>& message, std::size_t size)
    {
        for (auto& [clientId, dc] : dcs)
        {
            if (!dc || !dc->isOpen()) continue;

            dc->send(reinterpret_cast<const std::byte*>(message.data()), size);
        }
    }

    void Server::send_keep_alives()
    {
        timer.expires_after(std::chrono::seconds(10));

        timer.async_wait([this](asio::error_code ec) {
            broadcast(keep_alive_buffer, sizeof(keep_alive_buffer));
            send_keep_alives();
        });
    }

    
    // This is the WebRTC replacement for  handle_receive().
   void Server::handle_receive_from(int from_client_id, const uint8_t* data, size_t len)
    {
        if (!data || len == 0) return;

        
        if (len > buffer.size())
        {
            std::cout << "[server] packet too big from " << from_client_id << ": " << len << "\n";
            return;
        }

        
        MessageType type = static_cast<MessageType>(data[0]);

        switch (type)
        {
            case MessageType::ADD:
            {
                send_acknowl(from_client_id);
                std::vector<uint8_t> packet;
                packet.reserve(len + sizeof(uint32_t));
                packet.insert(packet.end(), data, data + len);

                uint32_t id = cur_object_id++;
                uint32_t net_object_id = htonl(id);
                uint8_t* id_bytes = reinterpret_cast<uint8_t*>(&net_object_id);
                packet.insert(packet.end(), id_bytes, id_bytes + sizeof(uint32_t));
                {
                    std::lock_guard<std::mutex> lock(peers_mutex);
                    client_last_packet_id[from_client_id] = packet;
                }

                broadcast_from(from_client_id, packet.data(), packet.size());
                return;
            }
            case MessageType::CONNECT:
                send_acknowl(from_client_id);
                handle_connect(from_client_id);
                return;
            case MessageType::DISCONNECT:
                send_acknowl(from_client_id);
                handle_disconnect(from_client_id);
                return;
            case MessageType::TRANSLATE:
            case MessageType::ROTATE:
            case MessageType::SCALE:
            case MessageType::REMOVE:
                send_acknowl(from_client_id);
                {
                    std::lock_guard<std::mutex> lock(peers_mutex);
                    client_last_packet_id[from_client_id] = std::vector<uint8_t>(data, data + len);
                }
                broadcast_from(from_client_id, data, len);
                return;
            case MessageType::REPARENT:
                send_acknowl(from_client_id);
                broadcast_from(from_client_id, data, len);
                break;
            case MessageType::ACKNOWL:
            {
                uint32_t received_num = 0;
                if (!ReadU32At(data, len, sizeof(MessageType), received_num)) return;

                bool should_nack = false;
                {
                    std::lock_guard<std::mutex> lock(peers_mutex);
                    uint32_t expected = client_expected_acknowl_num_id[from_client_id];

                    if (received_num == expected)
                        client_expected_acknowl_num_id[from_client_id] += 2;
                    else if (received_num > expected)
                        should_nack = true;
                }

                if (should_nack)
                    send_nacknowl(from_client_id);

                return;
            }
            case MessageType::NACKNOWL:
            {
                std::vector<uint8_t> packet;
                std::shared_ptr<rtc::DataChannel> dc;

                {
                    std::lock_guard<std::mutex> lock(peers_mutex);

                    auto itDc = dcs.find(from_client_id);
                    if (itDc == dcs.end() || !itDc->second || !itDc->second->isOpen())
                        return;
                    dc = itDc->second;

                    auto itPkt = client_last_packet_id.find(from_client_id);
                    if (itPkt == client_last_packet_id.end())
                        return;

                    packet = itPkt->second;
                }

                dc->send(reinterpret_cast<const std::byte*>(packet.data()), packet.size());
                return;
            }
            case MessageType::SEND_NODE_HIERARCHY:
                send_acknowl(from_client_id);
                broadcast_from(from_client_id, data, len);
                break;
            default:
                broadcast_from(from_client_id, data, len);
                return;
        }
    }

    void Server::broadcast_from(int from_client_id, const uint8_t* data, size_t len)
    {
        std::lock_guard<std::mutex> lock(peers_mutex);

        for (auto& [clientId, dc] : dcs)
        {
            if (!dc || !dc->isOpen()) continue;

            // std::cout << "[server] relaying " << len << " bytes from " << from_client_id
            // << " to " << (dcs.size() - 1) << " peers\n";

            dc->send(reinterpret_cast<const std::byte*>(data), len);
        }
    }

    void Server::run()
    {
        try {
            io_context.run();
        } catch (const std::exception& e) {
            std::cerr << "Server exception: " << e.what() << "\n";
        }
    }

    void Server::deinit()
    {
        is_running = false;

        {
            std::lock_guard<std::mutex> lock(peers_mutex);
            dcs.clear();
            pcs.clear();
            client_acknowl_counters_id.clear();
            client_expected_acknowl_num_id.clear();
            client_last_packet_id.clear();
        }

        try {
            if (sig_ws)
                sig_ws->close();
        }
        catch (const std::exception& e)
        {
            std::cout << "[client] signaling close failed: "
                    << e.what() << std::endl;
        }
        sig_ws.reset();

        
        if (server_thread.joinable()) server_thread.join();
    }

}