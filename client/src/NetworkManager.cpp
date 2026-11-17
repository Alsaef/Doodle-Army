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
#if defined(HAS_IXWEBSOCKET)
    if (connected && currentUrl == url && webSocket && webSocket->getReadyState() == ix::ReadyState::Open) {
        return;
    }

    disconnect();
    currentUrl = url;

    if (!webSocket) {
        webSocket = std::make_unique<ix::WebSocket>();
    }

    // Configure TLS options for secure WebSockets (wss://) on Render
    if (url.rfind("wss://", 0) == 0) {
        ix::SocketTLSOptions tlsOptions;
        // Disable strict root CA verification so secure WebSockets connect cleanly
        // on both Linux systems without local ca-certificates bundles and Android NDK
        tlsOptions.caFile = "NONE";
        webSocket->setTLSOptions(tlsOptions);
    }

    webSocket->setUrl(url);
    webSocket->enableAutomaticReconnection();
    webSocket->setMinWaitBetweenReconnectionRetries(1000);
    webSocket->setMaxWaitBetweenReconnectionRetries(4000);

    webSocket->setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                connected = true;
                incomingQueue.push_back("{\"__event__\":\"open\"}");
            }
            flushPendingMessages();
        } else if (msg->type == ix::WebSocketMessageType::Close) {
            std::lock_guard<std::mutex> lock(queueMutex);
            connected = false;
            incomingQueue.push_back("{\"__event__\":\"close\"}");
        } else if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(queueMutex);
            incomingQueue.push_back(msg->str);
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            std::lock_guard<std::mutex> lock(queueMutex);
            connected = false;
            incomingQueue.push_back("{\"__event__\":\"error\",\"message\":\"" + msg->errorInfo.reason + "\"}");
        }
    });

    std::cout << "[NetworkManager] Starting connection to: " << url << std::endl;
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
    pendingRequest.intent = INTENT_NONE;
    std::lock_guard<std::mutex> lock(queueMutex);
    incomingQueue.clear();
    pendingOutgoingQueue.clear();
}

void NetworkManager::flushPendingMessages() {
#if defined(HAS_IXWEBSOCKET)
    std::lock_guard<std::mutex> lock(queueMutex);
    if (webSocket && webSocket->getReadyState() == ix::ReadyState::Open) {
        for (const auto& msg : pendingOutgoingQueue) {
            std::cout << "[WS Flush Queued]: " << msg.substr(0, 50) << std::endl;
            webSocket->send(msg);
        }
        pendingOutgoingQueue.clear();
    }
#endif
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
                std::cout << "[WS] Connected successfully to server!" << std::endl;
                flushPendingMessages();
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

        // 1. Welcome handshake
        if (type == "welcome" || type == "WELCOME") {
            localPlayerId = payload.value("clientId", "");
            flushPendingMessages();

            // Automatically execute pending room action if queued while connecting
            if (pendingRequest.intent == INTENT_CREATE_ROOM) {
                json p;
                p["name"] = pendingRequest.name;
                p["avatarStyle"] = pendingRequest.avatarStyle;
                p["botCount"] = pendingRequest.botCount;
                sendJson("create_room", &p);
            } else if (pendingRequest.intent == INTENT_JOIN_ROOM) {
                json p;
                p["roomCode"] = pendingRequest.roomCode;
                p["name"] = pendingRequest.name;
                p["avatarStyle"] = pendingRequest.avatarStyle;
                sendJson("join_room", &p);
            } else if (pendingRequest.intent == INTENT_QUICK_PLAY) {
                json p;
                p["name"] = pendingRequest.name;
                p["avatarStyle"] = pendingRequest.avatarStyle;
                sendJson("quick_play", &p);
            }

            if (onWelcomeCb) onWelcomeCb(localPlayerId);
        }
        // 2. Room created (immediate in-game transition)
        else if (type == "room_created" || type == "ROOM_CREATED") {
            pendingRequest.intent = INTENT_NONE; // Room successfully created!
            roomCode = payload.value("roomCode", payload.value("code", ""));
            localPlayerId = payload.value("playerId", payload.value("clientId", localPlayerId));
            if (localPlayerId.empty() && payload.contains("player") && payload["player"].contains("id")) {
                localPlayerId = payload["player"]["id"].get<std::string>();
            }
            hostStatus = payload.value("isHost", true);
            if (onRoomCreatedCb) onRoomCreatedCb(roomCode, localPlayerId, hostStatus);
        }
        // 3. Room joined or Game Start (immediate in-game transition)
        else if (type == "room_joined" || type == "ROOM_JOINED" || type == "game_start" || type == "GAME_START") {
            pendingRequest.intent = INTENT_NONE; // Room successfully joined!
            roomCode = payload.value("roomCode", payload.value("code", roomCode));
            localPlayerId = payload.value("playerId", payload.value("clientId", localPlayerId));
            if (localPlayerId.empty() && payload.contains("player") && payload["player"].contains("id")) {
                localPlayerId = payload["player"]["id"].get<std::string>();
            }
            hostStatus = payload.value("isHost", false);
            if (onRoomJoinedCb) onRoomJoinedCb(roomCode, localPlayerId, hostStatus);
        }
        // 4. Live 30Hz Game State Snapshots
        else if (type == "game_state" || type == "GAME_STATE" || type == "state_update" || type == "STATE_UPDATE") {
            if (onGameStateCb) onGameStateCb(&payload);
        }
        // 5. Kill feed notifications
        else if (type == "kill_feed" || type == "KILL_FEED") {
            std::string killer = payload.value("killerName", "Soldier");
            std::string victim = payload.value("victimName", "Enemy");
            std::string weapon = payload.value("weapon", "RIFLE");
            if (onKillFeedCb) onKillFeedCb(killer, victim, weapon);
        }
        // 6. Combat hit sparks & damage numbers
        else if (type == "hit_impact" || type == "HIT_IMPACT") {
            float x = payload.value("x", 0.0f);
            float y = payload.value("y", 0.0f);
            int damage = payload.value("damage", 10);
            bool headshot = payload.value("headshot", false);
            if (onHitImpactCb) onHitImpactCb(x, y, damage, headshot);
        }
        // 7. Grenade explosions
        else if (type == "explosion" || type == "EXPLOSION") {
            float x = payload.value("x", 0.0f);
            float y = payload.value("y", 0.0f);
            float radius = payload.value("radius", 130.0f);
            if (onExplosionCb) onExplosionCb(x, y, radius);
        }
        // 8. Heartbeat pong latency response
        else if (type == "pong" || type == "PONG") {
            uint64_t sent = payload.value("timestamp", payload.value("clientTime", 0ULL));
            if (sent > 0) {
                pingMs = static_cast<int>(GetCurrentTimeMs() - sent);
            }
        }
        // 9. Error response
        else if (type == "error" || type == "ERROR") {
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
    if (webSocket && webSocket->getReadyState() == ix::ReadyState::Open) {
        webSocket->send(text);
    } else {
        std::lock_guard<std::mutex> lock(queueMutex);
        pendingOutgoingQueue.push_back(text);
        std::cout << "[WS Queued]: " << type << " (waiting for socket open)" << std::endl;
    }
#endif
#endif
}

// -------------------------------------------------------------------------
// Outgoing Packets (Matches client_web JSON structure exactly)
// -------------------------------------------------------------------------

void NetworkManager::createRoom(const std::string& name, const std::string& avatarStyle, int botCount) {
    pendingRequest.intent = INTENT_CREATE_ROOM;
    pendingRequest.name = name;
    pendingRequest.avatarStyle = avatarStyle;
    pendingRequest.botCount = botCount;
#if defined(NLOHMANN_JSON_VERSION_MAJOR) || defined(INCLUDE_NLOHMANN_JSON_HPP_)
    json p;
    p["name"] = name;
    p["avatarStyle"] = avatarStyle;
    p["botCount"] = botCount;
    sendJson("create_room", &p);
#endif
}

void NetworkManager::joinRoom(const std::string& code, const std::string& name, const std::string& avatarStyle) {
    pendingRequest.intent = INTENT_JOIN_ROOM;
    pendingRequest.roomCode = code;
    pendingRequest.name = name;
    pendingRequest.avatarStyle = avatarStyle;
#if defined(NLOHMANN_JSON_VERSION_MAJOR) || defined(INCLUDE_NLOHMANN_JSON_HPP_)
    json p;
    p["roomCode"] = code;
    p["name"] = name;
    p["avatarStyle"] = avatarStyle;
    sendJson("join_room", &p);
#endif
}

void NetworkManager::quickPlay(const std::string& name, const std::string& avatarStyle) {
    pendingRequest.intent = INTENT_QUICK_PLAY;
    pendingRequest.name = name;
    pendingRequest.avatarStyle = avatarStyle;
#if defined(NLOHMANN_JSON_VERSION_MAJOR) || defined(INCLUDE_NLOHMANN_JSON_HPP_)
    json p;
    p["name"] = name;
    p["avatarStyle"] = avatarStyle;
    sendJson("quick_play", &p);
#endif
}

void NetworkManager::sendInput(bool left, bool right, bool up, bool jump, float aimAngle, bool reload) {
#if defined(NLOHMANN_JSON_VERSION_MAJOR) || defined(INCLUDE_NLOHMANN_JSON_HPP_)
    json p;
    p["left"] = left;
    p["right"] = right;
    p["up"] = up;
    p["jump"] = jump;
    p["reload"] = reload;
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
    json p;
    lastPingSent = GetCurrentTimeMs();
    p["timestamp"] = lastPingSent;
    sendJson("ping", &p);
#endif
}

void NetworkManager::leaveRoom() {
    pendingRequest.intent = INTENT_NONE;
    sendJson("leave", nullptr);
    roomCode.clear();
}
