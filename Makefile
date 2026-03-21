# Makefile — EOMM Matchmaking System

# ── C compiler (EOMM engine) ──────────────────────────────
CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -lm

# ── Directories ───────────────────────────────────────────
SRC_DIR = src
INC_DIR = include
BIN_DIR = bin
OBJ_DIR = obj

# ── EOMM system target ────────────────────────────────────
EOMM_TARGET  = $(BIN_DIR)/eomm_system
EOMM_SRCS    = $(SRC_DIR)/eomm_system.c $(SRC_DIR)/eomm_main.c
EOMM_OBJS    = $(OBJ_DIR)/eomm_system.o $(OBJ_DIR)/eomm_main.o
EOMM_INCLUDE = -I$(INC_DIR)

# ── Default target ────────────────────────────────────────
.PHONY: all build eomm clean run test

all: eomm

# Build EOMM system
eomm: $(BIN_DIR) $(OBJ_DIR) $(EOMM_TARGET)

$(EOMM_TARGET): $(EOMM_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/eomm_system.o: $(SRC_DIR)/eomm_system.c $(INC_DIR)/eomm_system.h
	$(CC) $(CFLAGS) $(EOMM_INCLUDE) -c $< -o $@

$(OBJ_DIR)/eomm_main.o: $(SRC_DIR)/eomm_main.c $(INC_DIR)/eomm_system.h
	$(CC) $(CFLAGS) $(EOMM_INCLUDE) -c $< -o $@

# Directory creation
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Run EOMM system (interactive)
run: eomm
	$(EOMM_TARGET)

# Alias kept for CI compatibility
build: eomm

# Test target
test:
	@echo "Running EOMM build test..."
	$(MAKE) eomm
	@echo "Build test passed."

# Clean
clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)