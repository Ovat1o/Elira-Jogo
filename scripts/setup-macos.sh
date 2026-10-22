#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "==> Preparando o ELIRA no macOS"

if ! xcode-select -p >/dev/null 2>&1; then
    echo "As ferramentas da Apple ainda nao estao instaladas."
    echo "A janela de instalacao sera aberta agora."
    xcode-select --install
    echo "Depois que a instalacao terminar, execute este script novamente."
    exit 0
fi

if command -v brew >/dev/null 2>&1; then
    BREW_BIN="$(command -v brew)"
elif [ -x /opt/homebrew/bin/brew ]; then
    BREW_BIN=/opt/homebrew/bin/brew
elif [ -x /usr/local/bin/brew ]; then
    BREW_BIN=/usr/local/bin/brew
else
    echo "==> Instalando o Homebrew"
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

    if [ -x /opt/homebrew/bin/brew ]; then
        BREW_BIN=/opt/homebrew/bin/brew
    elif [ -x /usr/local/bin/brew ]; then
        BREW_BIN=/usr/local/bin/brew
    else
        echo "Nao foi possivel localizar o Homebrew apos a instalacao." >&2
        exit 1
    fi
fi

export PATH="$(dirname "$BREW_BIN"):$PATH"

if ! "$BREW_BIN" list --versions raylib >/dev/null 2>&1; then
    echo "==> Instalando a raylib"
    "$BREW_BIN" install raylib
else
    echo "==> raylib ja instalada"
fi

echo "==> Compilando o jogo"
cd "$PROJECT_DIR"
make clean
make

echo
echo "Pronto. Execute o jogo com:"
echo "  ./ELIRA"
