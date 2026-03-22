/*
 * eomm_system.h
 *
 * EOMM (Engagement Optimized Matchmaking) system — header file.
 * Defines all structures, constants, enums and function declarations.
 *
 * Player distribution:
 *   10% Smurfs    (skill: 58% WR, high arrogance → high troll chance)
 *   10% Hardstuck (skill: 42% WR, low confidence → low troll chance)
 *   80% Normal    (skill: 50% WR)
 *
 * All players start at MMR 1000 (Silver).
 * Placement phase: 10 games, K-factor 30.
 * Ranked phase:    configurable games, K-factor 25.
 */

#ifndef EOMM_SYSTEM_H
#define EOMM_SYSTEM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* =========================================================
 * Build constants
 * ========================================================= */

/* MMR */
#define START_MMR           1000.0f   /* all players begin at 1000 (Silver) */
#define K_FACTOR_PLACEMENT  30.0f     /* faster calibration during placements */
#define K_FACTOR_RANKED     25.0f     /* standard ranked K-factor */
#define PLACEMENT_GAMES     10        /* first 10 games = placement phase */

/* EOMM tuning */
#define SOFT_RESET_INTERVAL     14    /* every 14 games: full mental reset */
#define HIDDEN_FACTOR_MIN       0.50f /* floor for hidden_factor */
#define HIDDEN_FACTOR_MAX       1.20f /* ceiling for hidden_factor */
#define HIDDEN_FACTOR_START     1.00f /* initial hidden_factor */

/* Troll probability */
#define TROLL_BASE_HIGH_FACTOR  15.0f /* % troll chance when factor >= 0.95 */
#define TROLL_WIN_STREAK_BONUS  2.0f  /* extra % per win-streak count above 2 */
#define TROLL_PROB_CAP          60.0f /* absolute troll probability ceiling */

/* Troll penalty (applied even on a win) */
#define TROLL_PENALTY_BASE      0.10f /* base hidden_factor reduction per troll */
#define TROLL_PENALTY_SCALE     1.5f  /* multiplier: effective penalty = base * scale */
#define TROLL_ESCALATION        0.50f /* +50% per consecutive troll (cumulative) */

/* Hidden factor change per game outcome */
#define FACTOR_WIN_BONUS        0.02f /* factor gained on a win */
#define FACTOR_LOSS_PENALTY     0.05f /* factor lost on a loss */

/* Matchmaking balance tolerance (visible MMR spread) */
#define MMR_BALANCE_TOLERANCE   200.0f

/* Compensation boost for players on 7+ consecutive losses */
#define COMPENSATION_THRESHOLD  7       /* activate at 7 consecutive losses */
#define COMPENSATION_MAX_BONUS  0.50f   /* 50% max win-probability boost    */
#define COMPENSATION_BONUS_7    0.15f   /* +15% boost at exactly 7 losses   */
#define COMPENSATION_BONUS_8    0.25f   /* +25% boost at exactly 8 losses   */
#define COMPENSATION_BONUS_9    0.35f   /* +35% boost at exactly 9 losses   */

/* Team / match sizes */
#define TEAM_SIZE   5
#define MATCH_SIZE  (TEAM_SIZE * 2)

/* Player name buffer length */
#define PLAYER_NAME_LEN 32

/* =========================================================
 * Autofill constants
 * Max total chance = base + EOMM bonus must stay below 10%
 * ========================================================= */
#define AUTOFILL_RISK_SUPPORT   2.0f  /* rarely contested              */
#define AUTOFILL_RISK_JUNGLE    3.0f  /* protected / rare role          */
#define AUTOFILL_RISK_MID       4.0f  /* normal demand                 */
#define AUTOFILL_RISK_TOP       5.0f  /* normal demand                 */
#define AUTOFILL_RISK_ADC       6.0f  /* highly contested role          */

/* EOMM bonus applied when player is in STATE_NEGATIVE (max: 6%+3%=9%) */
#define AUTOFILL_EOMM_BONUS     3.0f

/* Tilt-resistance penalty applied when a player is autofilled */
#define AUTOFILL_TILT_PENALTY   0.15f

/* =========================================================
 * Enumerations
 * ========================================================= */

/*
 * Role — LoL roles used for autofill risk calculation.
 */
typedef enum {
    ROLE_SUPPORT = 0,
    ROLE_JUNGLE  = 1,
    ROLE_MID     = 2,
    ROLE_TOP     = 3,
    ROLE_ADC     = 4
} Role;

/*
 * SkillLevel — permanent skill category assigned at player creation.
 * Determines the player's base win-rate contribution.
 */
typedef enum {
    SKILL_NORMAL    = 0,  /* 80% of players — 50% base WR  */
    SKILL_SMURF     = 1,  /* 10% of players — 58% base WR  */
    SKILL_HARDSTUCK = 2   /* 10% of players — 42% base WR  */
} SkillLevel;

/*
 * HiddenState — player's current mental / behavioral category.
 * Derived each round from lose_streak and hidden_factor.
 *
 *   NEGATIVE : lose_streak >= 3  OR  factor <= 0.70
 *   POSITIVE : factor >= 0.95   AND  lose_streak == 0
 *   NEUTRAL  : all other cases
 */
typedef enum {
    STATE_NEGATIVE = -1,
    STATE_NEUTRAL  =  0,
    STATE_POSITIVE =  1
} HiddenState;

/* =========================================================
 * Structures
 * ========================================================= */

/*
 * Player — full player state for one simulation agent.
 */
typedef struct {
    int  id;
    char name[PLAYER_NAME_LEN];

    /* Skill profile (fixed at creation) */
    SkillLevel skill_level;
    float      win_rate;     /* base win probability: 0.50 / 0.58 / 0.42 */

    /* Visible MMR (what the player sees) */
    float visible_mmr;

    /* Hidden factor [0.50..1.20]:
     *   1.0  = neutral mental state
     *   >1.0 = on a hot streak / confident (may lead to trolling)
     *   <1.0 = tilted / degraded state (harder matchmaking) */
    float hidden_factor;

    /* Game counters */
    int wins;
    int losses;
    int total_games;

    /* Streak tracking */
    int win_streak;
    int lose_streak;

    /* EOMM dynamic state */
    HiddenState hidden_state;
    int         tilt_level;          /* 0=none, 1=light tilt, 2=heavy tilt */
    int         consecutive_trolls;  /* count of back-to-back troll picks   */
    int         is_troll_pick;       /* 1 if trolling this game             */

    /* Role preferences (2 safe roles, 0-indexed using Role enum) */
    Role        prefRoles[2];

    /* Tilt resistance [0.0..1.0]: reduced by autofill penalty */
    float       tilt_resistance;
} Player;

/*
 * Match — one 5v5 match.
 * team_a is designed to be the weaker/losing side,
 * team_b the stronger/winning side (per EOMM strategy).
 * The actual outcome is still determined by simulation.
 */
typedef struct {
    Player *team_a[TEAM_SIZE];
    Player *team_b[TEAM_SIZE];
    int     winner; /* 0 = team_a won, 1 = team_b won */
} Match;

/*
 * SkillStats — aggregate analytics for one skill group.
 */
typedef struct {
    SkillLevel skill_level;
    int   player_count;
    int   total_wins;
    int   total_games;
    float avg_mmr;
    float avg_hidden_factor;
    int   troll_count;        /* players currently flagged as troll this round */
    int   tilt2_count;        /* players at tilt_level == 2                    */
} SkillStats;

/* =========================================================
 * Function declarations
 * ========================================================= */

/* --- Player initialisation --- */
void init_player(Player *p, int id, SkillLevel skill);
void init_players(Player *players, int n);

/* --- EOMM core mechanics --- */

/* Returns troll probability [0..TROLL_PROB_CAP] (%) for a given player. */
float calculate_troll_probability(const Player *p);

/* Apply troll penalty to hidden_factor (even on a win). */
void apply_troll_penalty(Player *p);

/* Recompute and store hidden_state and tilt_level based on current state. */
void update_hidden_state(Player *p);

/* Update hidden_factor, streaks and tilt after a game result. */
void update_tilt(Player *p, int did_win);

/* Soft reset every SOFT_RESET_INTERVAL games (tilt→0, factor→1.0). */
void apply_soft_reset(Player *p);

/* Apply MMR delta (K-factor depends on total_games). */
void update_mmr(Player *p, int did_win);

/* Effective MMR used for matchmaking / team power: visible_mmr * hidden_factor. */
float effective_mmr(const Player *p);

/* --- Matchmaking --- */

/* Decide troll picks for every player in a match before simulation. */
void determine_troll_picks(Match *m);

/* Build matches from the player pool.
 * game_number == 0 → random first game; else → EOMM matchmaking. */
void create_matches(Player *players, int n, Match *matches, int *num_matches, int game_number);

/* Simulate match outcome; returns winner (0 or 1). */
int simulate_match(Match *m);

/* Apply win/loss bookkeeping + MMR + tilt for every player in the match. */
void update_players_after_match(Match *m);

/* --- Autofill --- */

/* Returns base autofill risk (%) for a given role. */
float get_base_autofill_risk(Role role);

/* Returns total autofill chance (%) for a player in their preferred role,
 * adding EOMM bonus when the player is in STATE_NEGATIVE. */
float calculate_autofill_chance(const Player *p, Role role);

/* Returns 1 if the player should be autofilled out of their preferred role. */
int should_autofill(const Player *p, Role pref_role);

/* Assign a random non-preferred role and apply the tilt penalty. */
void assign_autofill_role(Player *p);

/* --- Analytics --- */

/* Compute aggregate stats for each skill group. */
void compute_stats(const Player *players, int n, SkillStats stats[3]);

/* Print per-round skill-group statistics table. */
void print_stats(const SkillStats stats[3]);

/* Print the final comprehensive simulation report. */
void print_final_report(const Player *players, int n, int total_games);

#endif /* EOMM_SYSTEM_H */
