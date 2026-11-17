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
    ix::initNetSystem();

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
            LifecycleEvent ev;
            ev.type = "open";
            std::lock_guard<std::mutex> lock(eventMutex);
            pendingLifecycle.push_back(std::move(ev));
        } else if (msg->type == ix::WebSocketMessageType::Close) {
            LifecycleEvent ev;
            ev.type = "close";
            std::lock_guard<std::mutex> lock(eventMutex);
            pendingLifecycle.push_back(std::move(ev));
        } else if (msg->type == ix::WebSocketMessageType::Message) {
            // Processed directly on background network worker thread!
            handleRawMessage(msg->str);
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            LifecycleEvent ev;
            ev.type = "error";
            ev.message = msg->errorInfo.reason;
            std::lock_guard<std::mutex> lock(eventMutex);
            pendingLifecycle.push_back(std::move(ev));
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
    {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        hasNewSnapshot = false;
    }
    {
        std::lock_guard<std::mutex> lock(eventMutex);
        pendingKills.clear();
        pendingHits.clear();
        pendingExplosions.clear();
        pendingLifecycle.clear();
    }
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        pendingOutgoingQueue.clear();
    }
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

bool NetworkManager::getLatestSnapshot(GameSnapshot& out) {
    std::lock_guard<std::mutex> lock(snapshotMutex);
    if (!hasNewSnapshot) return false;
    out = std::move(latestSnapshot);
    hasNewSnapshot = false;
    return true;
}

void NetworkManager::pollMessages() {
    // 1. Process latest snapshot (zero JSON parsing on render thread)
    bool hadSnap = false;
    GameSnapshot snap;
    {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        if (hasNewSnapshot) {
            snap = std::move(latestSnapshot);
            hasNewSnapshot = false;
            hadSnap = true;
        }
    }
    if (hadSnap && onGameSnapshotCb) {
        onGameSnapshotCb(snap);
    }

    // 2. Process events
    std::vector<KillEvent> kills;
    std::vector<HitEvent> hits;
    std::vector<ExplosionEvent> explosions;
    std::vector<LifecycleEvent> lifecycle;
    {
        std::lock_guard<std::mutex> lock(eventMutex);
        kills.swap(pendingKills);
        hits.swap(pendingHits);
        explosions.swap(pendingExplosions);
        lifecycle.swap(pendingLifecycle);
    }

    for (const auto& ev : lifecycle) {
        if (ev.type == "open") {
            connected = true;
            std::cout << "[WS] Connected successfully to server!" << std::endl;
            flushPendingMessages();
        } else if (ev.type == "close") {
            connected = false;
            std::cout << "[WS] Disconnected from server." << std::endl;
            if (onDisconnectCb) onDisconnectCb();
        } else if (ev.type == "error") {
            connected = false;
            std::cout << "[WS Error]: " << ev.message << std::endl;
            if (onErrorCb) onErrorCb(ev.message);
        } else if (ev.type == "welcome") {
            if (onWelcomeCb) onWelcomeCb(ev.playerId);
        } else if (ev.type == "room_created") {
            if (onRoomCreatedCb) onRoomCreatedCb(ev.code, ev.playerId, ev.isHost);
        } else if (ev.type == "room_joined") {
            if (onRoomJoinedCb) onRoomJoinedCb(ev.code, ev.playerId, ev.isHost);
        }
    }

    for (const auto& k : kills) {
        if (onKillFeedCb) onKillFeedCb(k.killer, k.victim, k.weapon);
    }
    for (const auto& h : hits) {
        if (onHitImpactCb) onHitImpactCb(h.x, h.y, h.damage, h.isHeadshot);
    }
    for (const auto& exp : explosions) {
        if (onExplosionCb) onExplosionCb(exp.x, exp.y, exp.radius);
    }
}

void NetworkManager::handleRawMessage(const std::string& raw) {
#if defined(NLOHMANN_JSON_VERSION_MAJOR) || defined(INCLUDE_NLOHMANN_JSON_HPP_)
    try {
        json j = json::parse(raw);

        // Internal lifecycle events
        if (j.contains("__event__")) {
            std::string ev = j["__event__"].get<std::string>();
            LifecycleEvent lev;
            lev.type = ev;
            if (ev == "error") lev.message = j.value("message", "Network error");
            std::lock_guard<std::mutex> lock(eventMutex);
            pendingLifecycle.push_back(std::move(lev));
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

            LifecycleEvent ev;
            ev.type = "welcome";
            ev.playerId = localPlayerId;
            std::lock_guard<std::mutex> lock(eventMutex);
            pendingLifecycle.push_back(std::move(ev));
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

            LifecycleEvent ev;
            ev.type = "room_created";
            ev.code = roomCode;
            ev.playerId = localPlayerId;
            ev.isHost = hostStatus;
            std::lock_guard<std::mutex> lock(eventMutex);
            pendingLifecycle.push_back(std::move(ev));
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

            LifecycleEvent ev;
            ev.type = "room_joined";
            ev.code = roomCode;
            ev.playerId = localPlayerId;
            ev.isHost = hostStatus;
            std::lock_guard<std::mutex> lock(eventMutex);
            pendingLifecycle.push_back(std::move(ev));
        }
        // 4. Live 30Hz Game State Snapshots parsed completely on background network thread
        else if (type == "game_state" || type == "GAME_STATE" || type == "state_update" || type == "STATE_UPDATE") {
            GameSnapshot snap;
            snap.matchDuration = payload.value("matchDuration", 0);

            if (payload.contains("players") && payload["players"].is_object()) {
                const auto& playersObj = payload["players"];
                snap.players.reserve(playersObj.size());
                for (auto it = playersObj.begin(); it != playersObj.end(); ++it) {
                    NetPlayerSnapshot np;
                    np.id = it.key();
                    const auto& pData = it.value();
                    np.name = pData.value("name", "Soldier");
                    np.avatarStyle = pData.value("avatarStyle", "soldier");
                    np.isBot = pData.value("isBot", false);
                    np.x = pData.value("x", 1200.0f);
                    np.y = pData.value("y", 800.0f);
                    np.vx = pData.value("vx", 0.0f);
                    np.vy = pData.value("vy", 0.0f);
                    np.aimAngle = pData.value("aimAngle", 0.0f);
                    np.facing = pData.value("facing", 1);
                    np.health = pData.value("health", 100);
                    np.fuel = pData.value("fuel", 100);
                    np.weapon = pData.value("weapon", "RIFLE");
                    if (np.weapon == "SMG") np.maxAmmo = 36;
                    else if (np.weapon == "SHOTGUN") np.maxAmmo = 6;
                    else if (np.weapon == "SNIPER") np.maxAmmo = 5;
                    else if (np.weapon == "PISTOL") np.maxAmmo = 12;
                    else if (np.weapon == "LASER") np.maxAmmo = 14;
                    else if (np.weapon == "ROCKET") np.maxAmmo = 4;
                    else np.maxAmmo = 30;
                    np.ammo = pData.value("ammo", 30);
                    np.grenades = pData.value("grenades", 2);
                    np.isDead = pData.value("isDead", false);
                    np.isFlying = pData.value("isFlying", false);
                    np.isOnGround = pData.value("isOnGround", false);
                    np.reloading = pData.value("reloading", false);
                    np.kills = pData.value("kills", 0);
                    np.deaths = pData.value("deaths", 0);
                    np.score = pData.value("score", 0);
                    np.ping = pData.value("ping", 0);
                    snap.players.push_back(std::move(np));
                }
            }

            if (payload.contains("bullets") && payload["bullets"].is_array()) {
                const auto& arr = payload["bullets"];
                snap.bullets.reserve(arr.size());
                for (const auto& bJson : arr) {
                    BulletData b;
                    b.id = bJson.value("id", "");
                    b.x = bJson.value("x", 0.0f);
                    b.y = bJson.value("y", 0.0f);
                    b.vx = bJson.value("vx", 0.0f);
                    b.vy = bJson.value("vy", 0.0f);
                    b.color = bJson.value("color", "#fb923c");
                    b.length = bJson.value("length", 22.0f);
                    b.weaponType = bJson.value("weaponType", "RIFLE");
                    snap.bullets.push_back(std::move(b));
                }
            }

            if (payload.contains("grenades") && payload["grenades"].is_array()) {
                const auto& arr = payload["grenades"];
                snap.grenades.reserve(arr.size());
                for (const auto& gJson : arr) {
                    GrenadeData g;
                    g.id = gJson.value("id", "");
                    g.x = gJson.value("x", 0.0f);
                    g.y = gJson.value("y", 0.0f);
                    g.fuse = gJson.value("fuse", 2500.0f);
                    snap.grenades.push_back(std::move(g));
                }
            }

            if (payload.contains("pickups") && payload["pickups"].is_array()) {
                const auto& arr = payload["pickups"];
                snap.pickups.reserve(arr.size());
                for (const auto& pJson : arr) {
                    PickupData p;
                    p.id = pJson.value("id", "");
                    p.type = pJson.value("type", "HEALTH");
                    p.x = pJson.value("x", 0.0f);
                    p.y = pJson.value("y", 0.0f);
                    p.active = pJson.value("active", true);
                    snap.pickups.push_back(std::move(p));
                }
            }

            {
                std::lock_guard<std::mutex> lock(snapshotMutex);
                latestSnapshot = std::move(snap);
                hasNewSnapshot = true;
            }
        }
        // 5. Kill feed notifications
        else if (type == "kill_feed" || type == "KILL_FEED") {
            KillEvent k;
            k.killer = payload.value("killerName", "Soldier");
            k.victim = payload.value("victimName", "Enemy");
            k.weapon = payload.value("weapon", "RIFLE");
            std::lock_guard<std::mutex> lock(eventMutex);
            pendingKills.push_back(std::move(k));
        }
        // 6. Combat hit sparks & damage numbers
        else if (type == "hit_impact" || type == "HIT_IMPACT") {
            HitEvent h;
            h.x = payload.value("x", 0.0f);
            h.y = payload.value("y", 0.0f);
            h.damage = payload.value("damage", 10);
            h.isHeadshot = payload.value("headshot", false);
            std::lock_guard<std::mutex> lock(eventMutex);
            pendingHits.push_back(std::move(h));
        }
        // 7. Grenade explosions
        else if (type == "explosion" || type == "EXPLOSION") {
            ExplosionEvent exp;
            exp.x = payload.value("x", 0.0f);
            exp.y = payload.value("y", 0.0f);
            exp.radius = payload.value("radius", 130.0f);
            std::lock_guard<std::mutex> lock(eventMutex);
            pendingExplosions.push_back(std::move(exp));
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
            LifecycleEvent ev;
            ev.type = "error";
            ev.message = payload.value("message", "Unknown error");
            std::lock_guard<std::mutex> lock(eventMutex);
            pendingLifecycle.push_back(std::move(ev));
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
