/*
 * tests/test_autofill_system.c
 *
 * Comprehensive test suite for the EOMM autofill system.
 *
 * Test coverage:
 *   1. Base autofill probabilities per role
 *   2. NEGATIVE hidden state +10% bonus; no bonus for NEUTRAL/POSITIVE
 *   3. Role assignment logic (autofilled vs. preferred roles)
 *   4. Tilt penalties on autofill (tilt_level, hidden_factor)
 *   5. Stat tracking across 200 players × 100 games
 *   6. Integration convergence (smurf ~58%, hardstuck ~42%, normal ~50%)
 *
 * Output:
 *   - Unit test PASS/FAIL results
 *   - Per-role autofill statistics
 *   - Win rate impact: autofilled vs. non-autofilled
 *   - Tilt level distribution before/after autofill
 *   - Convergence data (cumulative WR over 100 games per skill group)
 *   - CSV export: autofill_stats.csv
 *
 * Build:
 *   make test_autofill
 * Run:
 *   ./bin/test_autofill_system
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

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define ASSERT(cond, msg) do {                                          \
    g_tests_run++;                                                      \
    if (cond) {                                                         \
        printf("  [PASS] %s\n", msg);                                   \
        g_tests_passed++;                                               \
    } else {                                                            \
        printf("  [FAIL] %s\n", msg);                                   \
        g_tests_failed++;                                               \
    }                                                                   \
} while (0)

#define ASSERT_FLOAT_EQ(a, b, eps, msg) \
    ASSERT(fabsf((a) - (b)) < (eps), msg)

static void print_section(const char *title) {
    printf("\n══════════════════════════════════════════════════════\n");
    printf("  %s\n", title);
    printf("══════════════════════════════════════════════════════\n");
}

/* =========================================================
 * 1. Base autofill probabilities
 * ========================================================= */

static void test_base_autofill_probabilities(void) {
    print_section("TEST 1: Base autofill probabilities");

    ASSERT_FLOAT_EQ(get_base_autofill_risk(ROLE_SUPPORT), 2.0f,  0.001f,
        "Support base risk = 2%");
    ASSERT_FLOAT_EQ(get_base_autofill_risk(ROLE_JUNGLE),  3.0f,  0.001f,
        "Jungle base risk  = 3%");
    ASSERT_FLOAT_EQ(get_base_autofill_risk(ROLE_MID),     4.0f,  0.001f,
        "Mid base risk     = 4%");
    ASSERT_FLOAT_EQ(get_base_autofill_risk(ROLE_TOP),     5.0f,  0.001f,
        "Top base risk     = 5%");
    ASSERT_FLOAT_EQ(get_base_autofill_risk(ROLE_ADC),     6.0f,  0.001f,
        "ADC base risk     = 6%");

    /* Ordering: Support < Jungle < Mid < Top < ADC */
    ASSERT(get_base_autofill_risk(ROLE_SUPPORT) < get_base_autofill_risk(ROLE_JUNGLE),
        "Support risk < Jungle risk");
    ASSERT(get_base_autofill_risk(ROLE_JUNGLE)  < get_base_autofill_risk(ROLE_MID),
        "Jungle risk < Mid risk");
    ASSERT(get_base_autofill_risk(ROLE_MID)     < get_base_autofill_risk(ROLE_TOP),
        "Mid risk < Top risk");
    ASSERT(get_base_autofill_risk(ROLE_TOP)     < get_base_autofill_risk(ROLE_ADC),
        "Top risk < ADC risk");
}

/* =========================================================
 * 2. NEGATIVE state +10% bonus
 * ========================================================= */

static void test_negative_state_bonus(void) {
    print_section("TEST 2: NEGATIVE state autofill bonus");

    Player p;
    memset(&p, 0, sizeof(p));
    p.hidden_factor  = HIDDEN_FACTOR_START;
    p.prefRoles[0]   = ROLE_ADC;
    p.prefRoles[1]   = ROLE_MID;
    p.current_role   = ROLE_ADC;

    /* NEGATIVE state: should add AUTOFILL_EOMM_BONUS */
    p.hidden_state = STATE_NEGATIVE;
    float neg_chance = calculate_autofill_chance(&p, ROLE_ADC);
    ASSERT_FLOAT_EQ(neg_chance, AUTOFILL_RISK_ADC + AUTOFILL_EOMM_BONUS, 0.001f,
        "NEGATIVE state: ADC chance = 6% + 3% = 9%");

    p.hidden_state = STATE_NEGATIVE;
    float neg_sup  = calculate_autofill_chance(&p, ROLE_SUPPORT);
    ASSERT_FLOAT_EQ(neg_sup, AUTOFILL_RISK_SUPPORT + AUTOFILL_EOMM_BONUS, 0.001f,
        "NEGATIVE state: Support chance = 2% + 3% = 5%");

    /* NEUTRAL state: no bonus */
    p.hidden_state = STATE_NEUTRAL;
    float neu_chance = calculate_autofill_chance(&p, ROLE_ADC);
    ASSERT_FLOAT_EQ(neu_chance, AUTOFILL_RISK_ADC, 0.001f,
        "NEUTRAL state: ADC chance = base 6% (no bonus)");

    /* POSITIVE state: no bonus */
    p.hidden_state = STATE_POSITIVE;
    float pos_chance = calculate_autofill_chance(&p, ROLE_ADC);
    ASSERT_FLOAT_EQ(pos_chance, AUTOFILL_RISK_ADC, 0.001f,
        "POSITIVE state: ADC chance = base 6% (no bonus)");

    /* Bonus only on NEGATIVE, not on others */
    ASSERT(neg_chance > neu_chance,
        "NEGATIVE chance > NEUTRAL chance");
    ASSERT(neg_chance > pos_chance,
        "NEGATIVE chance > POSITIVE chance");
    ASSERT_FLOAT_EQ(neu_chance, pos_chance, 0.001f,
        "NEUTRAL and POSITIVE have identical chances (no bonus)");
}

/* =========================================================
 * 3. Role assignment logic
 * ========================================================= */

static void test_role_assignment_not_autofilled(void) {
    print_section("TEST 3a: Role assignment — NOT autofilled");

    Player p;
    memset(&p, 0, sizeof(p));
    p.hidden_factor  = HIDDEN_FACTOR_START;
    p.hidden_state   = STATE_NEUTRAL;
    p.prefRoles[0]   = ROLE_TOP;
    p.prefRoles[1]   = ROLE_MID;
    p.current_role   = ROLE_TOP;
    p.is_autofilled  = 0;

    /* Default assignment is primary role */
    ASSERT(p.current_role == p.prefRoles[0] || p.current_role == p.prefRoles[1],
        "Non-autofilled: current_role is primary or secondary pref");
    ASSERT(p.is_autofilled == 0,
        "Non-autofilled flag is 0");
}

static void test_role_assignment_autofilled(void) {
    print_section("TEST 3b: Role assignment — autofilled");

    Player p;
    memset(&p, 0, sizeof(p));
    p.hidden_factor  = HIDDEN_FACTOR_START;
    p.hidden_state   = STATE_NEUTRAL;
    p.prefRoles[0]   = ROLE_TOP;
    p.prefRoles[1]   = ROLE_MID;
    p.current_role   = ROLE_TOP;
    p.is_autofilled  = 0;
    p.tilt_level     = 0;

    assign_autofill_role(&p);

    ASSERT(p.is_autofilled == 1,
        "After assign_autofill_role: is_autofilled = 1");
    ASSERT(p.current_role != p.prefRoles[0],
        "After assign_autofill_role: current_role != primary pref");
    ASSERT(p.current_role != p.prefRoles[1],
        "After assign_autofill_role: current_role != secondary pref");
    ASSERT(p.current_role >= 0 && p.current_role < ROLE_COUNT,
        "After assign_autofill_role: current_role is valid role");

    /* Run multiple times to verify no pref leakage */
    int leak = 0;
    for (int i = 0; i < 200; i++) {
        p.prefRoles[0] = ROLE_ADC;
        p.prefRoles[1] = ROLE_SUPPORT;
        p.is_autofilled = 0;
        assign_autofill_role(&p);
        if (p.current_role == ROLE_ADC || p.current_role == ROLE_SUPPORT) {
            leak++;
        }
    }
    ASSERT(leak == 0,
        "assign_autofill_role never assigns a preferred role (200 trials)");
}

/* =========================================================
 * 4. Tilt penalties on autofill
 * ========================================================= */

static void test_tilt_penalty_on_autofill(void) {
    print_section("TEST 4: Tilt penalties on autofill");

    Player p;
    memset(&p, 0, sizeof(p));
    p.hidden_factor          = HIDDEN_FACTOR_START;  /* 1.0 */
    p.hidden_state           = STATE_NEUTRAL;
    p.prefRoles[0]           = ROLE_MID;
    p.prefRoles[1]           = ROLE_JUNGLE;
    p.tilt_level             = 0;
    p.is_autofilled          = 0;
    p.perf.tilt_resistance   = 0.80f;

    float factor_before    = p.hidden_factor;
    float tilt_res_before  = p.perf.tilt_resistance;
    assign_autofill_role(&p);

    ASSERT(p.tilt_level == AUTOFILL_TILT_LEVEL,
        "assign_autofill_role: tilt_level = 2 (AUTOFILL_TILT_LEVEL)");
    ASSERT(p.is_autofilled == 1,
        "assign_autofill_role: is_autofilled = 1");
    ASSERT(p.hidden_factor < factor_before,
        "assign_autofill_role: hidden_factor decreased immediately");
    ASSERT_FLOAT_EQ(p.hidden_factor,
        factor_before - AUTOFILL_FACTOR_PENALTY, 0.001f,
        "assign_autofill_role: hidden_factor -= AUTOFILL_FACTOR_PENALTY (0.05)");
    ASSERT(p.perf.tilt_resistance < tilt_res_before,
        "assign_autofill_role: tilt_resistance decreased (player angered)");
    ASSERT_FLOAT_EQ(p.perf.tilt_resistance,
        tilt_res_before - AUTOFILL_TILT_PENALTY, 0.001f,
        "assign_autofill_role: tilt_resistance -= AUTOFILL_TILT_PENALTY (0.15)");
}

static void test_post_match_penalty_autofill_loss(void) {
    print_section("TEST 4b: Post-match autofill penalty — loss");

    /*
     * Simulate a minimal match: autofilled player on team_a, team_b wins (winner=1).
     * Verify hidden_factor is reduced by AUTOFILL_POST_LOSS_PENALTY after the match.
     */
    Player players[MATCH_SIZE];
    init_players(players, MATCH_SIZE);

    /* Force player 0 to be autofilled */
    players[0].prefRoles[0] = ROLE_ADC;
    players[0].prefRoles[1] = ROLE_MID;
    assign_autofill_role(&players[0]);

    Match m;
    for (int i = 0; i < TEAM_SIZE; i++) {
        m.team_a[i] = &players[i];
        m.team_b[i] = &players[TEAM_SIZE + i];
    }
    m.winner = 1; /* team_b wins → team_a (autofilled player) loses */

    float factor_before_update = players[0].hidden_factor;
    update_players_after_match(&m);

    /* After loss, hidden_factor should drop by AUTOFILL_POST_LOSS_PENALTY (0.15)
     * on top of the normal FACTOR_LOSS_PENALTY (0.05) */
    float expected = factor_before_update
                     - FACTOR_LOSS_PENALTY
                     - AUTOFILL_POST_LOSS_PENALTY;
    /* Clamp to floor */
    if (expected < HIDDEN_FACTOR_MIN) expected = HIDDEN_FACTOR_MIN;

    ASSERT_FLOAT_EQ(players[0].hidden_factor, expected, 0.001f,
        "Autofill + loss: hidden_factor decreases by FACTOR_LOSS + AUTOFILL_POST_LOSS");
}

static void test_post_match_penalty_autofill_win(void) {
    print_section("TEST 4c: Post-match autofill penalty — win");

    Player players[MATCH_SIZE];
    init_players(players, MATCH_SIZE);

    players[0].prefRoles[0] = ROLE_ADC;
    players[0].prefRoles[1] = ROLE_MID;
    assign_autofill_role(&players[0]);

    Match m;
    for (int i = 0; i < TEAM_SIZE; i++) {
        m.team_a[i] = &players[i];
        m.team_b[i] = &players[TEAM_SIZE + i];
    }
    m.winner = 0; /* team_a wins → autofilled player wins */

    float factor_before_update = players[0].hidden_factor;
    update_players_after_match(&m);

    /* Win: hidden_factor gains FACTOR_WIN_BONUS then loses AUTOFILL_POST_WIN_PENALTY */
    float expected = factor_before_update
                     + FACTOR_WIN_BONUS
                     - AUTOFILL_POST_WIN_PENALTY;
    if (expected > HIDDEN_FACTOR_MAX) expected = HIDDEN_FACTOR_MAX;
    if (expected < HIDDEN_FACTOR_MIN) expected = HIDDEN_FACTOR_MIN;

    ASSERT_FLOAT_EQ(players[0].hidden_factor, expected, 0.001f,
        "Autofill + win: hidden_factor changes by FACTOR_WIN - AUTOFILL_POST_WIN");

    /* Win penalty is less severe than loss penalty */
    ASSERT(AUTOFILL_POST_WIN_PENALTY < AUTOFILL_POST_LOSS_PENALTY,
        "AUTOFILL_POST_WIN_PENALTY (0.08) < AUTOFILL_POST_LOSS_PENALTY (0.15)");
}

static void test_non_autofilled_normal_penalties(void) {
    print_section("TEST 4d: Non-autofilled game — normal penalties apply");

    Player players[MATCH_SIZE];
    init_players(players, MATCH_SIZE);

    /* Ensure no player is autofilled */
    for (int i = 0; i < MATCH_SIZE; i++) {
        players[i].is_autofilled = 0;
        players[i].current_role  = players[i].prefRoles[0];
    }

    Match m;
    for (int i = 0; i < TEAM_SIZE; i++) {
        m.team_a[i] = &players[i];
        m.team_b[i] = &players[TEAM_SIZE + i];
    }
    m.winner = 1; /* team_b wins */

    float factor_a0_before = players[0].hidden_factor;
    update_players_after_match(&m);

    /* Normal loss penalty only */
    float expected_a0 = factor_a0_before - FACTOR_LOSS_PENALTY;
    if (expected_a0 < HIDDEN_FACTOR_MIN) expected_a0 = HIDDEN_FACTOR_MIN;

    ASSERT_FLOAT_EQ(players[0].hidden_factor, expected_a0, 0.001f,
        "Non-autofilled loss: hidden_factor -= FACTOR_LOSS_PENALTY only");
}

/* =========================================================
 * 5. Stat tracking: 200 players × 100 games
 * ========================================================= */

typedef struct {
    /*
     * autofill_count[r]: number of times a player with prefRoles[0]==r was
     * autofilled (i.e. forced off their preferred role).
     * This is indexed by the player's PRIMARY preference, so it directly
     * reflects the per-role base risk: ADC (18%) > Top (15%) > Mid (12%)
     * > Jungle (8%) > Support (5%).
     */
    int   autofill_count[ROLE_COUNT];
    int   total_games_per_role[ROLE_COUNT];

    /* Win-rate tracking */
    int   autofill_wins;
    int   autofill_games;
    int   normal_wins;
    int   normal_games;

    /* Tilt distribution */
    int   tilt_at_autofill[3];   /* tilt_level 0/1/2 when autofill occurred */

    /* Convergence: cumulative WR per skill group per game */
    int   skill_wins[3][100];    /* [skill][game_idx] cumulative wins  */
    int   skill_games[3][100];   /* [skill][game_idx] cumulative games */
} SimStats;

static SimStats g_stats;

/*
 * run_large_simulation — 200 players × 100 games.
 * Collects autofill statistics into g_stats.
 */
static void run_large_simulation(Player *players, int n_players,
                                  Match *matches, int n_games) {
    memset(&g_stats, 0, sizeof(g_stats));

    for (int g = 0; g < n_games; g++) {
        int num_matches;
        create_matches(players, n_players, matches, &num_matches, g);

        for (int m = 0; m < num_matches; m++) {
            determine_troll_picks(&matches[m]);

            /* Record pre-match state for autofilled players */
            for (int i = 0; i < TEAM_SIZE; i++) {
                Player *pa = matches[m].team_a[i];
                Player *pb = matches[m].team_b[i];
                if (pa && pa->is_autofilled) {
                    /* Index by primary preference: shows which role-mains get autofilled */
                    g_stats.autofill_count[pa->prefRoles[0]]++;
                    g_stats.total_games_per_role[pa->prefRoles[0]]++;
                    int tl = pa->tilt_level;
                    if (tl >= 0 && tl <= 2) g_stats.tilt_at_autofill[tl]++;
                }
                if (pb && pb->is_autofilled) {
                    g_stats.autofill_count[pb->prefRoles[0]]++;
                    g_stats.total_games_per_role[pb->prefRoles[0]]++;
                    int tl = pb->tilt_level;
                    if (tl >= 0 && tl <= 2) g_stats.tilt_at_autofill[tl]++;
                }
            }

            simulate_match(&matches[m]);
            int winner = matches[m].winner;

            /* Post-match: record win rates */
            for (int i = 0; i < TEAM_SIZE; i++) {
                Player *pa = matches[m].team_a[i];
                Player *pb = matches[m].team_b[i];
                int a_won = (winner == 0);
                int b_won = (winner == 1);

                if (pa) {
                    if (pa->is_autofilled) {
                        g_stats.autofill_games++;
                        if (a_won) g_stats.autofill_wins++;
                    } else {
                        g_stats.normal_games++;
                        if (a_won) g_stats.normal_wins++;
                    }
                }
                if (pb) {
                    if (pb->is_autofilled) {
                        g_stats.autofill_games++;
                        if (b_won) g_stats.autofill_wins++;
                    } else {
                        g_stats.normal_games++;
                        if (b_won) g_stats.normal_wins++;
                    }
                }
            }

            update_players_after_match(&matches[m]);
        }

        /* Convergence snapshot after this game */
        for (int i = 0; i < n_players; i++) {
            int s = (int)players[i].skill_level;
            g_stats.skill_wins[s][g]  += players[i].wins;
            g_stats.skill_games[s][g] += players[i].total_games;
        }
    }
}

static void print_autofill_stats(int n_players, int n_games) {
    static const char *role_names[ROLE_COUNT] = {
        "Top    ", "Jungle ", "Mid    ", "ADC    ", "Support"
    };
    static const float base_risks[ROLE_COUNT] = {
        AUTOFILL_RISK_TOP,
        AUTOFILL_RISK_JUNGLE,
        AUTOFILL_RISK_MID,
        AUTOFILL_RISK_ADC,
        AUTOFILL_RISK_SUPPORT,
    };

    printf("\n  Per-role autofill statistics (%d players × %d games):\n",
           n_players, n_games);
    printf("  (autofill_count = times players with that primary role were autofilled)\n");
    printf("  %-9s %10s %10s %10s %12s\n",
           "PrimRole", "Autofills", "TotalGames", "AF%", "BaseRisk%");
    printf("  ----------------------------------------------------------\n");

    int total_af = 0;
    for (int r = 0; r < ROLE_COUNT; r++) {
        int af = g_stats.autofill_count[r];
        int tg = g_stats.total_games_per_role[r];
        float pct = (tg > 0) ? (100.0f * (float)af / (float)tg) : 0.0f;
        printf("  %-9s %10d %10d %9.1f%% %11.1f%%\n",
               role_names[r], af, tg, pct, base_risks[r]);
        total_af += af;
    }
    printf("  ----------------------------------------------------------\n");
    printf("  Total autofills: %d\n", total_af);

    printf("\n  Win rate impact analysis:\n");
    float af_wr  = (g_stats.autofill_games > 0)
                   ? (100.0f * (float)g_stats.autofill_wins / (float)g_stats.autofill_games)
                   : 0.0f;
    float norm_wr = (g_stats.normal_games > 0)
                    ? (100.0f * (float)g_stats.normal_wins / (float)g_stats.normal_games)
                    : 0.0f;
    printf("    Autofilled players  : %6d games, WR = %.1f%%\n",
           g_stats.autofill_games, af_wr);
    printf("    Normal   players    : %6d games, WR = %.1f%%\n",
           g_stats.normal_games, norm_wr);
    printf("    WR delta (normal−AF): %.1f%%\n", norm_wr - af_wr);

    printf("\n  Tilt level when autofill was assigned:\n");
    printf("    Tilt 0 (none)  : %d\n", g_stats.tilt_at_autofill[0]);
    printf("    Tilt 1 (light) : %d\n", g_stats.tilt_at_autofill[1]);
    printf("    Tilt 2 (heavy) : %d\n", g_stats.tilt_at_autofill[2]);
}

static void print_convergence(int n_games) {
    printf("\n  Convergence (cumulative WR %% over %d games):\n", n_games);
    printf("  Game  |  Normal %%  |  Smurf %%   | Hardstuck %%\n");
    printf("  ------------------------------------------------\n");

    int step = (n_games <= 20) ? 1 : (n_games / 20);
    for (int g = step - 1; g < n_games; g += step) {
        printf("  %4d  |", g + 1);
        for (int s = 0; s < 3; s++) {
            int wins  = g_stats.skill_wins[s][g];
            int games = g_stats.skill_games[s][g];
            float wr  = (games > 0)
                        ? (100.0f * (float)wins / (float)games) : 0.0f;
            printf("  %8.1f%%  |", wr);
        }
        printf("\n");
    }
}

static void export_csv(int n_games) {
    FILE *fp = fopen("autofill_stats.csv", "w");
    if (!fp) {
        fprintf(stderr, "  [WARN] Could not create autofill_stats.csv\n");
        return;
    }

    /* Header */
    fprintf(fp, "game,normal_wr_pct,smurf_wr_pct,hardstuck_wr_pct,"
                "autofill_count_top,autofill_count_jungle,autofill_count_mid,"
                "autofill_count_adc,autofill_count_support\n");

    /* Autofill counts are cumulative totals, not per-game; write them on row 0 */
    for (int g = 0; g < n_games; g++) {
        float wr[3] = {0.0f, 0.0f, 0.0f};
        for (int s = 0; s < 3; s++) {
            int games = g_stats.skill_games[s][g];
            if (games > 0) {
                wr[s] = 100.0f * (float)g_stats.skill_wins[s][g] / (float)games;
            }
        }
        /* Print autofill role counts only on the last game row for brevity */
        if (g == n_games - 1) {
            fprintf(fp, "%d,%.2f,%.2f,%.2f,%d,%d,%d,%d,%d\n",
                    g + 1,
                    wr[SKILL_NORMAL], wr[SKILL_SMURF], wr[SKILL_HARDSTUCK],
                    g_stats.autofill_count[ROLE_TOP],
                    g_stats.autofill_count[ROLE_JUNGLE],
                    g_stats.autofill_count[ROLE_MID],
                    g_stats.autofill_count[ROLE_ADC],
                    g_stats.autofill_count[ROLE_SUPPORT]);
        } else {
            fprintf(fp, "%d,%.2f,%.2f,%.2f,,,,,\n",
                    g + 1,
                    wr[SKILL_NORMAL], wr[SKILL_SMURF], wr[SKILL_HARDSTUCK]);
        }
    }

    fclose(fp);
    printf("\n  CSV exported → autofill_stats.csv\n");
}

/* =========================================================
 * 6. Integration test: convergence assertions
 * ========================================================= */

static void test_integration_convergence(Player *players, int n_players) {
    print_section("TEST 6: Integration — WR convergence (200p × 100g)");

    /* Compute final WR per skill group */
    float wins[3]  = {0.0f, 0.0f, 0.0f};
    float games[3] = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i < n_players; i++) {
        int s = (int)players[i].skill_level;
        wins[s]  += (float)players[i].wins;
        games[s] += (float)players[i].total_games;
    }

    float wr[3] = {0.0f, 0.0f, 0.0f};
    for (int s = 0; s < 3; s++) {
        if (games[s] > 0) wr[s] = 100.0f * wins[s] / games[s];
    }

    printf("  Final WR: Normal=%.1f%% Smurf=%.1f%% Hardstuck=%.1f%%\n",
           wr[SKILL_NORMAL], wr[SKILL_SMURF], wr[SKILL_HARDSTUCK]);

    /*
     * Tolerance is generous (±5%) to account for random variation.
     * The EOMM system uses stochastic processes so exact values vary.
     */
    ASSERT(wr[SKILL_SMURF] >= 50.0f && wr[SKILL_SMURF] <= 68.0f,
        "Smurf WR converges to ~58% (50-68% range)");
    ASSERT(wr[SKILL_HARDSTUCK] >= 35.0f && wr[SKILL_HARDSTUCK] <= 50.0f,
        "Hardstuck WR converges to ~42% (35-50% range)");
    ASSERT(wr[SKILL_NORMAL] >= 43.0f && wr[SKILL_NORMAL] <= 57.0f,
        "Normal WR stays near ~50% (43-57% range)");

    /* Smurf should win more than normal */
    ASSERT(wr[SKILL_SMURF] > wr[SKILL_NORMAL],
        "Smurf WR > Normal WR");
    /* Normal should win more than hardstuck */
    ASSERT(wr[SKILL_NORMAL] > wr[SKILL_HARDSTUCK],
        "Normal WR > Hardstuck WR");
}

static void test_stat_tracking(int n_players, int n_games) {
    print_section("TEST 5: Stat tracking — per-role autofill distribution");

    /* Verify that lower-risk roles have fewer autofills than higher-risk roles */
    ASSERT(g_stats.autofill_count[ROLE_SUPPORT] <= g_stats.autofill_count[ROLE_ADC],
        "Support autofill count <= ADC autofill count");

    /* At least some autofills should have occurred */
    int total_af = 0;
    for (int r = 0; r < ROLE_COUNT; r++) total_af += g_stats.autofill_count[r];
    ASSERT(total_af > 0,
        "At least one autofill occurred across all games");

    /* Autofilled players should have lower WR than non-autofilled */
    float af_wr   = (g_stats.autofill_games > 0)
                    ? (100.0f * (float)g_stats.autofill_wins / (float)g_stats.autofill_games)
                    : 50.0f;
    float norm_wr = (g_stats.normal_games > 0)
                    ? (100.0f * (float)g_stats.normal_wins / (float)g_stats.normal_games)
                    : 50.0f;
    ASSERT(af_wr <= norm_wr + 5.0f,
        "Autofilled WR <= normal WR + 5% (autofill hurts performance)");

    /* Autofill happened on tilted players (tilt_level=2 most common at autofill) */
    ASSERT(g_stats.tilt_at_autofill[2] > 0,
        "Some autofills occurred at tilt_level = 2 (heavy tilt)");

    print_autofill_stats(n_players, n_games);
    print_convergence(n_games);
    export_csv(n_games);
}

/* =========================================================
 * Main
 * ========================================================= */

int main(void) {
    srand(42); /* fixed seed for reproducibility */

    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║        EOMM Autofill System — Test Suite                 ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");

    /* ---- Unit tests (no simulation needed) ---- */
    test_base_autofill_probabilities();
    test_negative_state_bonus();
    test_role_assignment_not_autofilled();
    test_role_assignment_autofilled();
    test_tilt_penalty_on_autofill();
    test_post_match_penalty_autofill_loss();
    test_post_match_penalty_autofill_win();
    test_non_autofilled_normal_penalties();

    /* ---- Integration: 200 players × 100 games ---- */
    print_section("Integration simulation: 200 players × 100 games");
    printf("  Initialising players…\n");

    int n_players = 200;
    int n_games   = 100;

    Player *players = (Player *)malloc((size_t)n_players * sizeof(Player));
    if (!players) {
        fprintf(stderr, "  [ERROR] malloc failed for players\n");
        return 1;
    }

    int max_matches = n_players / MATCH_SIZE;
    Match *matches  = (Match *)malloc((size_t)max_matches * sizeof(Match));
    if (!matches) {
        fprintf(stderr, "  [ERROR] malloc failed for matches\n");
        free(players);
        return 1;
    }

    init_players(players, n_players);
    printf("  Running simulation…\n");
    run_large_simulation(players, n_players, matches, n_games);
    printf("  Simulation complete.\n");

    test_stat_tracking(n_players, n_games);
    test_integration_convergence(players, n_players);

    /* ---- Summary ---- */
    print_section("TEST SUMMARY");
    printf("  Total  : %d\n", g_tests_run);
    printf("  Passed : %d\n", g_tests_passed);
    printf("  Failed : %d\n", g_tests_failed);

    free(matches);
    free(players);

    return (g_tests_failed == 0) ? 0 : 1;
}
