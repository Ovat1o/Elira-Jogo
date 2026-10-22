#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RAYLIB_DIR="$PROJECT_DIR/.deps/raylib"

echo "==> Preparando o ELIRA no Linux"

if command -v apt-get >/dev/null 2>&1; then
    sudo apt-get update
    sudo apt-get install -y build-essential git libasound2-dev libx11-dev \
        libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev \
        libxcursor-dev libxinerama-dev libwayland-dev libxkbcommon-dev
elif command -v dnf >/dev/null 2>&1; then
    sudo dnf install -y gcc make git alsa-lib-devel mesa-libGL-devel \
        libX11-devel libXrandr-devel libXi-devel libXcursor-devel \
        libXinerama-devel libatomic
elif command -v pacman >/dev/null 2>&1; then
    sudo pacman -Syu --needed base-devel git alsa-lib mesa libx11 \
        libxrandr libxi libxcursor libxinerama
else
    echo "Distribuicao nao reconhecida automaticamente." >&2
    echo "Instale GCC, Make, Git e as dependencias da raylib e tente novamente." >&2
    exit 1
fi

mkdir -p "$PROJECT_DIR/.deps"

if [ ! -d "$RAYLIB_DIR/.git" ]; then
    echo "==> Baixando a raylib 6.0"
    git clone --depth 1 --branch 6.0 https://github.com/raysan5/raylib.git "$RAYLIB_DIR"
else
    echo "==> raylib 6.0 ja baixada"
fi

echo "==> Compilando a raylib"
make -C "$RAYLIB_DIR/src" PLATFORM=PLATFORM_DESKTOP

echo "==> Compilando o jogo"
cd "$PROJECT_DIR"
make clean
make

echo
echo "Pronto. Execute o jogo com:"
echo "  ./ELIRA"
