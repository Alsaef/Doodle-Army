#include "AudioManager.hpp"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <iostream>

AudioManager::AudioManager() {}

AudioManager::~AudioManager() {
    unload();
}

void AudioManager::init(const std::string& assetsPath) {
    if (initialized) return;

    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        std::cerr << "[AudioManager] Audio device failed to initialize." << std::endl;
        return;
    }

    const char* soundKeys[] = {
        "rifle", "shotgun", "sniper", "smg", "pistol", "laser", "rocket",
        "jetpack", "explosion", "reload", "hit", "kill", "pickup", "click"
    };

    for (const char* key : soundKeys) {
        std::string fname = std::string(key) + ".wav";
        std::string candidatePaths[] = {
            assetsPath + "/audio/" + fname,
            assetsPath + "/" + fname,
            "assets/audio/" + fname,
            "client/assets/audio/" + fname,
            "audio/" + fname
        };

        std::string chosenPath = "";
        for (const auto& path : candidatePaths) {
            if (FileExists(path.c_str())) {
                chosenPath = path;
                break;
            }
        }

        sounds[key] = loadOrSynthesize(chosenPath, key);
    }

    // Set volumes matching studio specs
    if (sounds.count("rifle")) SetSoundVolume(sounds["rifle"], 0.85f);
    if (sounds.count("shotgun")) SetSoundVolume(sounds["shotgun"], 0.95f);
    if (sounds.count("sniper")) SetSoundVolume(sounds["sniper"], 1.0f);
    if (sounds.count("smg")) SetSoundVolume(sounds["smg"], 0.75f);
    if (sounds.count("pistol")) SetSoundVolume(sounds["pistol"], 0.85f);
    if (sounds.count("laser")) SetSoundVolume(sounds["laser"], 0.80f);
    if (sounds.count("rocket")) SetSoundVolume(sounds["rocket"], 0.95f);
    if (sounds.count("jetpack")) SetSoundVolume(sounds["jetpack"], 0.65f);
    if (sounds.count("explosion")) SetSoundVolume(sounds["explosion"], 1.0f);
    if (sounds.count("reload")) SetSoundVolume(sounds["reload"], 0.85f);
    if (sounds.count("hit")) SetSoundVolume(sounds["hit"], 0.70f);
    if (sounds.count("kill")) SetSoundVolume(sounds["kill"], 0.90f);
    if (sounds.count("pickup")) SetSoundVolume(sounds["pickup"], 0.75f);
    if (sounds.count("click")) SetSoundVolume(sounds["click"], 0.50f);

    initialized = true;
    std::cout << "[AudioManager] Initialized audio with " << sounds.size() << " sound channels." << std::endl;
}

void AudioManager::unload() {
    if (!initialized) return;

    for (auto& pair : sounds) {
        if (pair.second.frameCount > 0) {
            StopSound(pair.second);
            UnloadSound(pair.second);
        }
    }
    sounds.clear();

    if (IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    initialized = false;
}

Sound AudioManager::loadOrSynthesize(const std::string& path, const std::string& soundType) {
    if (!path.empty() && FileExists(path.c_str())) {
        Sound snd = LoadSound(path.c_str());
        if (snd.frameCount > 0) {
            return snd;
        }
    }
    return synthesizeFallback(soundType);
}

Sound AudioManager::synthesizeFallback(const std::string& soundType) {
    const int sampleRate = 44100;
    float duration = 0.25f;

    if (soundType == "explosion") duration = 0.6f;
    else if (soundType == "rocket") duration = 0.45f;
    else if (soundType == "jetpack") duration = 0.5f;
    else if (soundType == "sniper") duration = 0.4f;
    else if (soundType == "shotgun") duration = 0.35f;
    else if (soundType == "reload") duration = 0.35f;
    else if (soundType == "click") duration = 0.05f;

    int totalSamples = static_cast<int>(sampleRate * duration);
    short* samples = (short*)RL_MALLOC(totalSamples * sizeof(short));
    if (!samples) return Sound{0};

    for (int i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float progress = t / duration;
        float s = 0.0f;

        if (soundType == "laser") {
            // Downward pitch sweep
            float freq = 1200.0f - progress * 800.0f;
            s = sinf(2.0f * PI * freq * t) * (1.0f - progress);
        } else if (soundType == "rocket") {
            // Propelled rocket launch whoosh + low roar
            float freq = 140.0f - progress * 95.0f;
            float hum = sinf(2.0f * PI * freq * t);
            float noise = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f);
            s = (hum * 0.6f + noise * 0.4f) * (1.0f - progress);
        } else if (soundType == "explosion") {
            // Low rumble + noise
            float noise = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f);
            float lowRumble = sinf(2.0f * PI * 65.0f * t);
            s = (noise * 0.7f + lowRumble * 0.3f) * (1.0f - progress * progress);
        } else if (soundType == "jetpack") {
            // Continuous whoosh noise + 120Hz hum
            float noise = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f);
            float hum = sinf(2.0f * PI * 130.0f * t);
            s = (noise * 0.6f + hum * 0.4f);
        } else if (soundType == "click") {
            // Short pop
            s = sinf(2.0f * PI * 1800.0f * t) * (1.0f - progress);
        } else if (soundType == "kill") {
            // Uplifting two-tone chime
            float freq = (progress < 0.5f) ? 587.33f : 880.0f;
            s = sinf(2.0f * PI * freq * t) * (1.0f - progress);
        } else if (soundType == "pickup") {
            // Rising chime
            float freq = 440.0f + progress * 600.0f;
            s = sinf(2.0f * PI * freq * t) * (1.0f - progress);
        } else if (soundType == "reload") {
            // Dual mechanical clicks
            float click1 = (t > 0.05f && t < 0.1f) ? sinf(2.0f * PI * 1400.0f * t) : 0.0f;
            float click2 = (t > 0.22f && t < 0.27f) ? sinf(2.0f * PI * 1800.0f * t) : 0.0f;
            s = click1 + click2;
        } else {
            // Weapon gunshots: punchy noise + decaying pitch
            float noise = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f);
            float punch = sinf(2.0f * PI * (soundType == "sniper" ? 80.0f : 140.0f) * t);
            float env = powf(1.0f - progress, 2.5f);
            s = (noise * 0.7f + punch * 0.3f) * env;
        }

        s = std::max(-1.0f, std::min(1.0f, s));
        samples[i] = static_cast<short>(s * 30000.0f);
    }

    Wave wave = { 0 };
    wave.frameCount = totalSamples;
    wave.sampleRate = sampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = samples;

    Sound snd = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return snd;
}

void AudioManager::playShoot(const std::string& weaponType) {
    if (muted || !initialized) return;

    std::string key = "rifle";
    float minInterval = 0.12f;
    if (weaponType == "SMG") { key = "smg"; minInterval = 0.085f; }
    else if (weaponType == "SHOTGUN") { key = "shotgun"; minInterval = 0.55f; }
    else if (weaponType == "SNIPER") { key = "sniper"; minInterval = 0.80f; }
    else if (weaponType == "PISTOL") { key = "pistol"; minInterval = 0.20f; }
    else if (weaponType == "LASER") { key = "laser"; minInterval = 0.15f; }
    else if (weaponType == "ROCKET") { key = "rocket"; minInterval = 0.95f; }

    float now = static_cast<float>(GetTime());
    auto timeIt = lastPlayTimes.find(key);
    if (timeIt != lastPlayTimes.end()) {
        if (now - timeIt->second < minInterval) {
            return; // Throttle to prevent machine-drill buzzing
        }
    }
    lastPlayTimes[key] = now;

    auto it = sounds.find(key);
    if (it != sounds.end() && it->second.frameCount > 0) {
        // Organic micro-pitch variation: +/- 3%
        float pitch = 0.97f + static_cast<float>(rand() % 60) / 1000.0f;
        SetSoundPitch(it->second, pitch);
        PlaySound(it->second);
    }
}

void AudioManager::playHit() {
    if (muted || !initialized) return;

    float now = static_cast<float>(GetTime());
    auto timeIt = lastPlayTimes.find("hit");
    if (timeIt != lastPlayTimes.end()) {
        if (now - timeIt->second < 0.06f) return;
    }
    lastPlayTimes["hit"] = now;

    auto it = sounds.find("hit");
    if (it != sounds.end() && it->second.frameCount > 0) {
        float pitch = 0.96f + static_cast<float>(rand() % 80) / 1000.0f;
        SetSoundPitch(it->second, pitch);
        PlaySound(it->second);
    }
}

void AudioManager::playExplosion() {
    if (muted || !initialized) return;
    auto it = sounds.find("explosion");
    if (it != sounds.end() && it->second.frameCount > 0) {
        float pitch = 0.97f + static_cast<float>(rand() % 60) / 1000.0f;
        SetSoundPitch(it->second, pitch);
        PlaySound(it->second);
    }
}

void AudioManager::playReload() {
    if (muted || !initialized) return;
    auto it = sounds.find("reload");
    if (it != sounds.end()) {
        PlaySound(it->second);
    }
}

void AudioManager::playPickup() {
    if (muted || !initialized) return;
    auto it = sounds.find("pickup");
    if (it != sounds.end()) {
        PlaySound(it->second);
    }
}

void AudioManager::playKill() {
    if (muted || !initialized) return;
    auto it = sounds.find("kill");
    if (it != sounds.end()) {
        PlaySound(it->second);
    }
}

void AudioManager::playClick() {
    if (muted || !initialized) return;
    auto it = sounds.find("click");
    if (it != sounds.end()) {
        PlaySound(it->second);
    }
}

void AudioManager::updateJetpack(bool isFlying) {
    if (muted || !initialized) {
        if (jetpackPlaying) {
            auto it = sounds.find("jetpack");
            if (it != sounds.end()) StopSound(it->second);
            jetpackPlaying = false;
        }
        return;
    }

    auto it = sounds.find("jetpack");
    if (it == sounds.end()) return;

    if (isFlying) {
        if (!jetpackPlaying || !IsSoundPlaying(it->second)) {
            PlaySound(it->second);
            jetpackPlaying = true;
        }
    } else {
        if (jetpackPlaying) {
            StopSound(it->second);
            jetpackPlaying = false;
        }
    }
}

void AudioManager::setMuted(bool newMuted) {
    muted = newMuted;
    if (muted && jetpackPlaying) {
        auto it = sounds.find("jetpack");
        if (it != sounds.end()) StopSound(it->second);
        jetpackPlaying = false;
    }
}
