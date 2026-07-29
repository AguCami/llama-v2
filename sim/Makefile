# Simulador de escritorio del juego (mismo codigo que corre en el ESP32-S3).
#
#   make            -> compila build/llamasim
#   make shots      -> genera capturas PPM de todas las pantallas
#   make png        -> convierte las capturas a PNG

ROOT    := ..
CC      ?= cc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra -Wno-unused-parameter
INCLUDES = -I$(ROOT)/components/g3d/include -I$(ROOT)/components/llamapet/include \
           -I$(ROOT)/components/llamapet/src
LDLIBS   = -lm

SRC := $(wildcard $(ROOT)/components/g3d/src/*.c) \
       $(wildcard $(ROOT)/components/llamapet/src/*.c) \
       sim_main.c

BUILD := build
BIN   := $(BUILD)/llamasim
SHOTS := $(BUILD)/shots

SCENES := boot main food stats minigame sleep sick dead cria night shake

all: $(BIN)

$(BIN): $(SRC)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC) -o $@ $(LDLIBS)

shots: $(BIN)
	@mkdir -p $(SHOTS)
	@for s in $(SCENES); do $(BIN) $(SHOTS) $$s >/dev/null; done
	@echo "capturas en $(SHOTS)"

png: shots
	@python3 tools/ppm2png.py $(SHOTS)

clean:
	rm -rf $(BUILD)

.PHONY: all shots png clean
