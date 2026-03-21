/*
 * tests/test_debug_autofill.c
 *
 * Debug test: validates autofill base-risk constants and their actual usage.
 *
 * Tests:
 *   1. get_base_autofill_risk()  — prints actual value returned for every role
 *   2. calculate_autofill_chance() — all roles × NEUTRAL/NEGATIVE/POSITIVE states
 *   3. Role preference distribution — 200 players, counts prefRoles[0/1] per role
 *   4. should_autofill() probability — 10 000 rolls per role, observed vs expected (±2%)
 *   5. assign_autofill_role() — 1 000 calls, pref={ADC,Support}, verifies no leakage
 *
 * Build:
 *   make test_debug_autofill
 * Run:
 *   ./bin/test_debug_autofill
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../include/eomm_system.h"

/* =========================================================
 * Helpers
 * ========================================================= */

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define CHECK(cond, msg) do {                               \
    g_tests_run++;                                          \
    if (cond) {                                             \
        printf("  [PASS] %s\n", (msg));                     \
        g_tests_passed++;                                   \
    } else {                                                \
        printf("  [FAIL] %s\n", (msg));                     \
        g_tests_failed++;                                   \
    }                                                       \
} while (0)

#define CHECK_FLOAT(a, b, eps, msg) \
    CHECK(fabsf((a) - (b)) < (eps), (msg))

static void section(const char *title) {
    printf("\n══════════════════════════════════════════════════════════════\n");
    printf("  %s\n", title);
    printf("══════════════════════════════════════════════════════════════\n");
}

/* =========================================================
 * Role name helper
 * ========================================================= */
static const char *role_name(int role) {
    switch (role) {
        case ROLE_TOP:     return "Top    ";
        case ROLE_JUNGLE:  return "Jungle ";
        case ROLE_MID:     return "Mid    ";
        case ROLE_ADC:     return "ADC    ";
        case ROLE_SUPPORT: return "Support";
        default:           return "Unknown";
    }
}

/* =========================================================
 * Test 1 — get_base_autofill_risk(): print actual values
 * ========================================================= */
static void test_base_risk_values(void) {
    section("TEST 1: get_base_autofill_risk() — actual values per role");

    static const struct { int role; float expected; } cases[] = {
        { ROLE_SUPPORT,  5.0f },
        { ROLE_JUNGLE,   8.0f },
        { ROLE_MID,     12.0f },
        { ROLE_TOP,     15.0f },
        { ROLE_ADC,     18.0f },
    };
    int n = (int)(sizeof(cases) / sizeof(cases[0]));

    printf("  %-9s  %12s  %12s  %s\n",
           "Role", "Actual (%)", "Expected (%)", "Match?");
    printf("  --------------------------------------------------\n");

    for (int i = 0; i < n; i++) {
        float actual   = get_base_autofill_risk(cases[i].role);
        int   ok       = fabsf(actual - cases[i].expected) < 0.001f;
        printf("  %-9s  %12.2f  %12.2f  %s\n",
               role_name(cases[i].role), actual, cases[i].expected,
               ok ? "YES" : "NO  *** MISMATCH ***");

        char label[64];
        snprintf(label, sizeof(label),
                 "%s: get_base_autofill_risk() == %.0f%%",
                 role_name(cases[i].role), cases[i].expected);
        CHECK_FLOAT(actual, cases[i].expected, 0.001f, label);
    }

    /* Ordering guarantee */
    CHECK(get_base_autofill_risk(ROLE_SUPPORT) < get_base_autofill_risk(ROLE_JUNGLE),
          "Ordering: Support < Jungle");
    CHECK(get_base_autofill_risk(ROLE_JUNGLE)  < get_base_autofill_risk(ROLE_MID),
          "Ordering: Jungle < Mid");
    CHECK(get_base_autofill_risk(ROLE_MID)     < get_base_autofill_risk(ROLE_TOP),
          "Ordering: Mid < Top");
    CHECK(get_base_autofill_risk(ROLE_TOP)     < get_base_autofill_risk(ROLE_ADC),
          "Ordering: Top < ADC");
}

/* =========================================================
 * Test 2 — calculate_autofill_chance(): all roles × all states
 * ========================================================= */
static void test_calculate_chance(void) {
    section("TEST 2: calculate_autofill_chance() — roles × hidden states");

    static const struct { HiddenState state; const char *name; } states[] = {
        { STATE_NEUTRAL,  "NEUTRAL " },
        { STATE_NEGATIVE, "NEGATIVE" },
        { STATE_POSITIVE, "POSITIVE" },
    };
    int ns = (int)(sizeof(states) / sizeof(states[0]));

    static const int roles[] = {
        ROLE_SUPPORT, ROLE_JUNGLE, ROLE_MID, ROLE_TOP, ROLE_ADC
    };
    int nr = (int)(sizeof(roles) / sizeof(roles[0]));

    printf("  %-9s  %-10s  %11s  %11s  %s\n",
           "Role", "State", "Actual (%)", "Expected (%)", "Match?");
    printf("  -------------------------------------------------------\n");

    Player p;
    memset(&p, 0, sizeof(p));
    p.hidden_factor = HIDDEN_FACTOR_START;

    for (int ri = 0; ri < nr; ri++) {
        float base = get_base_autofill_risk(roles[ri]);
        for (int si = 0; si < ns; si++) {
            p.hidden_state = states[si].state;
            float actual   = calculate_autofill_chance(&p, roles[ri]);
            float expected = base + (states[si].state == STATE_NEGATIVE
                                     ? AUTOFILL_EOMM_BONUS : 0.0f);
            int   ok       = fabsf(actual - expected) < 0.001f;
            printf("  %-9s  %-10s  %11.2f  %11.2f  %s\n",
                   role_name(roles[ri]), states[si].name,
                   actual, expected,
                   ok ? "YES" : "NO  *** MISMATCH ***");

            char label[96];
            snprintf(label, sizeof(label),
                     "%s + %s: chance == %.0f%%",
                     role_name(roles[ri]), states[si].name, expected);
            CHECK_FLOAT(actual, expected, 0.001f, label);
        }
    }

    /* Verify bonus is only applied for NEGATIVE */
    p.hidden_state = STATE_NEGATIVE;
    float neg = calculate_autofill_chance(&p, ROLE_TOP);
    p.hidden_state = STATE_NEUTRAL;
    float neu = calculate_autofill_chance(&p, ROLE_TOP);
    p.hidden_state = STATE_POSITIVE;
    float pos = calculate_autofill_chance(&p, ROLE_TOP);

    CHECK(neg > neu, "NEGATIVE chance > NEUTRAL chance (Top)");
    CHECK(neg > pos, "NEGATIVE chance > POSITIVE chance (Top)");
    CHECK_FLOAT(neu, pos, 0.001f,
                "NEUTRAL == POSITIVE chance (no bonus in either)");
    CHECK_FLOAT(neg - neu, AUTOFILL_EOMM_BONUS, 0.001f,
                "NEGATIVE bonus == AUTOFILL_EOMM_BONUS (10%)");
}

/* =========================================================
 * Test 3 — role preference distribution across 200 players
 * ========================================================= */
static void test_role_preference_distribution(void) {
    section("TEST 3: Role preference distribution — 200 players");

    int n = 200;
    Player *players = (Player *)malloc((size_t)n * sizeof(Player));
    if (!players) {
        fprintf(stderr, "  [ERROR] malloc failed\n");
        return;
    }

    init_players(players, n);

    int prim_count[ROLE_COUNT]  = {0};
    int sec_count[ROLE_COUNT]   = {0};
    int prim_eq_sec             = 0;

    for (int i = 0; i < n; i++) {
        int r0 = players[i].prefRoles[0];
        int r1 = players[i].prefRoles[1];
        if (r0 >= 0 && r0 < ROLE_COUNT) prim_count[r0]++;
        if (r1 >= 0 && r1 < ROLE_COUNT) sec_count[r1]++;
        if (r0 == r1) prim_eq_sec++;
    }

    printf("  Distribution of primary preference (prefRoles[0]):\n");
    printf("  %-9s  %7s  %8s  %s\n",
           "Role", "Count", "Pct (%)", "Expected ~20%");
    printf("  ----------------------------------------\n");
    for (int r = 0; r < ROLE_COUNT; r++) {
        float pct = 100.0f * (float)prim_count[r] / (float)n;
        printf("  %-9s  %7d  %8.1f%%  %s\n",
               role_name(r), prim_count[r], pct,
               (pct >= 10.0f && pct <= 30.0f) ? "within 10-30% range" : "*** OUT OF RANGE ***");
    }

    printf("\n  Distribution of secondary preference (prefRoles[1]):\n");
    printf("  %-9s  %7s  %8s  %s\n",
           "Role", "Count", "Pct (%)", "Expected ~20%");
    printf("  ----------------------------------------\n");
    for (int r = 0; r < ROLE_COUNT; r++) {
        float pct = 100.0f * (float)sec_count[r] / (float)n;
        printf("  %-9s  %7d  %8.1f%%  %s\n",
               role_name(r), sec_count[r], pct,
               (pct >= 10.0f && pct <= 30.0f) ? "within 10-30% range" : "*** OUT OF RANGE ***");
    }

    printf("\n  Players where prefRoles[0] == prefRoles[1]: %d (must be 0)\n",
           prim_eq_sec);

    /* Assertions */
    CHECK(prim_eq_sec == 0,
          "No player has identical primary and secondary preference");

    int total_valid_prim = 0;
    int total_valid_sec  = 0;
    for (int r = 0; r < ROLE_COUNT; r++) {
        float pct_p = 100.0f * (float)prim_count[r] / (float)n;
        float pct_s = 100.0f * (float)sec_count[r]  / (float)n;
        if (pct_p >= 10.0f && pct_p <= 30.0f) total_valid_prim++;
        if (pct_s >= 10.0f && pct_s <= 30.0f) total_valid_sec++;
    }
    CHECK(total_valid_prim == ROLE_COUNT,
          "All primary preferences are within 10-30% (uniform distribution)");
    CHECK(total_valid_sec == ROLE_COUNT,
          "All secondary preferences are within 10-30% (uniform distribution)");

    free(players);
}

/* =========================================================
 * Test 4 — should_autofill() statistical validation (10 000 rolls)
 * ========================================================= */
static void test_should_autofill_probability(void) {
    section("TEST 4: should_autofill() — 10 000 rolls per role (observed vs expected)");

    static const int roles[] = {
        ROLE_SUPPORT, ROLE_JUNGLE, ROLE_MID, ROLE_TOP, ROLE_ADC
    };
    int nr      = (int)(sizeof(roles) / sizeof(roles[0]));
    int n_rolls = 10000;
    float tolerance_pct = 2.0f; /* ±2 percentage points */

    Player p;
    memset(&p, 0, sizeof(p));
    p.hidden_factor = HIDDEN_FACTOR_START;
    p.hidden_state  = STATE_NEUTRAL;

    printf("  Rolls per role: %d    Tolerance: ±%.0f%%\n\n", n_rolls, tolerance_pct);
    printf("  %-9s  %12s  %12s  %12s  %s\n",
           "Role", "Expected (%)", "Observed (%)", "Delta (%)", "Within ±2%?");
    printf("  ---------------------------------------------------------------\n");

    int all_ok = 1;
    for (int ri = 0; ri < nr; ri++) {
        int hits = 0;
        for (int t = 0; t < n_rolls; t++) {
            if (should_autofill(&p, roles[ri])) hits++;
        }
        float expected = get_base_autofill_risk(roles[ri]);
        float observed = 100.0f * (float)hits / (float)n_rolls;
        float delta    = fabsf(observed - expected);
        int   ok       = (delta <= tolerance_pct);
        printf("  %-9s  %12.2f  %12.2f  %12.2f  %s\n",
               role_name(roles[ri]), expected, observed, delta,
               ok ? "YES" : "NO  *** OUT OF RANGE ***");
        if (!ok) all_ok = 0;

        char label[96];
        snprintf(label, sizeof(label),
                 "%s: observed %.1f%% within ±2%% of expected %.0f%%",
                 role_name(roles[ri]), observed, expected);
        CHECK(ok, label);
    }

    if (all_ok) {
        printf("\n  All roles: observed autofill rates match expected (±2%%) ✓\n");
    } else {
        printf("\n  WARNING: one or more roles deviated from expected rate by >2%%\n");
    }
}

/* =========================================================
 * Test 5 — assign_autofill_role(): no pref leakage (1 000 calls)
 * ========================================================= */
static void test_assign_autofill_no_leakage(void) {
    section("TEST 5: assign_autofill_role() — 1 000 calls, pref={ADC, Support}");

    Player p;
    memset(&p, 0, sizeof(p));
    p.hidden_factor = HIDDEN_FACTOR_START;
    p.hidden_state  = STATE_NEUTRAL;
    p.prefRoles[0]  = ROLE_ADC;
    p.prefRoles[1]  = ROLE_SUPPORT;

    int n_calls = 1000;
    int count[ROLE_COUNT] = {0};
    int leaks             = 0;
    int flag_errors       = 0;

    for (int i = 0; i < n_calls; i++) {
        /* Reset per-call state (but keep same prefs) */
        p.is_autofilled = 0;
        p.tilt_level    = 0;
        p.hidden_factor = HIDDEN_FACTOR_START;

        assign_autofill_role(&p);

        if (p.current_role >= 0 && p.current_role < ROLE_COUNT) {
            count[p.current_role]++;
        }
        if (p.current_role == ROLE_ADC || p.current_role == ROLE_SUPPORT) {
            leaks++;
        }
        if (p.is_autofilled != 1) {
            flag_errors++;
        }
    }

    printf("  Preferred roles: ADC, Support\n");
    printf("  Expected autofill targets: Top, Jungle, Mid only\n\n");
    printf("  %-9s  %7s  %8s  %s\n",
           "Role", "Count", "Pct (%)", "Should be assigned?");
    printf("  -----------------------------------------------\n");
    static const int expected_targets[] = { ROLE_TOP, ROLE_JUNGLE, ROLE_MID };
    int n_targets = 3;
    for (int r = 0; r < ROLE_COUNT; r++) {
        float pct = 100.0f * (float)count[r] / (float)n_calls;
        int is_valid = 0;
        for (int t = 0; t < n_targets; t++) {
            if (r == expected_targets[t]) { is_valid = 1; break; }
        }
        printf("  %-9s  %7d  %8.1f%%  %s\n",
               role_name(r), count[r], pct,
               is_valid ? "YES (valid target)" : (count[r] == 0 ? "NO  (correctly avoided)" : "NO  *** LEAK ***"));
    }

    printf("\n  Total leaks (ADC or Support assigned): %d / %d\n",
           leaks, n_calls);
    printf("  is_autofilled flag errors:             %d / %d\n",
           flag_errors, n_calls);

    /* Verify ADC and Support with roughly equal distribution across Top/Jungle/Mid */
    float top_pct    = 100.0f * (float)count[ROLE_TOP]    / (float)n_calls;
    float jungle_pct = 100.0f * (float)count[ROLE_JUNGLE] / (float)n_calls;
    float mid_pct    = 100.0f * (float)count[ROLE_MID]    / (float)n_calls;
    printf("\n  Top:    %.1f%%   Jungle: %.1f%%   Mid: %.1f%%"
           "   (expected ~33.3%% each)\n",
           top_pct, jungle_pct, mid_pct);

    /* Assertions */
    CHECK(leaks == 0,
          "assign_autofill_role() NEVER returns ADC or Support (1 000 trials)");
    CHECK(flag_errors == 0,
          "is_autofilled == 1 after every assign_autofill_role() call");
    CHECK(count[ROLE_ADC] == 0,
          "ADC count == 0 (ADC is a preferred role)");
    CHECK(count[ROLE_SUPPORT] == 0,
          "Support count == 0 (Support is a preferred role)");
    CHECK(count[ROLE_TOP] + count[ROLE_JUNGLE] + count[ROLE_MID] == n_calls,
          "All 1 000 assignments landed on Top, Jungle, or Mid");

    /* Each non-preferred role should be assigned with roughly 1/3 frequency */
    CHECK(top_pct    >= 20.0f && top_pct    <= 47.0f,
          "Top    assigned ~1/3 of the time (20-47% range)");
    CHECK(jungle_pct >= 20.0f && jungle_pct <= 47.0f,
          "Jungle assigned ~1/3 of the time (20-47% range)");
    CHECK(mid_pct    >= 20.0f && mid_pct    <= 47.0f,
          "Mid    assigned ~1/3 of the time (20-47% range)");
}

/* =========================================================
 * Main
 * ========================================================= */

int main(void) {
    srand(42); /* fixed seed for reproducibility */

    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║       EOMM Autofill — Debug Validation Test                  ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n  Validates base-risk constants, state bonuses, role distribution,\n");
    printf("  statistical autofill rates, and assignment correctness.\n");

    test_base_risk_values();
    test_calculate_chance();
    test_role_preference_distribution();
    test_should_autofill_probability();
    test_assign_autofill_no_leakage();

    section("DEBUG TEST SUMMARY");
    printf("  Total  : %d\n", g_tests_run);
    printf("  Passed : %d\n", g_tests_passed);
    printf("  Failed : %d\n", g_tests_failed);

    if (g_tests_failed == 0) {
        printf("\n  All debug checks PASSED. Constants and functions are correct.\n");
    } else {
        printf("\n  %d check(s) FAILED — review output above for details.\n",
               g_tests_failed);
    }

    return (g_tests_failed == 0) ? 0 : 1;
}
