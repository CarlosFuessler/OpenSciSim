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
      src/modules/mathsim/mathsim.c \
      src/modules/calc/calc.c \
      src/modules/physics/physics.c \
      src/modules/physics/mechanics.c \
      src/modules/chemistry/chemistry.c \
      src/modules/physics/optics.c \
      src/modules/chemistry/chemsim.c

OBJ = $(SRC:.c=.o)
BIN = openscisim

# WASM / Emscripten settings
RAYLIB_PATH ?= $(HOME)/raylib
RAYLIB_WEB_LIB ?= $(firstword $(wildcard $(RAYLIB_PATH)/src/libraylib.web.a $(RAYLIB_PATH)/src/libraylib.a))
WEB_DIR = build/web
SHELL_FILE = web/shell.html
EMCC = emcc
EMCFLAGS = -Wall -Wextra -std=c11 -I src -I $(RAYLIB_PATH)/src \
           -Os -DPLATFORM_WEB
EMLDFLAGS = $(RAYLIB_WEB_LIB) -lm \
            -s USE_GLFW=3 -s ASYNCIFY -s TOTAL_MEMORY=67108864 \
            -s FORCE_FILESYSTEM=1 \
            --preload-file assets \
            --shell-file $(SHELL_FILE)

.PHONY: all clean run web web-clean web-serve

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(OBJ) $(BIN)

# --- WASM targets ---

web: $(WEB_DIR)/index.html

$(WEB_DIR)/index.html: $(SRC)
	@if [ -z "$(RAYLIB_WEB_LIB)" ]; then \
		echo "Error: raylib web library not found in $(RAYLIB_PATH)/src."; \
		echo "Build raylib with 'make PLATFORM=PLATFORM_WEB' (creates libraylib.web.a) and retry."; \
		exit 1; \
	fi
	@mkdir -p $(WEB_DIR)
	$(EMCC) $(EMCFLAGS) $(SRC) -o $(WEB_DIR)/index.html $(EMLDFLAGS)
	@echo "✓ WASM build complete → $(WEB_DIR)/index.html"

web-clean:
	rm -rf $(WEB_DIR)

web-serve: $(WEB_DIR)/index.html
	@echo "Serving at http://localhost:8080"
	cd $(WEB_DIR) && python3 -m http.server 8080
