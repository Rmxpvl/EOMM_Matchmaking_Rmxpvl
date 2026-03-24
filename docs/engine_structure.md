# System Architecture

## Source Code Organization

```
src/
├── eomm_main.c           ← Entry point
├── eomm_system.c         ← Core ELO + Matchmaking logic
├── match_history.c       ← Game result tracking
└── scripts/              ← Development utilities
    └── eomm_tools.py     ← Analysis & debugging scripts
```

## Core Components

### 1. **eomm_main.c** — CLI & Simulation Entry

**Responsibility**: Initialize simulation, manage game loop, output results

**Key functions**:
- `main()`: Parse args, setup players, run matches
- `simulate_matches()`: Main game loop (100K iterations)
- `print_results()`: Format & display final statistics

**Data flow**:
```
main() → init_players() → simulate_matches() → print_results()
```

---

### 2. **eomm_system.c** — Core Algorithm

**Responsibility**: Implement ELO logic and matchmaking

**Key structures**:
```c
typedef struct {
    int id;
    SkillLevel skill_level;
    float mmr_raw;
    float mmr_uncertainty;
    int total_games, wins, losses;
    int outcomes[MAX_GAMES_PER_PLAYER];  // Win/loss history
    float mmr_timeline[MAX_GAMES_PER_PLAYER + 1];  // MMR at each game
} PlayerTimeline;

typedef struct {
    int pool_size;
    int wins;
    float mmr_start, mmr_end;
    float win_rate;
} PoolStats;
```

**Key functions**:

| Function | Purpose |
|----------|---------|
| `calculate_expected()` | ELO expected win % |
| `calc_effective_mmr()` | Team MMR + carry bonus |
| `get_dynamic_K()` | K-factor by experience |
| `update_mmr()` | Full delta + WR-scaling |
| `simulate_match()` | Run one 5v5 |
| `find_closest_mmr_teams()` | Matchmaking algorithm |
| `mmr_to_rank()` | Display rank/division |

**ELO Calculation Pipeline**:
```
outcome: did_win (1.0 or 0.0)
           ↓
expected_win: calculate_expected(mmr_diff)
           ↓
clamp_expected: by skill tier [0.30-0.85] or [0.10-0.90]
           ↓
delta = K × (outcome - expected)
           ↓
wr_scaling: delta *= (1.0 + wr_factor × 1.0)
           ↓
MMR += delta
```

---

### 3. **match_history.c** — Result Tracking

**Responsibility**: Log and retrieve match results

**Key structures**:
```c
typedef struct {
    int match_id;
    int winner;  // 0=team_a, 1=team_b
} MatchRecord;
```

**Key functions**:
- `log_match()`: Record match outcome
- `get_match_history()`: Retrieve all matches
- `export_to_json()`: Dump results for analysis

---

## Game Loop Architecture

### Single Match Flow
```
1. Find 10 players in queue (closest MMR)
2. Split into Team A (5) and Team B (5)
3. Simulate match outcome (based on MMR diff + RNG)
4. Update both teams' MMR (apply ELO with all 3 layers)
5. Log match result
6. Return players to queue
```

### Population-Level Simulation
```
while (total_games < N_GAMES) {
    if (queue.size >= 10) {
        match = find_match();
        result = simulate_match();
        update_mmr_both_teams();
        log_match();
    }
    advance_time();
}
```

---

## Player Initialization

Each player spawned with:

| Skill Tier | Population % | Base MMR | Distribution |
|------------|--------------|----------|---------------|
| SMURF_HIGH | 2.2% | 1700-1900 | Tight |
| SMURF_MED | 2.2% | 1400-1600 | Tight |
| SMURF_LOW | 5.6% | 1200-1300 | Tight |
| NORMAL | 87.8% | 900-1100 | Wide |
| LOW_BAD | 1.2% | 600-750 | Tight |
| LOW_VERY_BAD | 0.6% | 400-550 | Tight |
| LOW_EXTREME | 0.4% | 100-300 | Tight |

All players spawn with random noise (~10% variance).

---

## Output Format

### Per-Player Analysis

```
╔═ 🔥🔥🔥 SMURF_HIGH (ID: #0021) ═╗
├─ TOTAL SEASON: 1000 games | 625 wins (62.5%) | MMR: 1910 → 735 [-1176]

Pool 1 (50g): 90.0% WR - Domination complète
         MMR: 1910 → 1912 [+2] | 💎 Diamond IV → 💎 Diamond IV
Pool 2 (100g): 86.0% WR - Domination complète
         MMR: 1910 → 1669 [-241] | 💎 Diamond IV → 🏆 Platinum IV
...
```

Components:
- **ID**: Unique player identifier
- **Skill tier**: Visual (🔥 = smurf, 💔 = weak, 📊 = normal)
- **Win rate**: Actual win % over season
- **MMR delta**: Start → End with bracket notation
- **Pool breakdown**: 4 checkpoint intervals (50/100/200/300 games)
- **Rank progression**: Bronze/Silver/Gold/Platinum progression
- **Convergence metrics**: Estimated true skill + oscillation

---

## Performance Characteristics

**Current Benchmark** (test_eomm_season_realistic.c):
- **1M games** (100K matches × 10 players/match)
- **1000 players** tracked individually
- **Compile time**: ~0.5s
- **Runtime**: ~2-3s
- **Memory**: ~150MB

Can scale to:
- 10K players: 10-15s runtime
- 10M games: 20-30s runtime

---

## Testing Strategy

### Main Test File
- `tests/test_eomm_season_realistic.c`: Full population simulation

**What it validates**:
1. ✅ No inflation (smurfs don't reach 2000+ MMR)
2. ✅ No exploitation (WR < 50% blocks climbing)
3. ✅ Natural convergence (7 tiers stabilize at realistic levels)
4. ✅ Mathematical invariants (WR ↔ MMR direction)

**Run**:
```bash
make test
# Or manually:
gcc -Wall -Wextra -std=c99 tests/test_eomm_season_realistic.c -o bin/test_season_realistic -lm
./bin/test_season_realistic
```

---

## Configuration Points

To tune the system, edit values in `tests/test_eomm_season_realistic.c`:

| Parameter | Line | Default | Effect |
|-----------|------|---------|--------|
| Carry bonus cap | ~125 | 90 MMR | Higher = more smurf advantage |
| Low-skill expected min | ~188 | 0.30 | Higher = easier for weak players |
| Low-skill expected max | ~189 | 0.85 | Lower = harder to exploit |
| K-factor boundaries | ~135-140 | 40/20/10 | Higher = faster convergence |
| WR scaling coeff | ~207 | 1.0 | Higher = stricter WR enforcement |

**Be careful**: Changing these can break invariants. Validate with full test run.

---

## Future Extensions

1. **Probability-based matching**: Instead of MMR-closest, use win probability
2. **Role-based ELO**: Separate rating per role (Mid/ADC/Support/etc)
3. **Leaderboard integration**: Track top 100, seasonal resets
4. **Smurf detection**: Flag abnormal WR curves
5. **Regional server simulation**: Match players by latency bucket