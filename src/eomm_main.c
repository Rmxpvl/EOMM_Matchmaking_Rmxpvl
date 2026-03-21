/*
 * eomm_main.c
 *
 * EOMM Matchmaking System — interactive main program.
 *
 * Usage:
 *   ./bin/eomm_system
 *
 * The program prompts for:
 *   1. Number of players (minimum 10; must allow at least one 5v5 match)
 *   2. Number of games per player (minimum 1)
 *
 * Player distribution (automatically computed):
 *   10% Smurfs    (58% base win rate)
 *   10% Hardstuck (42% base win rate)
 *   80% Normal    (50% base win rate)
 *
 * All players start at MMR 1000 (Silver).
 *   Placement phase : first 10 games, K-factor = 30
 *   Ranked phase    : remaining games, K-factor = 25
 *   Soft reset      : every 14 games (tilt → 0, hidden_factor → 1.0)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/eomm_system.h"

/* =========================================================
 * Input helpers
 * ========================================================= */

/* Flush any remaining characters from stdin to avoid stale input */
static void flush_stdin(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/*
 * read_int — prompt the user and return a validated integer >= min_val.
 * Loops until a valid value is entered.
 */
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

/*
 * run_simulation — execute the full EOMM simulation.
 *
 * For each game round:
 *   1. Create matches (random for game 0, EOMM thereafter)
 *   2. Determine troll picks for all players in each match
 *   3. Simulate match outcomes
 *   4. Apply post-match updates (MMR, tilt, soft reset)
 *
 * Progress is printed every 10 games and at the placement boundary.
 */
static void run_simulation(Player *players, int n_players,
                            Match *matches, int n_games) {
    for (int g = 0; g < n_games; g++) {
        int num_matches;
        create_matches(players, n_players, matches, &num_matches, g);

        for (int m = 0; m < num_matches; m++) {
            determine_troll_picks(&matches[m]);
            simulate_match(&matches[m]);
            update_players_after_match(&matches[m]);
        }

        /* Progress feedback */
        int game_num = g + 1;
        if (game_num == PLACEMENT_GAMES) {
            printf("  [Game %3d] Placement phase complete — entering ranked phase\n",
                   game_num);
        } else if (game_num % 10 == 0 || game_num == 1) {
            printf("  [Game %3d] Progress update:\n", game_num);
            SkillStats stats[3];
            compute_stats(players, n_players, stats);
            print_stats(stats);
            printf("\n");
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

    /* ---- Input ---- */
    printf("Configuration\n");
    printf("─────────────\n");
    int n_players = read_int(
        "  Number of players (min 10): ", MATCH_SIZE);
    int n_games   = read_int(
        "  Number of games per player (min 1): ", 1);

    /* Distribution breakdown */
    int n_smurfs    = n_players / 10;
    int n_hardstuck = n_players / 10;
    int n_normal    = n_players - n_smurfs - n_hardstuck;

    printf("\n");
    printf("  Players   : %d total\n", n_players);
    printf("    Smurfs    : %d (%.0f%%) — 58%% base win rate\n",
           n_smurfs,    100.0f * (float)n_smurfs    / (float)n_players);
    printf("    Hardstuck : %d (%.0f%%) — 42%% base win rate\n",
           n_hardstuck, 100.0f * (float)n_hardstuck / (float)n_players);
    printf("    Normal    : %d (%.0f%%) — 50%% base win rate\n",
           n_normal,    100.0f * (float)n_normal    / (float)n_players);
    printf("  Games     : %d\n", n_games);
    printf("  Start MMR : %.0f\n", START_MMR);
    printf("  Placement : first %d games (K = %.0f)\n",
           PLACEMENT_GAMES, K_FACTOR_PLACEMENT);
    printf("  Ranked    : remaining games (K = %.0f)\n", K_FACTOR_RANKED);
    printf("  Soft reset: every %d games\n\n", SOFT_RESET_INTERVAL);

    /* ---- Allocate ---- */
    Player *players = (Player *)malloc((size_t)n_players * sizeof(Player));
    if (!players) {
        fprintf(stderr, "Error: failed to allocate player pool.\n");
        return 1;
    }

    int max_matches = n_players / MATCH_SIZE;
    Match *matches  = (Match *)malloc((size_t)max_matches * sizeof(Match));
    if (!matches) {
        fprintf(stderr, "Error: failed to allocate match pool.\n");
        free(players);
        return 1;
    }

    /* ---- Initialise ---- */
    srand((unsigned int)time(NULL));
    init_players(players, n_players);

    printf("Simulation starting…\n");
    printf("─────────────────────────────────────────────────────────\n\n");

    /* ---- Run ---- */
    run_simulation(players, n_players, matches, n_games);

    /* ---- Final report ---- */
    print_final_report(players, n_players, n_games);

    /* ---- Clean up ---- */
    free(matches);
    free(players);
    return 0;
}
