/*
 * eomm_system.h
 *
 * EOMM (Engagement Optimized Matchmaking) system — header file.
 * Defines all structures, constants, enums and function declarations.
 *
 * Player distribution:
 *   10% Smurfs    (high performance stats 70-90% → win rate emerges ~56-60%)
 *   10% Hardstuck (low performance stats 10-35%  → win rate emerges ~35-42%)
 *   80% Normal    (variable stats 30-70%          → win rate emerges ~48-52%)
 *
 * Win rates are NOT pre-assigned; they emerge dynamically from PerformanceStats.
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
#define SOFT_RESET_INTERVAL     25    /* every 25 games: full mental reset */
#define HIDDEN_FACTOR_MIN       0.50f /* floor for hidden_factor */
#define HIDDEN_FACTOR_MAX       1.20f /* ceiling for hidden_factor */
#define HIDDEN_FACTOR_START     1.00f /* initial hidden_factor */
#define UNCERTAINTY_START       1.0f
#define UNCERTAINTY_DECAY       0.90f
#define UNCERTAINTY_MIN         0.30f

/* Troll probability */
#define TROLL_BASE_HIGH_FACTOR  15.0f /* % troll chance when factor >= 0.95 */
#define TROLL_WIN_STREAK_BONUS  2.0f  /* extra % per win-streak count above 2 */
#define TROLL_PROB_CAP          60.0f /* absolute troll probability ceiling */

/* Troll penalty (applied even on a win) */
#define TROLL_PENALTY_BASE      0.10f /* base hidden_factor reduction per troll */
#define TROLL_PENALTY_SCALE     1.5f  /* multiplier: effective penalty = base * scale */
#define TROLL_ESCALATION        0.50f /* +50% per consecutive troll (cumulative) */

/* Hidden factor change per game outcome */
#define FACTOR_WIN_BONUS        0.08f /* factor gained on a win */
#define FACTOR_LOSS_PENALTY     0.20f /* factor lost on a loss */

/* Matchmaking balance tolerance (visible MMR spread) */
#define MMR_BALANCE_TOLERANCE   200.0f

/* LoL role identifiers */
#define ROLE_TOP     0
#define ROLE_JUNGLE  1
#define ROLE_MID     2
#define ROLE_ADC     3
#define ROLE_SUPPORT 4
#define ROLE_COUNT   5

/* Autofill base risks (%) per role — higher = more likely to be autofilled */
#define AUTOFILL_RISK_SUPPORT   2.0f  /* support is rarely contested   */
#define AUTOFILL_RISK_JUNGLE    3.0f  /* jungle is protected / rare    */
#define AUTOFILL_RISK_MID       4.0f  /* normal demand                 */
#define AUTOFILL_RISK_TOP       5.0f  /* normal demand                 */
#define AUTOFILL_RISK_ADC       6.0f  /* highly contested role         */

/* EOMM dynamic autofill modifier: added when player is in NEGATIVE state */
#define AUTOFILL_EOMM_BONUS     3.0f  /* max effective chance: 6%+3% = 9% < 10% */

/* Autofill tilt penalties */
#define AUTOFILL_TILT_LEVEL         2      /* immediate tilt level on autofill              */
#define AUTOFILL_FACTOR_PENALTY     0.05f  /* immediate hidden_factor reduction              */
#define AUTOFILL_TILT_PENALTY       0.15f  /* immediate tilt_resistance reduction (−15%)    */
#define AUTOFILL_POST_WIN_PENALTY   0.08f  /* additional hidden_factor penalty on win        */
#define AUTOFILL_POST_LOSS_PENALTY  0.15f  /* additional hidden_factor penalty on loss       */

/* Momentum system — subtle streak encouragement */
#define MOMENTUM_WIN_GAIN       0.15f   /* momentum += on a win                  */
#define MOMENTUM_LOSS_COST      0.20f   /* momentum -= on a loss (asymmetric)   */
#define MOMENTUM_MIN           -1.0f    /* negative spiral floor                */
#define MOMENTUM_MAX            1.0f    /* positive streak ceiling              */
#define MOMENTUM_POWER_SCALE    0.3f    /* how much momentum affects player_power */

/* Compensation boost for players on 7+ consecutive losses (REDUCED) */
#define COMPENSATION_THRESHOLD  7       /* activate at 7 consecutive losses */
#define COMPENSATION_MAX_BONUS  0.03f   /* REDUCED: 3% max win-probability boost */
#define COMPENSATION_BONUS_7    0.01f   /* REDUCED: +1%  boost at exactly 7 losses */
#define COMPENSATION_BONUS_8    0.02f   /* REDUCED: +2%  boost at exactly 8 losses */
#define COMPENSATION_BONUS_9    0.03f   /* REDUCED: +3%  boost at exactly 9 losses */

/* Team / match sizes */
#define TEAM_SIZE   5
#define MATCH_SIZE  (TEAM_SIZE * 2)

/* Player name buffer length */
#define PLAYER_NAME_LEN 32

/* =========================================================
 * Enumerations
 * ========================================================= */

/*
 * SkillLevel — permanent skill category assigned at player creation.
 * Determines the player's performance stat ranges; win rate emerges from stats.
 */
typedef enum {
    SKILL_NORMAL    = 0,  /* 80% of players — stats 30-70%  (WR emerges ~48-52%) */
    SKILL_SMURF     = 1,  /* 10% of players — stats 70-90%  (WR emerges ~56-60%) */
    SKILL_HARDSTUCK = 2   /* 10% of players — stats 10-35%  (WR emerges ~35-42%) */
} SkillLevel;

/*
 * Rank — visible rank tier based on MMR (like League of Legends)
 *   Bronze    : 0 - 1200
 *   Silver    : 1200 - 2000
 *   Gold      : 2000 - 3000
 *   Platinum  : 3000 - 4000
 *   Diamond   : 4000 - 5500
 *   Master    : 5500+
 */
typedef enum {
    RANK_BRONZE    = 0,
    RANK_SILVER    = 1,
    RANK_GOLD      = 2,
    RANK_PLATINUM  = 3,
    RANK_DIAMOND   = 4,
    RANK_MASTER    = 5
} Rank;

/*
 * PlayerGroup — segmentation for differential EOMM treatment.
 *
 *   CORE        : 80% of players (SKILL_NORMAL, SKILL_HARDSTUCK normal)
 *   OUTLIERS_HIGH : Smurfs (SKILL_SMURF) — progressive difficulty for convergence
 *   OUTLIERS_LOW  : Hardstuck extreme cases — enhanced protection & compensation
 */
typedef enum {
    GROUP_CORE          = 0,
    GROUP_OUTLIERS_HIGH = 1,
    GROUP_OUTLIERS_LOW  = 2
} PlayerGroup;

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

/*
 * EngagementPhase — orchestrates hot/cold streak narratives per player.
 *
 *   PHASE_NEUTRAL    : no active streak direction; transitions possible
 *   PHASE_WIN_STREAK : player is in a hot streak (soft win bias)
 *   PHASE_LOSE_STREAK: player is in a cold streak (soft lose bias)
 */
typedef enum {
    PHASE_LOSE_STREAK = -1,  /* player in cold streak (must lose) */
    PHASE_NEUTRAL     =  0,  /* player neutral, can go either way  */
    PHASE_WIN_STREAK  =  1   /* player in hot streak (must win)    */
} EngagementPhase;

/* =========================================================
 * Structures
 * ========================================================= */

/*
 * PerformanceStats — independent skill attributes for one player.
 *
 * Each stat is in [0, 1].  Ranges by skill level:
 *   SMURF     : 70-90%  (globally high; one stat may dip to ~60%)
 *   NORMAL    : 30-70%  (high variance; each stat truly independent)
 *   HARDSTUCK : 10-35%  (globally low; one stat may reach ~40%)
 *
 * Win rate is NOT stored here; it is computed dynamically by
 * calculate_actual_winrate() from a weighted average of these stats.
 */
typedef struct {
    float mechanical_skill;      /* CSing, combat, positioning   */
    float decision_making;       /* Macro, objectives, timing    */
    float map_awareness;         /* Vision, information usage    */
    float tilt_resistance;       /* Emotional stability          */
    float champion_pool_depth;   /* Champion pool diversity      */
    float champion_proficiency;  /* Champion mastery level       */
    float wave_management;       /* Wave control, freeze timing  */
    float teamfight_positioning; /* Teamfight positioning        */
} PerformanceStats;

/*
 * Player — full player state for one simulation agent.
 */
typedef struct {
    int  id;
    char name[PLAYER_NAME_LEN];

    /* Skill profile (fixed at creation) */
    SkillLevel     skill_level;
    PerformanceStats perf;       /* independent performance attributes */

    /* Visible MMR (what the player sees) */
    float visible_mmr;
    
    /* League of Legends style system */
    Rank current_rank;              /* Visible rank (Bronze-Master) */
    int league_points;              /* LP within rank [0-99] */
    float hidden_mmr;               /* Hidden MMR for LP gain calculation */

    /* Uncertainty used for placement games (MMR volatility early game) */
    float mmr_uncertainty;

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

    /* Role preferences and autofill state */
    int         prefRoles[2];        /* preferred roles (ROLE_* constants)  */
    int         current_role;        /* role assigned for current game      */
    int         is_autofilled;       /* 1 if autofilled this game           */

    /* Engagement phase — streak orchestration */
    EngagementPhase engagement_phase;    /* current streak phase            */
    int             phase_progress;      /* games played since phase start  */
    int             target_streak;       /* target streak length (3-7)      */

    /* Momentum [-1.0, +1.0]: subtle bias towards extend streaks */
    float momentum;                     /* psychological game momentum      */
    
    /* Placement games tracking */
    int is_in_placement;                /* 1 if playing placement games (first 10) */
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

/* --- Rank system --- */
Rank get_rank_from_mmr(float mmr);
const char* rank_name(Rank r);

/* Calculate LP gain/loss based on hidden_mmr vs visible rank mismatch */
float calculate_lp_gain(const Player *p, int did_win);

/* --- Engagement phase orchestration --- */

/* Update engagement phase state (transition between NEUTRAL/WIN/LOSE streak). */
void update_engagement_phase(Player *p);

/* Apply soft hidden_factor modifiers based on current engagement phase. */
void apply_engagement_phase_modifiers(Player *p);

/* --- Player initialisation --- */
void init_player(Player *p, int id, SkillLevel skill);
void init_players(Player *players, int n);

/* --- Performance-based win rate --- */

/* Compute the player's current win probability from their PerformanceStats.
 * Returns a value in [0.25, 0.75]; applies a tilt penalty when the player
 * is in STATE_NEGATIVE.  Called at match time instead of using a fixed rate. */
float calculate_actual_winrate(const Player *p);

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

/* ELO calculation for expected win probability. */
float calculate_expected(float mmr_a, float mmr_b);

/* Determine player grouping for differential EOMM treatment. */
PlayerGroup get_player_group(const Player *p);

/* Calculate target MMR for convergence (group-dependent). */
float get_target_mmr_for_group(const Player *p);

/* Apply EOMM matchmaking bias (hidden_factor via opponent difficulty). */
float apply_eomm_bias(Player *p, float opponent_mmr);

/* Apply MMR delta using ELO formula: delta = K * (outcome - expected). */
void update_mmr(Player *p, float opponent_mmr, int did_win);

/* Effective MMR used for matchmaking / team power: visible_mmr * hidden_factor. */
float effective_mmr(const Player *p);

/* Compute individual player power (MMR × win rate) for team strength evaluation. */
float player_power(Player *p);

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

/* Return base autofill risk (%) for a given role (ROLE_* constant). */
float get_base_autofill_risk(int role);

/* Return total autofill chance (%) for a player considering their hidden_state. */
float calculate_autofill_chance(const Player *p, int role);

/* Dice-roll: returns 1 if this player should be autofilled for the given role. */
int should_autofill(const Player *p, int role);

/* Force the player into a role outside their two preferences; sets is_autofilled,
 * tilt_level and applies the immediate hidden_factor penalty. */
void assign_autofill_role(Player *p);

/* --- Analytics --- */

/* Compute aggregate stats for each skill group. */
void compute_stats(const Player *players, int n, SkillStats stats[3]);

/* Print per-round skill-group statistics table. */
void print_stats(const SkillStats stats[3]);

/* Print the final comprehensive simulation report. */
void print_final_report(const Player *players, int n, int total_games);

/* Compute average MMR across all players (used for inflation control). */
float compute_avg_mmr(const Player *players, int n);

/* Apply gentle inflation control: recenters MMR if it drifts above START_MMR.
 * This prevents unbounded growth while preserving relative differences. */
void apply_inflation_control(Player *players, int n);


/* =========================================================
 * Match History
 * ========================================================= */

/*
 * StoredMatch — immutable snapshot of one completed 5v5 match.
 */
typedef struct {
    int match_id;
    int timestamp;
    int team_a_ids[TEAM_SIZE];
    int team_b_ids[TEAM_SIZE];
    int winner;
    float team_a_power;
    float team_b_power;
    int troll_count_a;
    int troll_count_b;
} StoredMatch;

/*
 * MatchHistory — dynamic array of StoredMatch records.
 */
typedef struct {
    StoredMatch *matches;
    int count;
    int capacity;
} MatchHistory;

/* --- Match history --- */

MatchHistory *history_create(int initial_capacity);

void history_add_match(MatchHistory *h, const Match *m,
                       int match_id, int timestamp);

void history_export_json(const MatchHistory *h, const Player *players,
                         int n_players, const char *filename);

void history_free(MatchHistory *h);

#endif /* EOMM_SYSTEM_H */
