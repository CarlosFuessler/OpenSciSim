CC = cc
CFLAGS = -Wall -Wextra -std=c11 -I src $(shell pkg-config --cflags raylib)
LDFLAGS = $(shell pkg-config --libs raylib) -lm

SRC = src/main.c \
      src/ui/ui.c \
      src/utils/arena.c \
      src/modules/cas/cas.c \
      src/modules/cas/parser.c \
      src/modules/cas/eval.c \
      src/modules/cas/plotter.c \
      src/modules/cas/plotter3d.c \
      src/modules/calc/calc.c

OBJ = $(SRC:.c=.o)
BIN = openscisim

.PHONY: all clean run

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(OBJ) $(BIN)
