#!/bin/zsh

# Aura DAW Ultimate - Professional Packaging System
# This script creates a native macOS .app bundle.

PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
BUILD_DIR="$PROJECT_DIR/build"
APP_DIR="$PROJECT_DIR/Aura DAW.app"

echo "--------------------------------------------------------"
echo "  Aura DAW Ultimate Packaging Engine [v1.0]"
echo "--------------------------------------------------------"

# 1. CLEAN & BUILD
mkdir -p "$BUILD_DIR"
export SDK_PATH="/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK_PATH"
cmake --build "$BUILD_DIR" --config Release -j "$(sysctl -n hw.ncpu)"

if [ $? -ne 0 ]; then
    echo "[ERROR] Build failed."
    exit 1
fi

# 2. CREATE APP STRUCTURE
echo "[INFO] Creating macOS Application Bundle..."
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/Contents/MacOS"
mkdir -p "$APP_DIR/Contents/Resources"

# 3. COPY EXECUTABLE
EXE_SRC="$BUILD_DIR/bin/AuraUltimate"
if [ ! -f "$EXE_SRC" ]; then
    # Fallback for macOS bundle targets
    EXE_SRC="$BUILD_DIR/AuraUltimate.app/Contents/MacOS/AuraUltimate"
fi

if [ -f "$EXE_SRC" ]; then
    cp "$EXE_SRC" "$APP_DIR/Contents/MacOS/Aura DAW"
    chmod +x "$APP_DIR/Contents/MacOS/Aura DAW"
else
    echo "[ERROR] Binary not found at $EXE_SRC"
    exit 1
fi

# 4. COPY ASSETS & SHADERS
# --- HONEST FIX: SHADER BUNDLING ---
# Prevents EXC_BAD_ACCESS (0x8) by ensuring Metal library is in Resources
SHADER_BIN="$BUILD_DIR/default.metallib"
if [ -f "$SHADER_BIN" ]; then
    cp "$SHADER_BIN" "$APP_DIR/Contents/Resources/default.metallib"
    echo "[INFO] Metal Shaders Bundled."
fi

if [ -f "$PROJECT_DIR/AuraIcon.icns" ]; then
    cp "$PROJECT_DIR/AuraIcon.icns" "$APP_DIR/Contents/Resources/AuraIcon.icns"
fi


# 5. GENERATE INFO.PLIST
cat <<EOF > "$APP_DIR/Contents/Info.plist"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>Aura DAW</string>
    <key>CFBundleIdentifier</key>
    <string>com.aura.daw.ultimate</string>
    <key>CFBundleName</key>
    <string>Aura DAW</string>
    <key>CFBundleIconFile</key>
    <string>AuraIcon</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSMicrophoneUsageDescription</key>
    <string>Aura DAW requires microphone access for professional multi-track recording and live input monitoring.</string>
    <key>NSAppleEventsUsageDescription</key>
    <string>Aura DAW requires Apple Events for deep integration with Logic Pro project workflows.</string>
</dict>
</plist>
EOF

# 6. AD-HOC CODESIGN (Essential for ARM64)
echo "[INFO] Signing Application Bundle..."
codesign --force --deep --sign - "$APP_DIR"

echo "--------------------------------------------------------"
echo "[SUCCESS] Packaging Complete: $APP_DIR"
echo "--------------------------------------------------------"
open "$PROJECT_DIR"
