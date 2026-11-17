#include "Renderer.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

Renderer::Renderer() {}

Renderer::~Renderer() {
    unload();
}

void Renderer::init() {
    rlDisableBackfaceCulling();
}

void Renderer::unload() {
    for (auto& pair : textures) {
        UnloadTexture(pair.second);
    }
    textures.clear();
    texturesLoaded = false;
}

void Renderer::loadAssetTextures(const std::string& assetsPath) {
    if (texturesLoaded) return;

    struct TexEntry {
        std::string key;
        std::vector<std::string> paths;
    };

    std::vector<TexEntry> entries = {
        {"RIFLE", {"weapons/rifle.png", "weapon_rifle.png", "rifle.png"}},
        {"SMG", {"weapons/smg.png", "weapon_smg_clean.png", "weapon_smg.png", "smg.png"}},
        {"SHOTGUN", {"weapons/shotgun.png", "weapon_shotgun.png", "shotgun.png"}},
        {"SNIPER", {"weapons/sniper.png", "weapon_sniper.png", "sniper.png"}},
        {"PISTOL", {"weapons/pistol.png", "weapon_pistol.png", "pistol.png"}},
        {"LASER", {"weapons/laser.png", "weapon_laser.png", "laser.png"}},
        {"ROCKET", {"weapons/rocket.png", "weapon_rocket.png", "rocket.png"}},
        {"char_simon.png", {"characters/simon.png", "char_simon.png"}},
        {"char_raphina.png", {"characters/raphina.png", "char_raphina.png"}}
    };

    for (const auto& entry : entries) {
        bool loaded = false;
        for (const auto& rel : entry.paths) {
            std::string tryPaths[] = {
                rel,
                assetsPath + "/" + rel,
                "assets/" + rel,
                "client/assets/" + rel
            };

            for (const auto& p : tryPaths) {
                bool shouldTry = false;
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
                shouldTry = true;
#else
                shouldTry = FileExists(p.c_str());
#endif
                if (shouldTry) {
                    Image img = LoadImage(p.c_str());
                    if (img.data != nullptr) {
                        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
                        Color* pixels = (Color*)img.data;
                        if (pixels != nullptr) {
                            int total = img.width * img.height;
                            for (int px = 0; px < total; ++px) {
                                if (pixels[px].r > 240 && pixels[px].g > 240 && pixels[px].b > 240) {
                                    pixels[px] = BLANK;
                                }
                            }
                        }
                        Texture2D tex = LoadTextureFromImage(img);
                        UnloadImage(img);
                        if (tex.id > 0) {
                            SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
                            textures[entry.key] = tex;
                            textures[rel] = tex;
                            textures["weapon_" + entry.key + ".png"] = tex;
                            loaded = true;
                            break;
                        }
                    }
                }
            }
            if (loaded) break;
        }
    }

    texturesLoaded = true;
}

Texture2D Renderer::getWeaponTexture(const std::string& weaponType) {
    auto it = textures.find(weaponType);
    if (it != textures.end() && it->second.id > 0) return it->second;

    std::string alt1 = "weapon_" + weaponType + ".png";
    it = textures.find(alt1);
    if (it != textures.end() && it->second.id > 0) return it->second;

    std::string alt2 = "weapons/" + weaponType + ".png";
    it = textures.find(alt2);
    if (it != textures.end() && it->second.id > 0) return it->second;

    return Texture2D{0};
}

void Renderer::renderWorld(const GameWorld& world, Camera2D camera, const std::string& localPlayerId) {
    rlDisableBackfaceCulling();
    BeginMode2D(camera);

    // 1. Draw Space Backdrop & Shattered Window
    drawBackground(world, camera);

    // 2. Draw Anti-Gravity Core & Energy Vortex
    drawAntiGravityCore(world);

    // 3. Draw Solid Platforms & Floating Crates
    drawPlatforms(world);

    // 4. Draw Floating Pickups (Weapons, Health, Fuel, Ammo)
    drawPickups(world);

    // 5. Draw Projectiles (Bullets & Grenades)
    drawProjectiles(world);

    // 6. Draw Soldiers (Chibi soldiers with rotating weapon arms)
    drawSoldiers(world, localPlayerId);

    // 6b. Draw Tactical Gun Pointer & Crosshair for Local Player
    const PlayerData* localP = world.getLocalPlayer(localPlayerId);
    if (localP && !localP->isDead) {
        drawGunPointer(*localP, camera);
    }

    // 7. Draw Visual Particles (Flames, Sparks, Smoke)
    drawParticles(world);

    // 8. Draw Explosions
    drawExplosions(world);

    // 9. Draw In-World Floating Damage Numbers
    drawDamageTexts(world);

    EndMode2D();
}

void Renderer::drawBackground(const GameWorld& world, Camera2D camera) {
    float mw = world.getMapWidth();
    float mh = world.getMapHeight();

    // Deep space gradient backdrop
    DrawRectangleGradientV(0, 0, static_cast<int>(mw), static_cast<int>(mh),
                           Color{7, 10, 24, 255}, Color{15, 23, 42, 255});

    // Outer Cavern rock boundary shading
    DrawRectangle(0, 0, static_cast<int>(mw), 50, Color{20, 26, 38, 255}); // top ceiling
    DrawRectangle(0, static_cast<int>(mh - 60), static_cast<int>(mw), 60, Color{15, 20, 30, 255}); // floor
    DrawRectangle(0, 0, 50, static_cast<int>(mh), Color{20, 26, 38, 255}); // left rock
    DrawRectangle(static_cast<int>(mw - 50), 0, 50, static_cast<int>(mh), Color{20, 26, 38, 255}); // right rock

    // Shattered Space Window (Matches Reference Screenshot at x: 740, y: 520)
    drawShatteredWindow(740.0f, 520.0f, 1500.0f, 800.0f);
}

void Renderer::drawShatteredWindow(float cx, float cy, float w, float h) {
    // Glass window frame
    DrawRectangleLinesEx({cx - 300.0f, cy - 250.0f, 800.0f, 550.0f}, 4.0f, Color{30, 58, 138, 80});

    // Spiderweb glass crack lines radiating from impact center
    Color glassCrackColor = Color{147, 197, 253, 90};
    Color crackGlow = Color{191, 219, 254, 140};

    const int rayCount = 12;
    for (int i = 0; i < rayCount; ++i) {
        float angle = static_cast<float>(i) * (2.0f * PI / rayCount);
        float r1 = 60.0f + (i % 3) * 20.0f;
        float r2 = 280.0f + (i % 4) * 40.0f;

        Vector2 p1 = {cx + cosf(angle) * r1, cy + sinf(angle) * r1};
        Vector2 p2 = {cx + cosf(angle + 0.1f) * r2, cy + sinf(angle + 0.1f) * r2};

        DrawLineEx(p1, p2, 1.8f, glassCrackColor);

        if (i % 2 == 0) {
            Vector2 mid = Vector2Lerp(p1, p2, 0.5f);
            Vector2 branch = {mid.x + sinf(angle) * 45.0f, mid.y - cosf(angle) * 45.0f};
            DrawLineEx(mid, branch, 1.2f, glassCrackColor);
        }
    }

    // Concentric shattered rings
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 70.0f, glassCrackColor);
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 150.0f, Color{147, 197, 253, 50});
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 240.0f, Color{147, 197, 253, 30});
    DrawCircle(static_cast<int>(cx), static_cast<int>(cy), 8.0f, crackGlow);
}

void Renderer::drawAntiGravityCore(const GameWorld& world) {
    Vector2 corePos = world.getCorePos();
    float coreRadius = world.getCoreRadius();
    float time = static_cast<float>(GetTime());

    // Reactor Pylons (Top and bottom containment towers)
    DrawRectangleRounded({corePos.x - 70.0f, corePos.y - 200.0f, 140.0f, 70.0f}, 0.2f, 8, Color{15, 23, 42, 255});
    DrawRectangleRoundedLines({corePos.x - 70.0f, corePos.y - 200.0f, 140.0f, 70.0f}, 0.2f, 8, 3.0f, Color{56, 189, 248, 160});

    DrawRectangleRounded({corePos.x - 70.0f, corePos.y + 130.0f, 140.0f, 70.0f}, 0.2f, 8, Color{15, 23, 42, 255});
    DrawRectangleRoundedLines({corePos.x - 70.0f, corePos.y + 130.0f, 140.0f, 70.0f}, 0.2f, 8, 3.0f, Color{56, 189, 248, 160});

    // Glowing core vortex rings
    float pulse = sinf(time * 2.0f) * 15.0f;
    DrawCircleGradient(static_cast<int>(corePos.x), static_cast<int>(corePos.y),
                       coreRadius + 80.0f + pulse, Color{56, 189, 248, 40}, BLANK);
    DrawCircleGradient(static_cast<int>(corePos.x), static_cast<int>(corePos.y),
                       coreRadius + 20.0f, Color{125, 211, 252, 90}, Color{56, 189, 248, 10});

    // Intense glowing white center
    DrawCircle(static_cast<int>(corePos.x), static_cast<int>(corePos.y), coreRadius * 0.45f, WHITE);

    // Orbiting electric arcs
    rlPushMatrix();
    rlTranslatef(corePos.x, corePos.y, 0.0f);
    rlRotatef(time * 45.0f, 0, 0, 1);
    DrawCircleLines(0, 0, coreRadius * 0.85f, Color{186, 230, 253, 180});
    DrawCircleLines(0, 0, coreRadius * 1.15f, Color{14, 165, 233, 140});
    rlPopMatrix();
}

void Renderer::drawPlatforms(const GameWorld& world) {
    for (const auto& plat : world.getPlatforms()) {
        drawPlatform(plat);
    }
}

void Renderer::drawPlatform(const PlatformData& plat) {
    if (plat.type == "crate") {
        // Floating wooden crate in zero-g
        rlPushMatrix();
        rlTranslatef(plat.x + plat.w * 0.5f, plat.y + plat.h * 0.5f, 0.0f);
        rlRotatef(plat.angle * RAD2DEG, 0, 0, 1);

        Rectangle crateRect = {-plat.w * 0.5f, -plat.h * 0.5f, plat.w, plat.h};
        // Orange/brown wooden crate face
        DrawRectangleRec(crateRect, Color{194, 65, 12, 255});
        DrawRectangleLinesEx(crateRect, 3.0f, Color{124, 45, 18, 255});

        // Diagonal wooden support cross
        DrawLineEx({crateRect.x, crateRect.y},
                   {crateRect.x + crateRect.width, crateRect.y + crateRect.height},
                   3.5f, Color{124, 45, 18, 255});
        DrawLineEx({crateRect.x + crateRect.width, crateRect.y},
                   {crateRect.x, crateRect.y + crateRect.height},
                   3.5f, Color{124, 45, 18, 255});

        rlPopMatrix();
        return;
    }

    if (plat.type == "ground" || plat.type == "rock") {
        DrawRectangle(static_cast<int>(plat.x), static_cast<int>(plat.y),
                      static_cast<int>(plat.w), static_cast<int>(plat.h),
                      Color{15, 23, 42, 255});
        return;
    }

    // Space Station Sci-Fi Platform
    Rectangle rect = {plat.x, plat.y, plat.w, plat.h};

    // Platform main body
    DrawRectangleRec(rect, Color{30, 41, 59, 255});

    // Top metal walkway railing
    DrawRectangle(static_cast<int>(rect.x), static_cast<int>(rect.y), static_cast<int>(rect.width), 4, Color{71, 85, 105, 255});

    // Hazard Stripes on left & right ends
    float stripeW = 32.0f;
    Color hazardYellow = Color{234, 179, 8, 255};
    Color hazardDark = Color{15, 23, 42, 255};

    DrawRectangle(static_cast<int>(rect.x), static_cast<int>(rect.y), static_cast<int>(stripeW), static_cast<int>(rect.height), hazardYellow);
    DrawRectangle(static_cast<int>(rect.x + rect.width - stripeW), static_cast<int>(rect.y), static_cast<int>(stripeW), static_cast<int>(rect.height), hazardYellow);

    // Hazard slashes
    for (int s = 0; s < 4; ++s) {
        float sx1 = rect.x + s * 8.0f;
        DrawLineEx({sx1, rect.y}, {sx1 + 6.0f, rect.y + rect.height}, 2.5f, hazardDark);

        float sx2 = rect.x + rect.width - stripeW + s * 8.0f;
        DrawLineEx({sx2, rect.y}, {sx2 + 6.0f, rect.y + rect.height}, 2.5f, hazardDark);
    }

    // Truss triangle support under platform
    float trussY = rect.y + rect.height;
    int numTruss = static_cast<int>(rect.width / 40.0f);
    for (int t = 0; t < numTruss; ++t) {
        float tx = rect.x + t * 40.0f;
        DrawTriangle({tx, trussY}, {tx + 20.0f, trussY + 14.0f}, {tx + 40.0f, trussY}, Color{15, 23, 42, 200});
        DrawLineEx({tx, trussY}, {tx + 20.0f, trussY + 14.0f}, 1.5f, Color{51, 65, 85, 255});
        DrawLineEx({tx + 20.0f, trussY + 14.0f}, {tx + 40.0f, trussY}, 1.5f, Color{51, 65, 85, 255});
    }

    // Platform border
    DrawRectangleLinesEx(rect, 2.0f, Color{51, 65, 85, 255});
}

void Renderer::drawPickups(const GameWorld& world) {
    for (const auto& pk : world.getPickups()) {
        if (!pk.active) continue;
        drawPickup(pk);
    }
}

void Renderer::drawPickup(const PickupData& pk) {
    float px = pk.x;
    float py = pk.y + pk.floatOffset;

    // Outer glow card
    Rectangle card = {px - 18.0f, py - 18.0f, 36.0f, 36.0f};
    DrawRectangleRounded(card, 0.28f, 6, Color{15, 23, 42, 235});

    Color glowColor = Color{56, 189, 248, 255};
    const char* label = "";

    if (pk.type == "HEALTH") {
        glowColor = Color{239, 68, 68, 255};
        DrawRectangleRoundedLines(card, 0.28f, 6, 2.0f, glowColor);
        // Filled red heart symbol
        DrawCircle(static_cast<int>(px - 4), static_cast<int>(py - 3), 4.5f, glowColor);
        DrawCircle(static_cast<int>(px + 4), static_cast<int>(py - 3), 4.5f, glowColor);
        DrawTriangle({px - 8.5f, py - 1.5f}, {px + 8.5f, py - 1.5f}, {px, py + 8.0f}, glowColor);
    } else if (pk.type == "FUEL") {
        glowColor = Color{14, 165, 233, 255};
        DrawRectangleRoundedLines(card, 0.28f, 6, 2.0f, glowColor);
        // Stylized Lightning Bolt
        DrawTriangle({px + 1.0f, py - 9.0f}, {px - 5.0f, py - 1.0f}, {px + 1.0f, py - 1.0f}, Color{250, 204, 21, 255});
        DrawTriangle({px - 1.0f, py - 2.0f}, {px + 5.0f, py - 2.0f}, {px - 3.0f, py + 9.0f}, Color{250, 204, 21, 255});
    } else if (pk.type == "AMMO") {
        glowColor = Color{245, 158, 11, 255};
        DrawRectangleRoundedLines(card, 0.28f, 6, 2.0f, glowColor);
        DrawText("AMMO", static_cast<int>(px - 14), static_cast<int>(py - 5), 9, Color{251, 191, 36, 255});
    } else if (pk.type == "GRENADE") {
        glowColor = Color{34, 197, 94, 255};
        DrawRectangleRoundedLines(card, 0.28f, 6, 2.0f, glowColor);
        // Green grenade body + fuse
        DrawCircle(static_cast<int>(px), static_cast<int>(py + 2), 6.5f, Color{65, 87, 44, 255});
        DrawCircleLines(static_cast<int>(px), static_cast<int>(py + 2), 6.5f, Color{101, 163, 13, 255});
        DrawRectangle(static_cast<int>(px - 2), static_cast<int>(py - 6), 4, 3, Color{148, 163, 184, 255});
        DrawCircle(static_cast<int>(px + 3), static_cast<int>(py - 4), 2.0f, Color{250, 204, 21, 255});
    } else {
        // Weapon pickups (RIFLE, SMG, SHOTGUN, SNIPER, PISTOL, LASER, ROCKET)
        if (pk.type == "LASER") glowColor = Color{192, 132, 252, 255};
        else if (pk.type == "ROCKET") glowColor = Color{239, 68, 68, 255};
        else if (pk.type == "SNIPER") glowColor = Color{56, 189, 248, 255};
        else if (pk.type == "SHOTGUN") glowColor = Color{249, 115, 22, 255};
        else if (pk.type == "SMG") glowColor = Color{250, 204, 21, 255};
        else if (pk.type == "PISTOL") glowColor = Color{251, 191, 36, 255};
        else glowColor = Color{251, 146, 60, 255};

        DrawRectangleRoundedLines(card, 0.28f, 6, 2.0f, glowColor);
        label = pk.type.c_str();

        // Draw the authentic weapon sprite inside the pickup box!
        Texture2D wTex = getWeaponTexture(pk.type);
        if (wTex.id > 0) {
            float aspect = static_cast<float>(wTex.width) / wTex.height;
            float maxW = 28.0f;
            float maxH = 14.0f;
            float h = (pk.type == "ROCKET") ? 14.0f : ((pk.type == "PISTOL") ? 13.0f : 12.0f);
            float w = h * aspect;
            if (w > maxW) {
                w = maxW;
                h = w / aspect;
            }
            Rectangle src = {0.0f, 0.0f, static_cast<float>(wTex.width), static_cast<float>(wTex.height)};
            Rectangle dst = {px, py, w, h};
            Vector2 origin = {w * 0.5f, h * 0.5f};
            DrawTexturePro(wTex, src, dst, origin, 0.0f, WHITE);
        } else {
            // High quality fallback silhouette
            DrawRectangle(static_cast<int>(px - 10), static_cast<int>(py - 2), 20, 4, Color{226, 232, 240, 255});
            DrawRectangle(static_cast<int>(px - 3), static_cast<int>(py + 2), 4, 5, Color{226, 232, 240, 255});
        }

        // Weapon label text tag below card
        int tw = MeasureText(label, 9);
        DrawText(label, static_cast<int>(px - tw * 0.5f), static_cast<int>(py + 21.0f), 9, Color{241, 245, 249, 255});
    }
}

void Renderer::drawProjectiles(const GameWorld& world) {
    // 1. Bullets
    for (const auto& b : world.getBullets()) {
        float angle = atan2f(b.vy, b.vx);
        Vector2 dir = {cosf(angle), sinf(angle)};
        Vector2 tip = {b.x, b.y};
        Vector2 tail = {b.x - dir.x * b.length, b.y - dir.y * b.length};

        if (b.weaponType == "LASER") {
            // Purple beam with intense white core
            DrawLineEx(tail, tip, 8.0f, Color{192, 132, 252, 90});
            DrawLineEx(tail, tip, 3.5f, WHITE);
            DrawCircleV(tip, 4.0f, Color{240, 171, 252, 255});
        } else if (b.weaponType == "SNIPER") {
            // Cyan railgun vapor trail
            DrawLineEx(tail, tip, 7.0f, Color{56, 189, 248, 100});
            DrawLineEx(tail, tip, 3.0f, Color{224, 242, 254, 255});
            DrawCircleV(tip, 3.5f, Color{56, 189, 248, 255});
        } else if (b.weaponType == "SHOTGUN") {
            // Fiery orange buckshot pellet
            DrawLineEx(tail, tip, 5.0f, Color{234, 88, 12, 110});
            DrawCircleV(tip, 3.0f, Color{249, 115, 22, 255});
            DrawCircleV(tip, 1.8f, Color{254, 240, 138, 255});
        } else if (b.weaponType == "SMG") {
            // Bright yellow micro-tracer
            DrawLineEx(tail, tip, 4.0f, Color{250, 204, 21, 110});
            DrawLineEx(tail, tip, 2.0f, Color{254, 240, 138, 255});
        } else if (b.weaponType == "PISTOL") {
            // Amber kinetic tracer
            DrawLineEx(tail, tip, 4.5f, Color{251, 191, 36, 100});
            DrawLineEx(tail, tip, 2.2f, Color{254, 243, 199, 255});
        } else if (b.weaponType == "ROCKET") {
            // High-explosive propelled rocket projectile with exhaust flame
            DrawLineEx(tail, tip, 7.0f, Color{239, 68, 68, 120});
            DrawCircleV(tip, 5.0f, Color{239, 68, 68, 255});
            DrawCircleV({b.x - dir.x * 6.0f, b.y - dir.y * 6.0f}, 4.0f, Color{71, 85, 105, 255});
            DrawCircleV({b.x - dir.x * 12.0f, b.y - dir.y * 12.0f}, 3.0f, Color{245, 158, 11, 255});
        } else {
            // Assault rifle orange tracer
            DrawLineEx(tail, tip, 5.0f, Color{251, 146, 60, 120});
            DrawLineEx(tail, tip, 2.5f, Color{254, 240, 138, 255});
        }
    }

    // 2. Grenades
    for (const auto& g : world.getGrenades()) {
        DrawCircle(static_cast<int>(g.x), static_cast<int>(g.y), 7.0f, Color{65, 87, 44, 255});
        DrawCircleLines(static_cast<int>(g.x), static_cast<int>(g.y), 7.0f, Color{40, 56, 28, 255});

        bool blink = (static_cast<int>(g.fuse / 150.0f) % 2 == 0);
        DrawCircle(static_cast<int>(g.x), static_cast<int>(g.y) - 5, 2.5f, blink ? RED : DARKGRAY);
    }
}

void Renderer::drawParticles(const GameWorld& world) {
    for (const auto& p : world.getParticles()) {
        DrawCircleV(p.pos, p.size, p.color);
    }
}

void Renderer::drawExplosions(const GameWorld& world) {
    for (const auto& exp : world.getExplosions()) {
        float alpha = 1.0f - (exp.timer / exp.duration);
        Color shockColor = Color{251, 146, 60, static_cast<unsigned char>(255 * alpha)};
        DrawCircleLines(static_cast<int>(exp.pos.x), static_cast<int>(exp.pos.y), exp.radius, shockColor);
        DrawCircleLines(static_cast<int>(exp.pos.x), static_cast<int>(exp.pos.y), exp.radius - 2.0f, shockColor);

        if (exp.timer < 0.15f) {
            DrawCircle(static_cast<int>(exp.pos.x), static_cast<int>(exp.pos.y), exp.radius * 0.45f, WHITE);
        }
    }
}

void Renderer::drawDamageTexts(const GameWorld& world) {
    for (const auto& dt : world.getDamageTexts()) {
        DrawText(dt.text.c_str(), static_cast<int>(dt.pos.x), static_cast<int>(dt.pos.y), 16, dt.color);
    }
}

void Renderer::drawSoldiers(const GameWorld& world, const std::string& localPlayerId) {
    for (const auto& pair : world.getPlayers()) {
        const PlayerData& p = pair.second;
        if (p.isDead) continue;
        drawSoldier(p, p.id == localPlayerId);
    }
}

void Renderer::drawSoldier(const PlayerData& p, bool isLocal) {
    float px = p.renderX;
    float py = p.renderY;
    int facing = p.facing != 0 ? p.facing : 1;

    // 1. Backpack Jetpack (Behind body)
    Vector2 jetpackPos = {px - static_cast<float>(facing) * 16.0f, py - 4.0f};
    DrawRectangleRounded({jetpackPos.x - 6.0f, jetpackPos.y - 13.0f, 12.0f, 26.0f}, 0.35f, 6, Color{71, 85, 105, 255});
    DrawRectangleRoundedLines({jetpackPos.x - 6.0f, jetpackPos.y - 13.0f, 12.0f, 26.0f}, 0.35f, 6, 2.0f, Color{15, 23, 42, 255});
    // Nozzle
    Vector2 nozzle = {jetpackPos.x, jetpackPos.y + 13.0f};
    DrawRectangle(static_cast<int>(nozzle.x - 3.0f), static_cast<int>(nozzle.y), 6, 4, Color{30, 41, 59, 255});

    // Jetpack Flames when thrusting
    if (p.isFlying) {
        float flameLen = 22.0f + static_cast<float>(rand() % 10);
        // Orange outer flame
        DrawTriangle(
            {nozzle.x - 5.0f, nozzle.y + 3.0f},
            {nozzle.x + 5.0f, nozzle.y + 3.0f},
            {nozzle.x, nozzle.y + 3.0f + flameLen},
            Color{249, 115, 22, 255}
        );
        // Hot yellow inner core
        DrawTriangle(
            {nozzle.x - 2.5f, nozzle.y + 3.0f},
            {nozzle.x + 2.5f, nozzle.y + 3.0f},
            {nozzle.x, nozzle.y + 3.0f + flameLen * 0.6f},
            Color{254, 240, 138, 255}
        );
        DrawCircleV(nozzle, 10.0f, Color{251, 146, 60, 90});
    }

    // 2. Animated Legs & Shoes
    drawPlayerLegs(p, facing, p.walkCycle);

    // 3. Torso & Tactical Armor
    drawPlayerTorso(p, facing);

    // 4. Head, Helmet, Visor & Hair
    drawPlayerHead(p, facing);

    // 5. 360-degree Rotating Arm & Weapon
    drawAimingArmAndWeapon(p, p.aimAngle, facing);

    // 6. In-World Floating HUD (Nametag + Mini Health Bar)
    float tagY = py - 46.0f;
    const char* displayName = p.name.c_str();
    int nameW = MeasureText(displayName, 12);
    Rectangle tagRect = {px - nameW * 0.5f - 6.0f, tagY, static_cast<float>(nameW + 12), 16.0f};

    Color tagBg = isLocal ? Color{37, 99, 235, 210} : (p.isBot ? Color{55, 65, 81, 210} : Color{15, 23, 42, 210});
    DrawRectangleRounded(tagRect, 0.4f, 6, tagBg);
    DrawText(displayName, static_cast<int>(px - nameW * 0.5f), static_cast<int>(tagY + 2), 12, WHITE);

    // Mini Health Bar above nametag
    float barW = 44.0f;
    float barH = 5.0f;
    Rectangle hpBarBg = {px - barW * 0.5f, tagY - 8.0f, barW, barH};
    DrawRectangleRec(hpBarBg, Color{15, 23, 42, 230});

    float healthRatio = std::max(0.0f, std::min(1.0f, static_cast<float>(p.health) / 100.0f));
    Color hpCol = (healthRatio > 0.5f) ? Color{34, 197, 94, 255} : ((healthRatio > 0.25f) ? Color{234, 179, 8, 255} : Color{239, 68, 68, 255});
    DrawRectangle(static_cast<int>(hpBarBg.x), static_cast<int>(hpBarBg.y),
                  static_cast<int>(barW * healthRatio), static_cast<int>(barH), hpCol);
    DrawRectangleLinesEx(hpBarBg, 1.0f, Color{51, 65, 85, 255});

    if (p.reloading) {
        DrawText("RELOADING...", static_cast<int>(px - 34.0f), static_cast<int>(tagY - 22.0f), 10, Color{250, 204, 21, 255});
    }
}

void Renderer::drawShoe(const std::string& avatarStyle, float x, float y) {
    if (avatarStyle == "raphile") {
        // Red & White high-top sneakers!
        DrawRectangle(static_cast<int>(x), static_cast<int>(y), 9, 6, Color{239, 68, 68, 255}); // Red upper
        DrawRectangle(static_cast<int>(x + 5), static_cast<int>(y + 1), 4, 5, WHITE); // White toe cap
        DrawRectangle(static_cast<int>(x), static_cast<int>(y + 4), 9, 2, WHITE); // White rubber sole
    } else if (avatarStyle == "simon") {
        // Combat boot with rugged orange tread
        DrawRectangle(static_cast<int>(x), static_cast<int>(y), 9, 6, Color{30, 41, 59, 255});
        DrawRectangle(static_cast<int>(x), static_cast<int>(y + 4), 9, 2, Color{217, 119, 6, 255});
    } else {
        // Standard army combat boot
        DrawRectangle(static_cast<int>(x), static_cast<int>(y), 9, 6, Color{120, 53, 15, 255});
    }
}

void Renderer::drawPlayerLegs(const PlayerData& p, int facing, float walkCycle) {
    float px = p.renderX;
    float py = p.renderY;

    Color pantsColor = (p.avatarStyle == "raphile") ? Color{17, 24, 39, 255} :
                       (p.avatarStyle == "simon") ? Color{51, 65, 85, 255} : Color{75, 85, 99, 255};

    if (p.isOnGround && fabsf(p.vx) > 10.0f) {
        // Running swing animation
        float swing = sinf(walkCycle) * 6.0f;
        // Leg 1
        DrawRectangle(static_cast<int>(px - 6.0f), static_cast<int>(py + 10.0f + swing), 5, 14, pantsColor);
        drawShoe(p.avatarStyle, px - 8.0f, py + 22.0f + swing);
        // Leg 2
        DrawRectangle(static_cast<int>(px + 2.0f), static_cast<int>(py + 10.0f - swing), 5, 14, pantsColor);
        drawShoe(p.avatarStyle, px + 1.0f, py + 22.0f - swing);
    } else {
        // Airborne or idle bent legs
        DrawRectangle(static_cast<int>(px - 7.0f), static_cast<int>(py + 10.0f), 5, 14, pantsColor);
        drawShoe(p.avatarStyle, px - 9.0f, py + 22.0f);

        DrawRectangle(static_cast<int>(px + 2.0f), static_cast<int>(py + 10.0f), 5, 14, pantsColor);
        drawShoe(p.avatarStyle, px + 1.0f, py + 22.0f);

        // Simon metallic shin guards (greaves)
        if (p.avatarStyle == "simon") {
            DrawRectangle(static_cast<int>(px - 8.0f), static_cast<int>(py + 14.0f), 7, 7, Color{148, 163, 184, 255});
            DrawRectangle(static_cast<int>(px + 1.0f), static_cast<int>(py + 14.0f), 7, 7, Color{148, 163, 184, 255});
        }
    }
}

void Renderer::drawPlayerTorso(const PlayerData& p, int facing) {
    float px = p.renderX;
    float py = p.renderY;

    if (p.avatarStyle == "raphile") {
        // Charcoal hoodie with Crossed Dual Bullet Bandoliers in an X shape
        Rectangle torsoRect = {px - 12.0f, py - 13.0f, 24.0f, 23.0f};
        DrawRectangleRounded(torsoRect, 0.25f, 6, Color{17, 24, 39, 255});
        DrawRectangleRoundedLines(torsoRect, 0.25f, 6, 2.0f, Color{3, 7, 18, 255});

        // Crossed Bandoliers in an X
        DrawLineEx({px - 10.0f, py - 12.0f}, {px + 10.0f, py + 7.0f}, 3.5f, Color{120, 53, 15, 255});
        DrawLineEx({px + 10.0f, py - 12.0f}, {px - 10.0f, py + 7.0f}, 3.5f, Color{120, 53, 15, 255});

        // Red ammo shells along bandolier
        DrawRectangle(static_cast<int>(px - 7), static_cast<int>(py - 6), 3, 4, Color{239, 68, 68, 255});
        DrawRectangle(static_cast<int>(px + 4), static_cast<int>(py - 6), 3, 4, Color{239, 68, 68, 255});
        DrawRectangle(static_cast<int>(px - 3), static_cast<int>(py), 3, 4, WHITE);
        DrawRectangle(static_cast<int>(px + 1), static_cast<int>(py), 3, 4, WHITE);

        // Tactical belt with gold buckle & side holster
        DrawRectangle(static_cast<int>(px - 12), static_cast<int>(py + 6), 24, 5, Color{120, 53, 15, 255});
        DrawRectangle(static_cast<int>(px - 3), static_cast<int>(py + 6), 6, 5, Color{251, 191, 36, 255});
        DrawRectangle(static_cast<int>(px + 7), static_cast<int>(py + 8), 5, 7, Color{71, 85, 105, 255});
    } else if (p.avatarStyle == "simon") {
        // Yellow Tactical Combat Vest over Dark Undershirt with Grey Cowl Scarf
        DrawRectangle(static_cast<int>(px - 12), static_cast<int>(py - 13), 24, 23, Color{30, 41, 59, 255});

        Rectangle vestRect = {px - 11.0f, py - 9.0f, 22.0f, 16.0f};
        DrawRectangleRounded(vestRect, 0.2f, 4, Color{217, 119, 6, 255});
        DrawRectangleRoundedLines(vestRect, 0.2f, 4, 1.8f, Color{120, 53, 15, 255});

        // Utility chest pocket
        DrawRectangle(static_cast<int>(px - 8), static_cast<int>(py - 4), 6, 6, Color{180, 83, 9, 255});

        // Grey Cowl Scarf
        DrawTriangle({px - 10.0f, py - 13.0f}, {px + 10.0f, py - 13.0f}, {px, py - 5.0f}, Color{100, 116, 139, 255});

        // Tactical belt
        DrawRectangle(static_cast<int>(px - 12), static_cast<int>(py + 6), 24, 5, Color{51, 65, 85, 255});
        DrawRectangle(static_cast<int>(px - 3), static_cast<int>(py + 6), 6, 5, Color{148, 163, 184, 255});
    } else {
        // Standard Tactical Soldier Vest
        Rectangle vestRect = {px - 12.0f, py - 13.0f, 24.0f, 23.0f};
        DrawRectangleRounded(vestRect, 0.25f, 6, Color{63, 98, 18, 255});
        DrawRectangleRoundedLines(vestRect, 0.25f, 6, 2.0f, Color{20, 83, 45, 255});

        // Scarf / Bandana
        DrawTriangle({px - 9.0f, py - 12.0f}, {px + 9.0f, py - 12.0f}, {px, py - 4.0f}, Color{132, 204, 22, 255});

        // Tactical belt with gold buckle
        DrawRectangle(static_cast<int>(px - 12), static_cast<int>(py + 6), 24, 5, Color{120, 53, 15, 255});
        DrawRectangle(static_cast<int>(px - 3), static_cast<int>(py + 6), 6, 5, Color{251, 191, 36, 255});
    }
}

void Renderer::drawPlayerHead(const PlayerData& p, int facing) {
    float px = p.renderX;
    float headY = p.renderY - 22.0f;

    // Face skin
    DrawCircle(static_cast<int>(px), static_cast<int>(headY), 13.0f, Color{254, 215, 170, 255});
    DrawCircleLines(static_cast<int>(px), static_cast<int>(headY), 13.0f, Color{24, 24, 27, 255});

    // Eye
    float eyeX = px + static_cast<float>(facing) * 4.0f;
    DrawCircle(static_cast<int>(eyeX), static_cast<int>(headY - 1.0f), 2.2f, Color{24, 24, 27, 255});

    if (p.avatarStyle == "raphile") {
        // Futuristic White Cyber Helmet with Yellow Horn Crests & Black Visor
        // Left & Right Yellow Horn Crests
        float hxL = px - 9.0f;
        float hxR = px + 9.0f;
        DrawTriangle({hxL, headY - 4.0f}, {hxL - 3.0f, headY - 16.0f}, {hxL + 4.0f, headY - 8.0f}, Color{234, 179, 8, 255});
        DrawTriangle({hxR, headY - 4.0f}, {hxR + 3.0f, headY - 16.0f}, {hxR - 4.0f, headY - 8.0f}, Color{234, 179, 8, 255});

        // White Helmet Dome
        DrawCircle(static_cast<int>(px), static_cast<int>(headY), 13.5f, Color{248, 250, 252, 255});
        DrawCircleLines(static_cast<int>(px), static_cast<int>(headY), 13.5f, Color{9, 9, 11, 255});

        // Black Tinted Cyber Visor
        float visorCenterX = px + static_cast<float>(facing) * 3.0f;
        Rectangle visorRect = {visorCenterX - 9.0f, headY - 4.0f, 18.0f, 8.0f};
        DrawRectangleRounded(visorRect, 0.4f, 4, Color{9, 9, 11, 255});
        // Purple neon specular streak across visor
        DrawLineEx({visorRect.x + 3.0f, headY - 1.0f}, {visorRect.x + visorRect.width - 3.0f, headY - 1.0f}, 1.6f, Color{192, 132, 252, 255});

        // Silver mouth/chin vent
        DrawRectangle(static_cast<int>(px - 4), static_cast<int>(headY + 6), 8, 5, Color{148, 163, 184, 255});
        DrawRectangleLines(static_cast<int>(px - 4), static_cast<int>(headY + 6), 8, 5, Color{9, 9, 11, 255});
    } else if (p.avatarStyle == "simon") {
        // Simon: Long Dark Hair behind neck + Mustache + Pointed Goatee
        DrawRectangle(static_cast<int>(px - 14), static_cast<int>(headY - 2), 7, 16, Color{24, 24, 27, 255});
        DrawCircle(static_cast<int>(px), static_cast<int>(headY - 4), 13.5f, Color{24, 24, 27, 255}); // Hair top

        // Mustache & Goatee
        DrawRectangle(static_cast<int>(eyeX - 1), static_cast<int>(headY + 3), 6, 2, Color{24, 24, 27, 255});
        DrawTriangle({px - 2.0f, headY + 7.0f}, {px + 4.0f, headY + 7.0f}, {px + 1.0f, headY + 13.0f}, Color{24, 24, 27, 255});
    } else {
        // Soldier: Army camo helmet + Tactical Goggles
        DrawCircle(static_cast<int>(px), static_cast<int>(headY - 3), 13.5f, Color{77, 124, 15, 255});
        DrawCircleLines(static_cast<int>(px), static_cast<int>(headY - 3), 13.5f, Color{20, 83, 45, 255});

        // Blue goggles
        float gX = px + static_cast<float>(facing) * 3.0f;
        Rectangle gogRect = {gX - 7.0f, headY - 6.0f, 14.0f, 5.0f};
        DrawRectangleRounded(gogRect, 0.4f, 4, Color{2, 132, 199, 255});
        DrawRectangleRoundedLines(gogRect, 0.4f, 4, 1.2f, Color{8, 47, 73, 255});
    }
}

void Renderer::drawAimingArmAndWeapon(const PlayerData& p, float aimAngle, int facing) {
    float px = p.renderX;
    float py = p.renderY;
    Vector2 shoulder = {px, py - 6.0f};

    // Apply recoil offset along aim angle
    shoulder.x -= cosf(aimAngle) * p.recoilOffset;
    shoulder.y -= sinf(aimAngle) * p.recoilOffset;

    rlDisableBackfaceCulling();

    rlPushMatrix();
    rlTranslatef(shoulder.x, shoulder.y, 0.0f);
    rlRotatef(aimAngle * RAD2DEG, 0, 0, 1);

    bool aimingLeft = cosf(aimAngle) < 0.0f;
    if (aimingLeft) {
        rlScalef(1.0f, -1.0f, 1.0f);
    }

    // Arm sleeve matching character
    if (p.avatarStyle == "raphile") {
        DrawRectangle(0, -4, 4, 7, WHITE); // White hoodie cuff
        DrawRectangle(4, -4, 9, 7, Color{254, 215, 170, 255}); // Bare skin
        DrawRectangle(11, -3, 6, 6, Color{120, 53, 15, 255}); // Brown shooting glove
    } else if (p.avatarStyle == "simon") {
        DrawRectangle(0, -4, 5, 7, Color{234, 179, 8, 255}); // Yellow armband
        DrawRectangle(5, -4, 5, 7, Color{254, 215, 170, 255}); // Skin
        DrawRectangle(10, -4, 4, 7, Color{71, 85, 105, 255}); // Guard
        DrawRectangle(12, -3, 5, 6, Color{30, 41, 59, 255}); // Dark glove
    } else {
        DrawRectangle(0, -4, 13, 7, Color{77, 124, 15, 255}); // Tactical sleeve
        DrawRectangle(11, -3, 6, 6, Color{120, 53, 15, 255}); // Glove
    }

    // Weapon Sprite or Vector Drawing
    const std::string& weaponType = p.weapon.empty() ? "RIFLE" : p.weapon;
    Texture2D tex = getWeaponTexture(weaponType);
    float barrelLen = 36.0f;

    if (tex.id > 0) {
        Rectangle src = {0, 0, static_cast<float>(tex.width), static_cast<float>(tex.height)};
        float aspect = static_cast<float>(tex.width) / tex.height;
        float h = (weaponType == "PISTOL") ? 13.0f : ((weaponType == "SMG") ? 14.0f : ((weaponType == "SNIPER") ? 14.0f : ((weaponType == "ROCKET") ? 18.0f : 16.0f)));
        float w = h * aspect;
        w = std::min(weaponType == "ROCKET" ? 56.0f : 48.0f, std::max(20.0f, w));
        barrelLen = 12.0f + w;

        Rectangle dest = {12.0f, -h * 0.5f - 1.0f, w, h};
        DrawTexturePro(tex, src, dest, {0, 0}, 0.0f, WHITE);
    } else {
        // High quality vector gun fallback
        Color gunMetal = Color{30, 41, 59, 255};
        Color gunHighlight = Color{71, 85, 105, 255};

        if (weaponType == "SNIPER") {
            barrelLen = 48.0f;
            DrawRectangle(12, -3, 36, 6, gunMetal);
            DrawRectangle(18, -8, 14, 4, Color{15, 23, 42, 255}); // Scope
            DrawLine(18, -6, 32, -6, Color{56, 189, 248, 255});
        } else if (weaponType == "SHOTGUN") {
            barrelLen = 38.0f;
            DrawRectangle(12, -4, 26, 8, gunMetal);
            DrawRectangle(20, 0, 10, 5, Color{217, 119, 6, 255}); // Pump
        } else if (weaponType == "SMG") {
            barrelLen = 30.0f;
            DrawRectangle(12, -4, 18, 8, gunMetal);
            DrawRectangle(18, 4, 4, 8, Color{250, 204, 21, 255}); // Mag
        } else if (weaponType == "LASER") {
            barrelLen = 42.0f;
            DrawRectangle(12, -4, 30, 8, Color{76, 29, 149, 255});
            DrawRectangle(18, -2, 16, 4, Color{192, 132, 252, 255}); // Core
        } else if (weaponType == "PISTOL") {
            barrelLen = 26.0f;
            DrawRectangle(12, -3, 14, 6, gunMetal);
            DrawRectangle(12, 3, 4, 6, Color{120, 53, 15, 255});
        } else {
            // Assault Rifle
            barrelLen = 38.0f;
            DrawRectangle(12, -4, 26, 8, gunMetal);
            DrawRectangle(20, 4, 5, 8, Color{15, 23, 42, 255});
            DrawRectangle(32, -2, 8, 4, gunHighlight);
        }
    }

    rlPopMatrix();

    // Muzzle Flash Star Burst at Barrel Tip
    if (p.muzzleFlashTimer > 0.0f) {
        Vector2 barrelTip = {
            shoulder.x + cosf(aimAngle) * barrelLen,
            shoulder.y + sinf(aimAngle) * barrelLen
        };
        drawMuzzleFlash(barrelTip, aimAngle, weaponType);
    }
}

void Renderer::drawGunPointer(const PlayerData& p, Camera2D camera) {
    if (p.isDead) return;

    const std::string& weaponType = p.weapon.empty() ? "RIFLE" : p.weapon;
    float barrelLen = 36.0f;
    Texture2D tex = getWeaponTexture(weaponType);
    if (tex.id > 0) {
        float aspect = static_cast<float>(tex.width) / tex.height;
        float h = (weaponType == "PISTOL") ? 13.0f : ((weaponType == "SMG") ? 14.0f : ((weaponType == "SNIPER") ? 14.0f : ((weaponType == "ROCKET") ? 18.0f : 16.0f)));
        float w = h * aspect;
        w = std::min(weaponType == "ROCKET" ? 56.0f : 48.0f, std::max(20.0f, w));
        barrelLen = 12.0f + w;
    } else {
        if (weaponType == "SNIPER") barrelLen = 48.0f;
        else if (weaponType == "SHOTGUN") barrelLen = 38.0f;
        else if (weaponType == "SMG") barrelLen = 30.0f;
        else if (weaponType == "LASER") barrelLen = 42.0f;
        else if (weaponType == "PISTOL") barrelLen = 26.0f;
        else if (weaponType == "ROCKET") barrelLen = 50.0f;
        else barrelLen = 38.0f;
    }

    Vector2 shoulder = {p.renderX, p.renderY - 6.0f};
    shoulder.x -= cosf(p.aimAngle) * p.recoilOffset;
    shoulder.y -= sinf(p.aimAngle) * p.recoilOffset;

    Vector2 barrelTip = {
        shoulder.x + cosf(p.aimAngle) * barrelLen,
        shoulder.y + sinf(p.aimAngle) * barrelLen
    };

    // Authentic Mini Militia laser palette: Iconic Red & White dashed laser line
    Color mainLaserColor = Color{255, 36, 36, 255};   // Bright vibrant red
    Color laserGlowColor = Color{239, 68, 68, 90};    // Soft ambient red glow

    if (weaponType == "LASER") {
        mainLaserColor = Color{192, 132, 252, 255};   // Futuristic violet
        laserGlowColor = Color{192, 132, 252, 90};
    } else if (weaponType == "SNIPER") {
        mainLaserColor = Color{56, 189, 248, 255};    // High-power cyan
        laserGlowColor = Color{56, 189, 248, 90};
    }

    Vector2 dir = { cosf(p.aimAngle), sinf(p.aimAngle) };

    // Trajectory length across the screen
    float maxDist = 650.0f;
    #if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__)
    Vector2 mouseScreen = GetMousePosition();
    Vector2 mouseWorld = GetScreenToWorld2D(mouseScreen, camera);
    float mouseDist = Vector2Distance(barrelTip, mouseWorld);
    maxDist = std::max(650.0f, mouseDist);
    #endif

    // 1. Initial solid laser segment at the gun barrel
    float solidLen = 32.0f;
    Vector2 solidEnd = { barrelTip.x + dir.x * solidLen, barrelTip.y + dir.y * solidLen };
    DrawLineEx(barrelTip, solidEnd, 3.2f, laserGlowColor);
    DrawLineEx(barrelTip, solidEnd, 2.0f, mainLaserColor);

    // 2. Continuous soft glow baseline
    Vector2 lineEnd = { barrelTip.x + dir.x * maxDist, barrelTip.y + dir.y * maxDist };
    DrawLineEx(solidEnd, lineEnd, 3.0f, laserGlowColor);
    DrawLineEx(solidEnd, lineEnd, 1.2f, Color{mainLaserColor.r, mainLaserColor.g, mainLaserColor.b, 160});

    // 3. Alternating Red and White dashed segments matching Mini Militia reference
    float segLen = 18.0f;
    float animOffset = fmodf(static_cast<float>(GetTime()) * 40.0f, segLen * 2.0f);
    float currentDist = solidLen;

    while (currentDist < maxDist) {
        float nextDist = std::min(maxDist, currentDist + segLen);
        Vector2 p1 = { barrelTip.x + dir.x * currentDist, barrelTip.y + dir.y * currentDist };
        Vector2 p2 = { barrelTip.x + dir.x * nextDist, barrelTip.y + dir.y * nextDist };

        int segIndex = static_cast<int>((currentDist + animOffset) / segLen);
        bool isWhite = (segIndex % 2 == 1);

        if (isWhite) {
            DrawLineEx(p1, p2, 2.4f, WHITE);
        } else {
            DrawLineEx(p1, p2, 2.0f, mainLaserColor);
        }

        currentDist = nextDist;
    }

    // 4. Glowing laser diode at gun barrel tip
    DrawCircleV(barrelTip, 2.8f, mainLaserColor);
    DrawCircleV(barrelTip, 1.4f, WHITE);

    // 5. Subtle laser point at cursor / max range (clean red dot)
    #if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__)
    DrawCircleV(mouseWorld, 3.5f, mainLaserColor);
    DrawCircleV(mouseWorld, 1.8f, WHITE);
    DrawCircleV(mouseWorld, 7.0f, laserGlowColor);
    #else
    DrawCircleV(lineEnd, 3.5f, mainLaserColor);
    DrawCircleV(lineEnd, 1.8f, WHITE);
    DrawCircleV(lineEnd, 7.0f, laserGlowColor);
    #endif
}

void Renderer::drawMuzzleFlash(Vector2 barrelTip, float aimAngle, const std::string& weaponType) {
    Color outerColor = Color{251, 146, 60, 255};
    if (weaponType == "LASER") outerColor = Color{192, 132, 252, 255};
    else if (weaponType == "ROCKET") outerColor = Color{239, 68, 68, 255};
    else if (weaponType == "SNIPER") outerColor = Color{56, 189, 248, 255};
    else if (weaponType == "SMG") outerColor = Color{250, 204, 21, 255};

    float r = 12.0f;
    // Radiant 4-point star
    DrawTriangle(barrelTip,
                 {barrelTip.x + cosf(aimAngle + 1.2f) * r * 0.5f, barrelTip.y + sinf(aimAngle + 1.2f) * r * 0.5f},
                 {barrelTip.x + cosf(aimAngle) * (r * 1.5f), barrelTip.y + sinf(aimAngle) * (r * 1.5f)},
                 outerColor);
    DrawTriangle(barrelTip,
                 {barrelTip.x + cosf(aimAngle - 1.2f) * r * 0.5f, barrelTip.y + sinf(aimAngle - 1.2f) * r * 0.5f},
                 {barrelTip.x + cosf(aimAngle) * (r * 1.5f), barrelTip.y + sinf(aimAngle) * (r * 1.5f)},
                 outerColor);
    DrawCircleV(barrelTip, 4.0f, WHITE);
    DrawCircleV(barrelTip, 10.0f, Color{outerColor.r, outerColor.g, outerColor.b, 90});
}

HUDClickResult Renderer::renderHUD(const GameWorld& world, const std::string& localPlayerId,
                                   const std::string& roomCode, int pingMs, bool isMuted,
                                   int screenWidth, int screenHeight) {
    HUDClickResult result;
    const PlayerData* localPlayer = world.getLocalPlayer(localPlayerId);

    // 1. Top-Left Player Status Card (HP, Fuel, Ammo, Grenades)
    drawPlayerStatusCard(localPlayer, screenWidth, screenHeight);

    // 2. Top-Center Match Timer & Room Code
    drawTopCenterInfo(world.getMatchDuration(), roomCode, screenWidth, result.copyCodeClicked);

    // 3. Top-Right Buttons (Ping, Mute, Fullscreen, +Bot, -Bot, Leave)
    drawTopRightControls(pingMs, isMuted, screenWidth, result);

    // 4. Top-Right Live Kill Feed
    drawKillFeed(world.getKillFeed(), screenWidth);

    // 5. Desktop Controls Legend Footer (at bottom left)
    DrawRectangleRounded({20.0f, screenHeight - 34.0f, 480.0f, 22.0f}, 0.3f, 4, Color{15, 23, 42, 160});
    DrawText("A/D Move | W/Space Jetpack | L-Click Shoot | G/R-Click Grenade | R Reload",
             30, screenHeight - 30, 11, Color{148, 163, 184, 220});

    return result;
}

void Renderer::drawPlayerStatusCard(const PlayerData* localPlayer, int screenWidth, int screenHeight) {
    int hp = localPlayer ? localPlayer->health : 100;
    int fuel = localPlayer ? localPlayer->fuel : 100;
    std::string weaponName = localPlayer ? localPlayer->weapon : "RIFLE";
    int ammo = localPlayer ? localPlayer->ammo : 30;
    int maxAmmo = localPlayer ? localPlayer->maxAmmo : 30;
    int grenades = localPlayer ? localPlayer->grenades : 2;

    float cardX = 20.0f;
    float cardY = 16.0f;

    // --- HP BAR ---
    DrawCircle(static_cast<int>(cardX + 16.0f), static_cast<int>(cardY + 16.0f), 16.0f, Color{239, 68, 68, 255});
    DrawText("+", static_cast<int>(cardX + 10.0f), static_cast<int>(cardY + 6.0f), 20, WHITE);

    Rectangle hpBarRect = {cardX + 38.0f, cardY + 5.0f, 220.0f, 22.0f};
    DrawRectangleRounded(hpBarRect, 0.3f, 6, Color{15, 23, 42, 220});
    float hpRatio = std::max(0.0f, std::min(1.0f, static_cast<float>(hp) / 100.0f));
    Rectangle hpFill = {hpBarRect.x, hpBarRect.y, hpBarRect.width * hpRatio, hpBarRect.height};
    DrawRectangleRounded(hpFill, 0.3f, 6, Color{34, 197, 94, 255});
    DrawRectangleRoundedLines(hpBarRect, 0.3f, 6, 2.0f, Color{71, 85, 105, 200});

    char hpText[32];
    snprintf(hpText, sizeof(hpText), "HP: %d / 100", hp);
    DrawText(hpText, static_cast<int>(hpBarRect.x + 8.0f), static_cast<int>(hpBarRect.y + 4.0f), 13, WHITE);

    // --- FUEL BAR ---
    float fuelY = cardY + 36.0f;
    DrawCircle(static_cast<int>(cardX + 16.0f), static_cast<int>(fuelY + 16.0f), 16.0f, Color{14, 165, 233, 255});
    DrawText("F", static_cast<int>(cardX + 10.0f), static_cast<int>(fuelY + 7.0f), 18, WHITE);

    Rectangle fuelBarRect = {cardX + 38.0f, fuelY + 5.0f, 220.0f, 22.0f};
    DrawRectangleRounded(fuelBarRect, 0.3f, 6, Color{15, 23, 42, 220});
    float fuelRatio = std::max(0.0f, std::min(1.0f, static_cast<float>(fuel) / 100.0f));
    Rectangle fuelFill = {fuelBarRect.x, fuelBarRect.y, fuelBarRect.width * fuelRatio, fuelBarRect.height};
    Color fuelCol = (fuel <= 0) ? Color{239, 68, 68, 255} : Color{56, 189, 248, 255};
    DrawRectangleRounded(fuelFill, 0.3f, 6, fuelCol);
    DrawRectangleRoundedLines(fuelBarRect, 0.3f, 6, 2.0f, Color{71, 85, 105, 200});

    char fuelText[36];
    if (fuel <= 0) {
        snprintf(fuelText, sizeof(fuelText), "NO FUEL (LAND TO CHARGE)");
    } else {
        snprintf(fuelText, sizeof(fuelText), "FUEL: %d%%", fuel);
    }
    DrawText(fuelText, static_cast<int>(fuelBarRect.x + 8.0f), static_cast<int>(fuelBarRect.y + 4.0f), 12, WHITE);

    // --- WEAPON & AMMO STATUS CARD ---
    float wepY = fuelY + 38.0f;
    Rectangle wepCardRect = {cardX, wepY, 260.0f, 54.0f};
    DrawRectangleRounded(wepCardRect, 0.25f, 6, Color{15, 23, 42, 230});
    DrawRectangleRoundedLines(wepCardRect, 0.25f, 6, 2.0f, Color{51, 65, 85, 220});

    // Weapon title
    std::string displayName = weaponName;
    if (weaponName == "RIFLE") displayName = "ASSAULT RIFLE";
    else if (weaponName == "SMG") displayName = "SUBMACHINE GUN";
    else if (weaponName == "SHOTGUN") displayName = "COMBAT SHOTGUN";
    else if (weaponName == "SNIPER") displayName = "HEAVY SNIPER";
    else if (weaponName == "PISTOL") displayName = "HEAVY PISTOL";
    else if (weaponName == "LASER") displayName = "LASER RIFLE";
    else if (weaponName == "ROCKET") displayName = "ROCKET LAUNCHER";

    // Draw authentic weapon sprite on left of HUD card
    Texture2D hudGunTex = getWeaponTexture(weaponName);
    float gunBoxX = cardX + 8.0f;
    float gunBoxY = wepY + 6.0f;
    float gunBoxW = 56.0f;
    float gunBoxH = 42.0f;

    if (hudGunTex.id > 0) {
        float aspect = static_cast<float>(hudGunTex.width) / hudGunTex.height;
        float h = (weaponName == "PISTOL") ? 20.0f : ((weaponName == "ROCKET") ? 24.0f : 22.0f);
        float w = h * aspect;
        if (w > gunBoxW) {
            w = gunBoxW;
            h = w / aspect;
        }
        Rectangle src = {0.0f, 0.0f, static_cast<float>(hudGunTex.width), static_cast<float>(hudGunTex.height)};
        Rectangle dst = {gunBoxX + gunBoxW * 0.5f, gunBoxY + gunBoxH * 0.5f, w, h};
        Vector2 origin = {w * 0.5f, h * 0.5f};
        DrawTexturePro(hudGunTex, src, dst, origin, 0.0f, WHITE);
    } else {
        DrawRectangle(static_cast<int>(gunBoxX + 10), static_cast<int>(gunBoxY + 18), 36, 6, Color{100, 116, 139, 255});
    }

    float textStartX = cardX + 70.0f;
    DrawText(displayName.c_str(), static_cast<int>(textStartX), static_cast<int>(wepY + 8.0f), 12, Color{241, 245, 249, 255});

    char ammoText[36];
    if (localPlayer && localPlayer->reloading) {
        snprintf(ammoText, sizeof(ammoText), "RELOADING...");
        DrawText(ammoText, static_cast<int>(textStartX), static_cast<int>(wepY + 26.0f), 14, Color{250, 204, 21, 255});
    } else {
        snprintf(ammoText, sizeof(ammoText), "AMMO: %d / %d", ammo, maxAmmo);
        Color ammoColor = (ammo <= 5) ? Color{244, 63, 94, 255} : Color{251, 113, 133, 255};
        DrawText(ammoText, static_cast<int>(textStartX), static_cast<int>(wepY + 26.0f), 14, ammoColor);
    }

    // Grenade counter with icon
    float grenadeIconX = cardX + 210.0f;
    DrawCircle(static_cast<int>(grenadeIconX), static_cast<int>(wepY + 34.0f), 6.5f, Color{34, 197, 94, 255});
    DrawRectangle(static_cast<int>(grenadeIconX - 1.5f), static_cast<int>(wepY + 25.0f), 3, 3, Color{71, 85, 105, 255});
    char grenadeText[16];
    snprintf(grenadeText, sizeof(grenadeText), "x%d", grenades);
    DrawText(grenadeText, static_cast<int>(grenadeIconX + 11.0f), static_cast<int>(wepY + 27.0f), 13, Color{203, 213, 225, 255});
}

void Renderer::drawTopCenterInfo(int matchDuration, const std::string& roomCode, int screenWidth, bool& copyClicked) {
    float cx = screenWidth * 0.5f;

    // Timer Pill (e.g. "2:19")
    int mins = matchDuration / 60;
    int secs = matchDuration % 60;
    char timerStr[16];
    snprintf(timerStr, sizeof(timerStr), "%d:%02d", mins, secs);

    Rectangle timerRect = {cx - 45.0f, 16.0f, 90.0f, 30.0f};
    DrawRectangleRounded(timerRect, 0.4f, 6, Color{15, 23, 42, 230});
    DrawRectangleRoundedLines(timerRect, 0.4f, 6, 2.0f, Color{71, 85, 105, 200});
    DrawText(timerStr, static_cast<int>(cx - 18.0f), 22, 18, WHITE);

    // Room Code Badge with Copy button
    if (!roomCode.empty()) {
        char roomStr[32];
        snprintf(roomStr, sizeof(roomStr), "ROOM: %s", roomCode.c_str());
        int codeW = MeasureText(roomStr, 12);
        Rectangle codeRect = {cx - codeW * 0.5f - 18.0f, 52.0f, static_cast<float>(codeW + 36), 22.0f};

        DrawRectangleRounded(codeRect, 0.3f, 4, Color{15, 23, 42, 220});
        DrawRectangleRoundedLines(codeRect, 0.3f, 4, 1.5f, Color{56, 189, 248, 160});
        DrawText(roomStr, static_cast<int>(cx - codeW * 0.5f - 8.0f), 56, 12, Color{224, 242, 254, 255});

        // Copy icon / text
        DrawText("[C]", static_cast<int>(codeRect.x + codeRect.width - 20.0f), 56, 11, Color{56, 189, 248, 255});

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), codeRect)) {
            copyClicked = true;
            copyFeedbackTimer = 2.0f;
            SetClipboardText(roomCode.c_str());
        }

        if (copyFeedbackTimer > 0.0f) {
            copyFeedbackTimer -= GetFrameTime();
            DrawText("Copied!", static_cast<int>(cx + codeW * 0.5f + 24.0f), 56, 12, Color{74, 222, 128, 255});
        }
    }
}

void Renderer::drawTopRightControls(int pingMs, bool isMuted, int screenWidth, HUDClickResult& result) {
    float rightX = screenWidth - 20.0f;
    Vector2 mouse = GetMousePosition();
    bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    // 1. Leave Button
    Rectangle leaveRect = {rightX - 58.0f, 16.0f, 58.0f, 26.0f};
    DrawRectangleRounded(leaveRect, 0.3f, 4, Color{220, 38, 38, 230});
    DrawText("Leave", static_cast<int>(leaveRect.x + 11.0f), static_cast<int>(leaveRect.y + 6.0f), 13, WHITE);
    if (clicked && CheckCollisionPointRec(mouse, leaveRect)) {
        result.leaveClicked = true;
    }

    // 2. -Bot Button
    Rectangle remBotRect = {leaveRect.x - 52.0f, 16.0f, 46.0f, 26.0f};
    DrawRectangleRounded(remBotRect, 0.3f, 4, Color{234, 88, 12, 230});
    DrawText("- Bot", static_cast<int>(remBotRect.x + 7.0f), static_cast<int>(remBotRect.y + 6.0f), 13, WHITE);
    if (clicked && CheckCollisionPointRec(mouse, remBotRect)) {
        result.removeBotClicked = true;
    }

    // 3. +Bot Button
    Rectangle addBotRect = {remBotRect.x - 52.0f, 16.0f, 46.0f, 26.0f};
    DrawRectangleRounded(addBotRect, 0.3f, 4, Color{22, 163, 74, 230});
    DrawText("+ Bot", static_cast<int>(addBotRect.x + 6.0f), static_cast<int>(addBotRect.y + 6.0f), 13, WHITE);
    if (clicked && CheckCollisionPointRec(mouse, addBotRect)) {
        result.addBotClicked = true;
    }

    // 4. Fullscreen Button
    Rectangle fsRect = {addBotRect.x - 36.0f, 16.0f, 30.0f, 26.0f};
    DrawRectangleRounded(fsRect, 0.3f, 4, Color{30, 41, 59, 230});
    DrawRectangleRoundedLines(fsRect, 0.3f, 4, 1.2f, Color{71, 85, 105, 200});
    DrawText("[]", static_cast<int>(fsRect.x + 9.0f), static_cast<int>(fsRect.y + 6.0f), 13, Color{148, 163, 184, 255});
    if (clicked && CheckCollisionPointRec(mouse, fsRect)) {
        result.fullscreenClicked = true;
    }

    // 5. Mute Button
    Rectangle muteRect = {fsRect.x - 36.0f, 16.0f, 30.0f, 26.0f};
    DrawRectangleRounded(muteRect, 0.3f, 4, isMuted ? Color{239, 68, 68, 230} : Color{30, 41, 59, 230});
    DrawRectangleRoundedLines(muteRect, 0.3f, 4, 1.2f, isMuted ? Color{239, 68, 68, 255} : Color{71, 85, 105, 200});
    DrawText(isMuted ? "X" : "S", static_cast<int>(muteRect.x + 10.0f), static_cast<int>(muteRect.y + 6.0f), 13, WHITE);
    if (clicked && CheckCollisionPointRec(mouse, muteRect)) {
        result.muteClicked = true;
    }

    // 6. Ping indicator badge
    char pingStr[16];
    snprintf(pingStr, sizeof(pingStr), "%dms", (pingMs > 0 ? pingMs : 1));
    DrawText(pingStr, static_cast<int>(muteRect.x - 52.0f), 22, 13, Color{74, 222, 128, 255});
}

void Renderer::drawKillFeed(const std::deque<KillFeedEntry>& killFeed, int screenWidth) {
    float startX = screenWidth - 20.0f;
    float startY = 56.0f;

    for (size_t i = 0; i < killFeed.size(); ++i) {
        const auto& item = killFeed[i];
        float alpha = std::min(1.0f, item.timeRemaining);

        char feedText[128];
        snprintf(feedText, sizeof(feedText), "%s killed %s", item.killerName.c_str(), item.victimName.c_str());
        int tw = MeasureText(feedText, 12);

        Rectangle box = {startX - tw - 16.0f, startY + i * 24.0f, static_cast<float>(tw + 16), 20.0f};
        Color boxCol = Color{15, 23, 42, static_cast<unsigned char>(180 * alpha)};
        Color textCol = Color{248, 250, 252, static_cast<unsigned char>(255 * alpha)};

        DrawRectangleRounded(box, 0.3f, 4, boxCol);
        DrawText(feedText, static_cast<int>(box.x + 8.0f), static_cast<int>(box.y + 4.0f), 12, textCol);
    }
}
