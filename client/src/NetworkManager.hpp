#pragma once

#include <string>
#include <functional>
#include <deque>
#include <vector>
#include <mutex>
#include <memory>
#include "GameWorld.hpp"

#if __has_include(<ixwebsocket/IXWebSocket.h>)
#include <ixwebsocket/IXWebSocket.h>
#define HAS_IXWEBSOCKET 1
#elif __has_include("IXWebSocket.h")
#include "IXWebSocket.h"
#define HAS_IXWEBSOCKET 1
#endif

class NetworkManager {
public:
    using MessageCallback = std::function<void(const std::string& type, const void* payloadJson)>;

    NetworkManager();
    ~NetworkManager();

    // Connection control
    void connect(const std::string& url);
    void disconnect();
    bool isConnected() const { return connected; }
    const std::string& getLocalPlayerId() const { return localPlayerId; }
    const std::string& getRoomCode() const { return roomCode; }
    const std::string& getCurrentUrl() const { return currentUrl; }
    bool isHost() const { return hostStatus; }
    int getPingMs() const { return pingMs; }

    // Call on main loop each frame to dispatch incoming events with zero allocations
    void pollMessages();
    bool getLatestSnapshot(GameSnapshot& out);

    // Outgoing packets (Matching client_web structure exactly)
    void createRoom(const std::string& name, const std::string& avatarStyle, int botCount = 0);
    void joinRoom(const std::string& roomCode, const std::string& name, const std::string& avatarStyle);
    void quickPlay(const std::string& name, const std::string& avatarStyle);
    void sendInput(bool left, bool right, bool up, bool jump, float aimAngle, bool reload = false);
    void sendShoot(float aimAngle);
    void sendGrenade(float aimAngle);
    void sendReload();
    void sendAddBot(const std::string& name = "BOT");
    void sendRemoveBot();
    void sendPing();
    void leaveRoom();

    // Callbacks registration
    void setOnWelcome(std::function<void(const std::string&)> cb) { onWelcomeCb = cb; }
    void setOnRoomCreated(std::function<void(const std::string&, const std::string&, bool)> cb) { onRoomCreatedCb = cb; }
    void setOnRoomJoined(std::function<void(const std::string&, const std::string&, bool)> cb) { onRoomJoinedCb = cb; }
    void setOnGameSnapshot(std::function<void(const GameSnapshot&)> cb) { onGameSnapshotCb = cb; }
    void setOnGameState(std::function<void(const void*)> cb) { onGameStateCb = cb; }
    void setOnKillFeed(std::function<void(const std::string&, const std::string&, const std::string&)> cb) { onKillFeedCb = cb; }
    void setOnHitImpact(std::function<void(float, float, int, bool)> cb) { onHitImpactCb = cb; }
    void setOnExplosion(std::function<void(float, float, float)> cb) { onExplosionCb = cb; }
    void setOnError(std::function<void(const std::string&)> cb) { onErrorCb = cb; }
    void setOnDisconnect(std::function<void()> cb) { onDisconnectCb = cb; }

private:
    void sendJson(const std::string& type, const void* payloadJson);
    void handleRawMessage(const std::string& message);
    void flushPendingMessages();

    enum PendingIntent {
        INTENT_NONE = 0,
        INTENT_CREATE_ROOM,
        INTENT_JOIN_ROOM,
        INTENT_QUICK_PLAY
    };

    struct PendingRoomRequest {
        PendingIntent intent = INTENT_NONE;
        std::string name = "Soldier";
        std::string avatarStyle = "soldier";
        std::string roomCode = "";
        int botCount = 0;
    };

    struct KillEvent {
        std::string killer;
        std::string victim;
        std::string weapon;
    };

    struct HitEvent {
        float x;
        float y;
        int damage;
        bool isHeadshot;
    };

    struct ExplosionEvent {
        float x;
        float y;
        float radius;
    };

    struct LifecycleEvent {
        std::string type;
        std::string code;
        std::string playerId;
        bool isHost;
        std::string message;
    };

    PendingRoomRequest pendingRequest;

    bool connected = false;
    std::string currentUrl;
    std::string localPlayerId;
    std::string roomCode;
    bool hostStatus = false;
    int pingMs = 0;
    uint64_t lastPingSent = 0;

    std::mutex snapshotMutex;
    GameSnapshot latestSnapshot;
    bool hasNewSnapshot = false;

    std::mutex eventMutex;
    std::vector<KillEvent> pendingKills;
    std::vector<HitEvent> pendingHits;
    std::vector<ExplosionEvent> pendingExplosions;
    std::vector<LifecycleEvent> pendingLifecycle;

    std::mutex queueMutex;
    std::vector<std::string> pendingOutgoingQueue;

#ifdef HAS_IXWEBSOCKET
    std::unique_ptr<ix::WebSocket> webSocket;
#else
    void* webSocketPtr = nullptr;
#endif

    // Registered callbacks
    std::function<void(const std::string&)> onWelcomeCb;
    std::function<void(const std::string&, const std::string&, bool)> onRoomCreatedCb;
    std::function<void(const std::string&, const std::string&, bool)> onRoomJoinedCb;
    std::function<void(const GameSnapshot&)> onGameSnapshotCb;
    std::function<void(const void*)> onGameStateCb;
    std::function<void(const std::string&, const std::string&, const std::string&)> onKillFeedCb;
    std::function<void(float, float, int, bool)> onHitImpactCb;
    std::function<void(float, float, float)> onExplosionCb;
    std::function<void(const std::string&)> onErrorCb;
    std::function<void()> onDisconnectCb;
};
