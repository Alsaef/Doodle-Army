#pragma once

#include "raylib.h"
#include "GameWorld.hpp"
#include "TouchControls.hpp"
#include <string>
#include <unordered_map>
#include <vector>

struct HUDClickResult {
    bool leaveClicked = false;
    bool muteClicked = false;
    bool fullscreenClicked = false;
    bool addBotClicked = false;
    bool removeBotClicked = false;
    bool copyCodeClicked = false;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    void init();
    void unload();

    // Main render pipeline
    void renderWorld(const GameWorld& world, Camera2D camera, const std::string& localPlayerId);
    HUDClickResult renderHUD(const GameWorld& world, const std::string& localPlayerId,
                            const std::string& roomCode, int pingMs, bool isMuted,
                            int screenWidth, int screenHeight);

    // Texture loading with automatic white-pixel transparency removal
    void loadAssetTextures(const std::string& assetsPath);
    Texture2D getWeaponTexture(const std::string& weaponType);

private:
    // Sub-render passes
    void drawBackground(const GameWorld& world, Camera2D camera);
    void drawShatteredWindow(float cx, float cy, float w, float h);
    void drawAntiGravityCore(const GameWorld& world);
    void drawPlatforms(const GameWorld& world);
    void drawPlatform(const PlatformData& plat);
    void drawPickups(const GameWorld& world);
    void drawPickup(const PickupData& pk);
    void drawProjectiles(const GameWorld& world);
    void drawExplosions(const GameWorld& world);
    void drawParticles(const GameWorld& world);
    void drawDamageTexts(const GameWorld& world);
    void drawSoldiers(const GameWorld& world, const std::string& localPlayerId);
    void drawSoldier(const PlayerData& player, bool isLocal);

    // Layered character body parts matching client_web
    void drawPlayerLegs(const PlayerData& p, int facing, float walkCycle);
    void drawShoe(const std::string& avatarStyle, float x, float y);
    void drawPlayerTorso(const PlayerData& p, int facing);
    void drawPlayerHead(const PlayerData& p, int facing);
    void drawAimingArmAndWeapon(const PlayerData& p, float aimAngle, int facing);
    void drawGunPointer(const PlayerData& p, Camera2D camera);
    void drawMuzzleFlash(Vector2 barrelTip, float aimAngle, const std::string& weaponType);

    // HUD sub-components
    void drawPlayerStatusCard(const PlayerData* localPlayer, int screenWidth, int screenHeight);
    void drawTopCenterInfo(int matchDuration, const std::string& roomCode, int screenWidth, bool& copyClicked);
    void drawTopRightControls(int pingMs, bool isMuted, int screenWidth, HUDClickResult& result);
    void drawKillFeed(const std::deque<KillFeedEntry>& killFeed, int screenWidth);

    // Textures cache
    std::unordered_map<std::string, Texture2D> textures;
    bool texturesLoaded = false;
    float copyFeedbackTimer = 0.0f;
};
