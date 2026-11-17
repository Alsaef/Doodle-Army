#pragma once

#include "raylib.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <deque>

enum ParticleType {
    PARTICLE_JETPACK = 0,
    PARTICLE_SPARK,
    PARTICLE_SMOKE,
    PARTICLE_EXPLOSION,
    PARTICLE_STAR,
    PARTICLE_CORE_PULSE
};

struct Particle {
    Vector2 pos;
    Vector2 vel;
    Color color;
    float size;
    float life;
    float maxLife;
    ParticleType type;
};

struct DamageText {
    Vector2 pos;
    float vy;
    std::string text;
    Color color;
    float life;
    float maxLife;
};

struct PlayerData {
    std::string id;
    std::string name = "Soldier";
    std::string avatarStyle = "soldier";
    bool isBot = false;

    // Server authoritative coordinates
    float x = 1200.0f;
    float y = 800.0f;
    float vx = 0.0f;
    float vy = 0.0f;

    // Client smooth rendered coordinates (interpolated)
    float renderX = 1200.0f;
    float renderY = 800.0f;
    bool initialized = false;
    float aimAngle = 0.0f;
    int facing = 1; // 1 = right, -1 = left

    int health = 100;
    int fuel = 100;
    std::string weapon = "RIFLE";
    int ammo = 30;
    int maxAmmo = 30;
    int grenades = 2;

    bool isDead = false;
    bool isFlying = false;
    bool isOnGround = false;
    bool reloading = false;

    int kills = 0;
    int deaths = 0;
    int score = 0;
    int ping = 0;

    // Animation & Visual Timers
    float walkCycle = 0.0f;
    float jetpackFlicker = 0.0f;
    float muzzleFlashTimer = 0.0f;
    float recoilOffset = 0.0f;
};

struct BulletData {
    std::string id = "";
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    std::string color = "#fb923c";
    float length = 22.0f;
    std::string weaponType = "RIFLE";
};

struct GrenadeData {
    std::string id = "";
    float x = 0.0f;
    float y = 0.0f;
    float fuse = 2500.0f;
};

struct PickupData {
    std::string id;
    std::string type;
    float x = 0.0f;
    float y = 0.0f;
    bool active = true;
    float floatOffset = 0.0f;
};

struct PlatformData {
    std::string id;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    std::string type; // 'ground', 'metal', 'crate', 'rock'
    float angle = 0.0f;
    std::string name;
};

struct KillFeedEntry {
    std::string killerName;
    std::string victimName;
    std::string weapon;
    float timeRemaining = 5.0f; // Seconds to display
};

struct ExplosionEffect {
    Vector2 pos;
    float radius;
    float maxRadius;
    float duration;
    float timer;
};

class GameWorld {
public:
    GameWorld();
    ~GameWorld() = default;

    void initDefaultMap();
    void reset();
    void update(float dt, const std::string& localPlayerId);

    // Snapshot update from server
    void updateFromSnapshot(const void* jsonDoc); // Takes nlohmann::json pointer
    void addKillFeed(const std::string& killer, const std::string& victim, const std::string& weapon);
    void addHitImpact(float x, float y, int damage, bool isHeadshot);
    void addExplosion(float x, float y, float radius);

    // Particle emissions
    void spawnJetpackParticles(Vector2 pos, int facing, float spread = 0.4f);
    void spawnImpactSparks(Vector2 pos, Color col, int count = 8);
    void spawnExplosionParticles(Vector2 pos, float radius);

    // Getters
    const std::unordered_map<std::string, PlayerData>& getPlayers() const { return players; }
    const std::vector<BulletData>& getBullets() const { return bullets; }
    const std::vector<GrenadeData>& getGrenades() const { return grenades; }
    const std::vector<PickupData>& getPickups() const { return pickups; }
    const std::vector<PlatformData>& getPlatforms() const { return platforms; }
    const std::deque<KillFeedEntry>& getKillFeed() const { return killFeed; }
    const std::vector<DamageText>& getDamageTexts() const { return damageTexts; }
    const std::vector<Particle>& getParticles() const { return particles; }
    const std::vector<ExplosionEffect>& getExplosions() const { return explosions; }

    PlayerData* getLocalPlayer(const std::string& localPlayerId);
    const PlayerData* getLocalPlayer(const std::string& localPlayerId) const;
    int getMatchDuration() const { return matchDuration; }
    float getMapWidth() const { return mapWidth; }
    float getMapHeight() const { return mapHeight; }
    Vector2 getCorePos() const { return corePos; }
    float getCoreRadius() const { return coreRadius; }

private:
    float mapWidth = 2400.0f;
    float mapHeight = 1400.0f;
    Vector2 corePos = {1200.0f, 650.0f};
    float coreRadius = 120.0f;
    float coreFieldRadius = 420.0f;
    float corePulseTime = 0.0f;

    int matchDuration = 0;

    std::unordered_map<std::string, PlayerData> players;
    std::vector<BulletData> bullets;
    std::vector<GrenadeData> grenades;
    std::vector<PickupData> pickups;
    std::vector<PlatformData> platforms;
    std::deque<KillFeedEntry> killFeed;
    std::vector<DamageText> damageTexts;
    std::vector<Particle> particles;
    std::vector<ExplosionEffect> explosions;

    // Twinkling stars in background
    struct Star {
        Vector2 pos;
        float brightness;
        float twinkleSpeed;
        float size;
    };
    std::vector<Star> backgroundStars;
};
