# Anti-Gravity Outpost — Native C++17 Client (Raylib + IXWebSocket)

High-performance native 2D real-time multiplayer shooter inspired by *Mini Militia / Doodle Army 2*, engineered with **Raylib 5.0**, **raygui 4.0**, **IXWebSocket**, and **nlohmann/json** in modern C++17.

Features:
- **Zero-lag 60–120 FPS native gameplay** with delta-time prediction and smooth remote player interpolation.
- **Dual virtual touch joysticks** on mobile (Left thumb: move & fly; Right thumb: 360° aim & auto-fire) + dedicated buttons for Reload & Grenade.
- **Desktop keyboard & mouse support** (`WASD`, Mouse aim/shoot, `G` grenade, `R` reload).
- **Broken Space Outpost arena**: Starry backdrop, shattered glass window, multi-tier steel platforms with hazard stripes, floating wooden crates, and central pulsing Anti-Gravity Core.
- **Authoritative synchronization** with Node.js + Express + `ws` backend.

---

## 1. Quick Desktop Build (Linux & Windows)

### Prerequisites:
- **C++17 Compiler**: GCC 9+, Clang 10+, or MSVC 2019+
- **CMake**: Version 3.16+
- **Git** (for CMake FetchContent to pull dependencies automatically)

On Debian/Ubuntu:
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake git libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev libssl-dev
```

### Build & Run:
```bash
cd client
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run game
./build/MiniMilitiaNative
```

*(All dependencies — Raylib, raygui, IXWebSocket, and nlohmann/json — are fetched, configured, and compiled automatically by CMake).*

---

## 2. Android APK Build via Raymob

We use the official [Bigfoot71/raymob](https://github.com/Bigfoot71/raymob) template to compile the C++ client into an Android APK using the Android NDK and Gradle.

### Step-by-Step Instructions:
1. **Clone the Raymob template**:
   ```bash
   git clone --depth 1 https://github.com/Bigfoot71/raymob.git my-game-android
   ```
2. **Copy Client Source Code**:
   Copy everything from this repository's `client/src/`, `client/include/`, and `client/assets/` into the Raymob project:
   ```bash
   cp -r client/src/* my-game-android/app/src/main/cpp/
   cp -r client/include/* my-game-android/app/src/main/cpp/
   cp -r client/assets/* my-game-android/app/src/main/assets/
   cp client/android/AndroidManifest.xml my-game-android/app/src/main/AndroidManifest.xml
   ```
3. **Configure CMake in Raymob**:
   Open `my-game-android/app/src/main/cpp/CMakeLists.txt` and ensure `MiniMilitiaNative` source files are listed:
   ```cmake
   add_library(main SHARED
       main.cpp
       TouchControls.cpp
       NetworkManager.cpp
       GameWorld.cpp
       Renderer.cpp
   )
   target_link_libraries(main raylib ixwebsocket android log EGL GLESv2 OpenSLES)
   ```
4. **Compile the APK**:
   ```bash
   cd my-game-android
   ./gradlew assembleRelease
   ```
   The generated APK will be at:
   `app/build/outputs/apk/release/app-release-unsigned.apk`

5. **Install on Android Device**:
   ```bash
   adb install -r app/build/outputs/apk/release/app-release-unsigned.apk
   ```

---

## 3. Connecting to the Multiplayer Server

### Local Testing:
1. Start the server:
   ```bash
   cd ../server
   npm install
   npm start
   ```
   (Server listens on `ws://localhost:4000` or port 4001/4002).
2. Launch the client and connect to:
   `ws://localhost:4000` (or `ws://10.0.2.2:4000` when running inside Android Emulator).

### Deploying Backend to Render (Free Cloud Hosting):
1. Push this monorepo to GitHub.
2. In [Render Dashboard](https://dashboard.render.com/):
   - Click **New +** -> **Web Service**.
   - Root Directory: `server`
   - Environment: `Node`
   - Build Command: `npm install`
   - Start Command: `npm start`
3. Copy your live Render URL (e.g. `https://my-mini-militia.onrender.com`).
4. In the C++ game client's lobby menu, enter:
   `wss://my-mini-militia.onrender.com`
5. Click **QUICK PLAY** or **CREATE ROOM** to play with players worldwide!
