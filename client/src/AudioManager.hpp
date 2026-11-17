#pragma once

#include "raylib.h"
#include <string>
#include <unordered_map>

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    void init(const std::string& assetsPath = "assets");
    void unload();

    // Weapon fire sound per weapon type
    void playShoot(const std::string& weaponType);
    void playHit();
    void playExplosion();
    void playReload();
    void playPickup();
    void playKill();
    void playClick();

    // Jetpack continuous thruster sound
    void updateJetpack(bool isFlying);

    // Mute controls
    void setMuted(bool muted);
    bool isMuted() const { return muted; }
    void toggleMute() { setMuted(!muted); }

private:
    bool initialized = false;
    bool muted = false;
    bool jetpackPlaying = false;
    float lastShotTime = 0.0f;
    float jetpackCooldown = 0.0f;

    std::unordered_map<std::string, Sound> sounds;
    std::unordered_map<std::string, float> lastPlayTimes;

    Sound loadOrSynthesize(const std::string& path, const std::string& soundType);
    Sound synthesizeFallback(const std::string& soundType);
};
