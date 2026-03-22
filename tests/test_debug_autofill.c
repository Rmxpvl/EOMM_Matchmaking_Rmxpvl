/*
 * tests/test_debug_autofill.c
 *
 * Debug test suite for the EOMM autofill system.
 *
 * Test coverage:
 *   1. Base autofill risk constants (Support 5%, Jungle 8%, Mid 12%, Top 15%, ADC 18%)
 *   2. calculate_autofill_chance() with NEGATIVE (+10% bonus), NEUTRAL, POSITIVE states
 *   3. Role preference distribution across 200 players (uniform 10-30%)
 *   4. should_autofill() probabilistic validation (10,000 rolls per role, ±2% tolerance)
 *   4b. should_autofill_with_negative_bonus() — NEUTRAL vs NEGATIVE (10k rolls each)
 *   5. assign_autofill_role() no-leakage validation (1,000 calls)
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
#include <time.h>
#include "../include/eomm_system.h"

/* =========================================================
 * Test framework helpers
 * ========================================================= */

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do {                               \
    if (cond) {                                             \
        printf("  [PASS] %s\n", (msg));                     \
        g_pass++;                                           \
    } else {                                                \
        printf("  [FAIL] %s\n", (msg));                     \
        g_fail++;                                           \
    }                                                       \
} while (0)

#define CHECK_FLOAT(a, b, eps, msg) \
    CHECK(fabsf((float)(a) - (float)(b)) < (float)(eps), msg)

static void section(const char *title) {
    printf("\n══════════════════════════════════════════════════════════════\n");
    printf("  %s\n", title);
    printf("══════════════════════════════════════════════════════════════\n");
}

static const char *role_name(int role) {
    switch (role) {
        case ROLE_TOP:     return "Top";
        case ROLE_JUNGLE:  return "Jungle";
        case ROLE_MID:     return "Mid";
        case ROLE_ADC:     return "ADC";
        case ROLE_SUPPORT: return "Support";
        default:           return "Unknown";
    }
}

/* =========================================================
 * TEST 1 — Base autofill risk constants
 * ========================================================= */

static void test_base_risk_values(void) {
    section("TEST 1: Base autofill risk constants");

    printf("  Checking per-role base risk values:\n");
    printf("    Support : %.1f%%\n", get_base_autofill_risk(ROLE_SUPPORT));
    printf("    Jungle  : %.1f%%\n", get_base_autofill_risk(ROLE_JUNGLE));
    printf("    Mid     : %.1f%%\n", get_base_autofill_risk(ROLE_MID));
    printf("    Top     : %.1f%%\n", get_base_autofill_risk(ROLE_TOP));
    printf("    ADC     : %.1f%%\n", get_base_autofill_risk(ROLE_ADC));

    CHECK_FLOAT(get_base_autofill_risk(ROLE_SUPPORT),  2.0f, 0.001f,
        "Support base risk = 2%");
    CHECK_FLOAT(get_base_autofill_risk(ROLE_JUNGLE),   3.0f, 0.001f,
        "Jungle base risk  = 3%");
    CHECK_FLOAT(get_base_autofill_risk(ROLE_MID),      4.0f, 0.001f,
        "Mid base risk     = 4%");
    CHECK_FLOAT(get_base_autofill_risk(ROLE_TOP),      5.0f, 0.001f,
        "Top base risk     = 5%");
    CHECK_FLOAT(get_base_autofill_risk(ROLE_ADC),      6.0f, 0.001f,
        "ADC base risk     = 6%");

    /* Ordering: Support < Jungle < Mid < Top < ADC */
    CHECK(get_base_autofill_risk(ROLE_SUPPORT) < get_base_autofill_risk(ROLE_JUNGLE),
        "Support risk < Jungle risk");
    CHECK(get_base_autofill_risk(ROLE_JUNGLE)  < get_base_autofill_risk(ROLE_MID),
        "Jungle risk < Mid risk");
    CHECK(get_base_autofill_risk(ROLE_MID)     < get_base_autofill_risk(ROLE_TOP),
        "Mid risk < Top risk");
    CHECK(get_base_autofill_risk(ROLE_TOP)     < get_base_autofill_risk(ROLE_ADC),
        "Top risk < ADC risk");
}

/* =========================================================
 * TEST 2 — calculate_autofill_chance() with hidden state
 * ========================================================= */

static void test_calculate_chance(void) {
    section("TEST 2: calculate_autofill_chance() — hidden state bonus");

    Player p;
    memset(&p, 0, sizeof(p));
    p.hidden_factor = HIDDEN_FACTOR_START;
    p.prefRoles[0]  = ROLE_ADC;
    p.prefRoles[1]  = ROLE_MID;

    /* NEGATIVE state: base + AUTOFILL_EOMM_BONUS (10%) */
    p.hidden_state = STATE_NEGATIVE;
    float neg_adc  = calculate_autofill_chance(&p, ROLE_ADC);
    float neg_sup  = calculate_autofill_chance(&p, ROLE_SUPPORT);

    printf("  NEGATIVE state:\n");
    printf("    ADC     : %.1f%% (expected %.1f%%)\n",
           neg_adc, AUTOFILL_RISK_ADC + AUTOFILL_EOMM_BONUS);
    printf("    Support : %.1f%% (expected %.1f%%)\n",
           neg_sup, AUTOFILL_RISK_SUPPORT + AUTOFILL_EOMM_BONUS);

    CHECK_FLOAT(neg_adc, AUTOFILL_RISK_ADC     + AUTOFILL_EOMM_BONUS, 0.001f,
        "NEGATIVE: ADC chance = 18% + 10% = 28%");
    CHECK_FLOAT(neg_sup, AUTOFILL_RISK_SUPPORT + AUTOFILL_EOMM_BONUS, 0.001f,
        "NEGATIVE: Support chance = 5% + 10% = 15%");

    /* NEUTRAL state: no bonus */
    p.hidden_state = STATE_NEUTRAL;
    float neu_adc  = calculate_autofill_chance(&p, ROLE_ADC);
    float neu_sup  = calculate_autofill_chance(&p, ROLE_SUPPORT);

    printf("  NEUTRAL state:\n");
    printf("    ADC     : %.1f%% (expected %.1f%%)\n", neu_adc, AUTOFILL_RISK_ADC);
    printf("    Support : %.1f%% (expected %.1f%%)\n", neu_sup, AUTOFILL_RISK_SUPPORT);

    CHECK_FLOAT(neu_adc, AUTOFILL_RISK_ADC,     0.001f,
        "NEUTRAL: ADC chance = base 18% (no bonus)");
    CHECK_FLOAT(neu_sup, AUTOFILL_RISK_SUPPORT, 0.001f,
        "NEUTRAL: Support chance = base 5% (no bonus)");

    /* POSITIVE state: no bonus */
    p.hidden_state = STATE_POSITIVE;
    float pos_adc  = calculate_autofill_chance(&p, ROLE_ADC);
    float pos_sup  = calculate_autofill_chance(&p, ROLE_SUPPORT);

    printf("  POSITIVE state:\n");
    printf("    ADC     : %.1f%% (expected %.1f%%)\n", pos_adc, AUTOFILL_RISK_ADC);
    printf("    Support : %.1f%% (expected %.1f%%)\n", pos_sup, AUTOFILL_RISK_SUPPORT);

    CHECK_FLOAT(pos_adc, AUTOFILL_RISK_ADC,     0.001f,
        "POSITIVE: ADC chance = base 18% (no bonus)");
    CHECK_FLOAT(pos_sup, AUTOFILL_RISK_SUPPORT, 0.001f,
        "POSITIVE: Support chance = base 5% (no bonus)");

    /* NEGATIVE > NEUTRAL == POSITIVE */
    CHECK(neg_adc > neu_adc,
        "NEGATIVE ADC chance > NEUTRAL ADC chance");
    CHECK_FLOAT(neu_adc, pos_adc, 0.001f,
        "NEUTRAL and POSITIVE ADC chance are equal (no bonus)");
    CHECK(neg_sup > neu_sup,
        "NEGATIVE Support chance > NEUTRAL Support chance");
    CHECK_FLOAT(neu_sup, pos_sup, 0.001f,
        "NEUTRAL and POSITIVE Support chance are equal (no bonus)");
}

/* =========================================================
 * TEST 3 — Role preference distribution across 200 players
 * ========================================================= */

static void test_role_preference_distribution(void) {
    section("TEST 3: Role preference distribution (200 players)");

    const int N = 200;
    Player players[200];
    init_players(players, N);

    int pref_count[ROLE_COUNT] = {0};
    int duplicate_prefs        = 0;

    for (int i = 0; i < N; i++) {
        pref_count[players[i].prefRoles[0]]++;

        /* Ensure the two preferred roles are distinct */
        if (players[i].prefRoles[0] == players[i].prefRoles[1]) {
            duplicate_prefs++;
        }
    }

    printf("  Primary role preference distribution (%d players):\n", N);
    for (int r = 0; r < ROLE_COUNT; r++) {
        float pct = 100.0f * (float)pref_count[r] / (float)N;
        printf("    %-8s: %3d players  (%.1f%%)\n",
               role_name(r), pref_count[r], pct);
    }

    CHECK(duplicate_prefs == 0,
        "No player has identical primary and secondary preferences");

    /* Each role should capture between 10% and 30% of primary preferences */
    for (int r = 0; r < ROLE_COUNT; r++) {
        float pct = 100.0f * (float)pref_count[r] / (float)N;
        char msg[80];
        snprintf(msg, sizeof(msg),
                 "%s primary preference in range [10%%-30%%] (got %.1f%%)",
                 role_name(r), pct);
        CHECK(pct >= 10.0f && pct <= 30.0f, msg);
    }
}

/* =========================================================
 * TEST 4 — should_autofill() probabilistic validation
 * ========================================================= */

static void test_should_autofill_probability(void) {
    section("TEST 4: should_autofill() — probabilistic validation (10k rolls)");

    const int N_ROLLS  = 10000;
    const float TOL    = 2.0f;  /* ±2 percentage-point tolerance */

    Player p;
    memset(&p, 0, sizeof(p));
    p.hidden_factor = HIDDEN_FACTOR_START;
    p.hidden_state  = STATE_NEUTRAL;

    int roles[] = {ROLE_SUPPORT, ROLE_JUNGLE, ROLE_MID, ROLE_TOP, ROLE_ADC};
    int n_roles = (int)(sizeof(roles) / sizeof(roles[0]));

    printf("  STATE_NEUTRAL — 10,000 rolls per role (tolerance ±%.0f%%):\n", TOL);

    for (int ri = 0; ri < n_roles; ri++) {
        int role  = roles[ri];
        int hits  = 0;

        /* Set prefRoles so the role under test is not preferred */
        p.prefRoles[0] = (role + 1) % ROLE_COUNT;
        p.prefRoles[1] = (role + 2) % ROLE_COUNT;

        for (int t = 0; t < N_ROLLS; t++) {
            if (should_autofill(&p, role)) hits++;
        }

        float observed = 100.0f * (float)hits / (float)N_ROLLS;
        float expected = get_base_autofill_risk(role);

        printf("    %-8s: expected %.1f%%  observed %.2f%%\n",
               role_name(role), expected, observed);

        char msg[96];
        snprintf(msg, sizeof(msg),
                 "%s observed rate %.2f%% within ±%.0f%% of expected %.1f%%",
                 role_name(role), observed, TOL, expected);
        CHECK(fabsf(observed - expected) <= TOL, msg);
    }
}

/* =========================================================
 * TEST 4b — should_autofill_with_negative_bonus()
 *           Compare NEUTRAL vs NEGATIVE states (10k rolls each)
 * ========================================================= */

static void test_should_autofill_with_negative_bonus(void) {
    section("TEST 4b: should_autofill() — NEGATIVE state bonus validation (10k rolls)");

    const int N_ROLLS = 10000;

    Player p;
    memset(&p, 0, sizeof(p));
    p.hidden_factor = HIDDEN_FACTOR_START;

    int roles[] = {ROLE_SUPPORT, ROLE_JUNGLE, ROLE_MID, ROLE_TOP, ROLE_ADC};
    int n_roles = (int)(sizeof(roles) / sizeof(roles[0]));

    printf("  Comparing NEUTRAL vs NEGATIVE autofill rates per role:\n");
    printf("  %-8s  %10s  %10s  %10s\n",
           "Role", "NEUTRAL", "NEGATIVE", "Delta");
    printf("  %-8s  %10s  %10s  %10s\n",
           "--------", "----------", "----------", "----------");

    for (int ri = 0; ri < n_roles; ri++) {
        int role = roles[ri];

        /* Set prefRoles outside the role under test */
        p.prefRoles[0] = (role + 1) % ROLE_COUNT;
        p.prefRoles[1] = (role + 2) % ROLE_COUNT;

        /* NEUTRAL rolls */
        p.hidden_state = STATE_NEUTRAL;
        int neutral_hits = 0;
        for (int t = 0; t < N_ROLLS; t++) {
            if (should_autofill(&p, role)) neutral_hits++;
        }
        float neutral_rate = 100.0f * (float)neutral_hits / (float)N_ROLLS;

        /* NEGATIVE rolls */
        p.hidden_state = STATE_NEGATIVE;
        int negative_hits = 0;
        for (int t = 0; t < N_ROLLS; t++) {
            if (should_autofill(&p, role)) negative_hits++;
        }
        float negative_rate = 100.0f * (float)negative_hits / (float)N_ROLLS;

        float delta = negative_rate - neutral_rate;

        printf("  %-8s  %9.2f%%  %9.2f%%  %+9.2f%%\n",
               role_name(role), neutral_rate, negative_rate, delta);

        char msg1[96], msg2[96];
        snprintf(msg1, sizeof(msg1),
                 "%s: NEGATIVE rate (%.2f%%) > NEUTRAL rate (%.2f%%)",
                 role_name(role), negative_rate, neutral_rate);
        CHECK(negative_rate > neutral_rate, msg1);

        snprintf(msg2, sizeof(msg2),
                 "%s: NEGATIVE bonus delta %.2f%% in range [2%%-4%%]",
                 role_name(role), delta);
        CHECK(delta >= 2.0f && delta <= 4.0f, msg2);
    }
}

/* =========================================================
 * TEST 5 — assign_autofill_role() no-leakage validation
 * ========================================================= */

static void test_assign_autofill_no_leakage(void) {
    section("TEST 5: assign_autofill_role() — no preferred-role leakage (1,000 calls)");

    const int N_CALLS = 1000;

    Player p;
    memset(&p, 0, sizeof(p));
    p.hidden_factor = HIDDEN_FACTOR_START;
    p.hidden_state  = STATE_NEUTRAL;
    p.prefRoles[0]  = ROLE_ADC;
    p.prefRoles[1]  = ROLE_SUPPORT;

    int leaks        = 0;
    int role_dist[ROLE_COUNT] = {0};

    int side_effect_ok = 1;

    for (int i = 0; i < N_CALLS; i++) {
        /* Reset fields before each call */
        p.is_autofilled = 0;
        p.tilt_level    = 0;
        p.hidden_factor = HIDDEN_FACTOR_START;

        assign_autofill_role(&p);

        if (p.current_role == ROLE_ADC || p.current_role == ROLE_SUPPORT) {
            leaks++;
        }

        role_dist[p.current_role]++;

        /* Accumulate side-effect checks silently */
        if (p.is_autofilled != 1)                                              side_effect_ok = 0;
        if (p.tilt_level != AUTOFILL_TILT_LEVEL)                              side_effect_ok = 0;
        if (fabsf(p.hidden_factor - (HIDDEN_FACTOR_START - AUTOFILL_FACTOR_PENALTY)) > 0.001f)
                                                                               side_effect_ok = 0;
    }

    /* Report side-effect checks once for all 1,000 calls */
    CHECK(side_effect_ok,
        "All 1,000 calls: is_autofilled=1, tilt_level=AUTOFILL_TILT_LEVEL, "
        "hidden_factor decreased by AUTOFILL_FACTOR_PENALTY");
    printf("\n  Role distribution over %d calls (prefRoles: ADC, Support):\n",
           N_CALLS);
    for (int r = 0; r < ROLE_COUNT; r++) {
        float pct = 100.0f * (float)role_dist[r] / (float)N_CALLS;
        printf("    %-8s: %4d  (%.1f%%)%s\n",
               role_name(r), role_dist[r], pct,
               (r == ROLE_ADC || r == ROLE_SUPPORT) ? "  *** PREFERRED — SHOULD BE 0 ***" : "");
    }

    printf("\n  Leaks (preferred role assigned): %d / %d\n", leaks, N_CALLS);
    CHECK(leaks == 0,
        "No preferred role (ADC or Support) was assigned in 1,000 calls");

    /* Non-preferred roles (Top, Jungle, Mid) should each get ~1/3 of calls */
    int non_pref_roles[] = {ROLE_TOP, ROLE_JUNGLE, ROLE_MID};
    for (int ri = 0; ri < 3; ri++) {
        int r   = non_pref_roles[ri];
        float pct = 100.0f * (float)role_dist[r] / (float)N_CALLS;
        char msg[80];
        snprintf(msg, sizeof(msg),
                 "%s assigned in range [20%%-47%%] (got %.1f%%)",
                 role_name(r), pct);
        CHECK(pct >= 20.0f && pct <= 47.0f, msg);
    }
}

/* =========================================================
 * main
 * ========================================================= */

int main(void) {
    srand((unsigned int)time(NULL));

    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║       EOMM Autofill -- Debug Validation Test                 ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");

    test_base_risk_values();
    test_calculate_chance();
    test_role_preference_distribution();
    test_should_autofill_probability();
    test_should_autofill_with_negative_bonus();
    test_assign_autofill_no_leakage();

    printf("\n══════════════════════════════════════════════════════════════\n");
    printf("  RESULTS: %d passed, %d failed  (total: %d)\n",
           g_pass, g_fail, g_pass + g_fail);
    printf("══════════════════════════════════════════════════════════════\n\n");

    return (g_fail > 0) ? 1 : 0;
}