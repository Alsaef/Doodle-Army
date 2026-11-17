#include "TouchControls.hpp"
#include "raymath.h"
#include <cmath>
#include <algorithm>

TouchControls::TouchControls() {
    inputs = ControlInputs{};
    leftStickCenter = {140.0f, 580.0f};
    leftStickCurrent = leftStickCenter;
    rightStickCenter = {1140.0f, 580.0f};
    rightStickCurrent = rightStickCenter;
    reloadButtonRect = {1110.0f, 430.0f, 60.0f, 60.0f};
    grenadeButtonRect = {1010.0f, 550.0f, 60.0f, 60.0f};
}

void TouchControls::init(int screenWidth, int screenHeight) {
    float sw = static_cast<float>(screenWidth);
    float sh = static_cast<float>(screenHeight);

    // Responsive positioning based on screen size
    float marginX = std::max(120.0f, sw * 0.12f);
    float marginY = std::max(120.0f, sh * 0.22f);

    leftStickCenter = {marginX, sh - marginY};
    leftStickCurrent = leftStickCenter;

    rightStickCenter = {sw - marginX, sh - marginY};
    rightStickCurrent = rightStickCenter;

    // Place reload button above right stick
    reloadButtonRect = {sw - marginX - 25.0f, sh - marginY - 140.0f, 64.0f, 64.0f};

    // Place grenade button to inner-left of right stick
    grenadeButtonRect = {sw - marginX - 120.0f, sh - marginY - 40.0f, 60.0f, 60.0f};

    #if defined(PLATFORM_ANDROID) || defined(__ANDROID__)
    mobileDetected = true;
    #endif
}

bool TouchControls::isPointInCircle(Vector2 point, Vector2 center, float radius) {
    float dx = point.x - center.x;
    float dy = point.y - center.y;
    return (dx * dx + dy * dy) <= (radius * radius);
}

bool TouchControls::isPointInRect(Vector2 point, Rectangle rect) {
    return (point.x >= rect.x && point.x <= rect.x + rect.width &&
            point.y >= rect.y && point.y <= rect.y + rect.height);
}

void TouchControls::reset() {
    inputs.left = false;
    inputs.right = false;
    inputs.up = false;
    inputs.jump = false;
    inputs.reload = false;
    inputs.shoot = false;
    inputs.grenade = false;
    inputs.moveX = 0.0f;
    inputs.moveY = 0.0f;
    inputs.isTouchActive = false;

    leftStickActive = false;
    leftStickTouchId = -1;
    leftStickCurrent = leftStickCenter;

    rightStickActive = false;
    rightStickTouchId = -1;
    rightStickCurrent = rightStickCenter;

    reloadPressed = false;
    grenadePressed = false;
}

void TouchControls::update(int screenWidth, int screenHeight, Vector2 playerScreenPos) {
    // Re-adjust centers if screen size changed
    init(screenWidth, screenHeight);

    // Reset single-frame pulse inputs
    inputs.left = false;
    inputs.right = false;
    inputs.up = false;
    inputs.jump = false;
    inputs.reload = false;
    inputs.shoot = false;
    inputs.grenade = false;
    inputs.moveX = 0.0f;
    inputs.moveY = 0.0f;

    int touchCount = GetTouchPointCount();
    if (touchCount > 0) {
        mobileDetected = true;
        inputs.isTouchActive = true;
    }

    bool touchHandledLeft = false;
    bool touchHandledRight = false;
    reloadPressed = false;
    grenadePressed = false;

    // Process multi-touch points
    for (int i = 0; i < touchCount; ++i) {
        Vector2 touchPos = GetTouchPosition(i);
        int touchId = GetTouchPointId(i);

        // 1. Check Reload button touch
        if (isPointInRect(touchPos, reloadButtonRect)) {
            reloadPressed = true;
            inputs.reload = true;
            continue;
        }

        // 2. Check Grenade button touch
        if (isPointInRect(touchPos, grenadeButtonRect)) {
            grenadePressed = true;
            inputs.grenade = true;
            continue;
        }

        // 3. Left Stick Check (Touch on left half of screen)
        if (touchPos.x < screenWidth * 0.45f) {
            if (!leftStickActive || leftStickTouchId == touchId) {
                leftStickActive = true;
                leftStickTouchId = touchId;
                touchHandledLeft = true;

                // Compute offset from center
                Vector2 diff = Vector2Subtract(touchPos, leftStickCenter);
                float dist = Vector2Length(diff);

                if (dist > leftStickRadius) {
                    diff = Vector2Scale(Vector2Normalize(diff), leftStickRadius);
                }
                leftStickCurrent = Vector2Add(leftStickCenter, diff);

                // Calculate normalized inputs
                inputs.moveX = diff.x / leftStickRadius;
                inputs.moveY = diff.y / leftStickRadius;

                // Threshold checks
                if (inputs.moveX < -0.2f) inputs.left = true;
                if (inputs.moveX > 0.2f) inputs.right = true;

                // Upwards drag triggers jetpack thrust
                if (inputs.moveY < -0.25f) {
                    inputs.up = true;
                    inputs.jump = true;
                }
            }
        }

        // 4. Right Stick Check (Touch on right half of screen)
        if (touchPos.x >= screenWidth * 0.45f) {
            if (!rightStickActive || rightStickTouchId == touchId) {
                rightStickActive = true;
                rightStickTouchId = touchId;
                touchHandledRight = true;

                Vector2 diff = Vector2Subtract(touchPos, rightStickCenter);
                float dist = Vector2Length(diff);

                if (dist > rightStickRadius) {
                    diff = Vector2Scale(Vector2Normalize(diff), rightStickRadius);
                }
                rightStickCurrent = Vector2Add(rightStickCenter, diff);

                // Aim angle (360 degrees)
                if (dist > 8.0f) {
                    inputs.aimAngle = atan2f(diff.y, diff.x);
                }

                // Auto-fire when pulled beyond threshold
                if (dist >= rightStickRadius * fireThresholdRatio) {
                    inputs.shoot = true;
                }
            }
        }
    }

    // Release sticks if their touch point is no longer present
    if (!touchHandledLeft) {
        leftStickActive = false;
        leftStickTouchId = -1;
        // Smooth return to center
        leftStickCurrent = Vector2Lerp(leftStickCurrent, leftStickCenter, 0.35f);
    }
    if (!touchHandledRight) {
        rightStickActive = false;
        rightStickTouchId = -1;
        rightStickCurrent = Vector2Lerp(rightStickCurrent, rightStickCenter, 0.35f);
    }

    // ==========================================
    // Desktop Keyboard & Mouse Fallback
    // ==========================================
    if (!mobileDetected && !forceMobile) {
        // Horizontal Movement
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
            inputs.left = true;
            inputs.moveX = -1.0f;
        }
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
            inputs.right = true;
            inputs.moveX = 1.0f;
        }

        // Jetpack Thrust / Jump
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_UP)) {
            inputs.up = true;
            inputs.jump = true;
            inputs.moveY = -1.0f;
        }

        // Aiming with Mouse
        Vector2 mousePos = GetMousePosition();
        Vector2 aimDiff = Vector2Subtract(mousePos, playerScreenPos);
        inputs.aimAngle = atan2f(aimDiff.y, aimDiff.x);

        // Shooting: Left Mouse Click
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            inputs.shoot = true;
        }

        // Grenade: G Key or Right Mouse Click
        if (IsKeyPressed(KEY_G) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            inputs.grenade = true;
        }

        // Reload: R Key
        if (IsKeyPressed(KEY_R)) {
            inputs.reload = true;
        }
    }
}

void TouchControls::draw(int screenWidth, int screenHeight) {
    if (!mobileDetected && !forceMobile) return;

    // Colors matching high-tech sci-fi theme
    Color baseCircleColor = {15, 23, 42, 140};       // Slate dark transparent
    Color ringBorderColor = {56, 189, 248, 180};      // Cyan glow
    Color knobColor = {38, 198, 218, 220};            // Bright cyan
    Color knobBorderColor = {255, 255, 255, 220};
    Color fireZoneColor = {239, 68, 68, 70};          // Red outer ring when firing

    // -----------------------------------------------------------------
    // 1. Draw Left Virtual Joystick (Move & Jetpack)
    // -----------------------------------------------------------------
    DrawCircleV(leftStickCenter, leftStickRadius, baseCircleColor);
    DrawCircleLines(static_cast<int>(leftStickCenter.x), static_cast<int>(leftStickCenter.y), leftStickRadius, ringBorderColor);
    DrawCircleLines(static_cast<int>(leftStickCenter.x), static_cast<int>(leftStickCenter.y), leftStickRadius - 1.0f, ringBorderColor);

    // Upward arrow indicator for jetpack thrust
    DrawTriangle(
        {leftStickCenter.x, leftStickCenter.y - leftStickRadius + 14.0f},
        {leftStickCenter.x - 12.0f, leftStickCenter.y - leftStickRadius + 28.0f},
        {leftStickCenter.x + 12.0f, leftStickCenter.y - leftStickRadius + 28.0f},
        {56, 189, 248, 120}
    );
    DrawText("FLY", static_cast<int>(leftStickCenter.x) - 10, static_cast<int>(leftStickCenter.y - leftStickRadius) + 32, 10, {56, 189, 248, 140});

    // Joystick Knob
    DrawCircleV(leftStickCurrent, leftStickKnobRadius, knobColor);
    DrawCircleLines(static_cast<int>(leftStickCurrent.x), static_cast<int>(leftStickCurrent.y), leftStickKnobRadius, knobBorderColor);
    DrawCircle(static_cast<int>(leftStickCurrent.x), static_cast<int>(leftStickCurrent.y), 8.0f, WHITE);

    // -----------------------------------------------------------------
    // 2. Draw Right Virtual Joystick (Aim & Auto-Fire)
    // -----------------------------------------------------------------
    DrawCircleV(rightStickCenter, rightStickRadius, baseCircleColor);

    // Auto-fire danger threshold circle
    float fireRadius = rightStickRadius * fireThresholdRatio;
    DrawCircleLines(static_cast<int>(rightStickCenter.x), static_cast<int>(rightStickCenter.y), fireRadius, {249, 115, 22, 130});
    if (inputs.shoot) {
        DrawCircleV(rightStickCenter, rightStickRadius, fireZoneColor);
    }
    DrawCircleLines(static_cast<int>(rightStickCenter.x), static_cast<int>(rightStickCenter.y), rightStickRadius,
                    inputs.shoot ? Color{239, 68, 68, 220} : ringBorderColor);

    // Directional aim pointer
    Vector2 aimPointer = {
        rightStickCenter.x + cosf(inputs.aimAngle) * (rightStickRadius - 10.0f),
        rightStickCenter.y + sinf(inputs.aimAngle) * (rightStickRadius - 10.0f)
    };
    DrawLineEx(rightStickCenter, aimPointer, 2.5f, {249, 115, 22, 180});
    DrawCircleV(aimPointer, 4.0f, ORANGE);

    // Right Knob
    Color currentRightKnob = inputs.shoot ? Color{239, 68, 68, 220} : Color{249, 115, 22, 220};
    DrawCircleV(rightStickCurrent, rightStickKnobRadius, currentRightKnob);
    DrawCircleLines(static_cast<int>(rightStickCurrent.x), static_cast<int>(rightStickCurrent.y), rightStickKnobRadius, WHITE);
    DrawCircle(static_cast<int>(rightStickCurrent.x), static_cast<int>(rightStickCurrent.y), 8.0f, WHITE);

    // Label: AIM & FIRE
    DrawText("AIM / FIRE", static_cast<int>(rightStickCenter.x) - 30, static_cast<int>(rightStickCenter.y + rightStickRadius) + 8, 12, {249, 115, 22, 180});

    // -----------------------------------------------------------------
    // 3. Draw Virtual Buttons (Reload & Grenade)
    // -----------------------------------------------------------------
    // Reload Button
    Color reloadBg = reloadPressed ? Color{250, 204, 21, 220} : Color{30, 41, 59, 180};
    DrawRectangleRounded(reloadButtonRect, 0.4f, 8, reloadBg);
    DrawRectangleRoundedLines(reloadButtonRect, 0.4f, 8, 2.0f, {250, 204, 21, 240});
    DrawText("RELOAD", static_cast<int>(reloadButtonRect.x) + 7, static_cast<int>(reloadButtonRect.y) + 24, 12, WHITE);

    // Grenade Button
    Color grenadeBg = grenadePressed ? Color{239, 68, 68, 220} : Color{30, 41, 59, 180};
    DrawRectangleRounded(grenadeButtonRect, 0.4f, 8, grenadeBg);
    DrawRectangleRoundedLines(grenadeButtonRect, 0.4f, 8, 2.0f, {239, 68, 68, 240});
    DrawText("G", static_cast<int>(grenadeButtonRect.x) + 24, static_cast<int>(grenadeButtonRect.y) + 16, 24, WHITE);
    DrawText("GRENADE", static_cast<int>(grenadeButtonRect.x) + 4, static_cast<int>(grenadeButtonRect.y) + 44, 9, {252, 165, 165, 240});
}
