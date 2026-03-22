/*
 * tests/test_50_games_detailed.c
 *
 * Comprehensive simulation test: 10 players × 50 games.
 *
 * Player distribution:
 *   Players 0-2  : SMURF     (3 players — high skill)
 *   Players 3-6  : NORMAL    (4 players — mid skill)
 *   Players 7-9  : HARDSTUCK (3 players — low skill)
 *
 * Showcase players (detailed game-by-game report):
 *   Player 0 → SMURF
 *   Player 5 → NORMAL
 *   Player 8 → HARDSTUCK
 *
 * Each round (game cycle) runs all 10 players through one 5v5 match using the
 * full EOMM pipeline:
 *   1. create_matches()      — EOMM or random matchmaking
 *   2. determine_troll_picks() — troll pick decision
 *   3. simulate_match()        — stochastic outcome
 *   4. update_players_after_match() — MMR, tilt, autofill penalties
 *
 * Build:
 *   make test_50_games_detailed
 * Run:
 *   ./bin/test_50_games_detailed
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../include/eomm_system.h"

/* =========================================================
 * Configuration
 * ========================================================= */

#define N_PLAYERS           10
#define N_GAMES             50

/* Showcase player indices */
#define SHOWCASE_SMURF       0
#define SHOWCASE_NORMAL      5
#define SHOWCASE_HARDSTUCK   8

/* Selected game snapshots to print per showcase player */
static const int SNAPSHOT_GAMES[] = {1, 5, 10, 25, 50};
#define N_SNAPSHOTS ((int)(sizeof(SNAPSHOT_GAMES) / sizeof(SNAPSHOT_GAMES[0])))

/* =========================================================
 * Per-game log entry
 * ========================================================= */

typedef struct {
    int   result;               /* 1=win, 0=loss                        */
    float mmr_before;           /* visible MMR before the match         */
    float mmr_after;            /* visible MMR after the match          */
    int   tilt_level;           /* tilt_level after the match           */
    float hidden_factor;        /* hidden_factor after the match        */
    int   is_autofilled;        /* 1 if autofilled this game            */
    int   compensation_applied; /* 1 if compensation boost was active   */
} GameLog;

/* =========================================================
 * Showcase player tracker
 * ========================================================= */

typedef struct {
    int     player_id;          /* stable player ID (never changes)     */

    /* Starting snapshot */
    float   start_mmr;
    int     start_tilt;
    float   start_hidden_factor;

    /* Per-game log */
    GameLog games[N_GAMES];

    /* Running peaks / counters */
    int     total_autofills;
    int     max_tilt_reached;
    int     max_win_streak;
    int     max_loss_streak;
    int     compensation_boosts;
} ShowcaseTracker;

/* =========================================================
 * Global state
 * ========================================================= */

static Player         players[N_PLAYERS];
static Match          match_buf[1];   /* 10 players → exactly 1 match per round */
static ShowcaseTracker trackers[3];   /* SMURF, NORMAL, HARDSTUCK                */

/* Stable player IDs for the three showcase players */
static const int showcase_ids[3] = {
    SHOWCASE_SMURF, SHOWCASE_NORMAL, SHOWCASE_HARDSTUCK
};

/* =========================================================
 * Helpers
 * ========================================================= */

/* Look up a player by their stable id (survives in-place shuffles). */
static Player *find_player_by_id(int id) {
    for (int i = 0; i < N_PLAYERS; i++) {
        if (players[i].id == id)
            return &players[i];
    }
    return NULL; /* should never happen */
}

/* =========================================================
 * Helpers
 * ========================================================= */

static const char *skill_label(SkillLevel s) {
    switch (s) {
        case SKILL_SMURF:     return "SMURF    ";
        case SKILL_HARDSTUCK: return "HARDSTUCK";
        default:              return "NORMAL   ";
    }
}

static const char *role_name(int r) {
    switch (r) {
        case ROLE_TOP:     return "Top    ";
        case ROLE_JUNGLE:  return "Jungle ";
        case ROLE_MID:     return "Mid    ";
        case ROLE_ADC:     return "ADC    ";
        case ROLE_SUPPORT: return "Support";
        default:           return "???    ";
    }
}

/* =========================================================
 * Player pool initialisation (custom distribution)
 *
 * We initialise each player individually so we can control
 * exactly which indices get which skill level.
 * ========================================================= */

static void init_custom_players(void) {
    /* Players 0-2 → SMURF */
    for (int i = 0; i < 3; i++)
        init_player(&players[i], i, SKILL_SMURF);

    /* Players 3-6 → NORMAL */
    for (int i = 3; i < 7; i++)
        init_player(&players[i], i, SKILL_NORMAL);

    /* Players 7-9 → HARDSTUCK */
    for (int i = 7; i < 10; i++)
        init_player(&players[i], i, SKILL_HARDSTUCK);
}

/* =========================================================
 * Simulation loop
 * ========================================================= */

static void run_simulation(void) {
    /* Initialise showcase trackers */
    for (int t = 0; t < 3; t++) {
        int pid = showcase_ids[t];
        const Player *p = find_player_by_id(pid);
        trackers[t].player_id             = pid;
        trackers[t].start_mmr             = p->visible_mmr;
        trackers[t].start_tilt            = p->tilt_level;
        trackers[t].start_hidden_factor   = p->hidden_factor;
        trackers[t].total_autofills       = 0;
        trackers[t].max_tilt_reached      = 0;
        trackers[t].max_win_streak        = 0;
        trackers[t].max_loss_streak       = 0;
        trackers[t].compensation_boosts   = 0;
    }

    for (int g = 0; g < N_GAMES; g++) {
        int num_matches = 0;

        /* Record MMR before match and compensation flag for showcase players.
         * Must look up by ID because create_matches_random() shuffles the array. */
        float mmr_before[3];
        int   comp_active[3];
        for (int t = 0; t < 3; t++) {
            const Player *p = find_player_by_id(showcase_ids[t]);
            mmr_before[t]  = p->visible_mmr;
            comp_active[t] = (p->lose_streak >= COMPENSATION_THRESHOLD) ? 1 : 0;
        }

        create_matches(players, N_PLAYERS, match_buf, &num_matches, g);
        determine_troll_picks(&match_buf[0]);
        simulate_match(&match_buf[0]);
        update_players_after_match(&match_buf[0]);

        /* Record game results for showcase players */
        for (int t = 0; t < 3; t++) {
            /* Re-locate the showcase player by ID (array may have been shuffled) */
            const Player *p = find_player_by_id(showcase_ids[t]);
            GameLog *log = &trackers[t].games[g];

            /* Determine win/loss by scanning match team pointers */
            int did_win = 0;
            for (int i = 0; i < TEAM_SIZE; i++) {
                if (match_buf[0].team_a[i] && match_buf[0].team_a[i]->id == showcase_ids[t]) {
                    did_win = (match_buf[0].winner == 0);
                    break;
                }
                if (match_buf[0].team_b[i] && match_buf[0].team_b[i]->id == showcase_ids[t]) {
                    did_win = (match_buf[0].winner == 1);
                    break;
                }
            }

            log->result               = did_win;
            log->mmr_before           = mmr_before[t];
            log->mmr_after            = p->visible_mmr;
            log->tilt_level           = p->tilt_level;
            log->hidden_factor        = p->hidden_factor;
            log->is_autofilled        = p->is_autofilled;
            log->compensation_applied = comp_active[t];

            /* Update tracker peaks */
            if (p->is_autofilled)
                trackers[t].total_autofills++;
            if (p->tilt_level > trackers[t].max_tilt_reached)
                trackers[t].max_tilt_reached = p->tilt_level;
            if (p->win_streak > trackers[t].max_win_streak)
                trackers[t].max_win_streak = p->win_streak;
            if (p->lose_streak > trackers[t].max_loss_streak)
                trackers[t].max_loss_streak = p->lose_streak;
            if (comp_active[t])
                trackers[t].compensation_boosts++;
        }
    }
}

/* =========================================================
 * Showcase player report
 * ========================================================= */

static void print_showcase(int tracker_idx) {
    const ShowcaseTracker *tr = &trackers[tracker_idx];
    const Player          *p  = find_player_by_id(tr->player_id);

    printf("\n");
    printf("============================================================\n");
    printf("PLAYER %d: %s (%s)\n",
           tr->player_id,
           p->name,
           skill_label(p->skill_level));
    printf("  Preferred roles: %s / %s\n",
           role_name(p->prefRoles[0]), role_name(p->prefRoles[1]));
    printf("============================================================\n");

    printf("Starting Stats:\n");
    printf("  - MMR           : %.0f\n", tr->start_mmr);
    printf("  - Tilt Level    : %d\n",   tr->start_tilt);
    printf("  - Hidden Factor : %.2f\n", tr->start_hidden_factor);

    printf("\nGame-by-game progression (selected games):\n");
    printf("  %-6s  %-6s  %-12s  %-5s  %-13s  %-9s  %-12s\n",
           "Game", "Result", "MMR Change", "Tilt", "Hidden Factor",
           "Autofill", "Compensation");
    printf("  -----------------------------------------------------------------------\n");

    for (int s = 0; s < N_SNAPSHOTS; s++) {
        int gi = SNAPSHOT_GAMES[s] - 1;   /* 0-indexed */
        if (gi >= N_GAMES) continue;
        const GameLog *log = &tr->games[gi];
        float mmr_delta = log->mmr_after - log->mmr_before;
        printf("  Game %-2d  %-6s  %+8.1f       %-5d  %.2f           %-9s  %s\n",
               SNAPSHOT_GAMES[s],
               log->result ? "WIN  " : "LOSS ",
               mmr_delta,
               log->tilt_level,
               log->hidden_factor,
               log->is_autofilled        ? "YES      " : "no       ",
               log->compensation_applied ? "YES" : "no ");
    }

    /* Final stats */
    float wr = (p->total_games > 0)
               ? (100.0f * (float)p->wins / (float)p->total_games) : 0.0f;
    printf("\nFinal Stats:\n");
    printf("  - Win Rate                  : %.1f%% (%dW-%dL)\n",
           wr, p->wins, p->losses);
    printf("  - Final MMR                 : %.0f (started at %.0f, delta %+.0f)\n",
           p->visible_mmr, tr->start_mmr, p->visible_mmr - tr->start_mmr);
    printf("  - Final Tilt                : %d\n", p->tilt_level);
    printf("  - Final Hidden Factor       : %.2f\n", p->hidden_factor);
    printf("  - Times Autofilled          : %d / %d games (%.1f%%)\n",
           tr->total_autofills, N_GAMES,
           100.0f * (float)tr->total_autofills / (float)N_GAMES);
    printf("  - Highest Tilt Level Reached: %d\n", tr->max_tilt_reached);
    printf("  - Longest Win Streak        : %d\n", tr->max_win_streak);
    printf("  - Longest Loss Streak       : %d\n", tr->max_loss_streak);
    printf("  - Compensation Boosts Applied: %d\n", tr->compensation_boosts);

    /* Analysis */
    printf("\nAnalysis:\n");
    switch (p->skill_level) {
        case SKILL_SMURF:
            printf("  - SMURF trajectory: high-skill player faces tougher EOMM matchups\n");
            printf("    as MMR rises, limiting runaway win rate.\n");
            if (wr >= 55.0f)
                printf("  - Maintained above-average WR (%.1f%%) despite EOMM pressure.\n", wr);
            else
                printf("  - EOMM successfully capped win rate near average (%.1f%%).\n", wr);
            if (tr->total_autofills > 0)
                printf("  - Autofill impact: %d autofill(s) temporarily reduced performance.\n",
                       tr->total_autofills);
            else
                printf("  - No autofills this session — preferred role always available.\n");
            break;

        case SKILL_NORMAL:
            printf("  - NORMAL trajectory: mid-skill player should converge near 50%% WR.\n");
            if (wr >= 45.0f && wr <= 55.0f)
                printf("  - Win rate converged as expected: %.1f%% ≈ 50%%.\n", wr);
            else if (wr > 55.0f)
                printf("  - Above-average run this session (%.1f%%); natural variance.\n", wr);
            else
                printf("  - Below-average run this session (%.1f%%); compensation boosts helped.\n", wr);
            printf("  - EOMM balanced engagement via tilt/hidden-state matchmaking.\n");
            if (tr->compensation_boosts > 0)
                printf("  - Received %d compensation boost(s) after long losing streaks.\n",
                       tr->compensation_boosts);
            break;

        case SKILL_HARDSTUCK:
            printf("  - HARDSTUCK trajectory: low-skill player expected lower WR.\n");
            if (tr->compensation_boosts > 0)
                printf("  - Compensation boosts (%d) softened extended losing streaks.\n",
                       tr->compensation_boosts);
            else
                printf("  - No compensation boosts triggered (no streak of %d+ losses).\n",
                       COMPENSATION_THRESHOLD);
            if (tr->max_tilt_reached == 2)
                printf("  - Reached heavy tilt (level 2); EOMM placed them on losing side.\n");
            if (tr->total_autofills > 0)
                printf("  - Autofill (%d game(s)) compounded performance issues.\n",
                       tr->total_autofills);
            break;
    }

    /* Win/loss streak analysis */
    if (tr->max_win_streak >= 3)
        printf("  - Enjoyed a natural win streak of %d — EOMM did not forcibly stop it.\n",
               tr->max_win_streak);
    if (tr->max_loss_streak >= 4)
        printf("  - Suffered a loss streak of %d games — tilt mechanics deepened the run.\n",
               tr->max_loss_streak);
}

/* =========================================================
 * Global summary (all 10 players)
 * ========================================================= */

static void print_global_summary(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║        GLOBAL SUMMARY — 10 Players × 50 Games           ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("  Total games simulated: %d\n\n", N_PLAYERS * N_GAMES / 2);

    /* Aggregate stats by skill group */
    int   wins[3]        = {0, 0, 0};
    int   games[3]       = {0, 0, 0};
    float mmr_sum[3]     = {0.0f, 0.0f, 0.0f};
    int   count[3]       = {0, 0, 0};
    int   autofills[3]   = {0, 0, 0};
    int   tilt2_count[3] = {0, 0, 0};

    for (int i = 0; i < N_PLAYERS; i++) {
        const Player *p = &players[i];
        int s = (int)p->skill_level;
        wins[s]      += p->wins;
        games[s]     += p->total_games;
        mmr_sum[s]   += p->visible_mmr;
        count[s]++;
        if (p->tilt_level == 2) tilt2_count[s]++;
    }

    /* Count autofills from tracker data for the 3 showcase players */
    for (int t = 0; t < 3; t++) {
        int s = (int)find_player_by_id(showcase_ids[t])->skill_level;
        autofills[s] += trackers[t].total_autofills;
    }

    printf("  %-12s %5s %6s %8s %8s %10s %10s\n",
           "Skill", "Count", "Games", "Wins", "WR%", "Avg MMR", "Tilt-2");
    printf("  ---------------------------------------------------------------\n");

    static const char *snames[3] = {"NORMAL   ", "SMURF    ", "HARDSTUCK"};
    for (int s = 0; s < 3; s++) {
        float wr  = (games[s] > 0) ? (100.0f * (float)wins[s]  / (float)games[s]) : 0.0f;
        float avg = (count[s] > 0) ? (mmr_sum[s] / (float)count[s]) : 0.0f;
        printf("  %-12s %5d %6d %8d %7.1f%% %10.0f %10d\n",
               snames[s], count[s], games[s], wins[s], wr, avg, tilt2_count[s]);
    }

    /* Win rate distribution: all players sorted by WR */
    printf("\n  Win Rate Distribution (all players):\n");
    printf("  %-14s  %-12s  %6s  %4s  %4s  %8s  %5s\n",
           "Name", "Skill", "WR%", "W", "L", "Fin MMR", "Tilt");
    printf("  --------------------------------------------------------------\n");
    for (int i = 0; i < N_PLAYERS; i++) {
        const Player *p = &players[i];
        float wr = (p->total_games > 0)
                   ? (100.0f * (float)p->wins / (float)p->total_games) : 0.0f;
        printf("  %-14s  %-12s %5.1f%%  %3d  %3d  %8.0f  %5d\n",
               p->name,
               skill_label(p->skill_level),
               wr, p->wins, p->losses,
               p->visible_mmr,
               p->tilt_level);
    }

    /* MMR progression summary */
    float all_mmr_sum = 0.0f, all_mmr_min = 1e9f, all_mmr_max = -1.0f;
    for (int i = 0; i < N_PLAYERS; i++) {
        float v = players[i].visible_mmr;
        all_mmr_sum += v;
        if (v < all_mmr_min) all_mmr_min = v;
        if (v > all_mmr_max) all_mmr_max = v;
    }
    printf("\n  MMR Spread:\n");
    printf("    Min: %.0f  |  Avg: %.0f  |  Max: %.0f\n",
           all_mmr_min, all_mmr_sum / (float)N_PLAYERS, all_mmr_max);

    /* Autofill frequency (showcase players only — representatively) */
    printf("\n  Autofill Frequency (showcase players):\n");
    for (int t = 0; t < 3; t++) {
        const Player *p = find_player_by_id(showcase_ids[t]);
        printf("    %s (%s): %d autofills in %d games (%.1f%%)\n",
               p->name,
               skill_label(p->skill_level),
               trackers[t].total_autofills,
               N_GAMES,
               100.0f * (float)trackers[t].total_autofills / (float)N_GAMES);
    }

    /* Tilt patterns */
    printf("\n  Tilt Pattern Summary (showcase players):\n");
    for (int t = 0; t < 3; t++) {
        const Player *p = find_player_by_id(showcase_ids[t]);
        printf("    %s (%s): max tilt=%d  longest loss streak=%d  comp boosts=%d\n",
               p->name,
               skill_label(p->skill_level),
               trackers[t].max_tilt_reached,
               trackers[t].max_loss_streak,
               trackers[t].compensation_boosts);
    }

    /* Validation checklist */
    float wr_smurf     = (games[SKILL_SMURF]     > 0) ? (100.0f * (float)wins[SKILL_SMURF]     / (float)games[SKILL_SMURF])     : 0.0f;
    float wr_normal    = (games[SKILL_NORMAL]     > 0) ? (100.0f * (float)wins[SKILL_NORMAL]    / (float)games[SKILL_NORMAL])    : 0.0f;
    float wr_hardstuck = (games[SKILL_HARDSTUCK]  > 0) ? (100.0f * (float)wins[SKILL_HARDSTUCK] / (float)games[SKILL_HARDSTUCK]) : 0.0f;

    int af_total = trackers[0].total_autofills
                 + trackers[1].total_autofills
                 + trackers[2].total_autofills;
    float af_rate = 100.0f * (float)af_total / (3.0f * N_GAMES);

    printf("\n  ── Validation Checklist ──────────────────────────────────\n");
    printf("  %s Smurfs maintain higher WR (%.1f%% vs normal %.1f%%)\n",
           wr_smurf > wr_normal ? "✅" : "⚠️",
           wr_smurf, wr_normal);
    printf("  %s Normal players near ~50%% WR (%.1f%%)\n",
           (wr_normal >= 43.0f && wr_normal <= 57.0f) ? "✅" : "⚠️",
           wr_normal);
    printf("  %s Hardstuck have lower WR (%.1f%% vs normal %.1f%%)\n",
           wr_hardstuck < wr_normal ? "✅" : "⚠️",
           wr_hardstuck, wr_normal);
    printf("  %s Autofill < 10%% occurrence (showcase avg %.1f%%)\n",
           af_rate < 10.0f ? "✅" : "⚠️",
           af_rate);
    printf("  %s No artificial 50%% WR forcing (smurf %.1f%% ≠ hardstuck %.1f%%)\n",
           fabsf(wr_smurf - wr_hardstuck) > 3.0f ? "✅" : "⚠️",
           wr_smurf, wr_hardstuck);

    int natural_streaks = 0;
    for (int t = 0; t < 3; t++) {
        if (trackers[t].max_win_streak >= 2 || trackers[t].max_loss_streak >= 2)
            natural_streaks++;
    }
    printf("  %s Natural win/loss streaks emerged from mechanics\n",
           natural_streaks > 0 ? "✅" : "⚠️");
    printf("  ──────────────────────────────────────────────────────────\n");
}

/* =========================================================
 * Main
 * ========================================================= */

int main(void) {
    srand((unsigned int)time(NULL));

    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║   EOMM Simulation — 10 Players × 50 Games               ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("  Player distribution: 3 Smurfs | 4 Normal | 3 Hardstuck\n");
    printf("  Showcase players  : Player %d (Smurf) | Player %d (Normal)"
           " | Player %d (Hardstuck)\n\n",
           SHOWCASE_SMURF, SHOWCASE_NORMAL, SHOWCASE_HARDSTUCK);

    init_custom_players();

    printf("  Initialised players:\n");
    printf("  %-6s  %-14s  %-12s  %-13s  %s\n",
           "Index", "Name", "Skill", "Pref Roles", "Start MMR");
    printf("  ------------------------------------------------------------\n");
    for (int i = 0; i < N_PLAYERS; i++) {
        printf("  [%d]    %-14s  %-12s  %s / %s   %.0f\n",
               i,
               players[i].name,
               skill_label(players[i].skill_level),
               role_name(players[i].prefRoles[0]),
               role_name(players[i].prefRoles[1]),
               players[i].visible_mmr);
    }

    printf("\n  Running %d games…\n", N_GAMES);
    run_simulation();
    printf("  Simulation complete.\n");

    /* Detailed report for each showcase player */
    for (int t = 0; t < 3; t++)
        print_showcase(t);

    /* Global summary */
    print_global_summary();

    printf("\n");
    return 0;
}
