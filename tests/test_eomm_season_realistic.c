/*
 * test_eomm_season_realistic.c
 *
 * 👥 PLAYER-CENTRIC TIMELINE ANALYSIS
 *
 * Instead of analyzing global stats over 10K games,
 * analyze each PLAYER individually as a realistic SEASON.
 *
 * Concept:
 *   - Simulate 10K games (1000 players, matchmaking)
 *   - Track each player's complete history (sequence of outcomes)
 *   - Analyze per-player as a realistic season (100-300 games)
 *   - Focus on rolling metrics, streaks, convergence
 *   - Output: player-centric view (not aggregate averages)
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#define N_PLAYERS 1000
#define N_GAMES 100000                                      /* 100K total games */
#define MAX_GAMES_PER_PLAYER 1000                           /* 1000 games per player */

/* ============================================================
 * STRUCTURES
 * ============================================================ */

typedef enum {
    SKILL_NORMAL = 0,           /* baseline: 950-1050 MMR */
    SKILL_SMURF_LOW = 1,        /* +200-300 MMR above normal */
    SKILL_SMURF_MED = 2,        /* +400-600 MMR above normal */
    SKILL_SMURF_HIGH = 3,       /* +700-1000 MMR above normal */
    SKILL_LOW_BAD = 4,          /* 500-700 MMR */
    SKILL_LOW_VERY_BAD = 5,     /* 300-500 MMR */
    SKILL_LOW_EXTREME = 6       /* 100-300 MMR */
} SkillLevel;

typedef struct {
    float mechanical_skill, decision_making, map_awareness, tilt_resistance;
    float champion_pool_depth, champion_proficiency, wave_management, teamfight_positioning;
} PerformanceStats;

typedef struct {
    int id;
    SkillLevel skill_level;
    PerformanceStats perf;
    float mmr_raw;                           /* true skill (Elo baseline) - source of truth */
    float mmr_uncertainty;                   /* rating deviation (sigma) - high at start */
    float hidden_factor;
    int total_games, wins, losses;
    int win_streak, lose_streak;
    
    /* PLAYER-CENTRIC TRACKING */
    int outcomes[MAX_GAMES_PER_PLAYER];      /* 1=win, 0=loss, for THIS player's games */
    float mmr_timeline[MAX_GAMES_PER_PLAYER + 1];
    int actual_game_count;                   /* how many games this player actually played */
} PlayerTimeline;

/* Pool stats: cumulative stats for games 1 → N */
typedef struct {
    int pool_size;          /* games 1 → pool_size */
    int wins;
    float mmr_start, mmr_end;
    float mmr_avg, mmr_variance;
    int longest_win_streak, longest_lose_streak;
    float win_rate;
    float uncertainty_end;  /* uncertainty after pool_size games */
} PoolStats;

/* Tracked player: one of the 7 skill types */
typedef struct {
    PlayerTimeline *player;
    const char *label;
    PoolStats pools[4];  /* 50, 100, 200, 300 games */
} TrackedPlayer;

typedef struct {
    int match_id;
    int winner;  /* 0=team_a, 1=team_b */
} MatchRecord;

/* ============================================================
 * UTILITIES
 * ============================================================ */

static float randf(void) { return (float)rand() / (float)RAND_MAX; }
static float clampf(float x, float lo, float hi) { 
    return (x < lo) ? lo : ((x > hi) ? hi : x); 
}

float calculate_expected(float mmr_a, float mmr_b) {
    float diff = (mmr_a - mmr_b) / 400.0f;
    return 1.0f / (1.0f + powf(10.0f, -diff));
}

/* Sigmoid saturation function (prevents infinite carry scaling) */
float sigmoid_saturate(float x, float scale) {
    /* Maps skill difference to [0, 1] in a saturating way */
    /* scale controls the steepness (higher = sharper) */
    return 2.0f / (1.0f + expf(-x / scale)) - 1.0f;  /* range: [-1, 1] */
}

/* Calculate effective team MMR with saturation cap */
float calc_effective_mmr(float max_player_mmr, float avg_team_mmr) {
    /* Instead of linear: 0.4*max + 0.6*avg
       Use aggressive logarithmic saturation to prevent extreme domination */
    
    float skill_gap = max_player_mmr - avg_team_mmr;
    
    /* Logarithmic scaling: much slower growth for large gaps
       At gap=100: carry_bonus ~ 46
       At gap=200: carry_bonus ~ 84
       At gap=500: carry_bonus ~ 143 (capped well below linear)
       At gap=1000: carry_bonus ~ 160 (cap kicks in)
       This forces realistic convergence even for ultra-smurfs */
    
    float carry_bonus = skill_gap > 0 ? 
        logf(1.0f + skill_gap / 100.0f) * 100.0f :
        logf(1.0f - skill_gap / 100.0f) * (-100.0f);
    
    /* Reduced cap: maximum 90 MMR advantage (lowered from 120) */
    /* New targets: SMURF_HIGH ~80-85%, SMURF_MED ~65-75%, SMURF_LOW ~55-65% */
    if (carry_bonus > 90.0f) carry_bonus = 90.0f;
    if (carry_bonus < -90.0f) carry_bonus = -90.0f;
    
    return avg_team_mmr + carry_bonus;
}

/* Dynamic K-factor: slower progression over time */
float get_dynamic_K(int games_played) {
    if (games_played < 100)  return 40.0f;  /* early: rapid skill adjustment */
    if (games_played < 500)  return 20.0f;  /* mid: moderate adjustment */
    return 10.0f;                           /* late: slow refinement */
}

/* Rating deviation (uncertainty) decay over time */
float get_mmr_uncertainty(float initial_sigma, int games_played) {
    /* Exponential decay: e^(-rate * games) */
    /* At 50 games: ~71% of initial (200 * 0.707 ≈ 141) */
    /* At 100 games: ~50% of initial (200 * 0.507 ≈ 101) */
    /* At 500 games: ~7% of initial (200 * 0.069 ≈ 14) */
    /* rate ≈ 0.007 for above targets */
    float decay_rate = 0.007f;
    float factor = expf(-decay_rate * (float)games_played);
    float result = initial_sigma * factor;
    if (result < 10.0f) result = 10.0f;  /* minimum 10 uncertainty */
    return result;
}

/* Target MMR based on true skill level (equilibrium anchor) */
float get_target_mmr_for_skill_level(SkillLevel skill_level) {
    /* Maps skill tier to realistic equilibrium target
       Accounts for carry bonus saturation and realistic expected WR
       
       High-skill: target HIGH (they earn it with dominance)
       - SMURF_HIGH: Maintain ~90% WR → target = 1750 (stays at top)
       - SMURF_MED: Achieve ~70% WR → target = 1350
       - SMURF_LOW: Achieve ~55% WR → target = 1100
       
       Mid-skill: target baseline
       - NORMAL: Expected ~50% WR → target = 1000
       
       Low-skill: target LOW (blocked at their natural level)
       - LOW_BAD: Natural ~45% WR → target = 910
       - LOW_VERY_BAD: Natural ~43% WR → target = 890
       - LOW_EXTREME: Natural ~37% WR → target = 800
    */
    switch (skill_level) {
        case SKILL_SMURF_HIGH:     return 1750.0f;  /* Stay dominant: 90% WR equilibrium */
        case SKILL_SMURF_MED:      return 1350.0f;  /* Clear advantage: 70% WR equilibrium */
        case SKILL_SMURF_LOW:      return 1100.0f;  /* Slight edge: 55% WR equilibrium */
        case SKILL_NORMAL:         return 1000.0f;  /* Balanced: 50% WR (baseline) */
        case SKILL_LOW_BAD:        return 910.0f;   /* Slight disadvantage: 45% WR */
        case SKILL_LOW_VERY_BAD:   return 890.0f;   /* Significant disadvantage: 43% WR */
        case SKILL_LOW_EXTREME:    return 800.0f;   /* Extreme disadvantage: 37% WR */
        default:                   return 1000.0f;
    }
}

void update_mmr(PlayerTimeline *p, float opponent_mmr, int did_win) {
    float expected = calculate_expected(p->mmr_raw, opponent_mmr);
    
    /* CRITICAL FIX: Moderate clamp for low skill players to require ~55% WR to climb */
    if (p->skill_level == SKILL_LOW_VERY_BAD || p->skill_level == SKILL_LOW_EXTREME || p->skill_level == SKILL_LOW_BAD) {
        /* Moderate clamp: 0.30-0.85 instead of 0.10-0.90 */
        /* This requires about 55% WR to break even */
        if (expected > 0.85f) expected = 0.85f;
        if (expected < 0.30f) expected = 0.30f;
    } else {
        /* Standard clamp for normal/smurf players */
        if (expected > 0.90f) expected = 0.90f;
        if (expected < 0.10f) expected = 0.10f;
    }
    
    float outcome = did_win ? 1.0f : 0.0f;
    
    /* K-factor: smaller over time = slower convergence */
    float K = get_dynamic_K(p->total_games);
    
    /* K reduction for very weak players */
    if (p->skill_level == SKILL_LOW_VERY_BAD || p->skill_level == SKILL_LOW_EXTREME || p->skill_level == SKILL_LOW_BAD) {
        K *= 0.55f;
    }
    
    float delta = K * (outcome - expected);
    
    /* ESSENTIAL FIX: Apply WR-based scaling to enforce ELO fundamental law */
    /* Players with WR < 50% should not climb, WR > 50% should climb */
    if (p->total_games >= 20) {  /* Only after minimum sample size */
        float player_wr = (float)p->wins / (float)p->total_games;
        
        /* Factor ranges from -1.0 (0% WR) to +1.0 (100% WR) */
        float wr_factor = (player_wr - 0.5f) * 2.0f;
        
        /* Clamp to prevent extreme scaling at edges */
        /* This prevents 100% WR from getting 2x multiplier */
        if (wr_factor > 0.5f) wr_factor = 0.5f;
        if (wr_factor < -0.5f) wr_factor = -0.5f;
        
        /* Apply clamped scaling (coefficient 1.0) */
        /* 40% WR: wr_factor = -0.2, delta *= 0.8 (20% reduction) */
        /* 50% WR: wr_factor = 0, delta unchanged */
        /* 65% WR: wr_factor = 0.3, delta *= 1.3 (30% boost) */
        /* 100% WR (clamped to 0.5): delta *= 1.5 (50% boost, capped) */
        delta *= (1.0f + wr_factor * 1.0f);
    }
    
    
    p->mmr_raw += delta;
    if (p->mmr_raw < 0.0f) p->mmr_raw = 0.0f;
    
    /* Update mmr_uncertainty for next game */
    p->mmr_uncertainty = get_mmr_uncertainty(p->mmr_uncertainty, p->total_games + 1);
}

void update_tilt(PlayerTimeline *p, int did_win) {
    if (did_win) {
        p->win_streak++;
        p->lose_streak = 0;
    } else {
        p->lose_streak++;
        p->win_streak = 0;
    }
}

/* ============================================================
 * INITIALIZATION
 * ============================================================ */

void init_player(PlayerTimeline *p, int id, SkillLevel skill) {
    p->id = id;
    p->skill_level = skill;
    
    /* Performance stats based on skill level */
    float lo, hi;
    if (skill == SKILL_SMURF_LOW || skill == SKILL_SMURF_MED || skill == SKILL_SMURF_HIGH) {
        lo = 0.48f; hi = 0.68f;  /* High performer */
    } else if (skill == SKILL_LOW_BAD || skill == SKILL_LOW_VERY_BAD || skill == SKILL_LOW_EXTREME) {
        lo = 0.34f; hi = 0.54f;  /* Low performer */
    } else {
        lo = 0.30f; hi = 0.70f;  /* Normal range */
    }
    
    float range = hi - lo;
    p->perf.mechanical_skill = lo + randf() * range;
    p->perf.decision_making = lo + randf() * range;
    p->perf.map_awareness = lo + randf() * range;
    p->perf.tilt_resistance = lo + randf() * range;
    p->perf.champion_pool_depth = lo + randf() * range;
    p->perf.champion_proficiency = lo + randf() * range;
    p->perf.wave_management = lo + randf() * range;
    p->perf.teamfight_positioning = lo + randf() * range;
    
    /* MMR INIT by skill level (realistic distribution) */
    switch (skill) {
        case SKILL_SMURF_LOW:
            p->mmr_raw = 1200.0f + randf() * 100.0f;  /* +200-300 */
            break;
        case SKILL_SMURF_MED:
            p->mmr_raw = 1400.0f + randf() * 200.0f;  /* +400-600 */
            break;
        case SKILL_SMURF_HIGH:
            p->mmr_raw = 1700.0f + randf() * 300.0f;  /* +700-1000 */
            break;
        case SKILL_LOW_BAD:
            p->mmr_raw = 500.0f + randf() * 200.0f;   /* 500-700 */
            break;
        case SKILL_LOW_VERY_BAD:
            p->mmr_raw = 300.0f + randf() * 200.0f;   /* 300-500 */
            break;
        case SKILL_LOW_EXTREME:
            p->mmr_raw = 100.0f + randf() * 200.0f;   /* 100-300 */
            break;
        default:
            p->mmr_raw = 950.0f + randf() * 100.0f;   /* 950-1050: normal */
            break;
    }
    
    p->hidden_factor = 1.0f;
    p->total_games = 0;
    p->wins = p->losses = 0;
    p->win_streak = p->lose_streak = 0;
    p->actual_game_count = 0;
    
    /* MMR uncertainty (sigma): high initially, decays with games */
    p->mmr_uncertainty = 200.0f;  /* large initial uncertainty for all players */
    
    p->mmr_timeline[0] = p->mmr_raw;
    memset(p->outcomes, 0, sizeof(p->outcomes));
}

void init_players(PlayerTimeline *players, int n) {
    /* Distribution: Smurfs 2.2%, Low Skill 10%, Normal 87.8% */
    int n_smurfs_total = (int)(n * 0.022f);  /* ~22 for n=1000 */
    int n_low_total = (int)(n * 0.100f);    /* ~100 for n=1000 */
    /* rest = normal */
    
    /* Smurf sub-distribution: 50% LOW, 35% MED, 15% HIGH */
    int n_smurf_low = (int)(n_smurfs_total * 0.50f);
    int n_smurf_med = (int)(n_smurfs_total * 0.35f);
    int n_smurf_high = n_smurfs_total - n_smurf_low - n_smurf_med;
    
    /* Low skill sub-distribution: 50% BAD, 35% VERY_BAD, 15% EXTREME */
    int n_low_bad = (int)(n_low_total * 0.50f);
    int n_low_very_bad = (int)(n_low_total * 0.35f);
    int n_low_extreme = n_low_total - n_low_bad - n_low_very_bad;
    
    int idx = 0;
    
    /* Initialize smurf low */
    for (int i = 0; i < n_smurf_low; i++) {
        init_player(&players[idx], idx, SKILL_SMURF_LOW);
        idx++;
    }
    
    /* Initialize smurf med */
    for (int i = 0; i < n_smurf_med; i++) {
        init_player(&players[idx], idx, SKILL_SMURF_MED);
        idx++;
    }
    
    /* Initialize smurf high */
    for (int i = 0; i < n_smurf_high; i++) {
        init_player(&players[idx], idx, SKILL_SMURF_HIGH);
        idx++;
    }
    
    /* Initialize low bad */
    for (int i = 0; i < n_low_bad; i++) {
        init_player(&players[idx], idx, SKILL_LOW_BAD);
        idx++;
    }
    
    /* Initialize low very bad */
    for (int i = 0; i < n_low_very_bad; i++) {
        init_player(&players[idx], idx, SKILL_LOW_VERY_BAD);
        idx++;
    }
    
    /* Initialize low extreme */
    for (int i = 0; i < n_low_extreme; i++) {
        init_player(&players[idx], idx, SKILL_LOW_EXTREME);
        idx++;
    }
    
    /* Initialize normal (rest) */
    while (idx < n) {
        init_player(&players[idx], idx, SKILL_NORMAL);
        idx++;
    }
    
    /* CRITICAL: Shuffle players so skill distribution is random, not sequential */
    for (int i = n - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        PlayerTimeline tmp = players[i];
        players[i] = players[j];
        players[j] = tmp;
    }
}

/* ============================================================
 * ROLLING METRICS
 * ============================================================ */

typedef struct {
    float rolling_wr_20;
    float rolling_wr_50;
    float peak_wr_window;  /* best 50-game window */
    int peak_wr_start;     /* where the peak window starts */
    int longest_win_streak;
    int longest_lose_streak;
    
    /* CONVERGENCE METRICS */
    float convergence_error;      /* distance from estimated true skill plateau */
    float oscillation_amplitude;  /* stddev around the plateau */
    int recovery_speed;           /* games to recover after longest streak */
    float true_skill_plateau;     /* estimated converged MMR */
} PlayerMetrics;

const char* get_wr_comment(float wr) {
    if (wr >= 80.0f) return "Domination complète";
    if (wr >= 70.0f) return "Strong sustained carry";
    if (wr >= 60.0f) return "Clear advantage";
    if (wr >= 55.0f) return "Slight advantage";
    if (wr >= 50.0f) return "Balanced";
    if (wr >= 45.0f) return "Slight disadvantage";
    return "Convergence en cours";
}

/* Convert MMR to Rank with divisions (like League of Legends) */
const char* mmr_to_rank(float mmr) {
    if (mmr >= 2200.0f) return "💎 Master";
    
    /* Diamond: 1900-2199 */
    if (mmr >= 2100.0f) return "💎 Diamond I";
    if (mmr >= 2000.0f) return "💎 Diamond II";
    if (mmr >= 1950.0f) return "💎 Diamond III";
    if (mmr >= 1900.0f) return "💎 Diamond IV";
    
    /* Platinum: 1600-1899 */
    if (mmr >= 1800.0f) return "🏆 Platinum I";
    if (mmr >= 1750.0f) return "🏆 Platinum II";
    if (mmr >= 1700.0f) return "🏆 Platinum III";
    if (mmr >= 1600.0f) return "🏆 Platinum IV";
    
    /* Gold: 1300-1599 */
    if (mmr >= 1500.0f) return "⭐ Gold I";
    if (mmr >= 1450.0f) return "⭐ Gold II";
    if (mmr >= 1400.0f) return "⭐ Gold III";
    if (mmr >= 1300.0f) return "⭐ Gold IV";
    
    /* Silver: 1000-1299 */
    if (mmr >= 1200.0f) return "🥈 Silver I";
    if (mmr >= 1150.0f) return "🥈 Silver II";
    if (mmr >= 1100.0f) return "🥈 Silver III";
    if (mmr >= 1000.0f) return "🥈 Silver IV";
    
    /* Bronze: 700-999 */
    if (mmr >= 900.0f) return "🥉 Bronze I";
    if (mmr >= 850.0f) return "🥉 Bronze II";
    if (mmr >= 800.0f) return "🥉 Bronze III";
    if (mmr >= 700.0f) return "🥉 Bronze IV";
    
    /* Iron: 400-699 */
    if (mmr >= 600.0f) return "🔩 Iron I";
    if (mmr >= 550.0f) return "🔩 Iron II";
    if (mmr >= 500.0f) return "🔩 Iron III";
    if (mmr >= 400.0f) return "🔩 Iron IV";
    
    return "⚫ Unranked";
}

/* Classification helpers for aggregating smurf/normal/low skill groups */
int is_smurf(SkillLevel skill) {
    return skill == SKILL_SMURF_LOW || skill == SKILL_SMURF_MED || skill == SKILL_SMURF_HIGH;
}

int is_low_skill(SkillLevel skill) {
    return skill == SKILL_LOW_BAD || skill == SKILL_LOW_VERY_BAD || skill == SKILL_LOW_EXTREME;
}

const char* skill_label(SkillLevel skill) {
    switch (skill) {
        case SKILL_SMURF_LOW: return "🔥 SMURF_LOW";
        case SKILL_SMURF_MED: return "🔥🔥 SMURF_MED";
        case SKILL_SMURF_HIGH: return "🔥🔥🔥 SMURF_HIGH";
        case SKILL_LOW_BAD: return "💔 LOW_BAD";
        case SKILL_LOW_VERY_BAD: return "💔💔 LOW_VERY_BAD";
        case SKILL_LOW_EXTREME: return "💔💔💔 LOW_EXTREME";
        default: return "📊 NORMAL";
    }
}

float get_rolling_wr(PlayerTimeline *p, int window_size, int up_to_game) {
    if (up_to_game < window_size) return -1.0f;  /* not enough games */
    
    int wins = 0;
    for (int i = up_to_game - window_size; i < up_to_game; i++) {
        wins += p->outcomes[i];
    }
    return (float)wins / (float)window_size * 100.0f;
}

void calculate_player_metrics(PlayerTimeline *p, PlayerMetrics *metrics) {
    int num_games = p->actual_game_count;
    
    /* Rolling WR at end of season */
    metrics->rolling_wr_20 = get_rolling_wr(p, 20, num_games);
    if (metrics->rolling_wr_20 < 0.0f) metrics->rolling_wr_20 = 0.0f;
    
    metrics->rolling_wr_50 = get_rolling_wr(p, 50, num_games);
    if (metrics->rolling_wr_50 < 0.0f) metrics->rolling_wr_50 = 0.0f;
    
    /* Peak window (best consecutive 50-game window) */
    metrics->peak_wr_window = 0.0f;
    metrics->peak_wr_start = 0;
    if (num_games >= 50) {
        for (int i = 50; i <= num_games; i++) {
            float wr = get_rolling_wr(p, 50, i);
            if (wr > metrics->peak_wr_window) {
                metrics->peak_wr_window = wr;
                metrics->peak_wr_start = i - 50;
            }
        }
    }
    
    /* Longest streaks (over entire history) */
    metrics->longest_win_streak = 0;
    metrics->longest_lose_streak = 0;
    
    int current_streak = 0;
    for (int i = 0; i < num_games; i++) {
        if (p->outcomes[i] == 1) {
            current_streak++;
            if (current_streak > metrics->longest_win_streak) {
                metrics->longest_win_streak = current_streak;
            }
        } else {
            current_streak = 0;
        }
    }
    
    current_streak = 0;
    for (int i = 0; i < num_games; i++) {
        if (p->outcomes[i] == 0) {
            current_streak++;
            if (current_streak > metrics->longest_lose_streak) {
                metrics->longest_lose_streak = current_streak;
            }
        } else {
            current_streak = 0;
        }
    }
    
    /* ═══════════════════════════════════════════════════════ */
    /* CONVERGENCE METRICS */
    /* ═══════════════════════════════════════════════════════ */
    
    /* True skill plateau: estimated from last half of games (when converged) */
    int plateau_start = (num_games > 50) ? (num_games / 2) : 0;
    float sum_mmr_plateau = 0.0f;
    
    for (int i = plateau_start; i < num_games; i++) {
        sum_mmr_plateau += p->mmr_timeline[i];
    }
    
    int plateau_games = num_games - plateau_start;
    if (plateau_games <= 0) plateau_games = 1;
    
    metrics->true_skill_plateau = sum_mmr_plateau / (float)plateau_games;
    
    /* Convergence error: distance from current MMR to estimated plateau */
    metrics->convergence_error = fabsf(p->mmr_raw - metrics->true_skill_plateau);
    
    /* Oscillation amplitude: stddev around the plateau */
    float sum_sq_dev = 0.0f;
    for (int i = plateau_start; i < num_games; i++) {
        float dev = p->mmr_timeline[i] - metrics->true_skill_plateau;
        sum_sq_dev += dev * dev;
    }
    metrics->oscillation_amplitude = sqrtf(sum_sq_dev / (float)plateau_games);
    
    /* Recovery speed: games needed to return to plateau after longest streak */
    metrics->recovery_speed = 0;
    int longest_streak = (metrics->longest_win_streak > metrics->longest_lose_streak) 
                         ? metrics->longest_win_streak 
                         : metrics->longest_lose_streak;
    
    if (longest_streak > 0) {
        /* Find end of longest win streak */
        int current_w = 0;
        int streak_end = -1;
        for (int i = 0; i < num_games; i++) {
            if (p->outcomes[i] == 1) {
                current_w++;
                if (current_w == longest_streak) {
                    streak_end = i;
                    break;
                }
            } else {
                current_w = 0;
            }
        }
        
        /* Find longest lose streak similarly */
        int current_l = 0;
        for (int i = 0; i < num_games; i++) {
            if (p->outcomes[i] == 0) {
                current_l++;
                if (current_l == longest_streak) {
                    if (streak_end < 0 || i > streak_end) {
                        streak_end = i;
                    }
                    break;
                }
            } else {
                current_l = 0;
            }
        }
        
        /* Count games after streak until MMR returns within 2% of plateau */
        if (streak_end > 0 && streak_end < num_games) {
            float target_range = metrics->true_skill_plateau * 0.02f;  /* ±2% */
            
            for (int i = streak_end + 1; i < num_games; i++) {
                metrics->recovery_speed++;
                if (fabsf(p->mmr_timeline[i] - metrics->true_skill_plateau) <= target_range) {
                    break;
                }
            }
        }
    }
}

/* ============================================================
 * POOL STATS (Cumulative: games 1 → pool_size)
 * ============================================================ */

void calculate_pool_stats(PlayerTimeline *p, int pool_size, PoolStats *stats) {
    if (pool_size > p->actual_game_count) {
        pool_size = p->actual_game_count;
    }
    if (pool_size <= 0) {
        memset(stats, 0, sizeof(PoolStats));
        return;
    }
    
    stats->pool_size = pool_size;
    stats->mmr_start = p->mmr_timeline[0];
    stats->mmr_end = p->mmr_timeline[pool_size];
    
    /* Count wins */
    stats->wins = 0;
    for (int i = 0; i < pool_size; i++) {
        stats->wins += p->outcomes[i];
    }
    stats->win_rate = 100.0f * stats->wins / (float)pool_size;
    
    /* MMR stats */
    stats->mmr_avg = 0.0f;
    for (int i = 0; i <= pool_size; i++) {
        stats->mmr_avg += p->mmr_timeline[i];
    }
    stats->mmr_avg /= (float)(pool_size + 1);
    
    float sum_sq_dev = 0.0f;
    for (int i = 0; i <= pool_size; i++) {
        float dev = p->mmr_timeline[i] - stats->mmr_avg;
        sum_sq_dev += dev * dev;
    }
    stats->mmr_variance = sqrtf(sum_sq_dev / (float)(pool_size + 1));
    
    /* Calculate uncertainty at end of pool */
    stats->uncertainty_end = get_mmr_uncertainty(200.0f, pool_size);
    
    /* Streaks */
    stats->longest_win_streak = 0;
    stats->longest_lose_streak = 0;
    
    int current_streak = 0;
    for (int i = 0; i < pool_size; i++) {
        if (p->outcomes[i] == 1) {
            current_streak++;
            if (current_streak > stats->longest_win_streak) {
                stats->longest_win_streak = current_streak;
            }
        } else {
            current_streak = 0;
        }
    }
    
    current_streak = 0;
    for (int i = 0; i < pool_size; i++) {
        if (p->outcomes[i] == 0) {
            current_streak++;
            if (current_streak > stats->longest_lose_streak) {
                stats->longest_lose_streak = current_streak;
            }
        } else {
            current_streak = 0;
        }
    }
}

/* ============================================================
 * PHASE ANALYSIS
 * ============================================================ */

typedef struct {
    int phase_games;
    float mmr_start, mmr_end;
    float wr_percent;
    float rolling_wr_50;
} PhaseStats;

void analyze_phase(PlayerTimeline *p, int start_game, int end_game, PhaseStats *phase) {
    if (start_game >= p->actual_game_count) {
        phase->phase_games = 0;
        return;
    }
    
    int actual_end = (end_game <= p->actual_game_count) ? end_game : p->actual_game_count;
    int game_count = actual_end - start_game;
    
    if (game_count <= 0) {
        phase->phase_games = 0;
        return;
    }
    
    phase->phase_games = game_count;
    phase->mmr_start = p->mmr_timeline[start_game];
    phase->mmr_end = p->mmr_timeline[actual_end];
    
    int wins = 0;
    for (int i = start_game; i < actual_end; i++) {
        wins += p->outcomes[i];
    }
    phase->wr_percent = (float)wins / (float)game_count * 100.0f;
    
    /* rolling WR at end of phase */
    phase->rolling_wr_50 = get_rolling_wr(p, 50, actual_end);
    if (phase->rolling_wr_50 < 0.0f) phase->rolling_wr_50 = 0.0f;
}

/* ============================================================
 * MAIN
 * ============================================================ */

int main(void) {
    srand(999);
    
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║  👥 PLAYER-CENTRIC SEASON ANALYSIS (100K games)      ║\n");
    printf("║  1000 players × 1000 games each                      ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n\n");
    
    /* INIT */
    PlayerTimeline *players = malloc(sizeof(PlayerTimeline) * N_PLAYERS);
    init_players(players, N_PLAYERS);
    
    printf("Simulating 100,000 games with 1000 players...\n");
    printf("(Each player tracked individually - 1000 games each)\n\n");
    
    /* Find 7 tracked players: 1 of each skill type */
    TrackedPlayer tracked[7] = {0};
    int tracked_count = 0;
    
    for (int i = 0; i < N_PLAYERS && tracked_count < 7; i++) {
        if (players[i].skill_level == SKILL_SMURF_LOW && !tracked[0].player) {
            tracked[0].player = &players[i];
            tracked[0].label = "🔥 SMURF_LOW";
            tracked_count++;
        } else if (players[i].skill_level == SKILL_SMURF_MED && !tracked[1].player) {
            tracked[1].player = &players[i];
            tracked[1].label = "🔥🔥 SMURF_MED";
            tracked_count++;
        } else if (players[i].skill_level == SKILL_SMURF_HIGH && !tracked[2].player) {
            tracked[2].player = &players[i];
            tracked[2].label = "🔥🔥🔥 SMURF_HIGH";
            tracked_count++;
        } else if (players[i].skill_level == SKILL_NORMAL && !tracked[3].player) {
            tracked[3].player = &players[i];
            tracked[3].label = "📊 NORMAL";
            tracked_count++;
        } else if (players[i].skill_level == SKILL_LOW_BAD && !tracked[4].player) {
            tracked[4].player = &players[i];
            tracked[4].label = "💔 LOW_BAD";
            tracked_count++;
        } else if (players[i].skill_level == SKILL_LOW_VERY_BAD && !tracked[5].player) {
            tracked[5].player = &players[i];
            tracked[5].label = "💔💔 LOW_VERY_BAD";
            tracked_count++;
        } else if (players[i].skill_level == SKILL_LOW_EXTREME && !tracked[6].player) {
            tracked[6].player = &players[i];
            tracked[6].label = "💔💔💔 LOW_EXTREME";
            tracked_count++;
        }
    }
    
    /* ─────────────────────────────────────────────────────────── */
    /* SIMULATION */
    /* ─────────────────────────────────────────────────────────── */
    
    int overall_match_num = 0;
    int game_round = 0;
    
    /* Continue until ALL players have reached MAX_GAMES_PER_PLAYER */
    while (true) {
        /* Check if any player needs more games */
        int players_needing_games = 0;
        for (int i = 0; i < N_PLAYERS; i++) {
            if (players[i].actual_game_count < MAX_GAMES_PER_PLAYER) {
                players_needing_games++;
            }
        }
        if (players_needing_games == 0) break;  /* All done */
        
        /* OPTIMIZED: Each player plays ONCE per round (all 1000) */
        /* Shuffle only the assignment of players to matches, but all play */
        int player_indices[N_PLAYERS];
        for (int i = 0; i < N_PLAYERS; i++) {
            player_indices[i] = i;
        }
        
        /* Fisher-Yates shuffle of indices */
        for (int i = N_PLAYERS - 1; i > 0; --i) {
            int j = rand() % (i + 1);
            int tmp = player_indices[i];
            player_indices[i] = player_indices[j];
            player_indices[j] = tmp;
        }
        
        int matches = N_PLAYERS / 10;  /* 100 matches per round */
        
        for (int m = 0; m < matches; m++) {
            /* Assign 10 players from shuffled indices: 5 vs 5 */
            PlayerTimeline *team_a[5];
            PlayerTimeline *team_b[5];
            
            for (int i = 0; i < 5; i++) {
                team_a[i] = &players[player_indices[m * 10 + i]];
                team_b[i] = &players[player_indices[m * 10 + 5 + i]];
            }
            
            /* ═══════════════════════════════════════════════════════════════ */
            /* TWO-MMR SYSTEM: mmr_raw (truth) vs mmr_effective (matchmaking)  */
            /* ───────────────────────────────────────────────────────────────*/
            /* mmr_raw: true skill level (progresses via Elo)                 */
            /* mmr_effective: mmr_raw + carry_bias (EOMM-like snowball)       */
            /* ═══════════════════════════════════════════════════════════════ */
            
            /* Calculate team statistics from mmr_raw (true skill) */
            float team_a_raw_avg = 0.0f, team_b_raw_avg = 0.0f;
            float max_a_raw = 0.0f, max_b_raw = 0.0f;
            
            for (int i = 0; i < 5; i++) {
                team_a_raw_avg += team_a[i]->mmr_raw;
                team_b_raw_avg += team_b[i]->mmr_raw;
                
                if (team_a[i]->mmr_raw > max_a_raw) max_a_raw = team_a[i]->mmr_raw;
                if (team_b[i]->mmr_raw > max_b_raw) max_b_raw = team_b[i]->mmr_raw;
            }
            team_a_raw_avg /= 5.0f;
            team_b_raw_avg /= 5.0f;
            
            /* Effective MMR for matchmaking: incorporates snowball with SATURATION cap */
            /* Instead of linear 0.4*max + 0.6*avg, use sigmoid saturation */
            /* This prevents SMURF_HIGH from reaching 100% WR */
            float team_a_mmr_eff = calc_effective_mmr(max_a_raw, team_a_raw_avg);
            float team_b_mmr_eff = calc_effective_mmr(max_b_raw, team_b_raw_avg);
            
            /* ═══════════════════════════════════════════════════════ */
            /* OPTION B: MATCHMAKING TOLERANCE + UNCERTAINTY VARIANCE   */
            /* ───────────────────────────────────────────────────────*/
            /* - Accept 200-300 MMR gap as "typical"                  */
            /* - Incorporate mmr_uncertainty for dynamic variance      */
            /* - Higher uncertainty = higher variance in outcomes      */
            /* ═══════════════════════════════════════════════════════ */
            
            /* Calculate average uncertainty on both teams */
            float unc_a = 0.0f, unc_b = 0.0f;
            for (int i = 0; i < 5; i++) {
                unc_a += team_a[i]->mmr_uncertainty;
                unc_b += team_b[i]->mmr_uncertainty;
            }
            unc_a /= 5.0f;
            unc_b /= 5.0f;
            float avg_uncertainty = (unc_a + unc_b) / 2.0f;
            
            /* Normalize uncertainty to variance boost (200 sigma → 1.0x boost, 10 sigma → 0.05x boost) */
            float uncertainty_variance_boost = fmaxf(avg_uncertainty / 200.0f, 0.05f);
            
            /* MMR gap for variance (using effective ratings) */
            float mmr_gap = fabsf(team_a_mmr_eff - team_b_mmr_eff);
            
            /* Base win probability from ELO difference (using EFFECTIVE ratings) */
            float win_prob_a = calculate_expected(team_a_mmr_eff, team_b_mmr_eff);
            
            /* Asymmetric advantage from MMR gap */
            float strength_factor = fminf(mmr_gap / 2500.0f, 0.20f);
            float base_variance = fminf(mmr_gap / 4000.0f, 0.12f);
            
            /* Boost variance based on uncertainty (early games = more variance) */
            float random_variance = base_variance * uncertainty_variance_boost;
            
            /* Apply strength advantage */
            float team_advantage = (team_a_mmr_eff > team_b_mmr_eff) ? strength_factor : -strength_factor;
            win_prob_a += team_advantage;
            
            /* Add random variance around expectation */
            float random_swing = (randf() - 0.5f) * 2.0f * random_variance;
            win_prob_a += random_swing;
            
            /* FIX #2: Add structural penalty for LOW_VERY_BAD players */
            /* Count and apply penalty if team has weak players */
            int n_very_bad_a = 0, n_very_bad_b = 0;
            for (int i = 0; i < 5; i++) {
                if (team_a[i]->skill_level == SKILL_LOW_VERY_BAD) n_very_bad_a++;
                if (team_b[i]->skill_level == SKILL_LOW_VERY_BAD) n_very_bad_b++;
            }
            
            /* Apply penalty: 3% per LOW_VERY_BAD player (max 15%) */
            float penalty_a = n_very_bad_a * 0.03f;
            float penalty_b = n_very_bad_b * 0.03f;
            
            win_prob_a -= penalty_a;
            win_prob_a += penalty_b;
            
            win_prob_a = fmaxf(0.0f, fminf(1.0f, win_prob_a));  /* clamp to [0,1] */
            
            int winner = (randf() < win_prob_a) ? 0 : 1;
            
            /* Update players (both MUST be below limit, but they WILL be from the new INDEX) */
            for (int i = 0; i < 5; i++) {
                PlayerTimeline *pa = team_a[i];
                PlayerTimeline *pb = team_b[i];
                
                /* Since all players play = all are below limit; but safety check anyway */
                if (pa->actual_game_count >= MAX_GAMES_PER_PLAYER || 
                    pb->actual_game_count >= MAX_GAMES_PER_PLAYER) {
                    continue;
                }
                
                int a_won = (winner == 0);
                int b_won = (winner == 1);
                
                /* Track game outcome */
                pa->outcomes[pa->actual_game_count] = a_won ? 1 : 0;
                pb->outcomes[pb->actual_game_count] = b_won ? 1 : 0;
                
                /* General counters */
                pa->total_games++;
                pb->total_games++;
                if (a_won) { pa->wins++; pb->losses++; }
                else       { pa->losses++; pb->wins++; }
                
                /* MMR update against TEAM AVERAGE (not carry-weighted) */
                /* Carry weight affects win probability, not MMR gains */
                update_mmr(pa, team_b_raw_avg, a_won);
                update_mmr(pb, team_a_raw_avg, b_won);
                
                /* Store new MMR in timeline after update */
                pa->mmr_timeline[pa->actual_game_count + 1] = pa->mmr_raw;
                pb->mmr_timeline[pb->actual_game_count + 1] = pb->mmr_raw;
                
                /* Increment game counters */
                pa->actual_game_count++;
                pb->actual_game_count++;
                
                /* Tilt */
                update_tilt(pa, a_won);
                update_tilt(pb, b_won);
            }
            
            overall_match_num++;
        }
        
        game_round++;
        if (game_round % 100 == 0) {
            printf("  ✓ Round %d (%d players still need games)\n", game_round, players_needing_games);
        }
    }
    
    printf("\n✅ Simulation complete\n\n");
        /* ═════════════════════════════════════════════════════════════ */
    /* ANALYSIS: 7 TRACKED PLAYERS WITH POOL STATS */
    /* ═════════════════════════════════════════════════════════════ */
    
    printf("═════════════════════════════════════════════════════════════\n");
    printf("📊 7 TRACKED PLAYERS - CUMULATIVE POOL ANALYSIS (ALL SKILL TYPES)\n");
    printf("═════════════════════════════════════════════════════════════\n\n");
    
    /* Calculate pool stats for all 4 pools (50, 100, 200, 300) */
    int pool_sizes[] = {50, 100, 200, 300};
    for (int t = 0; t < 7; t++) {
        TrackedPlayer *tp = &tracked[t];
        if (!tp->player) continue;
        
        printf("╔═ %s (ID: #%04d) ═╗\n", tp->label, tp->player->id + 1);
        printf("├─ TOTAL SEASON: %d games | %d wins (%.1f%%) | MMR: %.0f → %.0f [%+.0f]\n",
               tp->player->actual_game_count,
               tp->player->wins,
               100.0f * tp->player->wins / (float)tp->player->actual_game_count,
               tp->player->mmr_timeline[0],
               tp->player->mmr_raw,
               tp->player->mmr_raw - tp->player->mmr_timeline[0]);
        
        printf("\n");
        for (int p = 0; p < 4; p++) {
            calculate_pool_stats(tp->player, pool_sizes[p], &tp->pools[p]);
            
            PoolStats *ps = &tp->pools[p];
            if (ps->pool_size == 0) continue;
            
            printf("Pool %d (%dg): %.1f%% WR - %s\n",
                   p + 1, ps->pool_size,
                   ps->win_rate,
                   get_wr_comment(ps->win_rate));
            printf("         MMR: %.0f → %.0f [%+.0f] | %s → %s\n",
                   ps->mmr_start,
                   ps->mmr_end,
                   ps->mmr_end - ps->mmr_start,
                   mmr_to_rank(ps->mmr_start),
                   mmr_to_rank(ps->mmr_end));
        }
        printf("\n");
    }
        /* ─────────────────────────────────────────────────────────── */
    /* ANALYSIS: SAMPLE PLAYERS */
    /* ─────────────────────────────────────────────────────────── */
    
    printf("═══════════════════════════════════════════════════════\n");
    printf("👥 PLAYER-CENTRIC SEASON ANALYSIS (SAMPLES)\n");
    printf("═══════════════════════════════════════════════════════\n\n");
    
    /* Find one of each skill type for detailed analysis */
    PlayerTimeline *samples[7] = {NULL};
    for (int i = 0; i < N_PLAYERS; i++) {
        if (players[i].skill_level == SKILL_SMURF_LOW && !samples[0]) {
            samples[0] = &players[i];
        } else if (players[i].skill_level == SKILL_SMURF_MED && !samples[1]) {
            samples[1] = &players[i];
        } else if (players[i].skill_level == SKILL_SMURF_HIGH && !samples[2]) {
            samples[2] = &players[i];
        } else if (players[i].skill_level == SKILL_NORMAL && !samples[3]) {
            samples[3] = &players[i];
        } else if (players[i].skill_level == SKILL_LOW_BAD && !samples[4]) {
            samples[4] = &players[i];
        } else if (players[i].skill_level == SKILL_LOW_VERY_BAD && !samples[5]) {
            samples[5] = &players[i];
        } else if (players[i].skill_level == SKILL_LOW_EXTREME && !samples[6]) {
            samples[6] = &players[i];
        }
    }
    
    const char *sample_labels[] = {
        "🔥 SMURF_LOW (High Skill)",
        "🔥🔥 SMURF_MED (High Skill)",
        "🔥🔥🔥 SMURF_HIGH (High Skill)",
        "📊 NORMAL (Medium Skill)",
        "💔 LOW_BAD (Low Skill)",
        "💔💔 LOW_VERY_BAD (Low Skill)",
        "💔💔💔 LOW_EXTREME (Low Skill)"
    };
    
    PlayerTimeline *smurf_sample = samples[0], *normal_sample = samples[3], *hardstuck_sample = samples[4];
    
    #define ANALYZE_PLAYER(p, label) \
    do { \
        printf("\n╔═ %s (ID: #%04d, %d games) ═╗\n", label, p->id + 1, p->actual_game_count); \
        \
        PlayerMetrics metrics; \
        calculate_player_metrics(p, &metrics); \
        \
        /* Calculate actual wins from outcomes array */ \
        int actual_wins = 0; \
        for (int i = 0; i < p->actual_game_count; i++) { \
            actual_wins += p->outcomes[i]; \
        } \
        float global_wr = (p->actual_game_count > 0) ? \
            (100.0f * actual_wins / p->actual_game_count) : 0.0f; \
        \
        printf("├─ ROLLING METRICS\n"); \
        printf("│  WR rolling 20: %.1f%%\n", metrics.rolling_wr_20); \
        printf("│  WR rolling 50: %.1f%%\n", metrics.rolling_wr_50); \
        \
        printf("├─ PEAK PERFORMANCE\n"); \
        printf("│  Peak WR window: %.1f%% (games %d-%d)\n", \
               metrics.peak_wr_window, metrics.peak_wr_start, \
               metrics.peak_wr_start + 50); \
        \
        printf("├─ STREAKS & RECOVERY\n"); \
        printf("│  Longest win streak: %d\n", metrics.longest_win_streak); \
        printf("│  Longest lose streak: %d\n", metrics.longest_lose_streak); \
        printf("│  Recovery time after streak: %d games\n", metrics.recovery_speed); \
        \
        printf("├─ CONVERGENCE ANALYSIS\n"); \
        printf("│  True skill plateau (est.): %.1f MMR\n", metrics.true_skill_plateau); \
        printf("│  Convergence error: %.1f MMR\n", metrics.convergence_error); \
        printf("│  Oscillation amplitude (σ): %.1f MMR\n", metrics.oscillation_amplitude); \
        \
        printf("├─ PHASE BREAKDOWN\n"); \
        \
        PhaseStats phase1, phase2, phase3; \
        analyze_phase(p, 0, 50, &phase1); \
        analyze_phase(p, 50, 200, &phase2); \
        analyze_phase(p, 200, 300, &phase3); \
        \
        if (phase1.phase_games > 0) { \
            printf("│  Early (0-%d):    %7.1f → %7.1f [%+6.1f] | WR %.1f%%\n", \
                   phase1.phase_games, phase1.mmr_start, phase1.mmr_end, \
                   phase1.mmr_end - phase1.mmr_start, phase1.wr_percent); \
        } \
        \
        if (phase2.phase_games > 0) { \
            printf("│  Mid (50-%d):     %7.1f → %7.1f [%+6.1f] | WR %.1f%%\n", \
                   phase1.phase_games + phase2.phase_games, phase2.mmr_start, \
                   phase2.mmr_end, phase2.mmr_end - phase2.mmr_start, phase2.wr_percent); \
        } \
        \
        if (phase3.phase_games > 0) { \
            printf("│  Late (200-%d):   %7.1f → %7.1f [%+6.1f] | WR %.1f%%\n", \
                   phase1.phase_games + phase2.phase_games + phase3.phase_games, \
                   phase3.mmr_start, phase3.mmr_end, phase3.mmr_end - phase3.mmr_start, \
                   phase3.wr_percent); \
        } \
        \
        printf("├─ OVERALL SEASON\n"); \
        printf("│  Total games: %d\n", p->actual_game_count); \
        printf("│  Global WR: %.1f%%\n", global_wr); \
        printf("│  MMR journey: %.1f → %.1f [%+.1f]\n", \
               p->mmr_timeline[0], p->mmr_raw, \
               p->mmr_raw - p->mmr_timeline[0]); \
        printf("└─\n\n"); \
    } while (0)
    
    ANALYZE_PLAYER(smurf_sample, "🔥 SMURF (High Skill)");
    ANALYZE_PLAYER(normal_sample, "📊 NORMAL (Medium Skill)");
    ANALYZE_PLAYER(hardstuck_sample, "💔 HARDSTUCK (Low Skill)");
    
    /* ─────────────────────────────────────────────────────────── */
    /* AGGREGATE STATISTICS */
    /* ─────────────────────────────────────────────────────────── */
    
    printf("═══════════════════════════════════════════════════════\n");
    printf("📈 AGGREGATE SEASON STATISTICS BY SKILL\n");
    printf("═══════════════════════════════════════════════════════\n\n");
    
    typedef struct {
        int count;
        int total_games_played;
        float total_wr;
        float total_mmr_delta;
        float avg_rolling_wr_50;
    } AggSeason;
    
    AggSeason agg_smurf = {0}, agg_normal = {0}, agg_low = {0};
    
    for (int i = 0; i < N_PLAYERS; i++) {
        PlayerMetrics metrics;
        calculate_player_metrics(&players[i], &metrics);
        
        /* Count actual wins from outcomes array */
        int actual_wins = 0;
        for (int j = 0; j < players[i].actual_game_count; j++) {
            actual_wins += players[i].outcomes[j];
        }
        
        AggSeason *agg_ptr = NULL;
        if (is_smurf(players[i].skill_level)) {
            agg_ptr = &agg_smurf;
        } else if (is_low_skill(players[i].skill_level)) {
            agg_ptr = &agg_low;
        } else {
            agg_ptr = &agg_normal;
        }
        
        agg_ptr->count++;
        agg_ptr->total_games_played += players[i].actual_game_count;
        
        if (players[i].actual_game_count > 0) {
            float wr = 100.0f * actual_wins / players[i].actual_game_count;
            agg_ptr->total_wr += wr;
        }
        agg_ptr->total_mmr_delta += (players[i].mmr_raw - 1000.0f);
        agg_ptr->avg_rolling_wr_50 += metrics.rolling_wr_50;
    }
    
    /* Print aggregates */
    AggSeason aggs[3] = {agg_normal, agg_smurf, agg_low};
    const char *labels[] = {"NORMAL", "SMURF", "LOW_SKILL"};
    
    for (int idx = 0; idx < 3; idx++) {
        AggSeason *agg = &aggs[idx];
        if (agg->count == 0) continue;
        
        float avg_games = (float)agg->total_games_played / agg->count;
        float avg_wr = agg->total_wr / agg->count;
        float avg_mmr_delta = agg->total_mmr_delta / agg->count;
        float avg_rolling_wr = agg->avg_rolling_wr_50 / agg->count;
        
        printf("%s (%d players)\n", labels[idx], agg->count);
        printf("  Avg games per season: %.1f\n", avg_games);
        printf("  Avg global WR: %.1f%%\n", avg_wr);
        printf("  Avg rolling WR 50: %.1f%%\n", avg_rolling_wr);
        printf("  Avg MMR delta from 1000: %+.1f\n\n", avg_mmr_delta);
    }
    
    printf("═══════════════════════════════════════════════════════\n");
    printf("✅ SEASON ANALYSIS COMPLETE\n");
    printf("═══════════════════════════════════════════════════════\n");
    
    free(players);
    return 0;
}
