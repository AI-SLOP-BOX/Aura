#!/bin/zsh

# Aura DAW Ultimate - App Icon Generator
# Converts a 1024x1024 PNG into a professional macOS .icns file.

PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
# HONEST FIX: Privacy Guard - Use $HOME to avoid leaking real names in public repos
ICON_SRC="${HOME}/.gemini/antigravity/brain/8cc6e288-2219-4ae4-9796-7a59ba77edba/aura_daw_flat_icon_new_1774152199639.png"
ICONSET_DIR="$PROJECT_DIR/AuraIcon.iconset"

echo "--------------------------------------------------------"
echo "  Aura DAW Ultimate Icon Factory [v1.1]"
echo "--------------------------------------------------------"

if [ ! -f "$ICON_SRC" ]; then
    echo "[ERROR] Source icon not found."
    exit 1
fi

mkdir -p "$ICONSET_DIR"

# RESIZE FOR MAC OS STANDARDS
echo "[INFO] Generating Flat Professional Vector Icon Layers..."
sips -s format png -z 16 16     "$ICON_SRC" --out "$ICONSET_DIR/icon_16x16.png" > /dev/null
sips -s format png -z 32 32     "$ICON_SRC" --out "$ICONSET_DIR/icon_16x16@2x.png" > /dev/null
sips -s format png -z 32 32     "$ICON_SRC" --out "$ICONSET_DIR/icon_32x32.png" > /dev/null
sips -s format png -z 64 64     "$ICON_SRC" --out "$ICONSET_DIR/icon_32x32@2x.png" > /dev/null
sips -s format png -z 128 128   "$ICON_SRC" --out "$ICONSET_DIR/icon_128x128.png" > /dev/null
sips -s format png -z 256 256   "$ICON_SRC" --out "$ICONSET_DIR/icon_128x128@2x.png" > /dev/null
sips -s format png -z 256 256   "$ICON_SRC" --out "$ICONSET_DIR/icon_256x256.png" > /dev/null
sips -s format png -z 512 512   "$ICON_SRC" --out "$ICONSET_DIR/icon_256x256@2x.png" > /dev/null
sips -s format png -z 512 512   "$ICON_SRC" --out "$ICONSET_DIR/icon_512x512.png" > /dev/null
sips -s format png -z 1024 1024 "$ICON_SRC" --out "$ICONSET_DIR/icon_512x512@2x.png" > /dev/null

echo "[INFO] Finalizing .icns container..."
iconutil -c icns "$ICONSET_DIR" -o "$PROJECT_DIR/AuraIcon.icns"
rm -rf "$ICONSET_DIR"

echo "--------------------------------------------------------"
echo "[SUCCESS] Flat Pro Icon generated: AuraIcon.icns"
echo "--------------------------------------------------------"
