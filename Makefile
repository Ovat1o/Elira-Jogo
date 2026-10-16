# Permite ao desenvolvedor definir caminhos locais sem alterar o git
-include config.mk

# Caminhos padrão (podem ser substituídos no config.mk, por variáveis de ambiente ou linha de comando)
RAYLIB_DIR ?= C:/raylib
TOOLCHAIN_DIR ?= $(RAYLIB_DIR)/w64devkit/bin

# Se encontrar o GCC do w64devkit no caminho especificado, usa ele (evita conflitos com outros MinGWs antigos).
# Se não encontrar, assume o 'gcc' padrão já configurado no PATH do sistema.
ifeq ($(wildcard $(TOOLCHAIN_DIR)/gcc.exe),)
    CC ?= gcc
else
    export PATH := $(TOOLCHAIN_DIR);$(PATH)
    CC ?= $(TOOLCHAIN_DIR)/gcc.exe
endif

CPPFLAGS ?= -I$(RAYLIB_DIR)/raylib/src
CFLAGS   ?= -std=c99 -Wall -Wextra

ifeq ($(OS),Windows_NT)
    LDLIBS ?= -L$(RAYLIB_DIR)/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm
    OUT := ELIRA.exe
else
    # Suporte para Linux / macOS
    LDLIBS ?= -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
    OUT := ELIRA
endif

SRC := src/main.c

.PHONY: all run clean

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) -o $(OUT) $(LDLIBS)

run: $(OUT)
	./$(OUT)

clean:
ifeq ($(OS),Windows_NT)
	-@cmd /c if exist $(OUT) del /f /q $(OUT) 2>nul
else
	-@$(RM) $(OUT) 2>/dev/null || true
endif

