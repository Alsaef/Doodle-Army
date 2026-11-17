#pragma once

#include "raylib.h"
#include <string>

struct ControlInputs {
    bool left = false;
    bool right = false;
    bool up = false;      // Jetpack thrust
    bool jump = false;    // Jump / Fly
    bool reload = false;
    bool shoot = false;
    bool grenade = false;
    float aimAngle = 0.0f;
    float moveX = 0.0f;   // -1.0 to 1.0 analog
    float moveY = 0.0f;   // -1.0 to 1.0 analog
    bool isTouchActive = false;
};

class TouchControls {
public:
    TouchControls();
    ~TouchControls() = default;

    void init(int screenWidth, int screenHeight);
    void update(int screenWidth, int screenHeight, Vector2 playerScreenPos);
    void draw(int screenWidth, int screenHeight);

    const ControlInputs& getInputs() const { return inputs; }

    // Manual overrides
    void reset();

    // Check if user is currently using touch screen
    bool isMobile() const { return mobileDetected || inputs.isTouchActive; }
    void setForceMobile(bool enable) { forceMobile = enable; }

private:
    ControlInputs inputs;
    bool mobileDetected = false;
    bool forceMobile = false;

    // Left Virtual Joystick (Movement & Jetpack)
    Vector2 leftStickCenter;
    Vector2 leftStickCurrent;
    float leftStickRadius = 80.0f;
    float leftStickKnobRadius = 36.0f;
    bool leftStickActive = false;
    int leftStickTouchId = -1;

    // Right Virtual Joystick (Aiming & Auto-fire)
    Vector2 rightStickCenter;
    Vector2 rightStickCurrent;
    float rightStickRadius = 80.0f;
    float rightStickKnobRadius = 36.0f;
    bool rightStickActive = false;
    int rightStickTouchId = -1;
    float fireThresholdRatio = 0.45f; // Pulling right stick > 45% radius fires weapon

    // Virtual Buttons
    Rectangle reloadButtonRect;
    Rectangle grenadeButtonRect;
    bool reloadPressed = false;
    bool grenadePressed = false;

    // Helper functions
    bool isPointInCircle(Vector2 point, Vector2 center, float radius);
    bool isPointInRect(Vector2 point, Rectangle rect);
};
