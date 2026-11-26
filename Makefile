# =============================================================================
# Makefile - ELIRA: Escape Run Temporal
# Estrutura da pasta Elira-Jogo:
#   src/main.c        - Ponto de entrada e loop principal de execucao
#   src/funcoes.c     - Implementacao por extenso de todas as funcoes do jogo
#   src/constantes.h  - Definicoes (#define) e constantes globais
#   src/funcoes.h     - Prototipos das funcoes, structs e enums
#   src/raylib.h      - Cabecalho principal da biblioteca Raylib
#   src/raymath.h     - Cabecalho de funcoes matematicas da Raylib
#   src/rlgl.h        - Cabecalho da camada OpenGL (rlgl) da Raylib
#   assets/           - Recursos visuais, fontes e audio do jogo
# =============================================================================

# Permite sobrescrever configuracoes locais sem alterar o repositorio
-include config.mk

# Diretorios do projeto
SRC_DIR        := src
SRC_RAYLIB_DIR := $(SRC_DIR)/raylib
ASSETS_DIR     := assets
DEPS_DIR       := .deps
LOCAL_RAYLIB   := $(DEPS_DIR)/raylib/src/libraylib.a
LOCAL_TOOLCHAIN_DIR := $(DEPS_DIR)/w64devkit/bin
UNAME_S        ?= $(shell uname -s)

# Caminhos padrao da instalacao da Raylib / compilador (para linkagem da libraylib)
RAYLIB_DIR    ?= C:/raylib
TOOLCHAIN_DIR ?= $(RAYLIB_DIR)/w64devkit/bin

# Os scripts de preparacao instalam dependencias locais em .deps/.
# Quando o w64devkit local existe, ele tem prioridade sobre C:/raylib.
ifneq ($(wildcard $(LOCAL_TOOLCHAIN_DIR)/gcc.exe),)
    TOOLCHAIN_DIR := $(LOCAL_TOOLCHAIN_DIR)
endif

# Selecao automatica do compilador GCC (Windows nativo, WSL com w64devkit, ou Linux/macOS)
ifneq ($(wildcard $(TOOLCHAIN_DIR)/gcc.exe),)
    export PATH := $(TOOLCHAIN_DIR);$(PATH)
    CC := $(TOOLCHAIN_DIR)/gcc.exe
else ifneq ($(wildcard /mnt/c/raylib/w64devkit/bin/gcc.exe),)
    export PATH := /mnt/c/raylib/w64devkit/bin:$(PATH)
    export WSLENV := PATH/l
    CC := WSLENV=PATH/l PATH="/mnt/c/raylib/w64devkit/bin:$$PATH" /mnt/c/raylib/w64devkit/bin/gcc.exe -BC:/raylib/w64devkit/bin/
else
    CC ?= gcc
endif

# Arquivos-fonte (.c) do projeto
SRC := $(SRC_DIR)/main.c \
       $(SRC_DIR)/funcoes.c \
       $(SRC_DIR)/invasao.c \
       $(SRC_DIR)/iafight.c

# Objetos (.o) gerados na compilacao
OBJ := $(SRC:.c=.o)

# Cabecalhos (.h) proprios do projeto
PROJECT_HEADERS := $(SRC_DIR)/constantes.h \
                   $(SRC_DIR)/funcoes.h \
                   $(SRC_DIR)/invasao.h \
                   $(SRC_DIR)/iafight.h \
                   $(SRC_DIR)/iafight_elira.h \
                   $(SRC_DIR)/iafight_claude.h \
                   $(SRC_DIR)/iafight_codex.h \
                   $(SRC_DIR)/iafight_antigravity.h \
                   $(SRC_DIR)/iafight_aura67.h \
                   $(SRC_DIR)/iafight_lia.h \
                   $(SRC_DIR)/iafight_deepseek.h \
                   $(SRC_DIR)/iafight_arena.h

# Arquivos de cabecalho da biblioteca Raylib presentes na pasta src/raylib/
RAYLIB_HEADERS  := $(SRC_RAYLIB_DIR)/raylib.h \
                   $(SRC_RAYLIB_DIR)/raymath.h \
                   $(SRC_RAYLIB_DIR)/rlgl.h \
                   $(SRC_RAYLIB_DIR)/rcamera.h \
                   $(SRC_RAYLIB_DIR)/rgestures.h

# Todos os cabecalhos monitorados pelo Make
HEADERS := $(PROJECT_HEADERS) $(RAYLIB_HEADERS)

# Flags de pre-processador e compilacao (busca .h em src/ e src/raylib/)
CPPFLAGS := -I$(SRC_DIR) -I$(SRC_RAYLIB_DIR)
CFLAGS   ?= -std=c99 -Wall -Wextra

# Configuracoes especificas de cada Sistema Operacional para linkagem
ifeq ($(OS),Windows_NT)
    LDLIBS ?= -L$(SRC_RAYLIB_DIR) -L$(SRC_DIR) -L$(RAYLIB_DIR)/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm
    OUT    := ELIRA.exe
else ifneq ($(wildcard /mnt/c/raylib/w64devkit/bin/gcc.exe),)
    LDLIBS ?= -L$(SRC_RAYLIB_DIR) -L$(SRC_DIR) -LC:/raylib/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm
    OUT    := ELIRA.exe
else ifeq ($(UNAME_S),Darwin)
    BREW_BIN := $(shell which brew 2>/dev/null || (test -x /opt/homebrew/bin/brew && echo /opt/homebrew/bin/brew) || (test -x /usr/local/bin/brew && echo /usr/local/bin/brew))
    RAYLIB_PREFIX ?= $(shell $(BREW_BIN) --prefix raylib 2>/dev/null)
    ifeq ($(RAYLIB_PREFIX),)
        ifneq ($(wildcard /opt/homebrew/opt/raylib/lib/libraylib.a),)
            RAYLIB_PREFIX := /opt/homebrew/opt/raylib
        else ifneq ($(wildcard /usr/local/opt/raylib/lib/libraylib.a),)
            RAYLIB_PREFIX := /usr/local/opt/raylib
        endif
    endif
    LDLIBS ?= $(RAYLIB_PREFIX)/lib/libraylib.a -framework Cocoa -framework IOKit -framework OpenGL -framework CoreVideo -framework QuartzCore
    OUT    := ELIRA
else
    ifneq ($(wildcard $(LOCAL_RAYLIB)),)
        RAYLIB_LINUX_LIB := $(LOCAL_RAYLIB)
    else
        RAYLIB_LINUX_LIB := -lraylib
    endif
    LDLIBS ?= $(RAYLIB_LINUX_LIB) -lGL -lm -lpthread -ldl -lrt -lX11
    OUT    := ELIRA
endif

.PHONY: all run win run-win web run-web clean

# "make" sem argumentos continua gerando o executavel nativo.
.DEFAULT_GOAL := all

# -----------------------------------------------------------------------------
# Versao para navegador (WebAssembly). Requer Emscripten (emcc) e git.
#   make web      -> gera dist/web/ (index.html, .js, .wasm, .data)
#   make run-web  -> serve dist/web/ em http://localhost:8080
# A raylib 6.0 e baixada e compilada para web em .deps/raylib-web/ na 1a vez.
# -----------------------------------------------------------------------------
EMCC            ?= emcc
RAYLIB_WEB_TAG  ?= 6.0
RAYLIB_WEB_DIR  := $(DEPS_DIR)/raylib-web
RAYLIB_WEB_LIB  := $(RAYLIB_WEB_DIR)/src/libraylib.web.a
WEB_OUT_DIR     := dist/web
WEB_SHELL       := web/shell.html
WEB_FLAGS       := -std=c99 -Os -Wall -DPLATFORM_WEB \
                   -sUSE_GLFW=3 -sASYNCIFY -sALLOW_MEMORY_GROWTH=1 \
                   -sINITIAL_MEMORY=268435456 -sSTACK_SIZE=1048576 \
                   -sFORCE_FILESYSTEM=1

web: $(WEB_OUT_DIR)/index.html

$(RAYLIB_WEB_LIB):
	@test -d $(RAYLIB_WEB_DIR)/.git || git clone --depth 1 --branch $(RAYLIB_WEB_TAG) \
	    --filter=blob:none --sparse https://github.com/raysan5/raylib.git $(RAYLIB_WEB_DIR)
	cd $(RAYLIB_WEB_DIR) && git sparse-checkout set src
	$(MAKE) -C $(RAYLIB_WEB_DIR)/src PLATFORM=PLATFORM_WEB CC=$(EMCC)

# Lista recursiva de arquivos em Make puro (funciona tambem no Windows).
rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))
WEB_ASSETS := $(call rwildcard,$(ASSETS_DIR),*)

$(WEB_OUT_DIR)/index.html: $(SRC) $(HEADERS) $(WEB_SHELL) $(RAYLIB_WEB_LIB) $(WEB_ASSETS)
	@mkdir -p $(WEB_OUT_DIR)
	$(EMCC) $(SRC) $(CPPFLAGS) $(WEB_FLAGS) $(RAYLIB_WEB_LIB) \
	    --preload-file $(ASSETS_DIR) --shell-file $(WEB_SHELL) -o $@

run-web: web
	cd $(WEB_OUT_DIR) && python3 -m http.server 8080

# Alvo padrao: compila o executavel do jogo
all: $(OUT)

# Linkagem final dos objetos para gerar o executavel
$(OUT): $(OBJ)
	$(CC) $(OBJ) -o $(OUT) $(LDLIBS)

# Compilacao individual de cada arquivo .c em .o (depende dos .h do projeto e dos 3 .h da Raylib em src/)
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Compila e executa o jogo
run: $(OUT)
	./$(OUT)

# Compila e executa a versao nativa de Windows (ELIRA.exe) mesmo a partir do WSL
WIN_BIN_WSL := /mnt/c/raylib/w64devkit/bin
WIN_BIN_WIN := C:/raylib/w64devkit/bin
WIN_CC      := $(WIN_BIN_WSL)/gcc.exe
WIN_LDLIBS  := -L$(SRC_RAYLIB_DIR) -L$(SRC_DIR) -LC:/raylib/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm

win: $(SRC) $(HEADERS)
	WSLENV=PATH/l PATH="$(WIN_BIN_WSL):$$PATH" $(WIN_CC) -B$(WIN_BIN_WIN)/ $(CPPFLAGS) $(CFLAGS) $(SRC) -o ELIRA.exe $(WIN_LDLIBS)

run-win: win
	./ELIRA.exe

# Remove o executavel e os arquivos-objeto (.o)
clean:
ifeq ($(OS),Windows_NT)
	-@cmd /c "del /f /q ELIRA.exe 2>nul"
	-@cmd /c "del /f /q ELIRA 2>nul"
	-@cmd /c "del /f /q $(SRC_DIR)\*.o 2>nul"
else
	-@$(RM) ELIRA ELIRA.exe $(OBJ) 2>/dev/null || true
endif
