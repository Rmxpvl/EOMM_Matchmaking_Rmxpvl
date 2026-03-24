# Makefile — EOMM Matchmaking System (ELO 3-Layer)

# ── Compiler & Flags ──────────────────────────────────────
CC       = gcc
CFLAGS   = -Wall -Wextra -std=c99
LDFLAGS  = -lm

# ── Directories ───────────────────────────────────────────
SRC_DIR  = src
INC_DIR  = include
BIN_DIR  = bin
TEST_DIR = tests

# ── Targets ───────────────────────────────────────────────

# Main EOMM system
EOMM_TARGET  = $(BIN_DIR)/eomm_system
EOMM_SRCS    = $(SRC_DIR)/eomm_system.c $(SRC_DIR)/match_history.c $(SRC_DIR)/eomm_main.c
EOMM_INCLUDE = -I$(INC_DIR)

# Primary test: realistic season simulation
TEST_TARGET  = $(BIN_DIR)/test_season_realistic
TEST_SRCS    = $(TEST_DIR)/test_eomm_season_realistic.c
TEST_INCLUDE = -I$(INC_DIR)

# Smurf injection test: analyze domination & climb
SMURF_TARGET = $(BIN_DIR)/test_smurf_injection
SMURF_SRCS   = $(TEST_DIR)/test_smurf_realistic_injection.c
SMURF_INCLUDE = -I$(INC_DIR)

# ── Phony targets ─────────────────────────────────────────
.PHONY: all build test clean help smurf

# ── Default target ────────────────────────────────────────
all: build test

help:
	@echo "EOMM Matchmaking — ELO 3-Layer System"
	@echo ""
	@echo "Usage:"
	@echo "  make build              Build main EOMM system"
	@echo "  make test               Run validation test (1M games simulation)"
	@echo "  make smurf              Run smurf injection test (realistic alt account)"
	@echo "  make clean              Remove compiled binaries"
	@echo "  make all                Build + test (default)"
	@echo "  make help               Show this help"

# Create bin directory
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# Build main EOMM system
build: $(BIN_DIR) $(EOMM_TARGET)

$(EOMM_TARGET): $(EOMM_SRCS) $(INC_DIR)/eomm_system.h
	@echo "[CC] Building $(EOMM_TARGET)..."
	@$(CC) $(CFLAGS) $(EOMM_INCLUDE) -o $@ $(EOMM_SRCS) $(LDFLAGS)
	@echo "✓ Build complete: $@"

# Build & run primary test
test: $(BIN_DIR) $(TEST_TARGET)
	@echo "[RUN] Executing test_eomm_season_realistic..."
	@echo ""
	@./$(TEST_TARGET) 2>&1 | head -150

$(TEST_TARGET): $(TEST_SRCS) $(INC_DIR)/eomm_system.h
	@echo "[CC] Building $(TEST_TARGET)..."
	@$(CC) $(CFLAGS) $(TEST_INCLUDE) -o $@ $(TEST_SRCS) $(LDFLAGS)
	@echo "✓ Test binary ready: $@"

# Build & run smurf injection test
smurf: $(BIN_DIR) $(SMURF_TARGET)
	@echo "[RUN] Executing smurf realistic injection test..."
	@echo ""
	@./$(SMURF_TARGET)

$(SMURF_TARGET): $(SMURF_SRCS)
	@echo "[CC] Building $(SMURF_TARGET)..."
	@$(CC) $(CFLAGS) $(SMURF_INCLUDE) -o $@ $(SMURF_SRCS) $(LDFLAGS)
	@echo "✓ Smurf test binary ready: $@"

# Clean build artifacts
clean:
	@echo "[CLEAN] Removing binaries..."
	@rm -f $(BIN_DIR)/* 2>/dev/null || true
	@echo "✓ Cleaned"

# ── Aliases ────────────────────────────────────────────────
run: build
	@./$(EOMM_TARGET)
	rm -rf $(BIN_DIR) $(OBJ_DIR)