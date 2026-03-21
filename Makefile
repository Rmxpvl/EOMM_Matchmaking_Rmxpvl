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

# ── Autofill test suite target ────────────────────────────
TEST_AUTOFILL_TARGET = $(BIN_DIR)/test_autofill_system
TEST_AUTOFILL_SRCS   = $(SRC_DIR)/eomm_system.c tests/test_autofill_system.c
TEST_AUTOFILL_INCLUDE = -I$(INC_DIR)

# ── Autofill debug test target ────────────────────────────
TEST_DEBUG_TARGET  = $(BIN_DIR)/test_debug_autofill
TEST_DEBUG_SRCS    = $(SRC_DIR)/eomm_system.c tests/test_debug_autofill.c
TEST_DEBUG_INCLUDE = -I$(INC_DIR)

# ── Default target ────────────────────────────────────────
.PHONY: all build eomm clean run test test_autofill test_debug_autofill

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

# Build autofill test suite
test_autofill: $(BIN_DIR) $(TEST_AUTOFILL_TARGET)

$(TEST_AUTOFILL_TARGET): $(TEST_AUTOFILL_SRCS) $(INC_DIR)/eomm_system.h
	$(CC) $(CFLAGS) $(TEST_AUTOFILL_INCLUDE) -o $@ $(TEST_AUTOFILL_SRCS)

# Build autofill debug test
test_debug_autofill: $(BIN_DIR) $(TEST_DEBUG_TARGET)

$(TEST_DEBUG_TARGET): $(TEST_DEBUG_SRCS) $(INC_DIR)/eomm_system.h
	$(CC) $(CFLAGS) $(TEST_DEBUG_INCLUDE) -o $@ $(TEST_DEBUG_SRCS)

# Run EOMM system (interactive)
run: eomm
	$(EOMM_TARGET)

# Alias kept for CI compatibility
build: eomm

# Test target
test: test_autofill test_debug_autofill
	@echo "Running autofill test suite..."
	$(TEST_AUTOFILL_TARGET)
	@echo "Test suite complete."
	@echo ""
	@echo "Running autofill debug test..."
	$(TEST_DEBUG_TARGET)
	@echo "Debug test complete."

# Clean
clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)