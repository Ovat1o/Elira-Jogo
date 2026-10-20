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
SRC_DIR    := src
ASSETS_DIR := assets

# Caminhos padrao da instalacao da Raylib / compilador (para linkagem da libraylib)
RAYLIB_DIR    ?= C:/raylib
TOOLCHAIN_DIR ?= $(RAYLIB_DIR)/w64devkit/bin

# Selecao automatica do compilador GCC
ifeq ($(wildcard $(TOOLCHAIN_DIR)/gcc.exe),)
    CC ?= gcc
else
    export PATH := $(TOOLCHAIN_DIR);$(PATH)
    CC ?= $(TOOLCHAIN_DIR)/gcc.exe
endif

# Arquivos-fonte (.c) do projeto
SRC := $(SRC_DIR)/main.c \
       $(SRC_DIR)/funcoes.c

# Objetos (.o) gerados na compilacao
OBJ := $(SRC:.c=.o)

# Cabecalhos (.h) proprios do projeto
PROJECT_HEADERS := $(SRC_DIR)/constantes.h \
                   $(SRC_DIR)/funcoes.h

# Os 3 arquivos .h necessarios da biblioteca Raylib presentes na pasta src/
RAYLIB_HEADERS  := $(SRC_DIR)/raylib.h \
                   $(SRC_DIR)/raymath.h \
                   $(SRC_DIR)/rlgl.h

# Todos os cabecalhos monitorados pelo Make
HEADERS := $(PROJECT_HEADERS) $(RAYLIB_HEADERS)

# Flags de pre-processador e compilacao (busca todos os .h diretamente em src/)
CPPFLAGS := -I$(SRC_DIR)
CFLAGS   ?= -std=c99 -Wall -Wextra

# Configuracoes especificas de cada Sistema Operacional para linkagem
ifeq ($(OS),Windows_NT)
    LDLIBS ?= -L$(SRC_DIR) -L$(RAYLIB_DIR)/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm
    OUT    := ELIRA.exe
else ifeq ($(shell uname -s),Darwin)
    RAYLIB_PREFIX ?= $(shell brew --prefix raylib 2>/dev/null)
    LDLIBS ?= -L$(SRC_DIR) $(RAYLIB_PREFIX)/lib/libraylib.a -framework Cocoa -framework IOKit -framework OpenGL -framework CoreVideo
    OUT    := ELIRA
else
    LDLIBS ?= -L$(SRC_DIR) -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
    OUT    := ELIRA
endif

.PHONY: all run clean

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

# Remove o executavel e os arquivos-objeto (.o)
clean:
ifeq ($(OS),Windows_NT)
	-@cmd /c if exist $(OUT) del /f /q $(OUT) 2>nul
	-@cmd /c if exist $(SRC_DIR)\*.o del /f /q $(SRC_DIR)\*.o 2>nul
else
	-@$(RM) $(OUT) $(OBJ) 2>/dev/null || true
endif
