#include <rtc/rtc.hpp>
#include <iostream>
#include <unordered_map>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include "network.h"
#include <mutex>

using rtc::WebSocket;
using rtc::WebSocketServer;

namespace Network
{
    bool Signaling::init()
    {
        if (is_running){
            return true;
        }

        bind_ip = "0.0.0.0";
        bind_port = 8000;

        WebSocketServer::Configuration cfg;
        cfg.port = bind_port;
        cfg.bindAddress = bind_ip;
        cfg.enableTls = false;
        cfg.maxMessageSize = 1024 * 1024;

        ws_server = std::make_unique<WebSocketServer>(cfg);

        appServer.reset();
        clients.clear();
        nextId = 1;

        ws_server->onClient([this](std::shared_ptr<WebSocket> ws) {
            std::cout << "[signaling] connected\n";

            std::shared_ptr<ConnState> connectionState = std::make_shared<ConnState>();

            // called when this websocket receives a message
            ws->onMessage([this, ws, connectionState](rtc::message_variant msg) {
                //check if msg is a string
                if (!std::holds_alternative<std::string>(msg)) {
                    return;
                }
                //extract the msg
                std::string str = std::get<std::string>(msg);

                std::lock_guard<std::mutex> lock(signaling_mutex);
                // set Roles for client and server
                if (!connectionState->roleSet) {
                    connectionState->roleSet = true;

                    if (str == "ROLE SERVER") {
                        if (appServer) {
                            ws->send("ERR app server already registered");
                            return;
                        }
                        connectionState->isServer = true;
                        connectionState->myId = 0;
                        appServer = ws;
                        ws->send("ID 0");
                        std::cout << "[signaling] registered APP SERVER\n";
                        return;
                    }

                    if (str == "ROLE CLIENT") {
                        connectionState->isServer = false;
                        connectionState->myId = nextId++;
                        clients[connectionState->myId] = ws;
                        ws->send("ID " + std::to_string(connectionState->myId));
                        std::cout << "[signaling] registered CLIENT id=" << connectionState->myId << "\n";
                        return;
                    }

                    ws->send("ERROR. expected ROLE");
                    return;
                }

                // ROUTING
                //client
                if (!connectionState->isServer) {
                    if (!appServer) {
                        ws->send("ERR no app server connected");
                        return;
                    }

                    if (str.rfind("OFFER\n", 0) == 0) {
                        appServer->send("OFFER " + std::to_string(connectionState->myId) + "\n" + str.substr(6));
                    }
                    else if (str.rfind("CAND\n", 0) == 0) {
                         appServer->send("CAND " + std::to_string(connectionState->myId) + "\n" + str.substr(5));
                    }
                }
                //server
                else {
                    if (str.rfind("ANSWER ", 0) == 0) {
                        std::string::size_type pos = str.find('\n');
                        if (pos == std::string::npos) return;
                        int id = std::stoi(str.substr(7, pos - 7));
                        auto it = clients.find(id);
                        if (it != clients.end()) {
                            it->second->send("ANSWER\n" + str.substr(pos + 1));
                        }
                    }
                    else if (str.rfind("CAND ", 0) == 0) {
                        std::string::size_type pos = str.find('\n');
                        if (pos == std::string::npos) return;
                        int id = std::stoi(str.substr(5, pos - 5));
                        auto it = clients.find(id);
                        if (it != clients.end()) {
                            it->second->send("CAND\n" + str.substr(pos + 1));
                        }
                    }
                }
            });
            // called when connection closes
            ws->onClosed([this, connectionState]() {
                std::lock_guard<std::mutex> lock(signaling_mutex);

                std::cout << "[signaling] closed\n";

                if (connectionState->roleSet && !connectionState->isServer && connectionState->myId > 0) {
                    clients.erase(connectionState->myId);
                }
                if (connectionState->roleSet && connectionState->isServer) {
                    appServer.reset();
                }
            });

            ws->onError([](std::string err) {
                std::cout << "[signaling] error: " << err << "\n";
            });
        });

        is_running = true;
        signaling_thread = std::thread(&Signaling::run, this);

        return true;
    }

    void Signaling::run()
    {
        while (is_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    void Signaling::deinit()
    {
        if (!is_running)
            return;

        is_running = false;

        if (signaling_thread.joinable()) {
            signaling_thread.join();
        }

        {
            std::lock_guard<std::mutex> lock(signaling_mutex);
            clients.clear();
            appServer.reset();
        }

        ws_server.reset();

        std::cout << "[signaling] stopped\n";
    }
}