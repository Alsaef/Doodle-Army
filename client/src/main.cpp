#include "raylib.h"
#include "raymath.h"
#include "TouchControls.hpp"
#include "NetworkManager.hpp"
#include "GameWorld.hpp"
#include "Renderer.hpp"
#include "AudioManager.hpp"
#include <string>
#include <cstring>
#include <memory>
#include <iostream>
#include <algorithm>

#define RAYGUI_IMPLEMENTATION
#if __has_include(<raygui.h>)
#include <raygui.h>
#elif __has_include("raygui.h")
#include "raygui.h"
#endif

enum GameState {
    STATE_LOBBY = 0,
    STATE_CONNECTING,
    STATE_PLAYING
};

struct AvatarMeta {
    const char* id;
    const char* name;
    const char* line1;
    const char* line2;
    const char* desc;
    Color color;
};

const AvatarMeta AVATARS[5] = {
    {"soldier", "Tactical Soldier", "Tactical", "Soldier", "Camo helmet, night-vision goggles, combat armor", Color{77, 124, 15, 255}},
    {"scout", "Outpost Scout", "Outpost", "Scout", "Spiky hair, aviator goggles, orange neck scarf", Color{234, 88, 12, 255}},
    {"veteran", "Space Veteran", "Space", "Veteran", "Bearded commander, officer coat, tactical goggles", Color{180, 83, 9, 255}},
    {"raphile", "Raphile", "Raphile", "", "Cyber-horned helmet, dual cross-bandolier, stealth hoodie", Color{234, 179, 8, 255}},
    {"simon", "Simon", "Simon", "", "Long hair & goatee, yellow tactical vest, armored greaves", Color{245, 158, 11, 255}}
};

static Texture2D loadTextureCandidate(const std::string& name) {
    std::string searchPaths[] = {
        name,
        "assets/characters/" + name,
        "client/assets/characters/" + name,
        "characters/" + name,
        "assets/" + name,
        "client/assets/" + name
    };

    for (const auto& path : searchPaths) {
        if (FileExists(path.c_str())) {
            Image img = LoadImage(path.c_str());
            if (img.data != nullptr) {
                ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
                Color* pixels = (Color*)img.data;
                int total = img.width * img.height;
                for (int p = 0; p < total; ++p) {
                    if (pixels[p].r > 240 && pixels[p].g > 240 && pixels[p].b > 240) {
                        pixels[p] = BLANK;
                    }
                }
                Texture2D tex = LoadTextureFromImage(img);
                SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
                UnloadImage(img);
                return tex;
            }
        }
    }
    return Texture2D{0};
}

static void drawAvatarBadge(const AvatarMeta& meta, Texture2D tex, float cx, float cy, float radius) {
    Color bgCol = meta.color;
    bgCol.a = 45;
    DrawCircle(static_cast<int>(cx), static_cast<int>(cy), radius, bgCol);
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), radius, meta.color);

    if (tex.id != 0) {
        float size = radius * 1.55f;
        Rectangle src = {0.0f, 0.0f, static_cast<float>(tex.width), static_cast<float>(tex.height)};
        Rectangle dst = {cx, cy, size, size};
        Vector2 origin = {size * 0.5f, size * 0.5f};
        DrawTexturePro(tex, src, dst, origin, 0.0f, WHITE);
    } else {
        DrawCircle(static_cast<int>(cx), static_cast<int>(cy), radius * 0.55f, meta.color);
    }
}

int main(int argc, char* argv[]) {
    // 1. Initialize Raylib Window
    const int screenWidth = 1280;
    const int screenHeight = 720;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(screenWidth, screenHeight, "Doodle Army - Anti-Gravity Outpost");
    SetTargetFPS(60);

    // 2. Initialize Subsystems
    TouchControls touchControls;
    touchControls.init(screenWidth, screenHeight);

    NetworkManager network;
    GameWorld world;
    Renderer renderer;
    renderer.init();
    renderer.loadAssetTextures("assets");

    AudioManager audio;
    audio.init("assets");

    // Load character textures for lobby
    Texture2D avatarTextures[5];
    avatarTextures[0] = loadTextureCandidate("soldier.png");
    avatarTextures[1] = loadTextureCandidate("scout.png");
    avatarTextures[2] = loadTextureCandidate("veteran.png");
    avatarTextures[3] = loadTextureCandidate("raphina.png");
    avatarTextures[4] = loadTextureCandidate("simon.png");

    // Camera setup for tracking player
    Camera2D camera = {0};
    camera.target = {1200.0f, 650.0f};
    camera.offset = {screenWidth * 0.5f, screenHeight * 0.5f};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    // Game state variables
    GameState currentState = STATE_LOBBY;
    std::string statusMessage = "";
    float pingTimer = 0.0f;
    float connectingTimer = 0.0f;
    float shootCooldownTimer = 0.0f;
    float grenadeCooldownTimer = 0.0f;

    // Lobby Inputs
    char nicknameBuffer[64];
    snprintf(nicknameBuffer, sizeof(nicknameBuffer), "SOLDIER_%d", GetRandomValue(100, 899));
    char serverUrlBuffer[128] = "wss://game-engine-867g.onrender.com";
    char joinCodeBuffer[16] = "";

    bool showCreateModal = false;
    bool showJoinModal = false;
    bool showHelpModal = false;
    int initialBotCount = 0; // 0 bots default for clean matches with friends
    int activeInputBox = 0; // 0 = none, 1 = nickname, 2 = join code
    float cursorBlinkTimer = 0.0f;
    int selectedAvatarIndex = 0;

    // 3. Pre-connect to Render backend immediately in background on startup
    network.connect(serverUrlBuffer);

    // 4. Register Network Event Callbacks
    network.setOnWelcome([&](const std::string& clientId) {
        std::cout << "[Client] Welcome received! Assigned ID: " << clientId << std::endl;
    });

    network.setOnRoomCreated([&](const std::string& code, const std::string& playerId, bool isHost) {
        std::cout << "[Client] Room created: " << code << std::endl;
        currentState = STATE_PLAYING;
        statusMessage.clear();
    });

    network.setOnRoomJoined([&](const std::string& code, const std::string& playerId, bool isHost) {
        std::cout << "[Client] Joined room: " << code << std::endl;
        currentState = STATE_PLAYING;
        statusMessage.clear();
    });

    network.setOnGameState([&](const void* payloadJson) {
        world.updateFromSnapshot(payloadJson);
        // Instant state transition (matches client_web behavior):
        if (currentState == STATE_CONNECTING) {
            currentState = STATE_PLAYING;
            statusMessage.clear();
        }
    });

    network.setOnKillFeed([&](const std::string& killer, const std::string& victim, const std::string& weapon) {
        world.addKillFeed(killer, victim, weapon);
        audio.playKill();
    });

    network.setOnHitImpact([&](float x, float y, int damage, bool isHeadshot) {
        world.addHitImpact(x, y, damage, isHeadshot);
        audio.playHit();
    });

    network.setOnExplosion([&](float x, float y, float radius) {
        world.addExplosion(x, y, radius);
        audio.playExplosion();
    });

    network.setOnError([&](const std::string& err) {
        if (currentState == STATE_PLAYING) {
            statusMessage = "Error: " + err;
            currentState = STATE_LOBBY;
        } else if (currentState == STATE_CONNECTING) {
            statusMessage = "Connecting to server (waking up Render backend)...";
        } else {
            statusMessage = "Error: " + err;
        }
    });

    network.setOnDisconnect([&]() {
        if (currentState == STATE_PLAYING) {
            statusMessage = "Disconnected from server.";
            currentState = STATE_LOBBY;
        }
    });

    // Main Game Loop
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        int curWidth = GetScreenWidth();
        int curHeight = GetScreenHeight();

        // Update network messages
        network.pollMessages();

        // -------------------------------------------------------------
        // State: LOBBY MENU (Matching Screenshots 1:1)
        // -------------------------------------------------------------
        if (currentState == STATE_LOBBY || currentState == STATE_CONNECTING) {
            if (currentState == STATE_CONNECTING) {
                connectingTimer += dt;
                int secondsElapsed = static_cast<int>(connectingTimer);
                if (secondsElapsed > 4) {
                    statusMessage = TextFormat("Connecting to server (%ds - waking up Render backend)...", secondsElapsed);
                }
            } else {
                connectingTimer = 0.0f;
            }

            BeginDrawing();
            ClearBackground(Color{6, 10, 18, 255});

            // Ambient background stars & outpost preview
            world.update(dt, "");
            renderer.renderWorld(world, camera, "");

            // Dark sci-fi backdrop overlay
            DrawRectangle(0, 0, curWidth, curHeight, Color{6, 10, 18, 215});

            // Glowing central nebula pulse
            float timeSec = static_cast<float>(GetTime());
            float nebulaPulse = 20.0f * sinf(timeSec * 1.5f);
            DrawCircleGradient(curWidth / 2, curHeight / 2, 260.0f + nebulaPulse,
                               Color{14, 165, 233, 20}, BLANK);

            Vector2 mouse = GetMousePosition();

            // Top-Right Quick Controls (Fullscreen + Sound)
            float trY = 24.0f;
            float trX = curWidth - 84.0f;
            Rectangle fsBtnRect = {trX, trY, 36.0f, 36.0f};
            Rectangle soundBtnRect = {trX + 44.0f, trY, 36.0f, 36.0f};

            bool fsHover = CheckCollisionPointRec(mouse, fsBtnRect);
            DrawRectangleRounded(fsBtnRect, 0.5f, 6, fsHover ? Color{30, 41, 59, 255} : Color{15, 23, 42, 230});
            DrawRectangleRoundedLines(fsBtnRect, 0.5f, 6, 1.5f, Color{71, 85, 105, 255});
            // Maximize / Fullscreen icon lines
            DrawLineEx({fsBtnRect.x + 10, fsBtnRect.y + 10}, {fsBtnRect.x + 16, fsBtnRect.y + 10}, 2.0f, Color{251, 191, 36, 255});
            DrawLineEx({fsBtnRect.x + 10, fsBtnRect.y + 10}, {fsBtnRect.x + 10, fsBtnRect.y + 16}, 2.0f, Color{251, 191, 36, 255});
            DrawLineEx({fsBtnRect.x + 26, fsBtnRect.y + 26}, {fsBtnRect.x + 20, fsBtnRect.y + 26}, 2.0f, Color{251, 191, 36, 255});
            DrawLineEx({fsBtnRect.x + 26, fsBtnRect.y + 26}, {fsBtnRect.x + 26, fsBtnRect.y + 20}, 2.0f, Color{251, 191, 36, 255});

            bool sndHover = CheckCollisionPointRec(mouse, soundBtnRect);
            DrawRectangleRounded(soundBtnRect, 0.5f, 6, sndHover ? Color{30, 41, 59, 255} : Color{15, 23, 42, 230});
            DrawRectangleRoundedLines(soundBtnRect, 0.5f, 6, 1.5f, Color{71, 85, 105, 255});
            // Speaker Icon
            Color sndIconColor = audio.isMuted() ? Color{239, 68, 68, 255} : Color{56, 189, 248, 255};
            DrawTriangle({soundBtnRect.x + 12, soundBtnRect.y + 18},
                         {soundBtnRect.x + 18, soundBtnRect.y + 12},
                         {soundBtnRect.x + 18, soundBtnRect.y + 24}, sndIconColor);
            DrawRectangle(static_cast<int>(soundBtnRect.x + 9), static_cast<int>(soundBtnRect.y + 15), 5, 6, sndIconColor);
            if (!audio.isMuted()) {
                DrawCircleSector({soundBtnRect.x + 18, soundBtnRect.y + 18}, 8.0f, -40, 40, 4, sndIconColor);
            } else {
                DrawLineEx({soundBtnRect.x + 22, soundBtnRect.y + 14}, {soundBtnRect.x + 28, soundBtnRect.y + 22}, 2.0f, Color{239, 68, 68, 255});
                DrawLineEx({soundBtnRect.x + 28, soundBtnRect.y + 14}, {soundBtnRect.x + 22, soundBtnRect.y + 22}, 2.0f, Color{239, 68, 68, 255});
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (fsHover) {
                    audio.playClick();
                    ToggleFullscreen();
                } else if (sndHover) {
                    audio.toggleMute();
                    if (!audio.isMuted()) audio.playClick();
                }
            }

            // ---------------------------------------------------------
            // Main Lobby Panel Card
            // ---------------------------------------------------------
            float panelW = 520.0f;
            float panelH = 585.0f;
            float panelX = (curWidth - panelW) * 0.5f;
            float panelY = (curHeight - panelH) * 0.5f;

            Rectangle panelRect = {panelX, panelY, panelW, panelH};
            DrawRectangleRounded(panelRect, 0.06f, 8, Color{15, 23, 42, 235});
            DrawRectangleRoundedLines(panelRect, 0.06f, 8, 2.0f, Color{51, 65, 85, 210});

            // 1. Header: Pill Badge
            float topY = panelY + 18.0f;
            float badgeW = 260.0f;
            float badgeH = 22.0f;
            Rectangle badgeRect = {panelX + (panelW - badgeW) * 0.5f, topY, badgeW, badgeH};
            DrawRectangleRounded(badgeRect, 0.5f, 6, Color{8, 47, 73, 220});
            DrawRectangleRoundedLines(badgeRect, 0.5f, 6, 1.2f, Color{14, 165, 233, 160});

            // Lightning Bolt symbol
            DrawTriangle({badgeRect.x + 18.0f, badgeRect.y + 5.0f},
                         {badgeRect.x + 12.0f, badgeRect.y + 12.0f},
                         {badgeRect.x + 17.0f, badgeRect.y + 12.0f}, Color{56, 189, 248, 255});
            DrawTriangle({badgeRect.x + 15.0f, badgeRect.y + 11.0f},
                         {badgeRect.x + 20.0f, badgeRect.y + 11.0f},
                         {badgeRect.x + 14.0f, badgeRect.y + 18.0f}, Color{56, 189, 248, 255});
            DrawText("2D REAL-TIME MULTIPLAYER SHOOTER", static_cast<int>(badgeRect.x + 26.0f), static_cast<int>(badgeRect.y + 5.0f), 10, Color{56, 189, 248, 255});

            // 2. Main Title: DOODLE ARMY
            topY += 32.0f;
            const char* title1 = "DOODLE ";
            const char* title2 = "ARMY";
            int title1W = MeasureText(title1, 38);
            int title2W = MeasureText(title2, 38);
            int totalTitleW = title1W + title2W;
            float titleStartX = panelX + (panelW - totalTitleW) * 0.5f;

            DrawText(title1, static_cast<int>(titleStartX), static_cast<int>(topY), 38, Color{56, 189, 248, 255});
            DrawText(title2, static_cast<int>(titleStartX + title1W), static_cast<int>(topY), 38, Color{249, 115, 22, 255});

            // Subtitle: ANTI-GRAVITY OUTPOST
            topY += 42.0f;
            const char* subTitle = "ANTI-GRAVITY OUTPOST";
            int subW = MeasureText(subTitle, 15);
            DrawText(subTitle, static_cast<int>(panelX + (panelW - subW) * 0.5f), static_cast<int>(topY), 15, Color{203, 213, 225, 255});

            // Live Server Pill
            topY += 26.0f;
            float serverPillW = 280.0f;
            float serverPillH = 20.0f;
            Rectangle serverPillRect = {panelX + (panelW - serverPillW) * 0.5f, topY, serverPillW, serverPillH};
            DrawRectangleRounded(serverPillRect, 0.5f, 6, Color{6, 78, 59, 180});
            DrawRectangleRoundedLines(serverPillRect, 0.5f, 6, 1.0f, Color{16, 185, 129, 140});

            // Pulsing dot
            float dotAlpha = 0.5f + 0.5f * sinf(timeSec * 4.0f);
            Color dotCol = Color{52, 211, 153, static_cast<unsigned char>(255 * dotAlpha)};
            DrawCircle(static_cast<int>(serverPillRect.x + 14.0f), static_cast<int>(serverPillRect.y + 10.0f), 3.5f, dotCol);
            DrawText("Live Server: game-engine-867g.onrender.com", static_cast<int>(serverPillRect.x + 24.0f), static_cast<int>(serverPillRect.y + 4.0f), 10, Color{110, 231, 183, 255});

            // ---------------------------------------------------------
            // Inputs & Customization Card Box
            // ---------------------------------------------------------
            topY += 34.0f;
            float formW = panelW - 48.0f;
            float formX = panelX + 24.0f;

            cursorBlinkTimer += dt;
            bool showCursor = (static_cast<int>(cursorBlinkTimer * 2.0f) % 2 == 0);

            // Handle typing into active input box
            int charKey = GetCharPressed();
            while (charKey > 0) {
                if (charKey >= 32 && charKey <= 126) {
                    if (activeInputBox == 1 && strlen(nicknameBuffer) < 16) {
                        size_t len = strlen(nicknameBuffer);
                        nicknameBuffer[len] = static_cast<char>(charKey);
                        nicknameBuffer[len + 1] = '\0';
                    } else if (activeInputBox == 2 && strlen(joinCodeBuffer) < 4) {
                        size_t len = strlen(joinCodeBuffer);
                        joinCodeBuffer[len] = static_cast<char>(toupper(charKey));
                        joinCodeBuffer[len + 1] = '\0';
                    }
                }
                charKey = GetCharPressed();
            }

            if (IsKeyPressed(KEY_BACKSPACE)) {
                if (activeInputBox == 1 && strlen(nicknameBuffer) > 0) {
                    nicknameBuffer[strlen(nicknameBuffer) - 1] = '\0';
                } else if (activeInputBox == 2 && strlen(joinCodeBuffer) > 0) {
                    joinCodeBuffer[strlen(joinCodeBuffer) - 1] = '\0';
                }
            }

            // 1. Soldier Codename
            DrawText("SOLDIER CODENAME", static_cast<int>(formX), static_cast<int>(topY), 11, Color{148, 163, 184, 255});
            DrawText("MAX 16 CHARS", static_cast<int>(formX + formW - 80.0f), static_cast<int>(topY), 10, Color{100, 116, 139, 255});

            topY += 16.0f;
            Rectangle nickRect = {formX, topY, formW, 36.0f};
            DrawRectangleRounded(nickRect, 0.2f, 4, Color{15, 23, 42, 255});
            DrawRectangleRoundedLines(nickRect, 0.2f, 4, 1.8f, (activeInputBox == 1 ? Color{56, 189, 248, 255} : Color{51, 65, 85, 255}));
            std::string nickDisplay = nicknameBuffer + std::string((activeInputBox == 1 && showCursor) ? "|" : "");
            DrawText(nickDisplay.c_str(), static_cast<int>(nickRect.x + 12.0f), static_cast<int>(nickRect.y + 10.0f), 15, WHITE);

            // 2. Select Character Class (5 cards)
            topY += 50.0f;
            DrawText("SELECT CHARACTER CLASS", static_cast<int>(formX), static_cast<int>(topY), 11, Color{148, 163, 184, 255});

            topY += 18.0f;
            float cardGap = 8.0f;
            float cardW = (formW - 4.0f * cardGap) / 5.0f;
            float cardH = 86.0f;

            for (int i = 0; i < 5; ++i) {
                float cardX = formX + i * (cardW + cardGap);
                Rectangle cardRect = {cardX, topY, cardW, cardH};
                bool isSelected = (selectedAvatarIndex == i);
                bool isHovered = CheckCollisionPointRec(mouse, cardRect);

                Color cardBg = isSelected ? Color{12, 74, 110, 190} : (isHovered ? Color{30, 41, 59, 200} : Color{15, 23, 42, 255});
                DrawRectangleRounded(cardRect, 0.16f, 6, cardBg);

                if (isSelected) {
                    DrawRectangleRoundedLines(cardRect, 0.16f, 6, 2.2f, Color{56, 189, 248, 255});
                } else {
                    DrawRectangleRoundedLines(cardRect, 0.16f, 6, 1.2f, Color{30, 41, 59, 255});
                }

                // Circular avatar badge
                float badgeCx = cardX + cardW * 0.5f;
                float badgeCy = topY + 26.0f;
                drawAvatarBadge(AVATARS[i], avatarTextures[i], badgeCx, badgeCy, 17.0f);

                // Two lines of text
                int l1W = MeasureText(AVATARS[i].line1, 11);
                DrawText(AVATARS[i].line1, static_cast<int>(cardX + (cardW - l1W) * 0.5f), static_cast<int>(topY + 50.0f), 11, WHITE);

                if (strlen(AVATARS[i].line2) > 0) {
                    int l2W = MeasureText(AVATARS[i].line2, 10);
                    DrawText(AVATARS[i].line2, static_cast<int>(cardX + (cardW - l2W) * 0.5f), static_cast<int>(topY + 64.0f), 10, Color{203, 213, 225, 255});
                }

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && isHovered) {
                    audio.playClick();
                    selectedAvatarIndex = i;
                }
            }

            // 3. Selected Character Info Box
            topY += cardH + 10.0f;
            Rectangle infoRect = {formX, topY, formW, 52.0f};
            DrawRectangleRounded(infoRect, 0.18f, 6, Color{15, 23, 42, 220});
            DrawRectangleRoundedLines(infoRect, 0.18f, 6, 1.2f, Color{30, 41, 59, 255});

            const AvatarMeta& curAv = AVATARS[selectedAvatarIndex];
            // Thumbnail on left
            drawAvatarBadge(curAv, avatarTextures[selectedAvatarIndex], infoRect.x + 28.0f, infoRect.y + 26.0f, 18.0f);

            // Name + ACTIVE pill
            float infoTextX = infoRect.x + 56.0f;
            DrawText(TextToUpper(curAv.name), static_cast<int>(infoTextX), static_cast<int>(infoRect.y + 10.0f), 13, WHITE);

            int nameW = MeasureText(TextToUpper(curAv.name), 13);
            Rectangle activePill = {infoTextX + nameW + 8.0f, infoRect.y + 10.0f, 44.0f, 16.0f};
            DrawRectangleRounded(activePill, 0.3f, 4, Color{8, 47, 73, 255});
            DrawRectangleRoundedLines(activePill, 0.3f, 4, 1.0f, Color{14, 165, 233, 200});
            DrawText("ACTIVE", static_cast<int>(activePill.x + 5.0f), static_cast<int>(activePill.y + 3.0f), 9, Color{56, 189, 248, 255});

            // Description
            DrawText(curAv.desc, static_cast<int>(infoTextX), static_cast<int>(infoRect.y + 28.0f), 11, Color{148, 163, 184, 255});

            // ---------------------------------------------------------
            // Action Buttons
            // ---------------------------------------------------------
            topY += 64.0f;

            // 1. Quick Play Button (Emerald)
            Rectangle qpRect = {formX, topY, formW, 44.0f};
            bool qpHover = CheckCollisionPointRec(mouse, qpRect);
            DrawRectangleRounded(qpRect, 0.22f, 6, qpHover ? Color{52, 211, 153, 255} : Color{16, 185, 129, 255});

            // Play Triangle icon
            DrawTriangle({qpRect.x + formW * 0.5f - 140.0f, qpRect.y + 14.0f},
                         {qpRect.x + formW * 0.5f - 140.0f, qpRect.y + 30.0f},
                         {qpRect.x + formW * 0.5f - 124.0f, qpRect.y + 22.0f}, WHITE);

            const char* qpText = "QUICK PLAY (SOLO WITH BOTS)";
            int qpTextW = MeasureText(qpText, 17);
            DrawText(qpText, static_cast<int>(qpRect.x + (formW - qpTextW) * 0.5f + 8.0f), static_cast<int>(qpRect.y + 13.0f), 17, WHITE);

            // 2. Create Room & Join Room Buttons (Row)
            topY += 52.0f;
            float btnHalfW = (formW - 10.0f) * 0.5f;
            Rectangle crRect = {formX, topY, btnHalfW, 38.0f};
            Rectangle jrRect = {formX + btnHalfW + 10.0f, topY, btnHalfW, 38.0f};

            bool crHover = CheckCollisionPointRec(mouse, crRect);
            DrawRectangleRounded(crRect, 0.22f, 6, crHover ? Color{14, 165, 233, 255} : Color{2, 132, 199, 255});
            // Plus symbol
            DrawLineEx({crRect.x + 36.0f, crRect.y + 19.0f}, {crRect.x + 46.0f, crRect.y + 19.0f}, 2.5f, WHITE);
            DrawLineEx({crRect.x + 41.0f, crRect.y + 14.0f}, {crRect.x + 41.0f, crRect.y + 24.0f}, 2.5f, WHITE);
            DrawText("CREATE ROOM", static_cast<int>(crRect.x + 54.0f), static_cast<int>(crRect.y + 11.0f), 13, WHITE);

            bool jrHover = CheckCollisionPointRec(mouse, jrRect);
            DrawRectangleRounded(jrRect, 0.22f, 6, jrHover ? Color{51, 65, 85, 255} : Color{30, 41, 59, 255});
            DrawRectangleRoundedLines(jrRect, 0.22f, 6, 1.5f, Color{71, 85, 105, 255});
            // Users symbol
            DrawCircle(static_cast<int>(jrRect.x + 36.0f), static_cast<int>(jrRect.y + 17.0f), 4.0f, WHITE);
            DrawCircle(static_cast<int>(jrRect.x + 44.0f), static_cast<int>(jrRect.y + 17.0f), 3.5f, Color{203, 213, 225, 255});
            DrawText("JOIN ROOM", static_cast<int>(jrRect.x + 58.0f), static_cast<int>(jrRect.y + 11.0f), 13, WHITE);

            // ---------------------------------------------------------
            // Footer: Engine Info & Controls Guide Link
            // ---------------------------------------------------------
            topY += 46.0f;
            DrawLineEx({formX, topY}, {formX + formW, topY}, 1.0f, Color{30, 41, 59, 255});

            topY += 8.0f;
            DrawText("Authoritative 60 FPS Physics Engine", static_cast<int>(formX), static_cast<int>(topY), 11, Color{100, 116, 139, 255});

            const char* helpText = "? Controls & Weapons Guide";
            int helpW = MeasureText(helpText, 11);
            Rectangle helpRect = {formX + formW - helpW - 6.0f, topY - 2.0f, static_cast<float>(helpW + 8), 18.0f};
            bool helpHover = CheckCollisionPointRec(mouse, helpRect);
            DrawText(helpText, static_cast<int>(helpRect.x), static_cast<int>(topY), 11, helpHover ? Color{56, 189, 248, 255} : Color{148, 163, 184, 255});

            // ---------------------------------------------------------
            // Mouse Click Handling for Main Lobby
            // ---------------------------------------------------------
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !showCreateModal && !showJoinModal && !showHelpModal) {
                if (CheckCollisionPointRec(mouse, nickRect)) {
                    activeInputBox = 1;
                } else {
                    activeInputBox = 0;
                }

                if (qpHover) {
                    audio.playClick();
                    world.reset();
                    if (!network.isConnected() || network.getCurrentUrl() != serverUrlBuffer) {
                        network.connect(serverUrlBuffer);
                    }
                    network.quickPlay(nicknameBuffer, AVATARS[selectedAvatarIndex].id);
                    currentState = STATE_CONNECTING;
                    connectingTimer = 0.0f;
                    statusMessage = "Connecting to Quick Play...";
                } else if (crHover) {
                    audio.playClick();
                    showCreateModal = true;
                } else if (jrHover) {
                    audio.playClick();
                    showJoinModal = true;
                    activeInputBox = 2;
                } else if (helpHover) {
                    audio.playClick();
                    showHelpModal = true;
                }
            }

            // Status message
            if (!statusMessage.empty()) {
                DrawText(statusMessage.c_str(), static_cast<int>(panelX + 40.0f), static_cast<int>(panelY + panelH + 14.0f), 13, Color{250, 204, 21, 255});
            }

            // Connecting status and cancel button
            if (currentState == STATE_CONNECTING) {
                Rectangle cancelConnectRect = {panelX + (panelW - 180.0f) * 0.5f, panelY + panelH + 36.0f, 180.0f, 32.0f};
                DrawRectangleRounded(cancelConnectRect, 0.25f, 6, Color{239, 68, 68, 220});
                DrawText("CANCEL", static_cast<int>(cancelConnectRect.x + 60.0f), static_cast<int>(cancelConnectRect.y + 8.0f), 14, WHITE);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, cancelConnectRect)) {
                    audio.playClick();
                    network.leaveRoom();
                    currentState = STATE_LOBBY;
                    statusMessage = "Connection cancelled.";
                }
            }

            // ---------------------------------------------------------
            // CREATE ROOM MODAL
            // ---------------------------------------------------------
            if (showCreateModal) {
                DrawRectangle(0, 0, curWidth, curHeight, Color{0, 0, 0, 180});
                float mw = 420.0f;
                float mh = 310.0f;
                Rectangle modalRect = {(curWidth - mw) * 0.5f, (curHeight - mh) * 0.5f, mw, mh};
                DrawRectangleRounded(modalRect, 0.12f, 8, Color{15, 23, 42, 255});
                DrawRectangleRoundedLines(modalRect, 0.12f, 8, 2.0f, Color{56, 189, 248, 255});

                // Title
                DrawText("+ CREATE OUTPOST ROOM", static_cast<int>(modalRect.x + 24.0f), static_cast<int>(modalRect.y + 20.0f), 18, Color{56, 189, 248, 255});

                // Close button (X)
                Rectangle closeRect = {modalRect.x + mw - 38.0f, modalRect.y + 16.0f, 24.0f, 24.0f};
                bool closeHover = CheckCollisionPointRec(mouse, closeRect);
                DrawText("X", static_cast<int>(closeRect.x + 5.0f), static_cast<int>(closeRect.y + 3.0f), 16, closeHover ? WHITE : Color{148, 163, 184, 255});

                // Host Summary
                Rectangle hostBox = {modalRect.x + 24.0f, modalRect.y + 54.0f, mw - 48.0f, 44.0f};
                DrawRectangleRounded(hostBox, 0.2f, 4, Color{30, 41, 59, 200});
                DrawText("Room Host:", static_cast<int>(hostBox.x + 12.0f), static_cast<int>(hostBox.y + 6.0f), 11, Color{148, 163, 184, 255});
                DrawText(nicknameBuffer, static_cast<int>(hostBox.x + 12.0f), static_cast<int>(hostBox.y + 20.0f), 15, WHITE);

                // Initial Bot Combatants
                DrawText("INITIAL BOT COMBATANTS", static_cast<int>(modalRect.x + 24.0f), static_cast<int>(modalRect.y + 112.0f), 11, Color{203, 213, 225, 255});
                DrawText("(0 bots for friend matches)", static_cast<int>(modalRect.x + 200.0f), static_cast<int>(modalRect.y + 112.0f), 10, Color{251, 191, 36, 255});

                float botBtnW = (mw - 48.0f - 3 * 8.0f) / 4.0f;
                for (int b = 0; b < 4; ++b) {
                    Rectangle bRect = {modalRect.x + 24.0f + b * (botBtnW + 8.0f), modalRect.y + 132.0f, botBtnW, 40.0f};
                    bool bSelected = (initialBotCount == b);
                    DrawRectangleRounded(bRect, 0.25f, 4, bSelected ? Color{2, 132, 199, 255} : Color{30, 41, 59, 255});
                    if (bSelected) {
                        DrawRectangleRoundedLines(bRect, 0.25f, 4, 1.8f, Color{56, 189, 248, 255});
                    }

                    const char* bLabel = (b == 0) ? "0 Bots" : (b == 1 ? "1 Bot" : (b == 2 ? "2 Bots" : "3 Bots"));
                    int bW = MeasureText(bLabel, 12);
                    DrawText(bLabel, static_cast<int>(bRect.x + (botBtnW - bW) * 0.5f), static_cast<int>(bRect.y + 13.0f), 12, WHITE);

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, bRect)) {
                        audio.playClick();
                        initialBotCount = b;
                    }
                }

                // Info note
                const char* note = (initialBotCount == 0)
                    ? "Clean room: No bots will spawn. Wait peacefully for friends!"
                    : "Spawns casual bots for immediate target practice.";
                DrawText(note, static_cast<int>(modalRect.x + 24.0f), static_cast<int>(modalRect.y + 184.0f), 11, Color{148, 163, 184, 255});

                // Action buttons: Cancel and Launch Room
                Rectangle cancelBtn = {modalRect.x + 24.0f, modalRect.y + mh - 56.0f, (mw - 58.0f) * 0.5f, 38.0f};
                Rectangle launchBtn = {modalRect.x + 24.0f + (mw - 58.0f) * 0.5f + 10.0f, modalRect.y + mh - 56.0f, (mw - 58.0f) * 0.5f, 38.0f};

                DrawRectangleRounded(cancelBtn, 0.22f, 4, Color{51, 65, 85, 255});
                DrawText("Cancel", static_cast<int>(cancelBtn.x + cancelBtn.width * 0.5f - 24.0f), static_cast<int>(cancelBtn.y + 11.0f), 14, Color{226, 232, 240, 255});

                DrawRectangleRounded(launchBtn, 0.22f, 4, Color{2, 132, 199, 255});
                DrawText("Launch Room", static_cast<int>(launchBtn.x + launchBtn.width * 0.5f - 46.0f), static_cast<int>(launchBtn.y + 11.0f), 14, WHITE);

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (CheckCollisionPointRec(mouse, closeRect) || CheckCollisionPointRec(mouse, cancelBtn)) {
                        audio.playClick();
                        showCreateModal = false;
                    } else if (CheckCollisionPointRec(mouse, launchBtn)) {
                        audio.playClick();
                        world.reset();
                        if (!network.isConnected() || network.getCurrentUrl() != serverUrlBuffer) {
                            network.connect(serverUrlBuffer);
                        }
                        network.createRoom(nicknameBuffer, AVATARS[selectedAvatarIndex].id, initialBotCount);
                        currentState = STATE_CONNECTING;
                        connectingTimer = 0.0f;
                        statusMessage = "Creating room on server...";
                        showCreateModal = false;
                    }
                }
            }

            // ---------------------------------------------------------
            // JOIN ROOM MODAL
            // ---------------------------------------------------------
            if (showJoinModal) {
                DrawRectangle(0, 0, curWidth, curHeight, Color{0, 0, 0, 180});
                float mw = 360.0f;
                float mh = 220.0f;
                Rectangle modalRect = {(curWidth - mw) * 0.5f, (curHeight - mh) * 0.5f, mw, mh};
                DrawRectangleRounded(modalRect, 0.15f, 8, Color{15, 23, 42, 255});
                DrawRectangleRoundedLines(modalRect, 0.15f, 8, 2.0f, Color{56, 189, 248, 255});

                DrawText("JOIN OUTPOST BATTLE", static_cast<int>(modalRect.x + 24.0f), static_cast<int>(modalRect.y + 20.0f), 16, WHITE);

                Rectangle closeRect = {modalRect.x + mw - 36.0f, modalRect.y + 16.0f, 22.0f, 22.0f};
                bool closeHover = CheckCollisionPointRec(mouse, closeRect);
                DrawText("X", static_cast<int>(closeRect.x + 4.0f), static_cast<int>(closeRect.y + 2.0f), 16, closeHover ? WHITE : Color{148, 163, 184, 255});

                DrawText("ENTER 4-LETTER ROOM CODE:", static_cast<int>(modalRect.x + 24.0f), static_cast<int>(modalRect.y + 54.0f), 11, Color{148, 163, 184, 255});

                Rectangle codeInputRect = {modalRect.x + 24.0f, modalRect.y + 74.0f, mw - 48.0f, 44.0f};
                DrawRectangleRounded(codeInputRect, 0.2f, 4, Color{15, 23, 42, 255});
                DrawRectangleRoundedLines(codeInputRect, 0.2f, 4, 1.8f, (activeInputBox == 2 ? Color{56, 189, 248, 255} : Color{71, 85, 105, 255}));

                std::string codeDisplay = (strlen(joinCodeBuffer) > 0 ? joinCodeBuffer : "e.g. K9X2") + std::string((activeInputBox == 2 && showCursor) ? "|" : "");
                int codeW = MeasureText(codeDisplay.c_str(), 20);
                DrawText(codeDisplay.c_str(), static_cast<int>(codeInputRect.x + (codeInputRect.width - codeW) * 0.5f),
                         static_cast<int>(codeInputRect.y + 12.0f), 20, (strlen(joinCodeBuffer) > 0 ? Color{56, 189, 248, 255} : Color{100, 116, 139, 255}));

                Rectangle cancelRect = {modalRect.x + 24.0f, modalRect.y + 144.0f, (mw - 58.0f) * 0.5f, 38.0f};
                Rectangle joinRect = {modalRect.x + 24.0f + (mw - 58.0f) * 0.5f + 10.0f, modalRect.y + 144.0f, (mw - 58.0f) * 0.5f, 38.0f};

                DrawRectangleRounded(cancelRect, 0.22f, 4, Color{51, 65, 85, 255});
                DrawText("Cancel", static_cast<int>(cancelRect.x + cancelRect.width * 0.5f - 24.0f), static_cast<int>(cancelRect.y + 11.0f), 14, Color{226, 232, 240, 255});

                DrawRectangleRounded(joinRect, 0.22f, 4, (strlen(joinCodeBuffer) > 0 ? Color{2, 132, 199, 255} : Color{30, 41, 59, 255}));
                DrawText("Join Match", static_cast<int>(joinRect.x + joinRect.width * 0.5f - 38.0f), static_cast<int>(joinRect.y + 11.0f), 14, WHITE);

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (CheckCollisionPointRec(mouse, codeInputRect)) {
                        activeInputBox = 2;
                    } else if (CheckCollisionPointRec(mouse, closeRect) || CheckCollisionPointRec(mouse, cancelRect)) {
                        audio.playClick();
                        showJoinModal = false;
                        activeInputBox = 0;
                    } else if (CheckCollisionPointRec(mouse, joinRect) && strlen(joinCodeBuffer) > 0) {
                        audio.playClick();
                        world.reset();
                        if (!network.isConnected() || network.getCurrentUrl() != serverUrlBuffer) {
                            network.connect(serverUrlBuffer);
                        }
                        network.joinRoom(joinCodeBuffer, nicknameBuffer, AVATARS[selectedAvatarIndex].id);
                        currentState = STATE_CONNECTING;
                        connectingTimer = 0.0f;
                        statusMessage = "Joining room " + std::string(joinCodeBuffer) + "...";
                        showJoinModal = false;
                    }
                }
            }

            // ---------------------------------------------------------
            // CONTROLS & WEAPONS GUIDE MODAL
            // ---------------------------------------------------------
            if (showHelpModal) {
                DrawRectangle(0, 0, curWidth, curHeight, Color{0, 0, 0, 185});
                float mw = 480.0f;
                float mh = 510.0f;
                Rectangle modalRect = {(curWidth - mw) * 0.5f, (curHeight - mh) * 0.5f, mw, mh};
                DrawRectangleRounded(modalRect, 0.1f, 8, Color{15, 23, 42, 255});
                DrawRectangleRoundedLines(modalRect, 0.1f, 8, 2.0f, Color{56, 189, 248, 255});

                DrawText("HOW TO PLAY & CONTROLS", static_cast<int>(modalRect.x + 24.0f), static_cast<int>(modalRect.y + 20.0f), 18, Color{56, 189, 248, 255});

                Rectangle closeRect = {modalRect.x + mw - 38.0f, modalRect.y + 18.0f, 22.0f, 22.0f};
                bool closeHover = CheckCollisionPointRec(mouse, closeRect);
                DrawText("X", static_cast<int>(closeRect.x + 4.0f), static_cast<int>(closeRect.y + 2.0f), 16, closeHover ? WHITE : Color{148, 163, 184, 255});

                float rowY = modalRect.y + 58.0f;
                const char* controls[][2] = {
                    {"Move Left / Right", "A / D or Left/Right"},
                    {"Jetpack Thrust", "W or Spacebar"},
                    {"Aim Weapon", "Mouse Cursor (360)"},
                    {"Fire Weapon", "Left Mouse Button"},
                    {"Throw Grenade", "G or Right Mouse"},
                    {"Reload Ammo", "R Key"},
                    {"Scoreboard", "Hold Tab"}
                };

                for (int r = 0; r < 7; ++r) {
                    DrawText(controls[r][0], static_cast<int>(modalRect.x + 24.0f), static_cast<int>(rowY), 12, Color{148, 163, 184, 255});
                    int valW = MeasureText(controls[r][1], 11);
                    Rectangle badgeR = {modalRect.x + mw - valW - 36.0f, rowY - 2.0f, static_cast<float>(valW + 12), 18.0f};
                    DrawRectangleRounded(badgeR, 0.3f, 4, Color{30, 41, 59, 255});
                    DrawText(controls[r][1], static_cast<int>(badgeR.x + 6.0f), static_cast<int>(rowY + 1.0f), 11, WHITE);
                    DrawLineEx({modalRect.x + 24.0f, rowY + 22.0f}, {modalRect.x + mw - 24.0f, rowY + 22.0f}, 1.0f, Color{30, 41, 59, 180});
                    rowY += 28.0f;
                }

                // Weapons & Combat Tips
                rowY += 6.0f;
                Rectangle tipsBox = {modalRect.x + 24.0f, rowY, mw - 48.0f, 130.0f};
                DrawRectangleRounded(tipsBox, 0.15f, 6, Color{8, 47, 73, 160});
                DrawRectangleRoundedLines(tipsBox, 0.15f, 6, 1.2f, Color{14, 165, 233, 140});

                DrawText("PRO COMBAT TIPS:", static_cast<int>(tipsBox.x + 12.0f), static_cast<int>(tipsBox.y + 10.0f), 12, Color{251, 191, 36, 255});
                DrawText("- Land on platforms to recharge your Jetpack fuel.", static_cast<int>(tipsBox.x + 12.0f), static_cast<int>(tipsBox.y + 30.0f), 11, Color{203, 213, 225, 255});
                DrawText("- Grab Rocket Launcher & Laser Rifle pickups for massive AoE damage.", static_cast<int>(tipsBox.x + 12.0f), static_cast<int>(tipsBox.y + 50.0f), 11, Color{203, 213, 225, 255});
                DrawText("- Central blue Anti-Gravity Core lowers gravity, letting you float!", static_cast<int>(tipsBox.x + 12.0f), static_cast<int>(tipsBox.y + 70.0f), 11, Color{203, 213, 225, 255});
                DrawText("- Headshots deal 1.75x critical damage.", static_cast<int>(tipsBox.x + 12.0f), static_cast<int>(tipsBox.y + 90.0f), 11, Color{203, 213, 225, 255});

                Rectangle okBtn = {modalRect.x + 24.0f, modalRect.y + mh - 50.0f, mw - 48.0f, 36.0f};
                DrawRectangleRounded(okBtn, 0.22f, 4, Color{30, 41, 59, 255});
                DrawRectangleRoundedLines(okBtn, 0.22f, 4, 1.5f, Color{71, 85, 105, 255});
                DrawText("GOT IT, SOLDIER!", static_cast<int>(okBtn.x + okBtn.width * 0.5f - 60.0f), static_cast<int>(okBtn.y + 10.0f), 14, WHITE);

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (CheckCollisionPointRec(mouse, closeRect) || CheckCollisionPointRec(mouse, okBtn)) {
                        audio.playClick();
                        showHelpModal = false;
                    }
                }
            }

            EndDrawing();
            continue;
        }

        // -------------------------------------------------------------
        // State: ACTIVE GAMEPLAY
        // -------------------------------------------------------------
        std::string localId = network.getLocalPlayerId();
        const PlayerData* localPlayer = world.getLocalPlayer(localId);

        // Compute local player screen position for mouse aiming calculation
        Vector2 playerScreenPos = {curWidth * 0.5f, curHeight * 0.5f};
        if (localPlayer) {
            playerScreenPos = GetWorldToScreen2D({localPlayer->renderX, localPlayer->renderY}, camera);
        }

        // Update virtual controls (touches on mobile, WASD+mouse on desktop)
        touchControls.update(curWidth, curHeight, playerScreenPos);
        const ControlInputs& inputs = touchControls.getInputs();

        // Instant local prediction for aim angle and facing (0ms latency response)
        PlayerData* localPlayerMutable = world.getLocalPlayer(localId);
        if (localPlayerMutable) {
            localPlayerMutable->aimAngle = inputs.aimAngle;
            localPlayerMutable->facing = (cosf(inputs.aimAngle) >= 0.0f) ? 1 : -1;
        }

        // Send local inputs to authoritative server (matching client_web payload structure)
        network.sendInput(inputs.left, inputs.right, inputs.up, inputs.jump, inputs.aimAngle, inputs.reload);

        // Firing and cooldown timers tracking
        if (shootCooldownTimer > 0.0f) {
            shootCooldownTimer -= dt;
        }
        if (grenadeCooldownTimer > 0.0f) {
            grenadeCooldownTimer -= dt;
        }

        if (inputs.shoot && localPlayer && !localPlayer->isDead) {
            // Cannot shoot while actively reloading
            if (localPlayer->reloading) {
                // Wait for reload
            } else if (localPlayer->ammo <= 0) {
                // Auto-trigger reload on empty chamber
                if (shootCooldownTimer <= 0.0f) {
                    audio.playReload();
                    network.sendReload();
                    shootCooldownTimer = 0.5f;
                }
            } else if (shootCooldownTimer <= 0.0f) {
                // Firing rate intervals matching server physics
                float interval = 0.140f; // RIFLE: ~7 shots/sec
                const std::string& w = localPlayer->weapon;
                if (w == "SMG") interval = 0.095f;        // SMG: ~10 shots/sec
                else if (w == "PISTOL") interval = 0.240f; // PISTOL: ~4 shots/sec
                else if (w == "SHOTGUN") interval = 0.650f;// SHOTGUN: heavy pump
                else if (w == "SNIPER") interval = 0.950f; // SNIPER: heavy bolt
                else if (w == "LASER") interval = 0.180f;  // LASER: pulse beam
                else if (w == "ROCKET") interval = 1.100f; // ROCKET: propelled launch

                shootCooldownTimer = interval;

                network.sendShoot(inputs.aimAngle);
                audio.playShoot(localPlayer->weapon);
            }
        }

        if (inputs.grenade && grenadeCooldownTimer <= 0.0f && localPlayer && !localPlayer->isDead) {
            if (localPlayer->grenades > 0) {
                network.sendGrenade(inputs.aimAngle);
                grenadeCooldownTimer = 0.8f;
            }
        }

        if (inputs.reload && localPlayer && !localPlayer->reloading) {
            network.sendReload();
            audio.playReload();
        }

        // Update continuous jetpack sound
        audio.updateJetpack(localPlayer ? localPlayer->isFlying : false);

        // Periodic Ping update every 1 second
        pingTimer += dt;
        if (pingTimer >= 1.0f) {
            pingTimer = 0.0f;
            network.sendPing();
        }

        // Smooth Camera2D Tracking of Local Player
        if (localPlayer) {
            Vector2 targetPos = {localPlayer->renderX, localPlayer->renderY};
            camera.target = Vector2Lerp(camera.target, targetPos, 0.12f);
            camera.offset = {curWidth * 0.5f, curHeight * 0.5f};
        }

        // Update GameWorld particles, interpolations, and animations
        world.update(dt, localId);

        // -------------------------------------------------------------
        // Render Pipeline
        // -------------------------------------------------------------
        BeginDrawing();
        ClearBackground(Color{7, 10, 24, 255});

        // 1. World Layer (Space map, platforms, core, soldiers, bullets, explosions)
        renderer.renderWorld(world, camera, localId);

        // 2. HUD Layer (Health bar, fuel bar, weapon ammo, timer, room code, kill feed)
        HUDClickResult hudClicks = renderer.renderHUD(world, localId, network.getRoomCode(),
                                                     network.getPingMs(), audio.isMuted(),
                                                     curWidth, curHeight);
        if (hudClicks.leaveClicked) {
            audio.playClick();
            network.leaveRoom();
            currentState = STATE_LOBBY;
            statusMessage.clear();
        }
        if (hudClicks.muteClicked) {
            audio.toggleMute();
            if (!audio.isMuted()) audio.playClick();
        }
        if (hudClicks.fullscreenClicked) {
            audio.playClick();
            ToggleFullscreen();
        }
        if (hudClicks.addBotClicked) {
            audio.playClick();
            network.sendAddBot();
        }
        if (hudClicks.removeBotClicked) {
            audio.playClick();
            network.sendRemoveBot();
        }
        if (hudClicks.copyCodeClicked) {
            audio.playClick();
        }

        // 3. Virtual Mobile Touch Controls Overlay
        touchControls.draw(curWidth, curHeight);

        EndDrawing();
    }

    // Cleanup
    for (int i = 0; i < 5; ++i) {
        if (avatarTextures[i].id != 0) {
            UnloadTexture(avatarTextures[i]);
        }
    }
    network.disconnect();
    audio.unload();
    renderer.unload();
    CloseWindow();
    return 0;
}
