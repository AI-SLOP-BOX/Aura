#!/bin/zsh

# Aura DAW Ultimate - Standard Build & Launch System
# This script automates the professional compilation and runtime setup.

# 1. SETUP PROJECT PATHS
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
EXE_PATH="$BUILD_DIR/bin/AuraUltimate.app/Contents/MacOS/AuraUltimate"

# 2. CHECK DEPENDENCIES (Zero-Kasu Check)
echo "--------------------------------------------------------"
echo "  Aura DAW Ultimate Build Engine [zsh-v1.0]"
echo "--------------------------------------------------------"

if ! command -v cmake &> /dev/null; then
    echo "[ERROR] CMake is not installed. Please install it via Homebrew: brew install cmake"
    exit 1
fi

# 3. BUILD PROCESS
echo "[INFO] Configuring project architecture..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR" || exit 1

cmake .. -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo "[ERROR] CMake configuration failed."
    exit 1
fi

echo "[INFO] Compiling for $(sysctl -n hw.machine) [Parallel Process: $(sysctl -n hw.ncpu)]"
cmake --build . --config Release -j "$(sysctl -n hw.ncpu)"

if [ $? -ne 0 ]; then
    echo "[ERROR] Compilation failed."
    exit 1
fi

# 4. RUNNER
echo "--------------------------------------------------------"
echo "[SUCCESS] Build Complete. Launching Aura DAW Ultimate..."
echo "--------------------------------------------------------"

if [ -f "$EXE_PATH" ]; then
    "$EXE_PATH"
else
    echo "[ERROR] Executable not found at $EXE_PATH"
fi
