#include "NetworkManager.hpp"
#include <chrono>
#include <iostream>

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#elif __has_include("json.hpp")
#include "json.hpp"
using json = nlohmann::json;
#endif

#if __has_include(<ixwebsocket/IXNetSystem.h>)
#include <ixwebsocket/IXNetSystem.h>
#define HAS_IXWEBSOCKET 1
#elif __has_include("IXNetSystem.h")
#include "IXNetSystem.h"
#define HAS_IXWEBSOCKET 1
#endif

static uint64_t GetCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

NetworkManager::NetworkManager() {
#if defined(HAS_IXWEBSOCKET)
    ix::initNetSystem();
    webSocket = std::make_unique<ix::WebSocket>();
#endif
}

NetworkManager::~NetworkManager() {
    disconnect();
#if defined(HAS_IXWEBSOCKET)
    ix::uninitNetSystem();
#endif
}

void NetworkManager::connect(const std::string& url) {
    disconnect();

#if defined(HAS_IXWEBSOCKET)
    if (!webSocket) {
        webSocket = std::make_unique<ix::WebSocket>();
    }

    // Configure TLS options for secure WebSockets (wss://) on Render
    if (url.rfind("wss://", 0) == 0) {
        ix::SocketTLSOptions tlsOptions;
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
        // Android NDK doesn't have standard /etc/ssl CA bundle; disable local file check
        tlsOptions.caFile = "NONE";
#else
        // Desktop Linux / Windows uses system CA bundle
        tlsOptions.caFile = "SYSTEM";
#endif
        webSocket->setTLSOptions(tlsOptions);
    }

    webSocket->setUrl(url);
    webSocket->enableAutomaticReconnection();
    webSocket->setMinWaitBetweenReconnectionRetries(1000);
    webSocket->setMaxWaitBetweenReconnectionRetries(4000);

    webSocket->setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            std::lock_guard<std::mutex> lock(queueMutex);
            incomingQueue.push_back("{\"__event__\":\"open\"}");
        } else if (msg->type == ix::WebSocketMessageType::Close) {
            std::lock_guard<std::mutex> lock(queueMutex);
            incomingQueue.push_back("{\"__event__\":\"close\"}");
        } else if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(queueMutex);
            incomingQueue.push_back(msg->str);
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            std::lock_guard<std::mutex> lock(queueMutex);
            incomingQueue.push_back("{\"__event__\":\"error\",\"message\":\"" + msg->errorInfo.reason + "\"}");
        }
    });

    webSocket->start();
#else
    std::cout << "[NetworkManager] Connecting to: " << url << std::endl;
    connected = true;
#endif
}

void NetworkManager::disconnect() {
#if defined(HAS_IXWEBSOCKET)
    if (webSocket) {
        webSocket->stop();
    }
#endif
    connected = false;
    roomCode.clear();
    localPlayerId.clear();
    std::lock_guard<std::mutex> lock(queueMutex);
    incomingQueue.clear();
}

void NetworkManager::pollMessages() {
    std::deque<std::string> localQueue;
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        localQueue.swap(incomingQueue);
    }

    for (const auto& raw : localQueue) {
        handleRawMessage(raw);
    }
}

void NetworkManager::handleRawMessage(const std::string& raw) {
#if defined(NLOHMANN_JSON_VERSION_MAJOR) || defined(INCLUDE_NLOHMANN_JSON_HPP_)
    try {
        json j = json::parse(raw);

        // Internal lifecycle events
        if (j.contains("__event__")) {
            std::string ev = j["__event__"].get<std::string>();
            if (ev == "open") {
                connected = true;
                std::cout << "[WS] Connected successfully!" << std::endl;
            } else if (ev == "close") {
                connected = false;
                std::cout << "[WS] Disconnected from server." << std::endl;
                if (onDisconnectCb) onDisconnectCb();
            } else if (ev == "error") {
                connected = false;
                std::string err = j.value("message", "Network error");
                std::cout << "[WS Error]: " << err << std::endl;
                if (onErrorCb) onErrorCb(err);
            }
            return;
        }

        // Server protocol messages: { type: string, payload: object }
        if (!j.contains("type")) return;
        std::string type = j["type"].get<std::string>();
        const auto& payload = j.contains("payload") ? j["payload"] : json::object();

        if (type == "welcome") {
            localPlayerId = payload.value("clientId", "");
            if (onWelcomeCb) onWelcomeCb(localPlayerId);
        } else if (type == "room_created") {
            roomCode = payload.value("roomCode", "");
            localPlayerId = payload.value("playerId", "");
            hostStatus = payload.value("isHost", true);
            if (onRoomCreatedCb) onRoomCreatedCb(roomCode, localPlayerId, hostStatus);
        } else if (type == "room_joined") {
            roomCode = payload.value("roomCode", "");
            localPlayerId = payload.value("playerId", "");
            hostStatus = payload.value("isHost", false);
            if (onRoomJoinedCb) onRoomJoinedCb(roomCode, localPlayerId, hostStatus);
        } else if (type == "game_state") {
            if (onGameStateCb) onGameStateCb(&payload);
        } else if (type == "kill_feed") {
            std::string killer = payload.value("killerName", "Soldier");
            std::string victim = payload.value("victimName", "Enemy");
            std::string weapon = payload.value("weapon", "RIFLE");
            if (onKillFeedCb) onKillFeedCb(killer, victim, weapon);
        } else if (type == "hit_impact") {
            float x = payload.value("x", 0.0f);
            float y = payload.value("y", 0.0f);
            int damage = payload.value("damage", 10);
            bool headshot = payload.value("headshot", false);
            if (onHitImpactCb) onHitImpactCb(x, y, damage, headshot);
        } else if (type == "explosion") {
            float x = payload.value("x", 0.0f);
            float y = payload.value("y", 0.0f);
            float radius = payload.value("radius", 130.0f);
            if (onExplosionCb) onExplosionCb(x, y, radius);
        } else if (type == "pong") {
            uint64_t sent = payload.value("clientTime", 0ULL);
            if (sent > 0) {
                pingMs = static_cast<int>(GetCurrentTimeMs() - sent);
            }
        } else if (type == "error") {
            std::string msg = payload.value("message", "Unknown error");
            if (onErrorCb) onErrorCb(msg);
        }
    } catch (const std::exception& e) {
        std::cerr << "[JSON parse error]: " << e.what() << std::endl;
    }
#endif
}

void NetworkManager::sendJson(const std::string& type, const void* payloadJson) {
#if defined(NLOHMANN_JSON_VERSION_MAJOR) || defined(INCLUDE_NLOHMANN_JSON_HPP_)
    json packet;
    packet["type"] = type;
    if (payloadJson) {
        packet["payload"] = *reinterpret_cast<const json*>(payloadJson);
    } else {
        packet["payload"] = json::object();
    }
    std::string text = packet.dump();

#if defined(HAS_IXWEBSOCKET)
    if (webSocket) {
        webSocket->send(text);
    }
#endif
#endif
}

void NetworkManager::createRoom(const std::string& name, const std::string& avatarStyle, int botCount) {
#if defined(NLOHMANN_JSON_VERSION_MAJOR) || defined(INCLUDE_NLOHMANN_JSON_HPP_)
    json p;
    p["name"] = name;
    p["avatarStyle"] = avatarStyle;
    p["botCount"] = botCount;
    sendJson("create_room", &p);
#endif
}

void NetworkManager::joinRoom(const std::string& code, const std::string& name, const std::string& avatarStyle) {
#if defined(NLOHMANN_JSON_VERSION_MAJOR) || defined(INCLUDE_NLOHMANN_JSON_HPP_)
    json p;
    p["roomCode"] = code;
    p["name"] = name;
    p["avatarStyle"] = avatarStyle;
    sendJson("join_room", &p);
#endif
}

void NetworkManager::quickPlay(const std::string& name, const std::string& avatarStyle) {
#if defined(NLOHMANN_JSON_VERSION_MAJOR) || defined(INCLUDE_NLOHMANN_JSON_HPP_)
    json p;
    p["name"] = name;
    p["avatarStyle"] = avatarStyle;
    sendJson("quick_play", &p);
#endif
}

void NetworkManager::sendInput(bool left, bool right, bool up, bool jump, float aimAngle) {
#if defined(NLOHMANN_JSON_VERSION_MAJOR) || defined(INCLUDE_NLOHMANN_JSON_HPP_)
    json p;
    p["left"] = left;
    p["right"] = right;
    p["up"] = up;
    p["jump"] = jump;
    p["aimAngle"] = aimAngle;
    sendJson("input", &p);
#endif
}

void NetworkManager::sendShoot(float aimAngle) {
#if defined(NLOHMANN_JSON_VERSION_MAJOR) || defined(INCLUDE_NLOHMANN_JSON_HPP_)
    json p;
    p["aimAngle"] = aimAngle;
    sendJson("shoot", &p);
#endif
}

void NetworkManager::sendGrenade(float aimAngle) {
#if defined(NLOHMANN_JSON_VERSION_MAJOR) || defined(INCLUDE_NLOHMANN_JSON_HPP_)
    json p;
    p["aimAngle"] = aimAngle;
    sendJson("grenade", &p);
#endif
}

void NetworkManager::sendReload() {
    sendJson("reload", nullptr);
}

void NetworkManager::sendAddBot(const std::string& name) {
#if defined(NLOHMANN_JSON_VERSION_MAJOR) || defined(INCLUDE_NLOHMANN_JSON_HPP_)
    json p;
    p["name"] = name;
    sendJson("add_bot", &p);
#endif
}

void NetworkManager::sendRemoveBot() {
    sendJson("remove_bot", nullptr);
}

void NetworkManager::sendPing() {
#if defined(NLOHMANN_JSON_VERSION_MAJOR) || defined(INCLUDE_NLOHMANN_JSON_HPP_)
    lastPingSent = GetCurrentTimeMs();
    json p;
    p["clientTime"] = lastPingSent;
    sendJson("ping", &p);
#endif
}

void NetworkManager::leaveRoom() {
    sendJson("leave_room", nullptr);
    roomCode.clear();
}
