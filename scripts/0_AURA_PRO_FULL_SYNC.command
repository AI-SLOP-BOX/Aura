#!/bin/zsh

# Aura DAW Ultimate - Professional Synchronization & Launch System
# [The Authoritative "One-Click" Workflow]

PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
export MACOSX_DEPLOYMENT_TARGET=14.0

echo "--------------------------------------------------------"
echo "  Aura DAW Ultimate: Professional Build & Sync [v2.0]"
echo "--------------------------------------------------------"

# 1. ENGINE SYNC (CMake)
echo "[1/4] Syncing C++ Engine & Bridge..."
mkdir -p "$PROJECT_DIR/build"
cmake -S "$PROJECT_DIR" -B "$PROJECT_DIR/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$PROJECT_DIR/build" --config Release -j 8

if [ $? -ne 0 ]; then
    echo "[ERROR] Engine build failed. Please check SDK/STL includes."
    exit 1
fi

# 2. UI SYNC (Cargo)
echo "[2/4] Building Rust UI (GPUI Zed Framework)..."
cd "$PROJECT_DIR/aura-ui"
cargo build --release

if [ $? -ne 0 ]; then
    echo "[ERROR] UI build failed. Check Cargo dependencies."
    exit 1
fi

# 3. PACKAGING
echo "[3/4] Generating High-Fidelity .app Bundle..."
cd "$PROJECT_DIR"
bash 0_BUILD_APP.command

# 4. LAUNCH
echo "[4/4] Launching Aura DAW v2.0 (High Priority)..."
open "$PROJECT_DIR/Aura DAW.app"

echo "--------------------------------------------------------"
echo "[SUCCESS] AURA ENGINE AWAKENED."
echo "--------------------------------------------------------"
