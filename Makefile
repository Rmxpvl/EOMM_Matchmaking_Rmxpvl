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
EOMM_SRCS    = $(SRC_DIR)/eomm_system.c $(SRC_DIR)/match_history.c $(SRC_DIR)/eomm_main.c
EOMM_OBJS    = $(OBJ_DIR)/eomm_system.o $(OBJ_DIR)/match_history.o $(OBJ_DIR)/eomm_main.o
EOMM_INCLUDE = -I$(INC_DIR)

# ── Autofill test suite target ────────────────────────────
TEST_AUTOFILL_TARGET = $(BIN_DIR)/test_autofill_system
TEST_AUTOFILL_SRCS   = $(SRC_DIR)/eomm_system.c tests/test_autofill_system.c
TEST_AUTOFILL_INCLUDE = -I$(INC_DIR)

# ── Autofill debug test target ────────────────────────────
TEST_DEBUG_TARGET  = $(BIN_DIR)/test_debug_autofill
TEST_DEBUG_SRCS    = $(SRC_DIR)/eomm_system.c tests/test_debug_autofill.c
TEST_DEBUG_INCLUDE = -I$(INC_DIR)

# ── Coefficient analysis test target ──────────────────────
TEST_COEFF_TARGET  = $(BIN_DIR)/test_coefficient_analysis
TEST_COEFF_SRCS    = $(SRC_DIR)/eomm_system.c tests/test_coefficient_analysis.c
TEST_COEFF_INCLUDE = -I$(INC_DIR)

# ── Performance stats test target ────────────────────────
TEST_PERF_TARGET  = $(BIN_DIR)/test_performance_stats
TEST_PERF_SRCS    = $(SRC_DIR)/eomm_system.c tests/test_performance_stats.c
TEST_PERF_INCLUDE = -I$(INC_DIR)
TEST_PERF_CFLAGS  = -Wall -Wextra -std=c99 -lm

# ── 50-games detailed simulation test target ─────────────
TEST_50G_TARGET  = $(BIN_DIR)/test_50_games_detailed
TEST_50G_SRCS    = $(SRC_DIR)/eomm_system.c tests/test_50_games_detailed.c
TEST_50G_INCLUDE = -I$(INC_DIR)

# ── EOMM auto 200-games test target ──────────────────────
TEST_AUTO200_TARGET  = $(BIN_DIR)/test_eomm_auto_200games
TEST_AUTO200_SRCS    = tests/test_eomm_auto_200games.c
TEST_AUTO200_INCLUDE = -I$(INC_DIR)

# ── Full EOMM 200-games test target ──────────────────────
TEST_FULL200_TARGET  = $(BIN_DIR)/test_full_eomm_200games
TEST_FULL200_SRCS    = tests/test_full_eomm_200games.c
TEST_FULL200_INCLUDE = -I$(INC_DIR)

# ── Default target ────────────────────────────────────────
.PHONY: all build eomm clean run test test_autofill test_debug_autofill test_coefficient_analysis test_performance_stats test_50_games_detailed test_eomm_auto_200games test_full_eomm_200games

all: eomm

# Build EOMM system
eomm: $(BIN_DIR) $(OBJ_DIR) $(EOMM_TARGET)

$(EOMM_TARGET): $(EOMM_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/eomm_system.o: $(SRC_DIR)/eomm_system.c $(INC_DIR)/eomm_system.h
	$(CC) $(CFLAGS) $(EOMM_INCLUDE) -c $< -o $@

$(OBJ_DIR)/match_history.o: $(SRC_DIR)/match_history.c $(INC_DIR)/eomm_system.h
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

# Build coefficient analysis test
test_coefficient_analysis: $(BIN_DIR) $(TEST_COEFF_TARGET)

$(TEST_COEFF_TARGET): $(TEST_COEFF_SRCS) $(INC_DIR)/eomm_system.h
	$(CC) $(CFLAGS) $(TEST_COEFF_INCLUDE) -o $@ $(TEST_COEFF_SRCS)

# Build performance stats test
test_performance_stats: $(BIN_DIR) $(TEST_PERF_TARGET)

$(TEST_PERF_TARGET): $(TEST_PERF_SRCS) $(INC_DIR)/eomm_system.h
	$(CC) $(CFLAGS) $(TEST_PERF_INCLUDE) -o $@ $(TEST_PERF_SRCS) -lm

# Build 50-games detailed simulation test
test_50_games_detailed: $(BIN_DIR) $(TEST_50G_TARGET)

$(TEST_50G_TARGET): $(TEST_50G_SRCS) $(INC_DIR)/eomm_system.h
	$(CC) $(CFLAGS) $(TEST_50G_INCLUDE) -o $@ $(TEST_50G_SRCS)

# Build EOMM auto 200-games test
test_eomm_auto_200games: $(BIN_DIR) $(TEST_AUTO200_TARGET)

$(TEST_AUTO200_TARGET): $(TEST_AUTO200_SRCS)
	$(CC) $(CFLAGS) $(TEST_AUTO200_INCLUDE) -o $@ $(TEST_AUTO200_SRCS)

# Build full EOMM 200-games test
test_full_eomm_200games: $(BIN_DIR) $(TEST_FULL200_TARGET)

$(TEST_FULL200_TARGET): $(TEST_FULL200_SRCS)
	$(CC) $(CFLAGS) $(TEST_FULL200_INCLUDE) -o $@ $(TEST_FULL200_SRCS)

# Run EOMM system (interactive)
run: eomm
	$(EOMM_TARGET)

# Alias kept for CI compatibility
build: eomm

# Test target
test: test_autofill test_debug_autofill test_coefficient_analysis test_performance_stats test_50_games_detailed
	@echo "Running autofill test suite..."
	$(TEST_AUTOFILL_TARGET)
	@echo "Test suite complete."
	@echo ""
	@echo "Running autofill debug test..."
	$(TEST_DEBUG_TARGET)
	@echo "Debug test complete."
	@echo ""
	@echo "Running coefficient analysis test..."
	$(TEST_COEFF_TARGET)
	@echo "Coefficient analysis complete."
	@echo ""
	@echo "Running performance stats test..."
	$(TEST_PERF_TARGET)
	@echo "Performance stats test complete."
	@echo ""
	@echo "Running 50-games detailed simulation..."
	$(TEST_50G_TARGET)
	@echo "50-games simulation complete."

# ── Python EOMM tools ────────────────────────────────────────
TOOLS = python3 src/scripts/eomm_tools.py

.PHONY: analyze simulate track fix report

analyze:
	$(TOOLS) analyze-match

simulate:
	$(TOOLS) simulate-aggressive

track:
	$(TOOLS) track-hidden-advanced

fix:
	@echo "Available fix commands:"
	@echo "  make fix-hardstuck-blend"
	@echo "  make fix-hardstuck-final"
	@echo "  make fix-hardstuck-perf"
	@echo "  make fix-hardstuck-v2"
	@echo "  make fix-hardstuck-wr"
	@echo "  make fix-update-tilt"
	@echo "  make fix-wr"

fix-hardstuck-blend:
	$(TOOLS) fix-hardstuck-blend

fix-hardstuck-final:
	$(TOOLS) fix-hardstuck-final

fix-hardstuck-perf:
	$(TOOLS) fix-hardstuck-perf

fix-hardstuck-v2:
	$(TOOLS) fix-hardstuck-v2

fix-hardstuck-wr:
	$(TOOLS) fix-hardstuck-wr

fix-update-tilt:
	$(TOOLS) fix-update-tilt

fix-wr:
	$(TOOLS) fix-wr

report:
	$(TOOLS) final-report

.PHONY: fix-hardstuck-blend fix-hardstuck-final fix-hardstuck-perf fix-hardstuck-v2 fix-hardstuck-wr fix-update-tilt fix-wr

# Clean
clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)