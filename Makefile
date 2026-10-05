RAYLIB_DIR := C:/raylib
TOOLCHAIN_DIR := $(RAYLIB_DIR)/w64devkit/bin

# Keep every compiler subprocess (assembler and linker included) on the same
# 64-bit toolchain. This avoids accidentally picking tools from C:/MinGW.
export PATH := $(TOOLCHAIN_DIR);$(PATH)

CC := $(TOOLCHAIN_DIR)/gcc.exe
CPPFLAGS := -I$(RAYLIB_DIR)/raylib/src
CFLAGS := -std=c99 -Wall -Wextra
LDLIBS := -L$(RAYLIB_DIR)/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm

SRC := src/main.c
OUT := ELIRA.exe

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
