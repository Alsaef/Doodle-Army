#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Use standalone OpenJDK 21 LTS with javac
if [ -d "$PROJECT_ROOT/tools/jdk" ]; then
    export JAVA_HOME="$PROJECT_ROOT/tools/jdk"
    export PATH="$JAVA_HOME/bin:$PATH"
elif [ -d "/usr/lib/jvm/java-21-openjdk-amd64" ]; then
    export JAVA_HOME="/usr/lib/jvm/java-21-openjdk-amd64"
    export PATH="$JAVA_HOME/bin:$PATH"
fi

echo "=========================================================="
echo "🤖 Setting up Raymob Android Project for Anti-Gravity Outpost"
echo "=========================================================="

# 1. Clone Raymob template if not present
if [ ! -d "$SCRIPT_DIR/raymob_template" ]; then
    echo "📥 Cloning Bigfoot71/raymob template..."
    git clone --depth 1 https://github.com/Bigfoot71/raymob.git "$SCRIPT_DIR/raymob_template"
fi

# Configure Android SDK & NDK paths in local.properties
if [ -d "$PROJECT_ROOT/tools/android-sdk" ]; then
    export ANDROID_HOME="$PROJECT_ROOT/tools/android-sdk"
    export ANDROID_SDK_ROOT="$PROJECT_ROOT/tools/android-sdk"
    echo "sdk.dir=$ANDROID_HOME" > "$SCRIPT_DIR/raymob_template/local.properties"
    if [ -d "$ANDROID_HOME/ndk" ]; then
        NDK_PATH=$(find "$ANDROID_HOME/ndk" -maxdepth 1 -mindepth 1 -type d | head -n 1)
        if [ -n "$NDK_PATH" ]; then
            echo "ndk.dir=$NDK_PATH" >> "$SCRIPT_DIR/raymob_template/local.properties"
        fi
    fi
fi

# 2. Sync Client C++ source files into Raymob project
echo "🔄 Copying C++ sources, headers and assets into Raymob..."
mkdir -p "$SCRIPT_DIR/raymob_template/app/src/main/cpp"
mkdir -p "$SCRIPT_DIR/raymob_template/app/src/main/assets"

cp -r "$PROJECT_ROOT/client/src/"* "$SCRIPT_DIR/raymob_template/app/src/main/cpp/"
cp -r "$PROJECT_ROOT/client/include/"* "$SCRIPT_DIR/raymob_template/app/src/main/cpp/" 2>/dev/null || true
cp -r "$PROJECT_ROOT/client/assets/"* "$SCRIPT_DIR/raymob_template/app/src/main/assets/"
cp "$SCRIPT_DIR/app/src/main/AndroidManifest.xml" "$SCRIPT_DIR/raymob_template/app/src/main/AndroidManifest.xml"
if [ -f "$SCRIPT_DIR/app/src/main/cpp/CMakeLists.txt" ]; then
    cp "$SCRIPT_DIR/app/src/main/cpp/CMakeLists.txt" "$SCRIPT_DIR/raymob_template/app/src/main/cpp/CMakeLists.txt"
fi

# 3. Build APK
echo "📦 Building Android APK with Gradle..."
cd "$SCRIPT_DIR/raymob_template"
chmod +x gradlew
./gradlew assembleDebug

echo "=========================================================="
echo "✅ Build Complete! APK generated at:"
echo "mobile-android/raymob_template/app/build/outputs/apk/debug/app-debug.apk"
echo "=========================================================="
