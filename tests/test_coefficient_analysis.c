/*
 * tests/test_coefficient_analysis.c
 *
 * Comprehensive simulation to evaluate FACTOR_WIN_BONUS / FACTOR_LOSS_PENALTY
 * coefficient combinations across three player skill profiles.
 *
 * Scenarios tested (100-game simulation each):
 *   - Smurf     (58% base WR) — expected hidden_factor > 1.0 at convergence
 *   - Normal    (50% base WR) — expected hidden_factor = 1.0 at convergence
 *   - Hardstuck (42% base WR) — expected hidden_factor < 1.0 at convergence
 *
 * Coefficient configurations:
 *   1. Current (Broken)    : WIN=0.020, LOSS=0.050
 *   2. Balanced 1:1        : WIN=0.035, LOSS=0.035
 *   3. Progressive 1.2:1   : WIN=0.036, LOSS=0.030
 *   4. Progressive 1.5:1   : WIN=0.040, LOSS=0.027
 *   5. Adaptive (WR-based) : WIN=0.045, LOSS=0.022
 *
 * Build:
 *   make test_coefficient_analysis
 * Run:
 *   ./bin/test_coefficient_analysis
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../include/eomm_system.h"

/* =========================================================
 * Simulation constants
 * ========================================================= */

#define NUM_GAMES       100
#define PRINT_INTERVAL  10

/* =========================================================
 * Coefficient configuration descriptor
 * ========================================================= */

typedef struct {
    const char *name;
    float win_bonus;
    float loss_penalty;
} CoeffConfig;

static const CoeffConfig CONFIGS[] = {
    { "Current (Broken)    WIN=0.020 LOSS=0.050", 0.020f, 0.050f },
    { "Balanced 1:1        WIN=0.035 LOSS=0.035", 0.035f, 0.035f },
    { "Progressive 1.2:1   WIN=0.036 LOSS=0.030", 0.036f, 0.030f },
    { "Progressive 1.5:1   WIN=0.040 LOSS=0.027", 0.040f, 0.027f },
    { "Adaptive (WR-based) WIN=0.045 LOSS=0.022", 0.045f, 0.022f },
};

#define NUM_CONFIGS ((int)(sizeof(CONFIGS) / sizeof(CONFIGS[0])))

/* =========================================================
 * Global pass/fail counters
 * ========================================================= */

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

/* =========================================================
 * Simulation helpers
 * ========================================================= */

/*
 * simulate_100_games — run a deterministic 100-game simulation for a player
 * with the given base win_rate and coefficient configuration.
 *
 * The simulation uses a fixed pseudo-random seed so results are reproducible
 * across runs.  Wins are determined by comparing a rand() draw against
 * (win_rate * RAND_MAX).
 *
 * hidden_factor is updated after each game:
 *   WIN  → factor += win_bonus  (clamped to [HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX])
 *   LOSS → factor -= loss_penalty (clamped)
 *
 * Every PRINT_INTERVAL games the current stats are printed.
 *
 * Returns the final hidden_factor value after 100 games.
 */
static float simulate_100_games(const char *skill_label,
                                 float       win_rate,
                                 float       win_bonus,
                                 float       loss_penalty,
                                 unsigned int seed)
{
    int   wins   = 0;
    int   losses = 0;
    float factor = HIDDEN_FACTOR_START;

    srand(seed);

    printf("  %-12s |", skill_label);
    printf(" (seed=%u)\n", seed);
    printf("  %s\n", "  Game  | Wins | Losses |   WR%   | hidden_factor");
    printf("  %s\n", "  ------|------|--------|---------|---------------");

    for (int g = 1; g <= NUM_GAMES; g++) {
        /* Determine outcome for this game */
        int won = ((float)rand() / (float)RAND_MAX) < win_rate;

        if (won) {
            wins++;
            factor += win_bonus;
            if (factor > HIDDEN_FACTOR_MAX) factor = HIDDEN_FACTOR_MAX;
        } else {
            losses++;
            factor -= loss_penalty;
            if (factor < HIDDEN_FACTOR_MIN) factor = HIDDEN_FACTOR_MIN;
        }

        if (g % PRINT_INTERVAL == 0) {
            float wr = (g > 0) ? ((float)wins / (float)g * 100.0f) : 0.0f;
            printf("  %6d | %4d | %6d | %6.1f%% | %.4f\n",
                   g, wins, losses, wr, factor);
        }
    }

    return factor;
}

/*
 * math_convergence — compute the theoretical convergence value of hidden_factor
 * for a player with the given win_rate and coefficient pair, assuming
 * the factor never hits the floor/ceiling clamps.
 *
 * At convergence: wins * bonus = losses * penalty
 *   wins / total  = win_rate
 *   losses / total = 1 - win_rate
 * => win_rate * bonus = (1 - win_rate) * penalty
 *
 * The net delta per game is: win_rate*bonus - (1-win_rate)*penalty
 * Positive → factor rises; negative → factor falls; zero → stable.
 *
 * Returns the net delta per game (not a factor value).
 */
static float math_net_delta(float win_rate, float win_bonus, float loss_penalty) {
    return win_rate * win_bonus - (1.0f - win_rate) * loss_penalty;
}

/* =========================================================
 * Run one full configuration test for a given skill level
 * ========================================================= */

/*
 * run_skill_test — simulate 100 games for one skill level across all configs.
 *
 * target_direction:
 *    1  → hidden_factor should finish > 1.0 (smurf)
 *    0  → hidden_factor should finish ≈ 1.0 (normal, tolerance 0.05)
 *   -1  → hidden_factor should finish < 1.0 (hardstuck)
 */
static void run_skill_test(const char *skill_label,
                            float       win_rate,
                            int         target_direction)
{
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  SKILL: %-10s  (base WR = %.0f%%)%*s║\n",
           skill_label, win_rate * 100.0f,
           (int)(26 - strlen(skill_label)), " ");
    printf("╚══════════════════════════════════════════════════════════════╝\n");

    const char *target_str =
        (target_direction > 0)  ? "> 1.0 (smurf should be boosted)" :
        (target_direction < 0)  ? "< 1.0 (hardstuck should be penalized)" :
                                   "= 1.0 (normal should stabilize)";

    printf("  Target: hidden_factor %s\n\n", target_str);

    /* Use different seeds for each skill level for variety */
    unsigned int seed = (unsigned int)(win_rate * 10000.0f + 42u);

    float final_factors[NUM_CONFIGS];
    float net_deltas[NUM_CONFIGS];

    for (int c = 0; c < NUM_CONFIGS; c++) {
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        printf("  Config %d: %s\n", c + 1, CONFIGS[c].name);
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

        float final = simulate_100_games(skill_label, win_rate,
                                         CONFIGS[c].win_bonus,
                                         CONFIGS[c].loss_penalty,
                                         seed);

        final_factors[c] = final;
        net_deltas[c]    = math_net_delta(win_rate,
                                           CONFIGS[c].win_bonus,
                                           CONFIGS[c].loss_penalty);

        /* Evaluate pass/fail */
        int passed;
        if (target_direction > 0)
            passed = (final > 1.0f);
        else if (target_direction < 0)
            passed = (final < 1.0f);
        else
            passed = (fabsf(final - 1.0f) <= 0.05f);

        const char *status = passed ? "✅ PASS" : "❌ FAIL";

        g_tests_run++;
        if (passed) g_tests_passed++;
        else         g_tests_failed++;

        printf("\n");
        printf("  ┌─ Convergence summary ─────────────────────────────────────┐\n");
        printf("  │  Final hidden_factor : %.4f                               │\n",  final);
        printf("  │  Math net delta/game : %+.5f                              │\n", net_deltas[c]);
        printf("  │  Target              : hidden_factor %s                │\n",   target_str);
        printf("  │  Status              : %s                                 │\n", status);
        printf("  └───────────────────────────────────────────────────────────┘\n");
        printf("\n");
    }

    /* ── Cross-config summary table for this skill level ── */
    printf("  ┌─ Summary: %s (WR=%.0f%%) across all configs ──────────────┐\n",
           skill_label, win_rate * 100.0f);
    printf("  │  Config │ Final factor │ Net Δ/game │ Status        │\n");
    printf("  │  -------|--------------|------------|------------── │\n");

    for (int c = 0; c < NUM_CONFIGS; c++) {
        int passed;
        if (target_direction > 0)
            passed = (final_factors[c] > 1.0f);
        else if (target_direction < 0)
            passed = (final_factors[c] < 1.0f);
        else
            passed = (fabsf(final_factors[c] - 1.0f) <= 0.05f);

        printf("  │    %d    │   %.4f     │  %+.5f  │ %s  │\n",
               c + 1, final_factors[c], net_deltas[c],
               passed ? "✅ PASS" : "❌ FAIL");
    }
    printf("  └───────────────────────────────────────────────────────────┘\n");
}

/* =========================================================
 * Per-skill test functions (one per skill level)
 * ========================================================= */

static void test_smurf(void) {
    run_skill_test("Smurf", 0.58f, 1);
}

static void test_normal(void) {
    run_skill_test("Normal", 0.50f, 0);
}

static void test_hardstuck(void) {
    run_skill_test("Hardstuck", 0.42f, -1);
}

/* =========================================================
 * Recommendation report
 * ========================================================= */

/*
 * print_recommendation — compare all configs on the three key metrics:
 *   1. Smurf factor > 1.0
 *   2. Normal factor ≈ 1.0
 *   3. Hardstuck factor < 1.0
 *   4. Relative advantage: smurf_factor / hardstuck_factor ratio
 *
 * The config that passes all three and has the highest ratio wins.
 */
static void print_recommendation(void) {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              COEFFICIENT RECOMMENDATION REPORT              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("  Mathematical analysis: net hidden_factor Δ per 100 games\n\n");
    printf("  %-42s | Smurf(58%%) | Normal(50%%) | Hardstuck(42%%) | Ratio\n",
           "Configuration");
    printf("  %s\n",
           "--------------------------------------------|-----------|------------|----------------|-------");

    /* Fixed seed, same as used in the per-skill tests */
    unsigned int seeds[3] = {5800u + 42u, 5000u + 42u, 4200u + 42u};
    float win_rates[3]    = {0.58f, 0.50f, 0.42f};

    int best_config = -1;
    float best_score = -1.0f;

    for (int c = 0; c < NUM_CONFIGS; c++) {
        float factors[3];
        for (int s = 0; s < 3; s++) {
            float factor = HIDDEN_FACTOR_START;
            srand(seeds[s]);
            for (int g = 0; g < NUM_GAMES; g++) {
                int won = ((float)rand() / (float)RAND_MAX) < win_rates[s];
                if (won) {
                    factor += CONFIGS[c].win_bonus;
                    if (factor > HIDDEN_FACTOR_MAX) factor = HIDDEN_FACTOR_MAX;
                } else {
                    factor -= CONFIGS[c].loss_penalty;
                    if (factor < HIDDEN_FACTOR_MIN) factor = HIDDEN_FACTOR_MIN;
                }
            }
            factors[s] = factor;
        }

        float ratio = (factors[2] > 0.001f) ? (factors[0] / factors[2]) : 0.0f;

        int smurf_ok    = (factors[0] > 1.0f);
        int normal_ok   = (fabsf(factors[1] - 1.0f) <= 0.05f);
        int hardstuck_ok = (factors[2] < 1.0f);
        int all_pass    = smurf_ok && normal_ok && hardstuck_ok;

        printf("  %-42s | %6.4f %s | %7.4f %s | %9.4f %s   | %.3f %s\n",
               CONFIGS[c].name,
               factors[0], smurf_ok    ? "✅" : "❌",
               factors[1], normal_ok   ? "✅" : "❌",
               factors[2], hardstuck_ok ? "✅" : "❌",
               ratio,
               all_pass ? "⭐" : "  ");

        /* Choose config with best ratio that passes all three checks */
        if (all_pass && ratio > best_score) {
            best_score  = ratio;
            best_config = c;
        }
    }

    printf("\n");
    if (best_config >= 0) {
        printf("  ★ RECOMMENDED CONFIG: %d — %s\n", best_config + 1,
               CONFIGS[best_config].name);
        printf("    → Best relative advantage ratio: %.3f\n", best_score);
        printf("    → Smurf > 1.0, Normal ≈ 1.0, Hardstuck < 1.0 ✅\n");
    } else {
        printf("  ⚠  No configuration passes all three criteria.\n");
        printf("  → Consider tuning WIN/LOSS values to satisfy:\n");
        printf("       win_rate * WIN_BONUS > (1 - win_rate) * LOSS_PENALTY  (for smurfs)\n");
        printf("       win_rate * WIN_BONUS = (1 - win_rate) * LOSS_PENALTY  (for 50%% WR)\n");
        printf("       win_rate * WIN_BONUS < (1 - win_rate) * LOSS_PENALTY  (for hardstuck)\n");
    }
    printf("\n");
}

/* =========================================================
 * Main
 * ========================================================= */

int main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║    EOMM — Coefficient Analysis: WIN_BONUS vs LOSS_PENALTY   ║\n");
    printf("║    100-game simulation × 3 skill levels × 5 configurations  ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("  hidden_factor range: [%.2f .. %.2f]  start: %.2f\n\n",
           HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX, HIDDEN_FACTOR_START);

    test_smurf();
    test_normal();
    test_hardstuck();

    print_recommendation();

    /* ── Final results ── */
    printf("══════════════════════════════════════════════════════════════\n");
    printf("  RESULTS: %d passed, %d failed  (total: %d)\n",
           g_tests_passed, g_tests_failed, g_tests_run);
    printf("══════════════════════════════════════════════════════════════\n\n");

    return (g_tests_failed > 0) ? 1 : 0;
}
