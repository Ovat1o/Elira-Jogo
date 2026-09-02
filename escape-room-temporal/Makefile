CC = gcc
CFLAGS = -Iinclude -Wall -std=c99
LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

SRC = src/main.c src/elira.c src/timeline.c src/ariel.c src/decifraIA.c
OBJ = $(SRC:.c=.o)
EXEC = build/game

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $(EXEC) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(EXEC)
	