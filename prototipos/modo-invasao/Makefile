# Prototipo standalone do Modo Invasao (nao faz parte do build principal do ELIRA).
# Uso (na raiz do repo, depois de scripts/setup-windows.ps1):
#   .deps/w64devkit/bin/make.exe -C prototipos/modo-invasao
ROOT     := ../..
TOOLBIN  := $(ROOT)/.deps/w64devkit/bin
CC       := $(TOOLBIN)/gcc.exe
export PATH := $(TOOLBIN);$(PATH)
CFLAGS   := -std=c99 -O2 -Wall -I$(ROOT)/src/raylib -I$(ROOT)/src
LDLIBS   := -L$(ROOT)/src/raylib -L$(ROOT)/src -lraylib -lopengl32 -lgdi32 -lwinmm

invasao.exe: invasao.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

clean:
	rm -f invasao.exe
