#include "raylib.h"
#include "raymath.h"
#include "TouchControls.hpp"
#include "NetworkManager.hpp"
#include "GameWorld.hpp"
#include "Renderer.hpp"
#include <string>
#include <cstring>
#include <memory>
#include <iostream>

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

int main(int argc, char* argv[]) {
    // 1. Initialize Raylib Window
    const int screenWidth = 1280;
    const int screenHeight = 720;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(screenWidth, screenHeight, "Anti-Gravity Outpost - 2D Battle Royale");
    SetTargetFPS(60);

    // 2. Initialize Subsystems
    TouchControls touchControls;
    touchControls.init(screenWidth, screenHeight);

    NetworkManager network;
    GameWorld world;
    Renderer renderer;
    renderer.init();
    renderer.loadAssetTextures("assets");

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

    // Lobby Inputs
    char nicknameBuffer[64] = "Soldier";
    char serverUrlBuffer[128] = "wss://game-engine-867g.onrender.com";
    char joinCodeBuffer[16] = "";
    bool showJoinModal = false;
    int activeInputBox = 0; // 0 = none, 1 = nickname, 2 = url, 3 = join code
    float cursorBlinkTimer = 0.0f;

    const char* avatarOptions[] = {"soldier", "scout", "simon", "raphile"};
    int selectedAvatarIndex = 0;

    // 3. Register Network Event Callbacks
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
    });

    network.setOnKillFeed([&](const std::string& killer, const std::string& victim, const std::string& weapon) {
        world.addKillFeed(killer, victim, weapon);
    });

    network.setOnHitImpact([&](float x, float y, int damage, bool isHeadshot) {
        world.addHitImpact(x, y, damage, isHeadshot);
    });

    network.setOnExplosion([&](float x, float y, float radius) {
        world.addExplosion(x, y, radius);
    });

    network.setOnError([&](const std::string& err) {
        statusMessage = "Error: " + err;
        currentState = STATE_LOBBY;
    });

    network.setOnDisconnect([&]() {
        statusMessage = "Disconnected from server.";
        currentState = STATE_LOBBY;
    });

    // Main Game Loop
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        int curWidth = GetScreenWidth();
        int curHeight = GetScreenHeight();

        // Update network messages
        network.pollMessages();

        // -------------------------------------------------------------
        // State: LOBBY MENU
        // -------------------------------------------------------------
        if (currentState == STATE_LOBBY || currentState == STATE_CONNECTING) {
            BeginDrawing();
            ClearBackground(Color{15, 23, 42, 255});

            // Ambient background stars
            world.update(dt, "");
            renderer.renderWorld(world, camera, "");

            // Dark sci-fi overlay
            DrawRectangle(0, 0, curWidth, curHeight, Color{10, 15, 30, 210});

            // Title Banner
            float panelW = 460.0f;
            float panelH = 480.0f;
            float panelX = (curWidth - panelW) * 0.5f;
            float panelY = (curHeight - panelH) * 0.5f;

            Rectangle panelRect = {panelX, panelY, panelW, panelH};
            DrawRectangleRounded(panelRect, 0.15f, 8, Color{15, 23, 42, 240});
            DrawRectangleRoundedLines(panelRect, 0.15f, 8, 2.5f, Color{56, 189, 248, 200});

            // Title Text
            DrawText("ANTI-GRAVITY OUTPOST", static_cast<int>(panelX + 48.0f), static_cast<int>(panelY + 28.0f), 28, Color{56, 189, 248, 255});
            DrawText("Real-Time 2D Multiplayer Shooter", static_cast<int>(panelX + 110.0f), static_cast<int>(panelY + 64.0f), 13, Color{148, 163, 184, 255});

            float formY = panelY + 110.0f;

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
                    } else if (activeInputBox == 2 && strlen(serverUrlBuffer) < 120) {
                        size_t len = strlen(serverUrlBuffer);
                        serverUrlBuffer[len] = static_cast<char>(charKey);
                        serverUrlBuffer[len + 1] = '\0';
                    } else if (activeInputBox == 3 && strlen(joinCodeBuffer) < 4) {
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
                } else if (activeInputBox == 2 && strlen(serverUrlBuffer) > 0) {
                    serverUrlBuffer[strlen(serverUrlBuffer) - 1] = '\0';
                } else if (activeInputBox == 3 && strlen(joinCodeBuffer) > 0) {
                    joinCodeBuffer[strlen(joinCodeBuffer) - 1] = '\0';
                }
            }

            // Nickname Input
            DrawText("PILOT NICKNAME:", static_cast<int>(panelX + 40.0f), static_cast<int>(formY), 12, Color{226, 232, 240, 255});
            Rectangle nickRect = {panelX + 40.0f, formY + 18.0f, panelW - 80.0f, 36.0f};
            DrawRectangleRounded(nickRect, 0.2f, 4, Color{30, 41, 59, 255});
            DrawRectangleRoundedLines(nickRect, 0.2f, 4, 1.5f, (activeInputBox == 1 ? Color{56, 189, 248, 255} : Color{71, 85, 105, 255}));
            std::string nickDisplay = nicknameBuffer + std::string((activeInputBox == 1 && showCursor) ? "|" : "");
            DrawText(nickDisplay.c_str(), static_cast<int>(nickRect.x + 10.0f), static_cast<int>(nickRect.y + 10.0f), 15, WHITE);

            // Server URL Input
            formY += 66.0f;
            DrawText("SERVER WEBSOCKET URL:", static_cast<int>(panelX + 40.0f), static_cast<int>(formY), 12, Color{226, 232, 240, 255});
            Rectangle urlRect = {panelX + 40.0f, formY + 18.0f, panelW - 80.0f, 36.0f};
            DrawRectangleRounded(urlRect, 0.2f, 4, Color{30, 41, 59, 255});
            DrawRectangleRoundedLines(urlRect, 0.2f, 4, 1.5f, (activeInputBox == 2 ? Color{56, 189, 248, 255} : Color{71, 85, 105, 255}));
            std::string urlDisplay = serverUrlBuffer + std::string((activeInputBox == 2 && showCursor) ? "|" : "");
            DrawText(urlDisplay.c_str(), static_cast<int>(urlRect.x + 10.0f), static_cast<int>(urlRect.y + 10.0f), 14, Color{56, 189, 248, 255});

            // Character Skin Selector
            formY += 66.0f;
            DrawText("CHARACTER AVATAR:", static_cast<int>(panelX + 40.0f), static_cast<int>(formY), 12, Color{226, 232, 240, 255});
            for (int av = 0; av < 4; ++av) {
                Rectangle avRect = {panelX + 40.0f + av * 96.0f, formY + 18.0f, 88.0f, 32.0f};
                bool isSelected = (selectedAvatarIndex == av);
                Color avBg = isSelected ? Color{2, 132, 199, 255} : Color{30, 41, 59, 200};
                DrawRectangleRounded(avRect, 0.2f, 4, avBg);
                DrawText(avatarOptions[av], static_cast<int>(avRect.x + 12.0f), static_cast<int>(avRect.y + 8.0f), 13, WHITE);

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), avRect)) {
                    selectedAvatarIndex = av;
                }
            }

            // Action Buttons
            formY += 66.0f;
            // Quick Play Button
            Rectangle qpRect = {panelX + 40.0f, formY, panelW - 80.0f, 40.0f};
            DrawRectangleRounded(qpRect, 0.25f, 6, Color{34, 197, 94, 255});
            DrawText("QUICK PLAY (SOLO / BOTS)", static_cast<int>(qpRect.x + 85.0f), static_cast<int>(qpRect.y + 11.0f), 16, WHITE);

            // Create Room & Join Room Buttons
            formY += 50.0f;
            Rectangle crRect = {panelX + 40.0f, formY, (panelW - 90.0f) * 0.5f, 38.0f};
            DrawRectangleRounded(crRect, 0.25f, 6, Color{14, 165, 233, 255});
            DrawText("CREATE ROOM", static_cast<int>(crRect.x + 18.0f), static_cast<int>(crRect.y + 11.0f), 14, WHITE);

            Rectangle jrRect = {panelX + 40.0f + (panelW - 90.0f) * 0.5f + 10.0f, formY, (panelW - 90.0f) * 0.5f, 38.0f};
            DrawRectangleRounded(jrRect, 0.25f, 6, Color{168, 85, 247, 255});
            DrawText("JOIN ROOM", static_cast<int>(jrRect.x + 28.0f), static_cast<int>(jrRect.y + 11.0f), 14, WHITE);

            // Button & input box clicks
            Vector2 mouse = GetMousePosition();
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (CheckCollisionPointRec(mouse, nickRect)) {
                    activeInputBox = 1;
                } else if (CheckCollisionPointRec(mouse, urlRect)) {
                    activeInputBox = 2;
                } else if (!showJoinModal) {
                    activeInputBox = 0;
                }

                if (CheckCollisionPointRec(mouse, qpRect)) {
                    network.connect(serverUrlBuffer);
                    network.quickPlay(nicknameBuffer, avatarOptions[selectedAvatarIndex]);
                    currentState = STATE_CONNECTING;
                    statusMessage = "Connecting to Quick Play...";
                } else if (CheckCollisionPointRec(mouse, crRect)) {
                    network.connect(serverUrlBuffer);
                    network.createRoom(nicknameBuffer, avatarOptions[selectedAvatarIndex], 2);
                    currentState = STATE_CONNECTING;
                    statusMessage = "Creating room on server...";
                } else if (CheckCollisionPointRec(mouse, jrRect)) {
                    showJoinModal = true;
                    activeInputBox = 3;
                }
            }

            // Status message
            if (!statusMessage.empty()) {
                DrawText(statusMessage.c_str(), static_cast<int>(panelX + 40.0f), static_cast<int>(panelY + panelH + 16.0f), 14, Color{250, 204, 21, 255});
            }

            // Join Room Dialog Modal
            if (showJoinModal) {
                Rectangle modalRect = {(curWidth - 360.0f) * 0.5f, (curHeight - 200.0f) * 0.5f, 360.0f, 200.0f};
                DrawRectangleRounded(modalRect, 0.2f, 8, Color{15, 23, 42, 250});
                DrawRectangleRoundedLines(modalRect, 0.2f, 8, 2.0f, Color{56, 189, 248, 255});
                DrawText("ENTER 4-DIGIT ROOM CODE:", static_cast<int>(modalRect.x + 30.0f), static_cast<int>(modalRect.y + 24.0f), 14, WHITE);

                Rectangle codeInputRect = {modalRect.x + 30.0f, modalRect.y + 56.0f, modalRect.width - 60.0f, 38.0f};
                DrawRectangleRounded(codeInputRect, 0.2f, 4, Color{30, 41, 59, 255});
                DrawRectangleRoundedLines(codeInputRect, 0.2f, 4, 1.5f, (activeInputBox == 3 ? Color{56, 189, 248, 255} : Color{71, 85, 105, 255}));
                std::string codeDisplay = (strlen(joinCodeBuffer) > 0 ? joinCodeBuffer : "e.g. 43J3") + std::string((activeInputBox == 3 && showCursor) ? "|" : "");
                DrawText(codeDisplay.c_str(),
                         static_cast<int>(codeInputRect.x + 12.0f), static_cast<int>(codeInputRect.y + 10.0f), 16,
                         (strlen(joinCodeBuffer) > 0 ? WHITE : GRAY));

                // Join button
                Rectangle jbRect = {modalRect.x + 30.0f, modalRect.y + 120.0f, 140.0f, 36.0f};
                DrawRectangleRounded(jbRect, 0.2f, 4, Color{34, 197, 94, 255});
                DrawText("JOIN", static_cast<int>(jbRect.x + 50.0f), static_cast<int>(jbRect.y + 10.0f), 15, WHITE);

                // Cancel button
                Rectangle cancelRect = {modalRect.x + 190.0f, modalRect.y + 120.0f, 140.0f, 36.0f};
                DrawRectangleRounded(cancelRect, 0.2f, 4, Color{239, 68, 68, 255});
                DrawText("CANCEL", static_cast<int>(cancelRect.x + 40.0f), static_cast<int>(cancelRect.y + 10.0f), 15, WHITE);

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (CheckCollisionPointRec(mouse, codeInputRect)) {
                        activeInputBox = 3;
                    } else if (CheckCollisionPointRec(mouse, jbRect)) {
                        if (strlen(joinCodeBuffer) > 0) {
                            network.connect(serverUrlBuffer);
                            network.joinRoom(joinCodeBuffer, nicknameBuffer, avatarOptions[selectedAvatarIndex]);
                            currentState = STATE_CONNECTING;
                            statusMessage = "Joining room " + std::string(joinCodeBuffer) + "...";
                            showJoinModal = false;
                        }
                    } else if (CheckCollisionPointRec(mouse, cancelRect)) {
                        showJoinModal = false;
                        activeInputBox = 0;
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

        // Send local inputs to authoritative server
        network.sendInput(inputs.left, inputs.right, inputs.up, inputs.jump, inputs.aimAngle);

        if (inputs.shoot) {
            network.sendShoot(inputs.aimAngle);
        }
        if (inputs.grenade) {
            network.sendGrenade(inputs.aimAngle);
        }
        if (inputs.reload) {
            network.sendReload();
        }

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

        // Check Top-Right In-Game Buttons
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            // Leave Button
            Rectangle leaveRect = {static_cast<float>(curWidth - 76), 16.0f, 56.0f, 26.0f};
            if (CheckCollisionPointRec(mouse, leaveRect)) {
                network.leaveRoom();
                currentState = STATE_LOBBY;
            }
        }

        // -------------------------------------------------------------
        // Render Pipeline
        // -------------------------------------------------------------
        BeginDrawing();
        ClearBackground(Color{7, 10, 24, 255});

        // 1. World Layer (Space map, platforms, core, soldiers, bullets, explosions)
        renderer.renderWorld(world, camera, localId);

        // 2. HUD Layer (Health bar, fuel bar, weapon ammo, timer, room code, kill feed)
        renderer.renderHUD(world, localId, network.getRoomCode(), network.getPingMs(), curWidth, curHeight);

        // 3. Virtual Mobile Touch Controls Overlay
        touchControls.draw(curWidth, curHeight);

        EndDrawing();
    }

    // Cleanup
    network.disconnect();
    renderer.unload();
    CloseWindow();
    return 0;
}
