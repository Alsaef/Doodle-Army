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
    // Basic initialization if needed
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

    // Attempt to load character and weapon png textures if present
    const char* textureNames[] = {
        "weapon_rifle.png",
        "weapon_smg_clean.png",
        "weapon_shotgun.png",
        "weapon_sniper.png",
        "weapon_pistol.png",
        "char_simon.png",
        "char_raphina.png"
    };

    for (const char* texName : textureNames) {
        std::string fullPath = assetsPath + "/" + texName;
        if (FileExists(fullPath.c_str())) {
            Texture2D tex = LoadTexture(fullPath.c_str());
            SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
            textures[texName] = tex;
        }
    }

    texturesLoaded = true;
}

void Renderer::renderWorld(const GameWorld& world, Camera2D camera, const std::string& localPlayerId) {
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
    Color glassCrackColor = Color{147, 197, 253, 90}; // Light blue glass crack
    Color crackGlow = Color{191, 219, 254, 140};

    const int rayCount = 12;
    for (int i = 0; i < rayCount; ++i) {
        float angle = static_cast<float>(i) * (2.0f * PI / rayCount);
        float r1 = 60.0f + (i % 3) * 20.0f;
        float r2 = 280.0f + (i % 4) * 40.0f;

        Vector2 p1 = {cx + cosf(angle) * r1, cy + sinf(angle) * r1};
        Vector2 p2 = {cx + cosf(angle + 0.1f) * r2, cy + sinf(angle + 0.1f) * r2};

        DrawLineEx(p1, p2, 1.8f, glassCrackColor);

        // Sub-cracks
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

    // Outer Zero-G low-gravity ambient zone
    float pulse = sinf(time * 2.0f) * 15.0f;
    DrawCircleGradient(static_cast<int>(corePos.x), static_cast<int>(corePos.y),
                       380.0f + pulse, Color{14, 165, 233, 45}, Color{14, 165, 233, 0});

    // Glowing core orb
    DrawCircleGradient(static_cast<int>(corePos.x), static_cast<int>(corePos.y),
                       coreRadius + pulse * 0.4f, Color{186, 230, 253, 200}, Color{2, 132, 199, 30});
    DrawCircle(static_cast<int>(corePos.x), static_cast<int>(corePos.y), coreRadius * 0.45f, WHITE);

    // Orbiting Zero-G energy rings (Matches screenshot)
    for (int r = 0; r < 2; ++r) {
        float rotAngle = time * (r == 0 ? 1.2f : -0.9f) + r * 1.57f;
        Color ringColor = Color{56, 189, 248, 190};

        // Draw rotated ellipse ring via segments
        const int segs = 36;
        Vector2 prevPt = {0, 0};
        for (int i = 0; i <= segs; ++i) {
            float th = static_cast<float>(i) * (2.0f * PI / segs);
            float ex = cosf(th) * (coreRadius * 1.6f);
            float ey = sinf(th) * (coreRadius * 0.55f);

            // Rotate by rotAngle
            float rx = ex * cosf(rotAngle) - ey * sinf(rotAngle);
            float ry = ex * sinf(rotAngle) + ey * cosf(rotAngle);
            Vector2 curPt = {corePos.x + rx, corePos.y + ry};

            if (i > 0) {
                DrawLineEx(prevPt, curPt, 2.8f, ringColor);
            }
            prevPt = curPt;
        }
    }

    // Electrical discharge arcs
    for (int a = 0; a < 3; ++a) {
        float arcAngle = time * 3.0f + a * 2.09f;
        Vector2 start = corePos;
        Vector2 mid = {corePos.x + cosf(arcAngle) * (coreRadius * 1.2f),
                       corePos.y + sinf(arcAngle) * (coreRadius * 0.9f)};
        Vector2 end = {corePos.x + cosf(arcAngle + 0.3f) * (coreRadius * 1.9f),
                      corePos.y + sinf(arcAngle + 0.3f) * (coreRadius * 1.7f)};

        DrawLineEx(start, mid, 2.0f, Color{125, 211, 252, 220});
        DrawLineEx(mid, end, 1.5f, Color{224, 242, 254, 255});
    }
}

void Renderer::drawPlatforms(const GameWorld& world) {
    for (const auto& plat : world.getPlatforms()) {
        drawPlatform(plat);
    }
}

void Renderer::drawPlatform(const PlatformData& plat) {
    if (plat.type == "crate") {
        // Floating Wooden Crate (Rotated by plat.angle)
        rlPushMatrix();
        rlTranslatef(plat.x + plat.w * 0.5f, plat.y + plat.h * 0.5f, 0.0f);
        rlRotatef(plat.angle * RAD2DEG, 0, 0, 1);

        Rectangle localRect = {-plat.w * 0.5f, -plat.h * 0.5f, plat.w, plat.h};

        // Orange/brown wood base
        DrawRectangleRec(localRect, Color{217, 119, 6, 255});
        // Darker wood border
        DrawRectangleLinesEx(localRect, 3.0f, Color{146, 64, 14, 255});
        // Wood cross brace (X)
        DrawLineEx({localRect.x, localRect.y},
                   {localRect.x + localRect.width, localRect.y + localRect.height},
                   3.0f, Color{180, 83, 9, 255});
        DrawLineEx({localRect.x + localRect.width, localRect.y},
                   {localRect.x, localRect.y + localRect.height},
                   3.0f, Color{180, 83, 9, 255});

        // Corner metal brackets
        DrawRectangle(static_cast<int>(localRect.x), static_cast<int>(localRect.y), 8, 8, Color{100, 116, 139, 255});
        DrawRectangle(static_cast<int>(localRect.x + localRect.width - 8), static_cast<int>(localRect.y), 8, 8, Color{100, 116, 139, 255});
        DrawRectangle(static_cast<int>(localRect.x), static_cast<int>(localRect.y + localRect.height - 8), 8, 8, Color{100, 116, 139, 255});
        DrawRectangle(static_cast<int>(localRect.x + localRect.width - 8), static_cast<int>(localRect.y + localRect.height - 8), 8, 8, Color{100, 116, 139, 255});

        rlPopMatrix();
        return;
    }

    if (plat.type == "rock" || plat.type == "ground") {
        // Dark rocky cavern boundaries
        DrawRectangle(static_cast<int>(plat.x), static_cast<int>(plat.y),
                      static_cast<int>(plat.w), static_cast<int>(plat.h), Color{30, 41, 59, 255});
        DrawRectangleLinesEx({plat.x, plat.y, plat.w, plat.h}, 2.0f, Color{51, 65, 85, 255});
        return;
    }

    // Industrial Sci-Fi Metal Platforms (Matching Screenshot)
    // 1. Dark steel platform body
    Rectangle bodyRect = {plat.x, plat.y, plat.w, plat.h};
    DrawRectangleRec(bodyRect, Color{30, 41, 59, 255});

    // 2. Cyan top walking edge / railing
    DrawLineEx({plat.x, plat.y}, {plat.x + plat.w, plat.y}, 3.0f, Color{56, 189, 248, 255});

    // 3. Yellow and Black Hazard Caution Stripes on Left and Right corners
    float hazardWidth = 32.0f;
    // Left hazard block
    DrawRectangle(static_cast<int>(plat.x), static_cast<int>(plat.y),
                  static_cast<int>(hazardWidth), static_cast<int>(plat.h), Color{234, 179, 8, 255});
    for (int s = 0; s < 3; ++s) {
        DrawLineEx({plat.x + s * 10.0f, plat.y},
                   {plat.x + s * 10.0f + 10.0f, plat.y + plat.h}, 3.0f, Color{15, 23, 42, 255});
    }

    // Right hazard block
    DrawRectangle(static_cast<int>(plat.x + plat.w - hazardWidth), static_cast<int>(plat.y),
                  static_cast<int>(hazardWidth), static_cast<int>(plat.h), Color{234, 179, 8, 255});
    for (int s = 0; s < 3; ++s) {
        DrawLineEx({plat.x + plat.w - hazardWidth + s * 10.0f, plat.y},
                   {plat.x + plat.w - hazardWidth + s * 10.0f + 10.0f, plat.y + plat.h}, 3.0f, Color{15, 23, 42, 255});
    }

    // Platform border outline
    DrawRectangleLinesEx(bodyRect, 2.0f, Color{71, 85, 105, 255});

    // 4. Underside metal beam trusses
    float beamSpacing = 60.0f;
    for (float bx = plat.x + hazardWidth + 10.0f; bx < plat.x + plat.w - hazardWidth - 20.0f; bx += beamSpacing) {
        DrawTriangle(
            {bx, plat.y + plat.h},
            {bx + 16.0f, plat.y + plat.h + 14.0f},
            {bx + 32.0f, plat.y + plat.h},
            Color{30, 41, 59, 180}
        );
        DrawLineEx({bx, plat.y + plat.h}, {bx + 16.0f, plat.y + plat.h + 14.0f}, 1.5f, Color{51, 65, 85, 220});
        DrawLineEx({bx + 16.0f, plat.y + plat.h + 14.0f}, {bx + 32.0f, plat.y + plat.h}, 1.5f, Color{51, 65, 85, 220});
    }
}

void Renderer::drawPickups(const GameWorld& world) {
    for (const auto& pk : world.getPickups()) {
        drawPickup(pk);
    }
}

void Renderer::drawPickup(const PickupData& pk) {
    if (!pk.active) return;

    float drawY = pk.y + pk.floatOffset;
    Rectangle box = {pk.x - 18.0f, drawY - 18.0f, 36.0f, 36.0f};

    // Dark semi-transparent card background
    DrawRectangleRounded(box, 0.3f, 6, Color{15, 23, 42, 200});

    // Neon glowing border based on pickup type
    Color borderColor = WHITE;
    if (pk.type == "HEALTH") borderColor = Color{239, 68, 68, 255};
    else if (pk.type == "FUEL") borderColor = Color{56, 189, 248, 255};
    else if (pk.type == "AMMO") borderColor = Color{234, 179, 8, 255};
    else if (pk.type == "GRENADE") borderColor = Color{34, 197, 94, 255};
    else if (pk.type == "LASER") borderColor = Color{192, 132, 252, 255};
    else borderColor = Color{251, 146, 60, 255};

    DrawRectangleRoundedLines(box, 0.3f, 6, 2.0f, borderColor);

    // Icon rendering inside pickup box
    if (pk.type == "HEALTH") {
        DrawCircle(static_cast<int>(pk.x), static_cast<int>(drawY), 6.0f, Color{239, 68, 68, 255});
        DrawText("+", static_cast<int>(pk.x) - 4, static_cast<int>(drawY) - 7, 14, WHITE);
    } else if (pk.type == "FUEL") {
        DrawText("L", static_cast<int>(pk.x) - 4, static_cast<int>(drawY) - 7, 14, Color{56, 189, 248, 255});
    } else if (pk.type == "GRENADE") {
        DrawCircle(static_cast<int>(pk.x), static_cast<int>(drawY) + 2, 7.0f, Color{34, 197, 94, 255});
        DrawRectangle(static_cast<int>(pk.x) - 2, static_cast<int>(drawY) - 6, 4, 4, LIGHTGRAY);
    } else {
        // Weapon pickup
        DrawText(pk.type.substr(0, 3).c_str(), static_cast<int>(pk.x) - 10, static_cast<int>(drawY) - 5, 10, borderColor);
    }
}

void Renderer::drawProjectiles(const GameWorld& world) {
    // 1. Bullets (Tracers)
    for (const auto& b : world.getBullets()) {
        Color bCol = Color{251, 146, 60, 255};
        if (b.weaponType == "SMG") bCol = Color{250, 204, 21, 255};
        else if (b.weaponType == "SNIPER") bCol = Color{56, 189, 248, 255};
        else if (b.weaponType == "LASER") bCol = Color{192, 132, 252, 255};
        else if (b.weaponType == "SHOTGUN") bCol = Color{249, 115, 22, 255};

        float speed = sqrtf(b.vx * b.vx + b.vy * b.vy);
        Vector2 dir = (speed > 1.0f) ? Vector2{b.vx / speed, b.vy / speed} : Vector2{1.0f, 0.0f};

        Vector2 tip = {b.x, b.y};
        Vector2 tail = {b.x - dir.x * b.length, b.y - dir.y * b.length};

        // Tracer beam
        DrawLineEx(tail, tip, (b.weaponType == "SNIPER" || b.weaponType == "LASER") ? 4.0f : 2.5f, bCol);
        DrawCircleV(tip, 2.5f, WHITE);
    }

    // 2. Grenades
    for (const auto& g : world.getGrenades()) {
        // Olive green grenade body
        DrawCircle(static_cast<int>(g.x), static_cast<int>(g.y), 7.0f, Color{65, 87, 44, 255});
        DrawCircleLines(static_cast<int>(g.x), static_cast<int>(g.y), 7.0f, Color{40, 56, 28, 255});

        // Blinking red fuse LED
        bool blink = (static_cast<int>(g.fuse / 150) % 2 == 0);
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
        // Expanding fiery ring
        float alpha = 1.0f - (exp.timer / exp.duration);
        Color shockColor = Color{251, 146, 60, static_cast<unsigned char>(255 * alpha)};
        DrawCircleLines(static_cast<int>(exp.pos.x), static_cast<int>(exp.pos.y), exp.radius, shockColor);
        DrawCircleLines(static_cast<int>(exp.pos.x), static_cast<int>(exp.pos.y), exp.radius - 2.0f, shockColor);

        // Core flash
        if (exp.timer < 0.15f) {
            DrawCircle(static_cast<int>(exp.pos.x), static_cast<int>(exp.pos.y), exp.radius * 0.4f, WHITE);
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
    int facing = p.facing;

    // 1. Backpack Jetpack (Behind soldier body)
    Vector2 jetpackPos = {px - static_cast<float>(facing) * 12.0f, py - 4.0f};
    DrawRectangleRounded({jetpackPos.x - 7.0f, jetpackPos.y - 14.0f, 14.0f, 26.0f}, 0.4f, 6, Color{51, 65, 85, 255});
    DrawRectangleRoundedLines({jetpackPos.x - 7.0f, jetpackPos.y - 14.0f, 14.0f, 26.0f}, 0.4f, 6, 2.0f, Color{100, 116, 139, 255});

    // Jetpack nozzle
    Vector2 nozzle = {jetpackPos.x, jetpackPos.y + 13.0f};
    DrawRectangle(static_cast<int>(nozzle.x - 3.0f), static_cast<int>(nozzle.y), 6, 4, Color{30, 41, 59, 255});

    // Thrust flame when flying
    if (p.isFlying) {
        float flameH = 12.0f + static_cast<float>(rand() % 8);
        DrawTriangle(
            {nozzle.x - 5.0f, nozzle.y + 4.0f},
            {nozzle.x + 5.0f, nozzle.y + 4.0f},
            {nozzle.x, nozzle.y + 4.0f + flameH},
            Color{249, 115, 22, 255}
        );
        DrawTriangle(
            {nozzle.x - 2.5f, nozzle.y + 4.0f},
            {nozzle.x + 2.5f, nozzle.y + 4.0f},
            {nozzle.x, nozzle.y + 4.0f + flameH * 0.5f},
            WHITE
        );
    }

    // 2. Legs & Combat Boots (Animated walk cycle)
    float legOffset = sinf(p.walkCycle) * 6.0f;
    Color pantsColor = Color{71, 85, 105, 255}; // Tactical slate pants
    Color bootColor = Color{15, 23, 42, 255};

    // Left leg
    DrawRectangle(static_cast<int>(px - 8.0f), static_cast<int>(py + 10.0f + legOffset), 6, 14, pantsColor);
    DrawRectangle(static_cast<int>(px - 10.0f), static_cast<int>(py + 22.0f + legOffset), 9, 5, bootColor);

    // Right leg
    DrawRectangle(static_cast<int>(px + 2.0f), static_cast<int>(py + 10.0f - legOffset), 6, 14, pantsColor);
    DrawRectangle(static_cast<int>(px + 1.0f), static_cast<int>(py + 22.0f - legOffset), 9, 5, bootColor);

    // 3. Body Torso (Tactical camo/armor vest)
    Color vestColor = (p.avatarStyle == "simon") ? Color{226, 232, 240, 255} : Color{57, 81, 48, 255}; // Simon: astronaut white; Others: tactical olive
    DrawRectangleRounded({px - 14.0f, py - 10.0f, 28.0f, 22.0f}, 0.3f, 6, vestColor);
    DrawRectangleRoundedLines({px - 14.0f, py - 10.0f, 28.0f, 22.0f}, 0.3f, 6, 2.0f, Color{30, 41, 59, 255});

    // 4. Head & Tactical Helmet / Visor
    Vector2 headPos = {px, py - 20.0f};
    // Helmet
    Color helmetColor = (p.avatarStyle == "simon") ? Color{248, 250, 252, 255} : Color{77, 107, 65, 255};
    DrawCircleV(headPos, 14.0f, helmetColor);
    DrawCircleLines(static_cast<int>(headPos.x), static_cast<int>(headPos.y), 14.0f, Color{30, 41, 59, 255});

    // Goggles / Visor (Aimed towards facing direction)
    float visorX = headPos.x + static_cast<float>(facing) * 5.0f;
    DrawRectangleRounded({visorX - 8.0f, headPos.y - 4.0f, 16.0f, 8.0f}, 0.5f, 4, Color{15, 23, 42, 255});
    DrawRectangleRoundedLines({visorX - 8.0f, headPos.y - 4.0f, 16.0f, 8.0f}, 0.5f, 4, 1.5f,
                              (p.avatarStyle == "simon") ? Color{56, 189, 248, 255} : Color{250, 204, 21, 255});

    // 5. 360-degree Rotating Weapon Arm
    Vector2 shoulder = {px + static_cast<float>(facing) * 2.0f, py - 4.0f};
    drawWeaponInHand(shoulder, p.aimAngle, p.weapon, facing, p.reloading);

    // 6. In-World Floating HUD (Nametag & Mini Health Bar)
    float tagY = py - 46.0f;
    const char* displayName = p.name.c_str();
    int nameW = MeasureText(displayName, 12);
    Rectangle tagRect = {px - nameW * 0.5f - 6.0f, tagY, static_cast<float>(nameW + 12), 16.0f};

    // Nametag background
    Color tagBg = isLocal ? Color{37, 99, 235, 200} : (p.isBot ? Color{55, 65, 81, 200} : Color{15, 23, 42, 200});
    DrawRectangleRounded(tagRect, 0.4f, 6, tagBg);
    DrawText(displayName, static_cast<int>(px - nameW * 0.5f), static_cast<int>(tagY + 2), 12, WHITE);

    // Mini Health Bar above nametag
    float barW = 44.0f;
    float barH = 5.0f;
    Rectangle hpBarBg = {px - barW * 0.5f, tagY - 8.0f, barW, barH};
    DrawRectangleRec(hpBarBg, Color{15, 23, 42, 220});

    float healthRatio = std::max(0.0f, std::min(1.0f, static_cast<float>(p.health) / 100.0f));
    Color hpCol = (healthRatio > 0.5f) ? Color{34, 197, 94, 255} : ((healthRatio > 0.25f) ? Color{234, 179, 8, 255} : Color{239, 68, 68, 255});
    DrawRectangle(static_cast<int>(hpBarBg.x), static_cast<int>(hpBarBg.y),
                  static_cast<int>(barW * healthRatio), static_cast<int>(barH), hpCol);
    DrawRectangleLinesEx(hpBarBg, 1.0f, Color{51, 65, 85, 255});

    // Reloading indicator
    if (p.reloading) {
        DrawText("RELOADING...", static_cast<int>(px - 34.0f), static_cast<int>(tagY - 22.0f), 10, Color{250, 204, 21, 255});
    }
}

void Renderer::drawWeaponInHand(Vector2 handPos, float aimAngle, const std::string& weaponType, int facing, bool reloading) {
    rlPushMatrix();
    rlTranslatef(handPos.x, handPos.y, 0.0f);
    rlRotatef(aimAngle * RAD2DEG, 0, 0, 1);

    if (cosf(aimAngle) < 0.0f) {
        rlScalef(1.0f, -1.0f, 1.0f); // Invert vertically when aiming left
    }

    // Check if we have loaded weapon texture
    std::string texKey = "weapon_rifle.png";
    if (weaponType == "SMG") texKey = "weapon_smg_clean.png";
    else if (weaponType == "SHOTGUN") texKey = "weapon_shotgun.png";
    else if (weaponType == "SNIPER") texKey = "weapon_sniper.png";
    else if (weaponType == "PISTOL") texKey = "weapon_pistol.png";

    auto it = textures.find(texKey);
    if (it != textures.end()) {
        Texture2D tex = it->second;
        Rectangle src = {0, 0, static_cast<float>(tex.width), static_cast<float>(tex.height)};
        Rectangle dest = {4.0f, -10.0f, 38.0f, 20.0f};
        DrawTexturePro(tex, src, dest, {0, 0}, 0.0f, WHITE);
    } else {
        // High quality procedural weapon drawing fallback
        Color gunMetal = Color{30, 41, 59, 255};
        Color gunHighlight = Color{71, 85, 105, 255};

        if (weaponType == "SNIPER") {
            // Long barrel
            DrawRectangle(0, -3, 36, 6, gunMetal);
            DrawRectangle(2, -2, 34, 4, gunHighlight);
            // Scope
            DrawRectangle(10, -8, 14, 4, Color{15, 23, 42, 255});
            DrawLine(10, -6, 24, -6, Color{56, 189, 248, 255});
        } else if (weaponType == "SHOTGUN") {
            // Heavy pump barrel
            DrawRectangle(0, -4, 28, 8, gunMetal);
            DrawRectangle(8, 0, 10, 5, Color{217, 119, 6, 255}); // Wooden pump
        } else if (weaponType == "SMG") {
            // Compact SMG
            DrawRectangle(0, -4, 20, 8, gunMetal);
            DrawRectangle(10, 4, 4, 10, Color{15, 23, 42, 255}); // Mag
        } else if (weaponType == "LASER") {
            // Sci-fi emitter
            DrawRectangle(0, -4, 30, 8, Color{76, 29, 149, 255});
            DrawRectangle(6, -2, 20, 4, Color{192, 132, 252, 255});
        } else {
            // Assault Rifle default
            DrawRectangle(0, -3, 26, 6, gunMetal);
            DrawRectangle(8, 3, 5, 8, Color{15, 23, 42, 255}); // Mag
        }
    }

    rlPopMatrix();
}

void Renderer::renderHUD(const GameWorld& world, const std::string& localPlayerId,
                         const std::string& roomCode, int pingMs, int screenWidth, int screenHeight) {
    const PlayerData* localPlayer = world.getLocalPlayer(localPlayerId);

    // 1. Top-Left Player Status Card (HP, Fuel, Ammo)
    drawPlayerStatusCard(localPlayer, screenWidth, screenHeight);

    // 2. Top-Center Match Timer & Room Code
    drawTopCenterInfo(world.getMatchDuration(), roomCode, screenWidth);

    // 3. Top-Right Buttons (Ping, Fullscreen, Leave)
    drawTopRightControls(pingMs, screenWidth);

    // 4. Top-Right Live Kill Feed
    drawKillFeed(world.getKillFeed(), screenWidth);

    // 5. Desktop Controls Legend Footer (at bottom left)
    DrawRectangleRounded({20.0f, screenHeight - 34.0f, 480.0f, 22.0f}, 0.3f, 4, Color{15, 23, 42, 160});
    DrawText("A/D Move | W/Space Jetpack | L-Click Shoot | G/R-Click Grenade | R Reload",
             30, screenHeight - 30, 11, Color{148, 163, 184, 220});
}

void Renderer::drawPlayerStatusCard(const PlayerData* localPlayer, int screenWidth, int screenHeight) {
    int hp = localPlayer ? localPlayer->health : 100;
    int fuel = localPlayer ? localPlayer->fuel : 100;
    std::string weaponName = localPlayer ? localPlayer->weapon : "RIFLE";
    int ammo = localPlayer ? localPlayer->ammo : 30;
    int grenades = localPlayer ? localPlayer->grenades : 2;

    float cardX = 20.0f;
    float cardY = 16.0f;

    // --- HP BAR ---
    // Circular Heart Icon
    DrawCircle(static_cast<int>(cardX + 16.0f), static_cast<int>(cardY + 16.0f), 16.0f, Color{239, 68, 68, 255});
    DrawText("+", static_cast<int>(cardX + 9.0f), static_cast<int>(cardY + 7.0f), 20, WHITE);

    // Green HP Bar
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
    // Circular Lightning Icon
    DrawCircle(static_cast<int>(cardX + 16.0f), static_cast<int>(fuelY + 16.0f), 16.0f, Color{14, 165, 233, 255});
    DrawText("F", static_cast<int>(cardX + 10.0f), static_cast<int>(fuelY + 8.0f), 18, WHITE);

    // Cyan/Blue Fuel Bar
    Rectangle fuelBarRect = {cardX + 38.0f, fuelY + 5.0f, 220.0f, 22.0f};
    DrawRectangleRounded(fuelBarRect, 0.3f, 6, Color{15, 23, 42, 220});
    float fuelRatio = std::max(0.0f, std::min(1.0f, static_cast<float>(fuel) / 100.0f));
    Rectangle fuelFill = {fuelBarRect.x, fuelBarRect.y, fuelBarRect.width * fuelRatio, fuelBarRect.height};
    DrawRectangleRounded(fuelFill, 0.3f, 6, Color{56, 189, 248, 255});
    DrawRectangleRoundedLines(fuelBarRect, 0.3f, 6, 2.0f, Color{71, 85, 105, 200});

    char fuelText[32];
    snprintf(fuelText, sizeof(fuelText), "FUEL: %d%%", fuel);
    DrawText(fuelText, static_cast<int>(fuelBarRect.x + 8.0f), static_cast<int>(fuelBarRect.y + 4.0f), 13, WHITE);

    // --- WEAPON & AMMO STATUS CARD ---
    float wepY = fuelY + 38.0f;
    Rectangle wepCardRect = {cardX, wepY, 200.0f, 54.0f};
    DrawRectangleRounded(wepCardRect, 0.25f, 6, Color{15, 23, 42, 220});
    DrawRectangleRoundedLines(wepCardRect, 0.25f, 6, 2.0f, Color{71, 85, 105, 200});

    DrawText(weaponName.c_str(), static_cast<int>(cardX + 12.0f), static_cast<int>(wepY + 8.0f), 11, Color{148, 163, 184, 255});

    char ammoText[32];
    snprintf(ammoText, sizeof(ammoText), "AMMO: %d", ammo);
    DrawText(ammoText, static_cast<int>(cardX + 12.0f), static_cast<int>(wepY + 26.0f), 16, Color{250, 204, 21, 255});

    // Grenade Counter icon
    DrawCircle(static_cast<int>(cardX + 140.0f), static_cast<int>(wepY + 34.0f), 6.0f, Color{34, 197, 94, 255});
    char grenadeText[16];
    snprintf(grenadeText, sizeof(grenadeText), "x%d", grenades);
    DrawText(grenadeText, static_cast<int>(cardX + 152.0f), static_cast<int>(wepY + 28.0f), 13, WHITE);
}

void Renderer::drawTopCenterInfo(int matchDuration, const std::string& roomCode, int screenWidth) {
    float cx = screenWidth * 0.5f;

    // Timer Pill (Matches "2:19" in screenshot)
    int mins = matchDuration / 60;
    int secs = matchDuration % 60;
    char timerStr[16];
    snprintf(timerStr, sizeof(timerStr), "%d:%02d", mins, secs);

    Rectangle timerRect = {cx - 45.0f, 16.0f, 90.0f, 30.0f};
    DrawRectangleRounded(timerRect, 0.4f, 6, Color{15, 23, 42, 220});
    DrawRectangleRoundedLines(timerRect, 0.4f, 6, 2.0f, Color{71, 85, 105, 200});
    DrawText(timerStr, static_cast<int>(cx - 18.0f), 22, 18, WHITE);

    // Room Code Badge (Matches "ROOM: 43J3")
    if (!roomCode.empty()) {
        char roomStr[32];
        snprintf(roomStr, sizeof(roomStr), "ROOM: %s", roomCode.c_str());
        int codeW = MeasureText(roomStr, 12);
        Rectangle codeRect = {cx - codeW * 0.5f - 8.0f, 52.0f, static_cast<float>(codeW + 16), 20.0f};
        DrawRectangleRounded(codeRect, 0.3f, 4, Color{15, 23, 42, 200});
        DrawRectangleRoundedLines(codeRect, 0.3f, 4, 1.5f, Color{56, 189, 248, 160});
        DrawText(roomStr, static_cast<int>(cx - codeW * 0.5f), 56, 12, Color{224, 242, 254, 255});
    }
}

void Renderer::drawTopRightControls(int pingMs, int screenWidth) {
    float rightX = screenWidth - 20.0f;

    // 1. Leave Button
    Rectangle leaveRect = {rightX - 56.0f, 16.0f, 56.0f, 26.0f};
    DrawRectangleRounded(leaveRect, 0.3f, 4, Color{220, 38, 38, 220});
    DrawText("Leave", static_cast<int>(leaveRect.x + 10.0f), static_cast<int>(leaveRect.y + 6.0f), 13, WHITE);

    // 2. Ping indicator
    char pingStr[16];
    snprintf(pingStr, sizeof(pingStr), "%dms", (pingMs > 0 ? pingMs : 1));
    DrawText(pingStr, static_cast<int>(rightX - 120.0f), 22, 13, Color{74, 222, 128, 255});
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
