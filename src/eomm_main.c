/*
 * eomm_main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/eomm_system.h"

/* =========================================================
 * Input helpers
 * ========================================================= */

static void flush_stdin(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

static int read_int(const char *prompt, int min_val) {
    int value;
    while (1) {
        printf("%s", prompt);
        fflush(stdout);
        if (scanf("%d", &value) == 1) {
            flush_stdin();
            if (value >= min_val) return value;
            printf("  ✗ Value must be at least %d. Try again.\n", min_val);
        } else {
            flush_stdin();
            printf("  ✗ Invalid input. Please enter a whole number.\n");
        }
    }
}

/* =========================================================
 * Simulation runner
 * ========================================================= */

static void run_simulation(Player *players, int n_players,
                            Match *matches, int n_games, MatchHistory *history) {

    for (int g = 0; g < n_games; g++) {

        int num_matches;
        create_matches(players, n_players, matches, &num_matches, g);

        for (int m = 0; m < num_matches; m++) {

            for (int i = 0; i < TEAM_SIZE; i++) {
                update_engagement_phase(matches[m].team_a[i]);
                apply_engagement_phase_modifiers(matches[m].team_a[i]);

                update_engagement_phase(matches[m].team_b[i]);
                apply_engagement_phase_modifiers(matches[m].team_b[i]);
            }

            determine_troll_picks(&matches[m]);
            simulate_match(&matches[m]);
            update_players_after_match(&matches[m]);

            history_add_match(history, &matches[m], m + g * num_matches, g);
        }

        int game_num = g + 1;

        if (game_num == PLACEMENT_GAMES) {
            printf("  [Game %3d] Placement finished — ranked starts\n", game_num);
        } else if (game_num % 10 == 0 || game_num == 1) {
            printf("  [Game %3d] Progress update\n", game_num);

            SkillStats stats[3];
            compute_stats(players, n_players, stats);
            print_stats(stats);

            printf("\n");
        }

        /* Harmless inflation control: every 20 games, gently recenter MMR */
        if (game_num % 20 == 0) {
            apply_inflation_control(players, n_players);
        }
    }
}

/* =========================================================
 * Main
 * ========================================================= */

int main(void) {

    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║        EOMM Matchmaking System — Interactive Mode        ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    printf("Configuration\n");
    printf("─────────────\n");

    int n_players = read_int("  Number of players (min 10): ", MATCH_SIZE);
    int n_games   = read_int("  Number of games per player (min 1): ", 1);

    /* -----------------------------------------------------
     * NO FORCED WINRATES HERE (IMPORTANT FIX)
     * ----------------------------------------------------- */
    int n_smurfs    = n_players / 10;
    int n_hardstuck = n_players / 10;
    int n_normal    = n_players - n_smurfs - n_hardstuck;

    printf("\n");
    printf("  Players   : %d total\n", n_players);
    printf("    Smurfs    : %d (%.0f%%)\n",
           n_smurfs, 100.0f * (float)n_smurfs / (float)n_players);
    printf("    Hardstuck : %d (%.0f%%)\n",
           n_hardstuck, 100.0f * (float)n_hardstuck / (float)n_players);
    printf("    Normal    : %d (%.0f%%)\n",
           n_normal, 100.0f * (float)n_normal / (float)n_players);

    printf("\n  Simulation settings:\n");
    printf("  Placement : first %d games (K = %.0f)\n",
           PLACEMENT_GAMES, K_FACTOR_PLACEMENT);
    printf("  Ranked    : remaining games (K = %.0f)\n",
           K_FACTOR_RANKED);
    printf("  Soft reset: every %d games\n\n",
           SOFT_RESET_INTERVAL);

    /* ---- Allocate ---- */
    Player *players = malloc((size_t)n_players * sizeof(Player));
    if (!players) {
        fprintf(stderr, "Error: failed to allocate players.\n");
        return 1;
    }

    int max_matches = n_players / MATCH_SIZE;
    Match *matches = malloc((size_t)max_matches * sizeof(Match));
    if (!matches) {
        fprintf(stderr, "Error: failed to allocate matches.\n");
        free(players);
        return 1;
    }

    MatchHistory *history = history_create(n_games * max_matches);
    if (!history) {
        fprintf(stderr, "Error: failed to allocate history.\n");
        free(matches);
        free(players);
        return 1;
    }

    /* ---- Init ---- */
    srand((unsigned int)time(NULL));
    init_players(players, n_players);   // ⚠️ MUST assign SKILL here, not winrate

    printf("Simulation starting…\n");
    printf("─────────────────────────────────────────────────────────\n\n");

    run_simulation(players, n_players, matches, n_games, history);

    print_final_report(players, n_players, n_games);

    history_export_json(history, players, n_players, "match_history.json");
    printf("\n✓ Match history exported\n");

    history_free(history);
    free(matches);
    free(players);

    return 0;
}