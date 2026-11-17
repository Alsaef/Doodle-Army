#include "GameWorld.hpp"
#include "raymath.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#elif __has_include("json.hpp")
#include "json.hpp"
using json = nlohmann::json;
#endif

// Helper to convert hex color string to raylib Color
static Color HexToColor(const std::string& hex) {
    if (hex.empty() || hex[0] != '#' || hex.length() < 7) {
        return ORANGE;
    }
    unsigned int r, g, b;
    if (sscanf(hex.c_str(), "#%02x%02x%02x", &r, &g, &b) == 3) {
        return Color{static_cast<unsigned char>(r),
                     static_cast<unsigned char>(g),
                     static_cast<unsigned char>(b), 255};
    }
    return ORANGE;
}

GameWorld::GameWorld() {
    initDefaultMap();

    // Generate random twinkling stars across space arena
    for (int i = 0; i < 180; ++i) {
        Star star;
        star.pos = {
            static_cast<float>(rand() % static_cast<int>(mapWidth)),
            static_cast<float>(rand() % static_cast<int>(mapHeight))
        };
        star.brightness = static_cast<float>(rand() % 100) / 100.0f;
        star.twinkleSpeed = 0.5f + static_cast<float>(rand() % 100) / 50.0f;
        star.size = (rand() % 10 < 3) ? 2.5f : 1.5f;
        backgroundStars.push_back(star);
    }
}

void GameWorld::initDefaultMap() {
    mapWidth = 2400.0f;
    mapHeight = 1400.0f;
    corePos = {1200.0f, 650.0f};
    coreRadius = 120.0f;
    coreFieldRadius = 420.0f;

    platforms.clear();

    // Boundaries
    platforms.push_back({"bound-bottom", 0, mapHeight - 60, mapWidth, 60, "ground", 0.0f, "Floor"});
    platforms.push_back({"bound-top", 0, 0, mapWidth, 50, "rock", 0.0f, "Ceiling"});
    platforms.push_back({"bound-left", 0, 0, 50, mapHeight, "rock", 0.0f, "Left Wall"});
    platforms.push_back({"bound-right", mapWidth - 50, 0, 50, mapHeight, "rock", 0.0f, "Right Wall"});

    // Central Sector
    platforms.push_back({"plat-center-main", 920, 880, 560, 36, "metal", 0.0f, "Reactor Deck"});
    platforms.push_back({"plat-center-top", 1020, 380, 360, 28, "metal", 0.0f, "Upper Catwalk"});

    // Left Sector
    platforms.push_back({"plat-left-low", 160, 980, 460, 32, "metal", 0.0f, "Left Low"});
    platforms.push_back({"plat-left-mid", 260, 720, 400, 28, "metal", 0.0f, "Left Mid"});
    platforms.push_back({"plat-left-high", 120, 440, 380, 28, "metal", 0.0f, "Left High"});
    platforms.push_back({"plat-left-bridge", 560, 560, 240, 24, "metal", 0.0f, "Left Bridge"});

    // Right Sector
    platforms.push_back({"plat-right-low", 1780, 980, 460, 32, "metal", 0.0f, "Right Low"});
    platforms.push_back({"plat-right-mid", 1740, 740, 420, 28, "metal", 0.0f, "Right Mid"});
    platforms.push_back({"plat-right-high", 1900, 440, 380, 28, "metal", 0.0f, "Right High"});
    platforms.push_back({"plat-right-bridge", 1600, 560, 240, 24, "metal", 0.0f, "Right Bridge"});

    // Floating zero-g wooden crates
    platforms.push_back({"crate-1", 780, 720, 64, 64, "crate", -0.15f, "Crate 1"});
    platforms.push_back({"crate-2", 1560, 700, 68, 68, "crate", 0.22f, "Crate 2"});
    platforms.push_back({"crate-3", 860, 460, 56, 56, "crate", 0.12f, "Crate 3"});
    platforms.push_back({"crate-4", 1480, 440, 60, 60, "crate", -0.18f, "Crate 4"});
    platforms.push_back({"crate-5", 1170, 220, 58, 58, "crate", 0.08f, "Crate 5"});
    platforms.push_back({"crate-6", 1680, 880, 64, 64, "crate", -0.05f, "Crate 6"});
    platforms.push_back({"crate-7", 650, 880, 64, 64, "crate", 0.09f, "Crate 7"});
}

void GameWorld::reset() {
    players.clear();
    bullets.clear();
    grenades.clear();
    particles.clear();
    explosions.clear();
    damageTexts.clear();
    killFeed.clear();
    matchDuration = 0;
}

PlayerData* GameWorld::getLocalPlayer(const std::string& localPlayerId) {
    auto it = players.find(localPlayerId);
    if (it != players.end()) {
        return &it->second;
    }
    return nullptr;
}

const PlayerData* GameWorld::getLocalPlayer(const std::string& localPlayerId) const {
    auto it = players.find(localPlayerId);
    if (it != players.end()) {
        return &it->second;
    }
    return nullptr;
}

#if defined(NLOHMANN_JSON_VERSION_MAJOR) || defined(INCLUDE_NLOHMANN_JSON_HPP_)
static float SafeFloat(const json& j, const std::string& key, float def = 0.0f) {
    if (!j.contains(key) || j[key].is_null()) return def;
    try {
        if (j[key].is_number()) return j[key].get<float>();
        if (j[key].is_string()) return std::stof(j[key].get<std::string>());
    } catch (...) {}
    return def;
}

static int SafeInt(const json& j, const std::string& key, int def = 0) {
    if (!j.contains(key) || j[key].is_null()) return def;
    try {
        if (j[key].is_number()) return j[key].get<int>();
        if (j[key].is_string()) return std::stoi(j[key].get<std::string>());
        if (j[key].is_boolean()) return j[key].get<bool>() ? 1 : 0;
    } catch (...) {}
    return def;
}

static std::string SafeString(const json& j, const std::string& key, const std::string& def = "") {
    if (!j.contains(key) || j[key].is_null()) return def;
    try {
        if (j[key].is_string()) return j[key].get<std::string>();
        if (j[key].is_number()) return std::to_string(j[key].get<int>());
    } catch (...) {}
    return def;
}

static bool SafeBool(const json& j, const std::string& key, bool def = false) {
    if (!j.contains(key) || j[key].is_null()) return def;
    try {
        if (j[key].is_boolean()) return j[key].get<bool>();
        if (j[key].is_number()) return j[key].get<int>() != 0;
        if (j[key].is_string()) {
            std::string s = j[key].get<std::string>();
            return (s == "true" || s == "1" || s == "TRUE");
        }
    } catch (...) {}
    return def;
}

void GameWorld::updateFromSnapshot(const void* jsonDoc) {
    if (!jsonDoc) return;
    const json& j = *reinterpret_cast<const json*>(jsonDoc);

    if (j.contains("matchDuration")) {
        matchDuration = SafeInt(j, "matchDuration", matchDuration);
    }

    // 1. Update Players
    if (j.contains("players") && j["players"].is_object()) {
        const auto& playersObj = j["players"];
        std::vector<std::string> incomingIds;

        for (auto it = playersObj.begin(); it != playersObj.end(); ++it) {
            std::string pId = it.key();
            incomingIds.push_back(pId);
            const auto& pData = it.value();

            PlayerData& p = players[pId];
            p.id = pId;
            p.name = SafeString(pData, "name", p.name);
            p.avatarStyle = SafeString(pData, "avatarStyle", p.avatarStyle);
            p.isBot = SafeBool(pData, "isBot", p.isBot);

            float sx = SafeFloat(pData, "x", p.x);
            float sy = SafeFloat(pData, "y", p.y);
            p.vx = SafeFloat(pData, "vx", p.vx);
            p.vy = SafeFloat(pData, "vy", p.vy);
            p.aimAngle = SafeFloat(pData, "aimAngle", p.aimAngle);
            p.facing = SafeInt(pData, "facing", p.facing);

            // First time spawn: snap directly to spawn coordinates
            if (!p.initialized) {
                p.renderX = sx;
                p.renderY = sy;
                p.initialized = true;
            }
            p.x = sx;
            p.y = sy;

            p.health = SafeInt(pData, "health", p.health);
            p.fuel = SafeInt(pData, "fuel", p.fuel);
            p.weapon = SafeString(pData, "weapon", p.weapon);

            // Calculate max ammo for weapon type
            if (p.weapon == "SMG") p.maxAmmo = 36;
            else if (p.weapon == "SHOTGUN") p.maxAmmo = 6;
            else if (p.weapon == "SNIPER") p.maxAmmo = 5;
            else if (p.weapon == "PISTOL") p.maxAmmo = 12;
            else if (p.weapon == "LASER") p.maxAmmo = 14;
            else if (p.weapon == "ROCKET") p.maxAmmo = 4;
            else p.maxAmmo = 30; // RIFLE

            int newAmmo = SafeInt(pData, "ammo", p.ammo);
            if (newAmmo < p.ammo) {
                p.muzzleFlashTimer = 0.08f;
                p.recoilOffset = 5.0f;
            }
            p.ammo = newAmmo;

            p.grenades = SafeInt(pData, "grenades", p.grenades);
            p.isDead = SafeBool(pData, "isDead", p.isDead);
            p.isFlying = SafeBool(pData, "isFlying", p.isFlying);
            p.isOnGround = SafeBool(pData, "isOnGround", p.isOnGround);
            p.reloading = SafeBool(pData, "reloading", p.reloading);
            p.kills = SafeInt(pData, "kills", p.kills);
            p.deaths = SafeInt(pData, "deaths", p.deaths);
            p.score = SafeInt(pData, "score", p.score);
            p.ping = SafeInt(pData, "ping", p.ping);
        }

        // Clean up disconnected players
        for (auto it = players.begin(); it != players.end();) {
            if (std::find(incomingIds.begin(), incomingIds.end(), it->first) == incomingIds.end()) {
                it = players.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 2. Update Bullets
    bullets.clear();
    if (j.contains("bullets") && j["bullets"].is_array()) {
        for (const auto& bJson : j["bullets"]) {
            BulletData b;
            b.id = SafeString(bJson, "id", "");
            b.x = SafeFloat(bJson, "x", 0.0f);
            b.y = SafeFloat(bJson, "y", 0.0f);
            b.vx = SafeFloat(bJson, "vx", 0.0f);
            b.vy = SafeFloat(bJson, "vy", 0.0f);
            b.color = SafeString(bJson, "color", "#fb923c");
            b.length = SafeFloat(bJson, "length", 22.0f);
            b.weaponType = SafeString(bJson, "weaponType", "RIFLE");
            bullets.push_back(b);
        }
    }

    // 3. Update Grenades
    grenades.clear();
    if (j.contains("grenades") && j["grenades"].is_array()) {
        for (const auto& gJson : j["grenades"]) {
            GrenadeData g;
            g.id = SafeString(gJson, "id", "");
            g.x = SafeFloat(gJson, "x", 0.0f);
            g.y = SafeFloat(gJson, "y", 0.0f);
            g.fuse = SafeFloat(gJson, "fuse", 2500.0f);
            grenades.push_back(g);
        }
    }

    // 4. Update Pickups
    pickups.clear();
    if (j.contains("pickups") && j["pickups"].is_array()) {
        for (const auto& pJson : j["pickups"]) {
            PickupData p;
            p.id = SafeString(pJson, "id", "");
            p.type = SafeString(pJson, "type", "HEALTH");
            p.x = SafeFloat(pJson, "x", 0.0f);
            p.y = SafeFloat(pJson, "y", 0.0f);
            p.active = SafeBool(pJson, "active", true);
            pickups.push_back(p);
        }
    }
}
#else
void GameWorld::updateFromSnapshot(const void* /*jsonDoc*/) {}
#endif

void GameWorld::addKillFeed(const std::string& killer, const std::string& victim, const std::string& weapon) {
    KillFeedEntry entry;
    entry.killerName = killer;
    entry.victimName = victim;
    entry.weapon = weapon;
    entry.timeRemaining = 4.5f;

    killFeed.push_front(entry);
    if (killFeed.size() > 6) {
        killFeed.pop_back();
    }
}

void GameWorld::addHitImpact(float x, float y, int damage, bool isHeadshot) {
    DamageText dt;
    dt.pos = {x + (rand() % 20 - 10), y - 20.0f};
    dt.vy = -55.0f;
    dt.text = (isHeadshot ? "CRIT -" : "-") + std::to_string(damage);
    dt.color = isHeadshot ? Color{239, 68, 68, 255} : Color{250, 204, 21, 255};
    dt.life = 0.9f;
    dt.maxLife = 0.9f;
    damageTexts.push_back(dt);

    spawnImpactSparks({x, y}, dt.color, 6);
}

void GameWorld::addExplosion(float x, float y, float radius) {
    ExplosionEffect exp;
    exp.pos = {x, y};
    exp.radius = 10.0f;
    exp.maxRadius = radius > 10.0f ? radius : 130.0f;
    exp.duration = 0.55f;
    exp.timer = 0.0f;
    explosions.push_back(exp);

    spawnExplosionParticles({x, y}, radius);
}

void GameWorld::spawnJetpackParticles(Vector2 pos, int facing, float spread) {
    for (int i = 0; i < 3; ++i) {
        Particle p;
        p.pos = pos;
        float angle = PI / 2.0f + (static_cast<float>(rand() % 100) / 100.0f - 0.5f) * spread;
        float speed = 120.0f + static_cast<float>(rand() % 140);
        p.vel = {cosf(angle) * speed, sinf(angle) * speed};

        // Orange, yellow, and white fiery particle gradient
        int choice = rand() % 3;
        if (choice == 0) p.color = Color{255, 255, 255, 255};
        else if (choice == 1) p.color = Color{251, 146, 60, 255};
        else p.color = Color{249, 115, 22, 255};

        p.size = 4.0f + static_cast<float>(rand() % 4);
        p.life = 0.22f + static_cast<float>(rand() % 10) / 50.0f;
        p.maxLife = p.life;
        p.type = PARTICLE_JETPACK;
        particles.push_back(p);
    }
}

void GameWorld::spawnImpactSparks(Vector2 pos, Color col, int count) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.pos = pos;
        float angle = static_cast<float>(rand() % 360) * DEG2RAD;
        float speed = 80.0f + static_cast<float>(rand() % 180);
        p.vel = {cosf(angle) * speed, sinf(angle) * speed};
        p.color = col;
        p.size = 2.5f + static_cast<float>(rand() % 3);
        p.life = 0.3f + static_cast<float>(rand() % 10) / 40.0f;
        p.maxLife = p.life;
        p.type = PARTICLE_SPARK;
        particles.push_back(p);
    }
}

void GameWorld::spawnExplosionParticles(Vector2 pos, float radius) {
    for (int i = 0; i < 40; ++i) {
        Particle p;
        p.pos = pos;
        float angle = static_cast<float>(rand() % 360) * DEG2RAD;
        float speed = 60.0f + static_cast<float>(rand() % 320);
        p.vel = {cosf(angle) * speed, sinf(angle) * speed};

        int c = rand() % 4;
        if (c == 0) p.color = Color{255, 255, 255, 255};
        else if (c == 1) p.color = Color{250, 204, 21, 255};
        else if (c == 2) p.color = Color{239, 68, 68, 255};
        else p.color = Color{100, 116, 139, 200}; // Smoke

        p.size = 6.0f + static_cast<float>(rand() % 12);
        p.life = 0.45f + static_cast<float>(rand() % 20) / 40.0f;
        p.maxLife = p.life;
        p.type = (c == 3) ? PARTICLE_SMOKE : PARTICLE_EXPLOSION;
        particles.push_back(p);
    }
}

void GameWorld::update(float dt, const std::string& localPlayerId) {
    corePulseTime += dt * 1.5f;

    // 1. Update background star brightness
    for (auto& star : backgroundStars) {
        star.brightness += sinf(corePulseTime * star.twinkleSpeed) * 0.005f;
        if (star.brightness < 0.2f) star.brightness = 0.2f;
        if (star.brightness > 1.0f) star.brightness = 1.0f;
    }

    // 2. Interpolate Players & Animate
    for (auto& pair : players) {
        PlayerData& p = pair.second;
        if (p.isDead) continue;

        // Smooth Lerp toward server position (30 Hz snapshot -> 60/120 Hz render)
        p.renderX = Lerp(p.renderX, p.x, 0.28f);
        p.renderY = Lerp(p.renderY, p.y, 0.28f);

        // Weapon recoil and muzzle flash decay
        p.muzzleFlashTimer = std::max(0.0f, p.muzzleFlashTimer - dt);
        p.recoilOffset = Lerp(p.recoilOffset, 0.0f, 0.25f);

        // Walk cycle animation
        if (fabsf(p.vx) > 10.0f && p.isOnGround) {
            p.walkCycle += dt * 12.0f;
        } else {
            p.walkCycle = 0.0f;
        }

        // Jetpack particles when flying
        if (p.isFlying) {
            Vector2 jetpackNozzle = {
                p.renderX - static_cast<float>(p.facing) * 14.0f,
                p.renderY + 6.0f
            };
            spawnJetpackParticles(jetpackNozzle, p.facing);
        }
    }

    // 2b. Smooth Bullet & Grenade Simulation between network snapshots
    for (auto& b : bullets) {
        b.x += b.vx * dt;
        b.y += b.vy * dt;
    }
    for (auto& g : grenades) {
        g.fuse = std::max(0.0f, g.fuse - dt * 1000.0f);
    }

    // 3. Update Particles
    for (size_t i = 0; i < particles.size();) {
        Particle& p = particles[i];
        p.life -= dt;
        if (p.life <= 0.0f) {
            particles.erase(particles.begin() + i);
            continue;
        }

        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        // Friction on particles
        p.vel.x *= 0.94f;
        p.vel.y *= 0.94f;

        // Alpha fade out
        float alphaRatio = p.life / p.maxLife;
        p.color.a = static_cast<unsigned char>(255 * alphaRatio);

        if (p.type == PARTICLE_SMOKE) {
            p.size += dt * 8.0f; // Smoke expands
        }

        ++i;
    }

    // 4. Update Damage Texts
    for (size_t i = 0; i < damageTexts.size();) {
        DamageText& dtItem = damageTexts[i];
        dtItem.life -= dt;
        if (dtItem.life <= 0.0f) {
            damageTexts.erase(damageTexts.begin() + i);
            continue;
        }
        dtItem.pos.y += dtItem.vy * dt;
        dtItem.vy *= 0.92f;
        float alpha = dtItem.life / dtItem.maxLife;
        dtItem.color.a = static_cast<unsigned char>(255 * alpha);
        ++i;
    }

    // 5. Update Explosions
    for (size_t i = 0; i < explosions.size();) {
        ExplosionEffect& exp = explosions[i];
        exp.timer += dt;
        if (exp.timer >= exp.duration) {
            explosions.erase(explosions.begin() + i);
            continue;
        }
        float progress = exp.timer / exp.duration;
        exp.radius = Lerp(10.0f, exp.maxRadius, sqrtf(progress));
        ++i;
    }

    // 6. Update Kill Feed
    for (auto it = killFeed.begin(); it != killFeed.end();) {
        it->timeRemaining -= dt;
        if (it->timeRemaining <= 0.0f) {
            it = killFeed.erase(it);
        } else {
            ++it;
        }
    }

    // 7. Update Pickups floating bobbing effect
    for (auto& pk : pickups) {
        pk.floatOffset = sinf(corePulseTime * 2.5f + pk.x * 0.05f) * 5.0f;
    }
}
