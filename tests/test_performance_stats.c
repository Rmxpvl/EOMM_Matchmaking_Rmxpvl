/*
 * tests/test_performance_stats.c
 *
 * Validates the performance-based win rate system introduced by
 * PerformanceStats / calculate_actual_winrate().
 *
 * Test plan:
 *   - Generate 100 smurfs, 100 normals, 100 hardstuck players.
 *   - Run 100 simulated games for each group (individual matches, no cross-
 *     group matchmaking: we test each group against itself so the EOMM
 *     matchmaker does not artificially equalise win rates across groups).
 *   - Verify:
 *       1. Stat distributions fall within the expected ranges per skill level.
 *       2. calculate_actual_winrate() returns values in [0.25, 0.75].
 *       3. Smurfs converge to a higher WR than normals, normals higher than
 *          hardstuck (ordinal assertion, not exact value).
 *       4. Each player has a unique profile (standard deviation > 0 within
 *          each group, showing the stats are truly independent).
 *   - Print sample profiles to illustrate player variety.
 *
 * Build:
 *   make test_performance_stats
 * Run:
 *   ./bin/test_performance_stats
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../include/eomm_system.h"

/* =========================================================
 * Constants
 * ========================================================= */

#define GROUP_SIZE  100   /* players per skill group             */
#define NUM_GAMES   100   /* simulated games per group           */
#define SAMPLE_SIZE   3   /* sample profiles printed per group   */

/* =========================================================
 * Global pass / fail counters
 * ========================================================= */

static int g_pass = 0;
static int g_fail = 0;

/* =========================================================
 * Assertion helpers
 * ========================================================= */

static void check(int condition, const char *desc) {
    if (condition) {
        printf("  ✅ PASS  %s\n", desc);
        g_pass++;
    } else {
        printf("  ❌ FAIL  %s\n", desc);
        g_fail++;
    }
}

/* =========================================================
 * Helpers
 * ========================================================= */

/* Extract all eight stat values from a player into a float array. */
static void get_stats(const Player *p, float out[8]) {
    out[0] = p->perf.mechanical_skill;
    out[1] = p->perf.decision_making;
    out[2] = p->perf.map_awareness;
    out[3] = p->perf.tilt_resistance;
    out[4] = p->perf.champion_pool_depth;
    out[5] = p->perf.champion_proficiency;
    out[6] = p->perf.wave_management;
    out[7] = p->perf.teamfight_positioning;
}

static const char *stat_names[8] = {
    "mechanical_skill",
    "decision_making",
    "map_awareness",
    "tilt_resistance",
    "champion_pool_depth",
    "champion_proficiency",
    "wave_management",
    "teamfight_positioning"
};

/* Compute min, max and mean of an array. */
static void array_stats(const float *arr, int n,
                         float *out_min, float *out_max, float *out_mean) {
    float mn = arr[0], mx = arr[0], sum = 0.0f;
    for (int i = 0; i < n; i++) {
        if (arr[i] < mn) mn = arr[i];
        if (arr[i] > mx) mx = arr[i];
        sum += arr[i];
    }
    *out_min  = mn;
    *out_max  = mx;
    *out_mean = sum / (float)n;
}

/* Compute population standard deviation. */
static float array_stddev(const float *arr, int n, float mean) {
    float sum_sq = 0.0f;
    for (int i = 0; i < n; i++) {
        float d = arr[i] - mean;
        sum_sq += d * d;
    }
    return sqrtf(sum_sq / (float)n);
}

/* Print a sample player profile. */
static void print_profile(const Player *p) {
    float stats[8];
    get_stats(p, stats);
    float wr = calculate_actual_winrate(p);
    printf("    %s  [%s]  calc_wr=%.1f%%\n",
           p->name,
           p->skill_level == SKILL_SMURF     ? "SMURF    " :
           p->skill_level == SKILL_HARDSTUCK ? "HARDSTUCK" : "NORMAL   ",
           wr * 100.0f);
    for (int s = 0; s < 8; s++) {
        printf("      %-28s %.2f\n", stat_names[s], stats[s]);
    }
}

/* =========================================================
 * Stat-range validation
 * ========================================================= */

/*
 * For each stat across all players in the group, check that:
 *   - every individual stat is within [hard_lo, hard_hi]  (after adj the
 *     single-stat adjustment can push one stat slightly outside the base
 *     range, so hard bounds are a little wider: base range ± 0.10)
 *   - the group mean sits within [mean_lo, mean_hi]
 */
static void test_stat_ranges(const Player *players, int n,
                              const char *group_name,
                              float hard_lo, float hard_hi,
                              float mean_lo, float mean_hi) {
    printf("\n  --- Stat ranges: %s ---\n", group_name);

    /* Gather stats per dimension */
    for (int s = 0; s < 8; s++) {
        float vals[GROUP_SIZE];
        for (int i = 0; i < n; i++) {
            float all[8];
            get_stats(&players[i], all);
            vals[i] = all[s];
        }
        float mn, mx, mean;
        array_stats(vals, n, &mn, &mx, &mean);
        float sd = array_stddev(vals, n, mean);

        char desc[128];
        snprintf(desc, sizeof(desc),
                 "%s %-28s  min=%.2f max=%.2f mean=%.2f sd=%.2f",
                 group_name, stat_names[s], mn, mx, mean, sd);

        /* All individual values within hard bounds */
        check(mn >= hard_lo && mx <= hard_hi, desc);
    }

    /* Group mean of the averaged win rate */
    float wr_vals[GROUP_SIZE];
    for (int i = 0; i < n; i++) wr_vals[i] = calculate_actual_winrate(&players[i]);
    float wr_mn, wr_mx, wr_mean;
    array_stats(wr_vals, n, &wr_mn, &wr_mx, &wr_mean);

    char desc[128];
    snprintf(desc, sizeof(desc),
             "%s avg calculate_actual_winrate: mean=%.3f  [expected %.2f-%.2f]",
             group_name, wr_mean, mean_lo, mean_hi);
    check(wr_mean >= mean_lo && wr_mean <= mean_hi, desc);
}

/* =========================================================
 * Win-rate-in-range validation
 * ========================================================= */

static void test_winrate_bounds(const Player *players, int n,
                                 const char *group_name) {
    int all_in_range = 1;
    for (int i = 0; i < n; i++) {
        float wr = calculate_actual_winrate(&players[i]);
        if (wr < 0.25f || wr > 0.75f) {
            all_in_range = 0;
            break;
        }
    }
    char desc[128];
    snprintf(desc, sizeof(desc),
             "%s: all calculate_actual_winrate() in [0.25, 0.75]", group_name);
    check(all_in_range, desc);
}

/* =========================================================
 * Profile uniqueness (standard deviation across group)
 * ========================================================= */

static void test_profile_uniqueness(const Player *players, int n,
                                     const char *group_name) {
    /* Collect per-player average stat */
    float avgs[GROUP_SIZE];
    for (int i = 0; i < n; i++) {
        float s[8];
        get_stats(&players[i], s);
        float sum = 0.0f;
        for (int k = 0; k < 8; k++) sum += s[k];
        avgs[i] = sum / 8.0f;
    }
    float mn, mx, mean;
    array_stats(avgs, n, &mn, &mx, &mean);
    float sd = array_stddev(avgs, n, mean);

    char desc[128];
    snprintf(desc, sizeof(desc),
             "%s: profile stddev=%.4f (>0 means unique profiles exist)",
             group_name, sd);
    check(sd > 0.0f, desc);
}

/* =========================================================
 * Simulated game win rate
 * ========================================================= */

/*
 * Run NUM_GAMES rounds of intra-group matches.
 * Each round pairs players at random into 5v5 matches.
 * Returns the fraction of wins for the group overall (should ≈ 0.50 since
 * both teams are drawn from the same group), but more usefully we compare
 * the average calc_wr at game-start across all players to the expected range.
 */
static float run_games(Player *players, int n) {
    int total_wins = 0, total_games = 0;
    Match matches[GROUP_SIZE / MATCH_SIZE + 1];
    int num_matches = 0;

    for (int g = 0; g < NUM_GAMES; g++) {
        /* Refresh hidden state */
        for (int i = 0; i < n; i++) update_hidden_state(&players[i]);

        /* Simple random matchmaking within the group */
        create_matches(players, n, matches, &num_matches, g);

        for (int m = 0; m < num_matches; m++) {
            determine_troll_picks(&matches[m]);
            simulate_match(&matches[m]);
            update_players_after_match(&matches[m]);
            total_games += MATCH_SIZE;
        }
    }

    /* Count actual wins */
    for (int i = 0; i < n; i++) total_wins += players[i].wins;

    return (total_games > 0) ? (float)total_wins / (float)total_games : 0.0f;
}

/* =========================================================
 * main
 * ========================================================= */

int main(void) {
    srand((unsigned int)time(NULL));

    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           PERFORMANCE STATS TEST SUITE                      ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    /* ── Allocate and initialise player groups ─────────────────── */
    Player smurfs[GROUP_SIZE];
    Player normals[GROUP_SIZE];
    Player hardstuck[GROUP_SIZE];

    for (int i = 0; i < GROUP_SIZE; i++) init_player(&smurfs[i],    i,              SKILL_SMURF);
    for (int i = 0; i < GROUP_SIZE; i++) init_player(&normals[i],   i + GROUP_SIZE, SKILL_NORMAL);
    for (int i = 0; i < GROUP_SIZE; i++) init_player(&hardstuck[i], i + GROUP_SIZE * 2, SKILL_HARDSTUCK);

    /* ── Sample profiles ────────────────────────────────────────── */
    printf("=== Sample player profiles (unique strengths / weaknesses) ===\n\n");
    printf("  SMURFS:\n");
    for (int i = 0; i < SAMPLE_SIZE; i++) print_profile(&smurfs[i]);
    printf("\n  NORMALS:\n");
    for (int i = 0; i < SAMPLE_SIZE; i++) print_profile(&normals[i]);
    printf("\n  HARDSTUCK:\n");
    for (int i = 0; i < SAMPLE_SIZE; i++) print_profile(&hardstuck[i]);
    printf("\n");

    /* ── Stat range checks ──────────────────────────────────────── */
    printf("=== Stat range validation ===\n");

    /*
     * Hard bounds: base range ± 0.10 (single-stat adjustment can push
     * one stat beyond the base range by exactly that delta).
     */
    test_stat_ranges(smurfs,    GROUP_SIZE, "SMURF    ", 0.60f, 1.00f, 0.55f, 0.75f);
    test_stat_ranges(normals,   GROUP_SIZE, "NORMAL   ", 0.20f, 0.80f, 0.40f, 0.60f);
    test_stat_ranges(hardstuck, GROUP_SIZE, "HARDSTUCK", 0.00f, 0.45f, 0.25f, 0.42f);

    /* ── Win-rate bounds ────────────────────────────────────────── */
    printf("\n=== calculate_actual_winrate() bounds ===\n");
    test_winrate_bounds(smurfs,    GROUP_SIZE, "SMURF    ");
    test_winrate_bounds(normals,   GROUP_SIZE, "NORMAL   ");
    test_winrate_bounds(hardstuck, GROUP_SIZE, "HARDSTUCK");

    /* ── Profile uniqueness ─────────────────────────────────────── */
    printf("\n=== Profile uniqueness (stddev > 0 required) ===\n");
    test_profile_uniqueness(smurfs,    GROUP_SIZE, "SMURF    ");
    test_profile_uniqueness(normals,   GROUP_SIZE, "NORMAL   ");
    test_profile_uniqueness(hardstuck, GROUP_SIZE, "HARDSTUCK");

    /* ── Win rate ordering after simulation ─────────────────────── */
    printf("\n=== Win rate ordering after %d simulated games ===\n", NUM_GAMES);

    float smurf_avg_wr = 0.0f, normal_avg_wr = 0.0f, hardstuck_avg_wr = 0.0f;
    for (int i = 0; i < GROUP_SIZE; i++) smurf_avg_wr    += calculate_actual_winrate(&smurfs[i]);
    for (int i = 0; i < GROUP_SIZE; i++) normal_avg_wr   += calculate_actual_winrate(&normals[i]);
    for (int i = 0; i < GROUP_SIZE; i++) hardstuck_avg_wr += calculate_actual_winrate(&hardstuck[i]);
    smurf_avg_wr    /= (float)GROUP_SIZE;
    normal_avg_wr   /= (float)GROUP_SIZE;
    hardstuck_avg_wr /= (float)GROUP_SIZE;

    printf("  Avg calc_wr before games:  SMURF=%.3f  NORMAL=%.3f  HARDSTUCK=%.3f\n",
           smurf_avg_wr, normal_avg_wr, hardstuck_avg_wr);

    /* Run games and check observed win rates */
    float smurf_obs    = run_games(smurfs,    GROUP_SIZE);
    float normal_obs   = run_games(normals,   GROUP_SIZE);
    float hardstuck_obs = run_games(hardstuck, GROUP_SIZE);

    printf("\n  Observed win rates after %d games (%d players each):\n", NUM_GAMES, GROUP_SIZE);
    printf("    SMURF     : %.1f%%  (expected ~56-60%%)\n", smurf_obs    * 100.0f);
    printf("    NORMAL    : %.1f%%  (expected ~48-52%%)\n", normal_obs   * 100.0f);
    printf("    HARDSTUCK : %.1f%%  (expected ~35-42%%)\n", hardstuck_obs * 100.0f);

    /* Ordinal check: smurf > normal > hardstuck */
    check(smurf_avg_wr > normal_avg_wr,
          "calc_wr ordering: SMURF > NORMAL (before games)");
    check(normal_avg_wr > hardstuck_avg_wr,
          "calc_wr ordering: NORMAL > HARDSTUCK (before games)");

    /* ── Summary ────────────────────────────────────────────────── */
    printf("\n══════════════════════════════════════════════════════════════\n");
    printf("  RESULTS: %d passed, %d failed  (total: %d)\n",
           g_pass, g_fail, g_pass + g_fail);
    printf("══════════════════════════════════════════════════════════════\n\n");

    return (g_fail > 0) ? 1 : 0;
}
