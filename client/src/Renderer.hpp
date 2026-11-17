#pragma once

#include "raylib.h"
#include "GameWorld.hpp"
#include "TouchControls.hpp"
#include <string>
#include <unordered_map>

class Renderer {
public:
    Renderer();
    ~Renderer();

    void init();
    void unload();

    // Main render pipeline
    void renderWorld(const GameWorld& world, Camera2D camera, const std::string& localPlayerId);
    void renderHUD(const GameWorld& world, const std::string& localPlayerId, const std::string& roomCode, int pingMs, int screenWidth, int screenHeight);

    // Texture loading
    void loadAssetTextures(const std::string& assetsPath);

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

    // HUD sub-components
    void drawPlayerStatusCard(const PlayerData* localPlayer, int screenWidth, int screenHeight);
    void drawTopCenterInfo(int matchDuration, const std::string& roomCode, int screenWidth);
    void drawTopRightControls(int pingMs, int screenWidth);
    void drawKillFeed(const std::deque<KillFeedEntry>& killFeed, int screenWidth);

    // Textures cache
    std::unordered_map<std::string, Texture2D> textures;
    bool texturesLoaded = false;

    // Procedural weapon drawing
    void drawWeaponInHand(Vector2 handPos, float aimAngle, const std::string& weaponType, int facing, bool reloading);
};
