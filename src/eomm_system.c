/*
 * eomm_system.c
 *
 * EOMM (Engagement Optimized Matchmaking) — core mechanics implementation.
 *
 * Implements:
 *   1. Player initialisation with skill-based PerformanceStats (no fixed win rates)
 *   2. Dynamic win rate calculation from performance attributes
 *   3. Troll probability (arrogance + win-streak based)
 *   4. Troll penalty (hidden_factor reduction, escalates with consecutive trolls)
 *   5. Tilt factor system (lose streaks degrade state)
 *   6. Hidden state tracking (NEGATIVE / NEUTRAL / POSITIVE)
 *   7. Soft resets (every 14 games)
 *   8. Dynamic EOMM matchmaking (tilt → losing team, healthy → winning team)
 *   9. Match simulation (MMR × hidden_factor × calculated win-rate)
 *  10. Analytics / reporting
 */

#include "../include/eomm_system.h"

/* =========================================================
 * Internal helpers
 * ========================================================= */

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/*
 * calculate_eomm_weight — compute global EOMM influence weight.
 *
 * Purpose: Scale all EOMM effects (bias, engagement, compensation) based
 * on the calibration phase and total games played.
 *
 * Calibration phase (games 0-50): weight = 0 (EOMM disabled)
 * Ramp-up phase (games 50-150): weight gradually increases to 1.0
 * Stable phase (games 150+): weight = 1.0
 *
 * This ensures early skill expression while preventing over-correction.
 */
static inline float calculate_eomm_weight(int total_games) {
    if (total_games < 50) {
        return 0.0f;  /* Calibration: zero EOMM */
    }
    if (total_games <= 150) {
        return (float)(total_games - 50) / 100.0f;  /* Ramp from 0 to 1 */
    }
    return 1.0f;  /* Full EOMM after 150 games */
}

/*
 * get_player_group — categorize a player for differential EOMM treatment.
 *
 * CORE (GROUP_CORE): 80% baseline
 *   - SKILL_NORMAL players
 *   - SKILL_HARDSTUCK (normal low skill, not extreme)
 *   → Standard EOMM, standard K-factor, standard compensation
 *
 * OUTLIERS_HIGH (GROUP_OUTLIERS_HIGH): Smurfs who dominate
 *   - SKILL_SMURF
 *   → Progressive difficulty: harder opponents as they climb
 *   → Effect: Prevent roflstomp at low MMR, natural convergence
 *
 * OUTLIERS_LOW (GROUP_OUTLIERS_LOW): Hardstuck extreme / very low skill
 *   - SKILL_HARDSTUCK with very low perf stats (avg < 0.25)
 *   → Enhanced protection: 2x compensation bonus
 *   → Softer matchmaking: reduced bias on losses
 */
PlayerGroup get_player_group(const Player *p) {
    if (p->skill_level == SKILL_SMURF) {
        return GROUP_OUTLIERS_HIGH;
    }
    
    if (p->skill_level == SKILL_HARDSTUCK) {
        /* Check if extreme low skill (very low performance average) */
        float avg_perf = (p->perf.mechanical_skill
                        + p->perf.decision_making
                        + p->perf.map_awareness
                        + p->perf.tilt_resistance
                        + p->perf.champion_pool_depth
                        + p->perf.champion_proficiency
                        + p->perf.wave_management
                        + p->perf.teamfight_positioning) / 8.0f;
        
        if (avg_perf < 0.25f) {
            return GROUP_OUTLIERS_LOW;
        }
    }
    
    return GROUP_CORE;
}

/*
 * get_target_mmr_for_group — return equilibrium MMR target for convergence.
 *
 * Smurfs should converge at higher MMR (their true skill);
 * normal players at 1000; hardstuck at lower MMR.
 *
 * For OUTLIERS_HIGH: target-based progressive difficulty
 *   - SMURF climbing to target gets progressively harder opposition
 *   - Once at target, additional EOMM bias prevents roflstomp
 */
float get_target_mmr_for_group(const Player *p) {
    PlayerGroup group = get_player_group(p);
    
    switch (group) {
        case GROUP_OUTLIERS_HIGH:
            /* Smurfs converge at high MMR (their natural skill level)
               Higher visible_mmr → need stronger opposition for convergence */
            if (p->visible_mmr < 1500.0f) {
                return 1400.0f;  /* Target for SMURF_MED (70% expected WR) */
            } else {
                return 1600.0f;  /* Target for SMURF_HIGH (80%+ expected WR) */
            }
        
        case GROUP_OUTLIERS_LOW:
            /* Hardstuck extreme: protect at low MMR (800-900) */
            return 850.0f;
        
        case GROUP_CORE:
        default:
            /* Normal players converge at baseline */
            if (p->skill_level == SKILL_HARDSTUCK) {
                return 900.0f;  /* Hardstuck: slightly below baseline */
            }
            return 1000.0f;     /* NORMAL: perfect balance */
    }
}

/*
 * get_compensation_bonus — returns the win-probability bonus for a player
 * who has suffered COMPENSATION_THRESHOLD or more consecutive losses.
 *
 * REFACTORED with GROUP consideration:
 *   - GROUP_OUTLIERS_LOW: 2x multiplier (enhanced protection)
 *   - GROUP_OUTLIERS_HIGH: full bonus (but they rarely get here)
 *   - GROUP_CORE: standard bonus
 *
 * Gradual escalation:
 *   7  losses → +15% (or +30% for LOW outliers)
 *   8  losses → +25% (or +50% for LOW outliers)
 *   9  losses → +35% (or +70% for LOW outliers)
 *  10+ losses → +50% (or +100% for LOW outliers)
 */
static inline float get_compensation_bonus(int lose_streak, int total_games, const Player *p) {
    if (lose_streak < COMPENSATION_THRESHOLD) return 0.0f;

    float bonus;
    if (lose_streak == 7) {
        bonus = COMPENSATION_BONUS_7;
    } else if (lose_streak == 8) {
        bonus = COMPENSATION_BONUS_8;
    } else if (lose_streak == 9) {
        bonus = COMPENSATION_BONUS_9;
    } else {
        bonus = COMPENSATION_MAX_BONUS;
    }
    
    /* Apply global EOMM weight */
    float weight = calculate_eomm_weight(total_games);
    bonus = bonus * weight;
    
    /* GROUP consideration: double bonus for OUTLIERS_LOW */
    if (get_player_group(p) == GROUP_OUTLIERS_LOW) {
        bonus *= 2.0f;
    }
    
    return bonus;
}

static float randf(void) {
    return (float)rand() / (float)RAND_MAX;
}

/* Fisher-Yates shuffle on a Player* pointer array */
static void shuffle_ptrs(Player **arr, int n) {
    for (int i = n - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        Player *tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

/* Fisher-Yates shuffle on a Player array (by value) */
static void shuffle_players(Player *arr, int n) {
    for (int i = n - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        Player tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

/* =========================================================
 * Player initialisation
 * ========================================================= */

void init_player(Player *p, int id, SkillLevel skill) {
    p->id = id;
    snprintf(p->name, PLAYER_NAME_LEN, "Player%04d", id + 1);

    p->skill_level = skill;

    /* Initialise independent PerformanceStats based on skill level.
     *
     * Each stat is drawn uniformly from the skill-level range so that
     * every player has a unique profile.  After all stats are set, one
     * randomly chosen stat is adjusted (upward for hardstuck, downward
     * for smurfs, either for normals) to create additional variance and
     * avoid players that are purely uniform.
     *
     * Ranges (before the single-stat adjustment):
     *   SMURF     : [0.70, 0.90]
     *   NORMAL    : [0.30, 0.70]
     *   HARDSTUCK : [0.05, 0.25]
     */
    float lo, hi, adj_delta;
    switch (skill) {
        case SKILL_SMURF:
            /* HIGH skill floor: 48-68% performance baseline */
            lo = 0.48f; hi = 0.68f;
            adj_delta = -0.08f; /* one weaker stat for variance */
            break;
        case SKILL_HARDSTUCK:
            /* LOW skill floor: 34-54% performance baseline */
            lo = 0.34f; hi = 0.54f;
            adj_delta = +0.04f; /* minimal boost for variance */
            break;
        default: /* SKILL_NORMAL */
            /* MEDIUM skill floor: 30-70% performance baseline */
            lo = 0.30f; hi = 0.70f;
            adj_delta = (randf() < 0.5f) ? +0.10f : -0.10f;
            break;
    }

    float range = hi - lo;
    p->perf.mechanical_skill      = lo + randf() * range;
    p->perf.decision_making       = lo + randf() * range;
    p->perf.map_awareness         = lo + randf() * range;
    p->perf.tilt_resistance       = lo + randf() * range;
    p->perf.champion_pool_depth   = lo + randf() * range;
    p->perf.champion_proficiency  = lo + randf() * range;
    p->perf.wave_management       = lo + randf() * range;
    p->perf.teamfight_positioning = lo + randf() * range;

    /* Apply the single-stat adjustment to a random stat index */
    float *stats[8] = {
        &p->perf.mechanical_skill,
        &p->perf.decision_making,
        &p->perf.map_awareness,
        &p->perf.tilt_resistance,
        &p->perf.champion_pool_depth,
        &p->perf.champion_proficiency,
        &p->perf.wave_management,
        &p->perf.teamfight_positioning
    };
    int adj_idx = rand() % 8;
    *stats[adj_idx] = clampf(*stats[adj_idx] + adj_delta, 0.0f, 1.0f);

    p->visible_mmr   = START_MMR;
    p->hidden_factor = HIDDEN_FACTOR_START;

    p->wins        = 0;
    p->losses      = 0;
    p->total_games = 0;

    p->win_streak  = 0;
    p->lose_streak = 0;

    p->hidden_state      = STATE_NEUTRAL;
    p->tilt_level        = 0;
    p->consecutive_trolls = 0;
    p->is_troll_pick     = 0;

    /* Assign two distinct random preferred roles */
    p->prefRoles[0] = rand() % ROLE_COUNT;
    do {
        p->prefRoles[1] = rand() % ROLE_COUNT;
    } while (p->prefRoles[1] == p->prefRoles[0]);

    p->current_role  = p->prefRoles[0];
    p->is_autofilled = 0;

    /* Engagement phase initialisation */
    p->engagement_phase        = PHASE_NEUTRAL;
    p->phase_progress          = 0;
    p->target_streak           = 0;
}

/*
 * init_players — populate n players with the EOMM distribution:
 *   first  10% → SMURF
 *   next   10% → HARDSTUCK
 *   rest   80% → NORMAL
 *
 * The order is shuffled after assignment so the distribution is random.
 */
void init_players(Player *players, int n) {
    int n_smurfs    = n / 10;
    int n_hardstuck = n / 10;

    for (int i = 0; i < n; i++) {
        SkillLevel skill;
        if (i < n_smurfs) {
            skill = SKILL_SMURF;
        } else if (i < n_smurfs + n_hardstuck) {
            skill = SKILL_HARDSTUCK;
        } else {
            skill = SKILL_NORMAL;
        }
        init_player(&players[i], i, skill);
    }

    /* Shuffle so players of each type are spread across the pool */
    shuffle_players(players, n);

    /* Re-assign sequential IDs after shuffling */
    for (int i = 0; i < n; i++) {
        players[i].id = i;
        snprintf(players[i].name, PLAYER_NAME_LEN, "Player%04d", i + 1);
    }
}

/* =========================================================
 * Troll probability
 * ========================================================= */

/*
 * calculate_troll_probability — arrogance-based troll chance (%).
 *
 * Rules:
 *   factor >= 0.95  → base = TROLL_BASE_HIGH_FACTOR (15%)
 *   factor <  0.95  → base scales linearly from 0% (at 0.50) to 15% (at 0.95)
 *   win_streak > 2  → +TROLL_WIN_STREAK_BONUS % per extra win
 *   result capped at TROLL_PROB_CAP (60%)
 */
float calculate_troll_probability(const Player *p) {
    float base;
    if (p->hidden_factor >= 0.95f) {
        base = TROLL_BASE_HIGH_FACTOR;
    } else {
        float range = 0.95f - HIDDEN_FACTOR_MIN; /* 0.45 */
        float pos   = p->hidden_factor - HIDDEN_FACTOR_MIN;
        base = (pos / range) * TROLL_BASE_HIGH_FACTOR;
        if (base < 0.0f) base = 0.0f;
    }

    /* Win streak bonus (arrogance from dominating) */
    if (p->win_streak > 2) {
        base += (float)(p->win_streak - 2) * TROLL_WIN_STREAK_BONUS;
    }

    return clampf(base, 0.0f, TROLL_PROB_CAP);
}

/* =========================================================
 * Troll penalty
 * ========================================================= */

/*
 * apply_troll_penalty — disrespect punishment, applied even on wins.
 *
 * Penalty formula:
 *   penalty = TROLL_PENALTY_BASE * TROLL_PENALTY_SCALE
 *             * (1 + consecutive_trolls * TROLL_ESCALATION)
 *
 * hidden_factor is clamped to HIDDEN_FACTOR_MIN (0.50).
 * consecutive_trolls is incremented.
 */
void apply_troll_penalty(Player *p) {
    float penalty = TROLL_PENALTY_BASE * TROLL_PENALTY_SCALE
                    * (1.0f + (float)p->consecutive_trolls * TROLL_ESCALATION);
    p->hidden_factor -= penalty;
    p->hidden_factor  = clampf(p->hidden_factor, HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX);
    p->consecutive_trolls++;
}

/* =========================================================
 * Hidden state
 * ========================================================= */

/*
 * update_hidden_state — classify player's mental state.
 *
 *   NEGATIVE: lose_streak >= 3  OR  factor <= 0.70
 *   POSITIVE: factor >= 0.95   AND  lose_streak == 0
 *   NEUTRAL:  all other cases
 *
 * Also refreshes tilt_level:
 *   2 (heavy) → NEGATIVE with lose_streak >= 3 or factor <= 0.70
 *   1 (light) → NEGATIVE but less severe
 *   0 (none)  → NEUTRAL or POSITIVE
 */
void update_hidden_state(Player *p) {
    if (p->lose_streak >= 3 || p->hidden_factor <= 0.70f) {
        p->tilt_level  = 2;
        p->hidden_state = STATE_NEGATIVE;
    } else if (p->lose_streak > 0 || p->hidden_factor < 0.90f) {
        p->tilt_level  = 1;
        p->hidden_state = STATE_NEGATIVE;
    } else if (p->hidden_factor >= 0.95f && p->lose_streak == 0) {
        p->tilt_level  = 0;
        p->hidden_state = STATE_POSITIVE;
    } else {
        p->tilt_level  = 0;
        p->hidden_state = STATE_NEUTRAL;
    }
}

/* =========================================================
 * Tilt factor update
 * ========================================================= */

/*
 * update_tilt — adjust hidden_factor and streaks after a game result.
 *
 * On WIN:
 *   - hidden_factor += FACTOR_WIN_BONUS (0.02)
 *   - win_streak++, lose_streak = 0
 *   - tilt_level decreases by 1 (minimum 0)
 *   - consecutive_trolls resets on clean win without trolling
 *
 * On LOSS:
 *   - hidden_factor -= FACTOR_LOSS_PENALTY (0.05)
 *   - lose_streak++, win_streak = 0
 *   - tilt_level: 1 on first loss, 2 at >= 3 consecutive losses
 *   - consecutive_trolls resets (anger resets arrogance cycle)
 */
void update_tilt(Player *p, int did_win) {
    if (did_win) {
        p->win_streak++;
        p->lose_streak = 0;
        
        float bonus = FACTOR_WIN_BONUS;
        if (p->win_streak > 3) {
            bonus *= 1.5f;  /* 1.5x multiplier on streak */
        }
        
        /* REFACTOR: Apply hidden_factor smoothing (inertia) instead of direct update.
           This prevents instant reactions and allows natural variance.
           Smooth: new_hf = 0.9 * old_hf + 0.1 * (old_hf + bonus)
        */
        float target_factor = p->hidden_factor + bonus;
        float weight = calculate_eomm_weight(p->total_games);
        if (weight > 0.0f) {
            /* After calibration: apply smoothing */
            p->hidden_factor = 0.9f * p->hidden_factor + 0.1f * target_factor;
        } else {
            /* During calibration: no hidden_factor changes */
            /* p->hidden_factor stays at 1.0 */
        }
        
        /* Reduce tilt on win */
        if (p->tilt_level > 0) p->tilt_level--;
        
        /* Reset consecutive trolls on clean win */
        if (!p->is_troll_pick) {
            p->consecutive_trolls = 0;
        }
    } else {
        p->lose_streak++;
        p->win_streak = 0;
        
        float penalty = FACTOR_LOSS_PENALTY;
        if (p->lose_streak > 3) {
            penalty *= 1.5f;  /* 1.5x multiplier on streak */
        }
        
        /* REFACTOR: Apply hidden_factor smoothing (inertia) instead of direct update */
        float target_factor = p->hidden_factor - penalty;
        float weight = calculate_eomm_weight(p->total_games);
        if (weight > 0.0f) {
            /* After calibration: apply smoothing */
            p->hidden_factor = 0.9f * p->hidden_factor + 0.1f * target_factor;
        } else {
            /* During calibration: no hidden_factor changes */
            /* p->hidden_factor stays at 1.0 */
        }
        
        /* Increase tilt on loss */
        if (p->lose_streak >= 3) {
            p->tilt_level = 2;
        } else if (p->lose_streak >= 1) {
            p->tilt_level = 1;
        }
        
        /* Anger resets arrogance: reset consecutive trolls */
        p->consecutive_trolls = 0;
    }
    
    p->hidden_factor = clampf(p->hidden_factor, HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX);
    update_hidden_state(p);
}
/* =========================================================
 * Soft reset
 * ========================================================= */

/*
 * apply_soft_reset — every SOFT_RESET_INTERVAL games, give the player
 * a mental fresh start: tilt_level → 0, hidden_factor → 1.0.
 */
void apply_soft_reset(Player *p) {
    if (p->total_games > 0 && (p->total_games % SOFT_RESET_INTERVAL) == 0) {
        p->tilt_level        = 0;
        p->hidden_factor     = HIDDEN_FACTOR_START;
        p->consecutive_trolls = 0;
        update_hidden_state(p);
    }
}

/* =========================================================
 * ELO CALCULATION & EOMM BIAS
 * ========================================================= */

/*
 * calculate_expected — standard Elo formula for expected win probability.
 *
 * Formula: expected_prob = 1 / (1 + 10^(-(mmr_a - mmr_b) / 400))
 *
 * Returns a probability in range [~0.01, ~0.99] based on MMR difference.
 */
float calculate_expected(float mmr_a, float mmr_b) {
    float diff = (mmr_a - mmr_b) / 400.0f;
    return 1.0f / (1.0f + powf(10.0f, -diff));
}

/*
 * apply_eomm_bias — compute effective opponent MMR with EOMM adjustment.
 *
 * REFACTORED for decoupling and calibration:
 * Purpose: Move hidden_factor influence from RATING (MMR) to MATCHMAKING.
 * The hidden_factor still affects engagement but via opponent difficulty,
 * not via inflating/deflating the player's own rating.
 *
 * Changes from original:
 *   - Reduced bias magnitude: 50 MMR → 10 MMR (drastic reduction)
 *   - Apply global EOMM weight (0.0-1.0 based on calibration phase)
 *   - Zero bias during calibration (first 50 games)
 *
 * Logic:
 *   NEGATIVE state (tilted): bias is NEGATIVE → opponent appears WEAKER
 *   POSITIVE state (hot): bias is POSITIVE → opponent appears STRONGER
 *   NEUTRAL state: bias is neutral
 */
float apply_eomm_bias(Player *p, float opponent_mmr) {
    /* During calibration, return unmodified opponent MMR */
    if (p->total_games < 50) {
        return opponent_mmr;
    }
    
    /* Deviation from baseline (1.0) */
    float hf_deviation = p->hidden_factor - 1.0f;
    
    /* REDUCED bias magnitude: 10 MMR instead of 50 */
    float bias_magnitude = 10.0f;  /* ±10 MMR range (was ±50) */
    float bias = 0.0f;
    
    if (p->hidden_state == STATE_NEGATIVE) {
        bias = -bias_magnitude * fabsf(hf_deviation);  /* negative bias → easier games */
    } else if (p->hidden_state == STATE_POSITIVE) {
        bias = +bias_magnitude * hf_deviation;         /* positive bias → harder games */
    }
    
    /* Apply global EOMM weight (scales from 0 to 1 after calibration) */
    float weight = calculate_eomm_weight(p->total_games);
    bias *= weight;
    
    return opponent_mmr + bias;
}

/* =========================================================
 * MMR update
 * ========================================================= */

/*
 * update_mmr — apply Elo-style K-factor delta.
 * During the placement phase (total_games < PLACEMENT_GAMES) K = 30.
 * After placement K = 25.
 */
/*
 * update_mmr — apply ELO-style rating update (PRO VERSION).
 *
 * Formula: delta = K * (outcome - expected_prob)
 *
 * This separates RATING (MMR) from ENGAGEMENT (hidden_factor):
 *   - MMR reflects pure skill (Elo standard)
 *   - hidden_factor affects matchmaking via apply_eomm_bias() only
 *   - No K-factor multiplication → stability + fairness
 */
void update_mmr(Player *p, float opponent_mmr, int did_win) {
    /* 1. Expected probability (Elo) */
    float expected = calculate_expected(p->visible_mmr, opponent_mmr);
    
    /* 2. Outcome (1 = win, 0 = loss) */
    float outcome = did_win ? 1.0f : 0.0f;
    
    /* 3. K-factor: simpler + cleaner (no hidden_factor modulation) */
    float K;
    if (p->total_games < PLACEMENT_GAMES)
        K = 35.0f;   /* placement: faster calibration */
    else
        K = 25.0f;   /* ranked: standard Elo */
    
    /* 4. Update MMR (FORMULA CLÉ: delta = K * (outcome - expected)) */
    float delta = K * (outcome - expected);
    p->visible_mmr += delta;
    
    /* 5. Clamp: safety */
    if (p->visible_mmr < 0.0f)
        p->visible_mmr = 0.0f;
}

/*
 * effective_mmr — the value used for team power calculation.
 *   effectiveMMR = visible_mmr * hidden_factor
 */
float effective_mmr(const Player *p) {
    /* ⚠️ ARCH-FIX: Pure skill-based MMR for matchmaking
       REMOVED: p->visible_mmr * p->hidden_factor
       NOW: p->visible_mmr ONLY
       
       This eliminates the implicit HF² in simulate_match where:
         power = (mmr * HF) * (perf_wr * hf_blend) was squaring HF
       
       EOMM intent (placing Smurfs→team_b) is NOW achieved via
       skill_level_adjustment in calculate_matchmaking_bias() instead.
    */
    return p->visible_mmr;
}

/* =========================================================
 * Performance-based win rate
 * ========================================================= */

/*
 * calculate_actual_winrate — derive a player's win probability from their
 * PerformanceStats rather than a pre-assigned fixed value.
 *
 * Algorithm:
 *   1. Compute the weighted average of all eight performance stats.
 *      (Equal weights; tilt_resistance is included in the average so that
 *      players with higher mental stability also have a higher baseline WR.)
 *   2. Map the average performance [0, 1] linearly to win rate [25%, 75%]:
 *        win_rate = 0.25 + avg_perf * 0.50
 *      This centres normal players (avg ~0.50) at exactly 50% WR while
 *      smurfs (avg ~0.80) land near 65% and hardstuck (avg ~0.225) near 36%.
 *   3. Apply a tilt penalty when the player is in STATE_NEGATIVE.
 *      The penalty scales with low tilt_resistance so emotionally fragile
 *      players suffer more when on a losing streak.
 *   4. Clamp the final value to [0.25, 0.75].
 *
 * The actual observed win rate in the simulation will differ from this
 * per-game value because EOMM matchmaking adjusts opponent difficulty as
 * each player's MMR evolves.
 */
float calculate_actual_winrate(const Player *p) {
    /* Equal-weight average of all eight performance stats */
    float avg = (p->perf.mechanical_skill
               + p->perf.decision_making
               + p->perf.map_awareness
               + p->perf.tilt_resistance
               + p->perf.champion_pool_depth
               + p->perf.champion_proficiency
               + p->perf.wave_management
               + p->perf.teamfight_positioning) / 8.0f;

    /* Map [0, 1] performance to [0.25, 0.75] win rate */
    float wr = 0.25f + avg * 0.50f;

    if (p->hidden_state == STATE_NEGATIVE) {
        float fragility = 1.0f - p->perf.tilt_resistance;
        float penalty = fragility * 0.05f; /* base penalty */
        
        /* Hardstuck players suffer more when tilted */
        if (p->skill_level == SKILL_HARDSTUCK) {
            penalty *= 2.0f;  /* 2x penalty for hardstuck */
        }
        wr -= penalty;
    }

    wr = clampf(wr, 0.25f, 0.75f);

    /* ⚠️ ARCH-FIX: Remove hidden_factor from match result determination
       REMOVED: wr *= (0.5f + 0.5f * p->hidden_factor)  [hf_blend]
       
       Hidden_factor NO LONGER affects match outcomes directly.
       Match win probability now depends ONLY on:
       - Skill/performance (perf_stats)
       - Tilt state (penalty if STATE_NEGATIVE)
       - Team average MMR diff (via power calculation in simulate_match)
       
       HF now affects ONLY K-factor progression in update_mmr(),
       controlling how fast skill improvements/regressions happen,
       not WHETHER they happen.
    */

    return clampf(wr, 0.25f, 0.75f);
}
/* =========================================================
 * Engagement phase orchestration
 * ========================================================= */

/*
 * update_engagement_phase — orchestrate hot/cold streak phases.
 *
 * When a player has completed their target_streak:
 *   70% chance → extend the phase a little longer
 *   30% chance → transition to PHASE_NEUTRAL
 *
 * When PHASE_NEUTRAL:
 *   20% → enter WIN_STREAK  (target 3-7 games)
 *   20% → enter LOSE_STREAK (target 3-7 games)
 *   60% → stay NEUTRAL
 */
void update_engagement_phase(Player *p) {
    /* Check if current phase has reached its target */
    if (p->engagement_phase != PHASE_NEUTRAL &&
        p->phase_progress >= p->target_streak) {
        if (randf() < 0.70f) {
            /* Soft extension: stay in phase a bit longer */
            p->target_streak += 1 + (rand() % 3);
        } else {
            /* Transition to neutral */
            p->engagement_phase = PHASE_NEUTRAL;
            p->phase_progress   = 0;
            p->target_streak    = 0;
        }
    }

    /* Assign a new phase if currently neutral */
    if (p->engagement_phase == PHASE_NEUTRAL) {
        float r = randf() * 100.0f;
        if (r < 20.0f) {
            p->engagement_phase = PHASE_WIN_STREAK;
            p->target_streak    = 3 + (rand() % 5); /* 3-7 games */
            p->phase_progress   = 0;
        } else if (r < 40.0f) {
            p->engagement_phase = PHASE_LOSE_STREAK;
            p->target_streak    = 3 + (rand() % 5); /* 3-7 games */
            p->phase_progress   = 0;
        }
        /* else: stay NEUTRAL (60% chance) */
    }
}

/*
 * apply_engagement_phase_modifiers — apply soft hidden_factor and
 * troll/autofill probability adjustments driven by the engagement phase.
 *
 * REFACTORED:
 * WIN_STREAK  : +2% hidden_factor (reduced from ±5% for softer effect)
 * LOSE_STREAK : -2% hidden_factor (reduced from ±5%)
 *
 * These modifiers are scaled down and further reduced via global EOMM weight.
 * During calibration phase, engagement effects are disabled.
 *
 * Troll and autofill probabilities are modulated inside the EOMM
 * matchmaker via calculate_matchmaking_bias(); this function only
 * touches the hidden_factor.
 */
void apply_engagement_phase_modifiers(Player *p) {
    /* During calibration phase, don't apply engagement modifiers */
    if (p->total_games < 50) {
        return;
    }
    
    /* Apply global EOMM weight to scale engagement effects */
    float weight = calculate_eomm_weight(p->total_games);
    if (weight <= 0.0f) return;  /* Skip if weight is zero */
    
    if (p->engagement_phase == PHASE_WIN_STREAK) {
        /* Reduced effect: 1.02 instead of 1.05, further scaled by weight */
        float factor = 1.0f + (0.02f * weight);  /* 2% base, scaled by weight */
        p->hidden_factor *= factor;
        p->hidden_factor  = clampf(p->hidden_factor,
                                   HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX);
    } else if (p->engagement_phase == PHASE_LOSE_STREAK) {
        /* Reduced effect: 0.98 instead of 0.95, further scaled by weight */
        float factor = 1.0f - (0.02f * weight);  /* 2% base, scaled by weight */
        p->hidden_factor *= factor;
        p->hidden_factor  = clampf(p->hidden_factor,
                                   HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX);
    }
}

/*
 * calculate_matchmaking_bias — return a soft bias score for sorting
 * players into teams.  A higher value means the player is preferred
 * for the losing side (team_a); a lower value for the winning side.
 *
 * Base score = MMR distance from pool average.
 * LOSE_STREAK players get a +50 adjustment (favour losing side / team_a).
 * WIN_STREAK  players get a -50 adjustment (favour winning side / team_b).
 */
static float calculate_matchmaking_bias(const Player *p, float mmr_distance) {
    float bias = mmr_distance;

    if (p->engagement_phase == PHASE_WIN_STREAK) {
        bias -= 50.0f;  /* favour team_b (winning side) */
    } else if (p->engagement_phase == PHASE_LOSE_STREAK) {
        bias += 50.0f;  /* favour team_a (losing side) */
    }
    
    /* ⚠️ ARCH-FIX: Skill-level-based selection (replaces HF variable selection)
       Instead of using hidden_factor (variable psychology) in effective_mmr,
       we NOW use skill_level (stable characteristic) directly.
       This preserves EOMM intent without variable HF dependency.
    */
    if (p->skill_level == SKILL_SMURF) {
        bias -= 50.0f;  /* HIGH bias → favours team_b (winning side) */
    } else if (p->skill_level == SKILL_HARDSTUCK) {
        bias += 50.0f;  /* LOW bias → favours team_a (losing side) */
    }
    /* SKILL_NORMAL: no adjustment, neutral */

    return bias;
}

/* =========================================================
 * Troll pick determination
 * ========================================================= */

/*
 * determine_troll_picks — roll troll probability for every player in a match.
 * Applies troll penalty immediately (before MMR update) even on a win.
 */
void determine_troll_picks(Match *m) {
    for (int i = 0; i < TEAM_SIZE; i++) {
        Player *pa = m->team_a[i];
        Player *pb = m->team_b[i];

        float prob_a = calculate_troll_probability(pa);
        float prob_b = calculate_troll_probability(pb);

        pa->is_troll_pick = (randf() * 100.0f < prob_a) ? 1 : 0;
        pb->is_troll_pick = (randf() * 100.0f < prob_b) ? 1 : 0;

        if (pa->is_troll_pick) apply_troll_penalty(pa);
        if (pb->is_troll_pick) apply_troll_penalty(pb);
    }
}

/* =========================================================
 * Match simulation
 * ========================================================= */

/*
 * simulate_match — determine match winner based on team power.
 *
 * Team power = sum of (effective_mmr * calculate_actual_winrate) for each player.
 * Win rate is computed dynamically from each player's PerformanceStats so that
 * skill differences emerge from behaviour rather than pre-assigned constants.
 * Win probability for team_a = power_a / (power_a + power_b).
 * Result is stochastic (random roll against win probability).
 *
 * Compensation boost: if any team_a player has >= COMPENSATION_THRESHOLD
 * consecutive losses the highest applicable bonus is applied to win_prob_a
 * and clamped to [0.05, 0.95].  This is transparent to the player.
 */
/*
 * simulate_match — determine match winner based on skill (MMR + perf).
 *
 * ARCHITECTURE:
 *   1. Power = sum of (effective_mmr * calculate_actual_winrate) for each player
 *      → This is PURE skill-based (no HF multiplicative)
 *   2. Win probability = power_a / power_b (Elo-based ratio)
 *   3. Add compensation bonus for losing streak
 *   4. Apply HF as SIMPLE local variance (±3% max)
 *      → HF affects HOW each player performs, not WHETHER team wins
 *      → Additive noise, not multiplicative global effect
 *   5. Stochastic roll
 */
int simulate_match(Match *m) {
    /* ═══════════════════════════════════════════════════════════
       SAME PROBABILISTIC SPACE: ratio(pow(mmr,1+k) * perf)
       
       Amplifies MMR sensitivity via EXPONENTIATION (not new layer)
       → Keeps single reference frame
       → Progressive tuning of skill dominance
       ═══════════════════════════════════════════════════════════ */
    
    float power_a = 0.0f, power_b = 0.0f;
    
    /* k_mmr_boost = 0.12: amplifies MMR sensitivity
       Normal MMR (1000): 1000^1.12 ≈ 1127 (mild boost)
       MMR diff 400: (1200/800)^1.12 ≈ 1.52x power advantage
       → Structural amplification without changing espace probabiliste */
    float k_mmr_boost = 0.12f;
    
    for (int i = 0; i < TEAM_SIZE; i++) {
        float mmr_a = effective_mmr(m->team_a[i]);
        float mmr_b = effective_mmr(m->team_b[i]);
        float perf_a = calculate_actual_winrate(m->team_a[i]);
        float perf_b = calculate_actual_winrate(m->team_b[i]);
        
        /* Power: MMR exponentiation + perf (same ratio metric) */
        power_a += powf(mmr_a, 1.0f + k_mmr_boost) * perf_a;
        power_b += powf(mmr_b, 1.0f + k_mmr_boost) * perf_b;
    }
    
    float total = power_a + power_b;
    float win_prob_a = (total > 0.0f) ? (power_a / total) : 0.5f;
    /* → win_prob_a maintains single metric space */

    /* STEP 2: Apply compensation bonus (with EOMM weight scaling) */
    float max_bonus = 0.0f;
    for (int i = 0; i < TEAM_SIZE; i++) {
        float bonus = get_compensation_bonus(m->team_a[i]->lose_streak, m->team_a[i]->total_games, m->team_a[i]);
        if (bonus > max_bonus) max_bonus = bonus;
    }
    if (max_bonus > 0.0f) {
        win_prob_a = clampf(win_prob_a * (1.0f + max_bonus), 0.05f, 0.95f);
    }

    /* STEP 3: Hidden_factor as LOCAL noise (unchanged) */
    float hf_variance = 0.0f;
    for (int i = 0; i < TEAM_SIZE; i++) {
        /* Each player's HF deviates from neutral (1.0) */
        hf_variance += (m->team_a[i]->hidden_factor - 1.0f);
        hf_variance -= (m->team_b[i]->hidden_factor - 1.0f);
    }
    
    /* Average per team */
    hf_variance /= (float)TEAM_SIZE;
    
    /* Apply as local variance modifier (max ±1% swing) */
    float hf_effect = hf_variance * 0.05f;  /* Maps [-0.2,+0.2] to [-1%, +1%] */
    win_prob_a += hf_effect;
    win_prob_a = clampf(win_prob_a, 0.05f, 0.95f);

    /* STEP 4: Final stochastic roll */
    m->winner = (randf() < win_prob_a) ? 0 : 1;
    return m->winner;
}

/* =========================================================
 * Post-match player updates
 * ========================================================= */

/*
 * update_players_after_match — apply win/loss bookkeeping, MMR change,
 * tilt update and soft reset for every player in the match.
 */
void update_players_after_match(Match *m) {
    int winner = m->winner; /* 0=team_a, 1=team_b */

    for (int i = 0; i < TEAM_SIZE; i++) {
        Player *pa = m->team_a[i];
        Player *pb = m->team_b[i];

        int a_won = (winner == 0);
        int b_won = (winner == 1);

        /* Counters */
        pa->total_games++;
        pb->total_games++;

        if (a_won) { pa->wins++;   pb->losses++; }
        else       { pa->losses++; pb->wins++;   }

        /* MMR (now with opponent MMR for Elo calculation) */
        update_mmr(pa, pb->visible_mmr, a_won);
        update_mmr(pb, pa->visible_mmr, b_won);

        /* Tilt / hidden factor */
        update_tilt(pa, a_won);
        update_tilt(pb, b_won);

        /* Autofill post-match penalty */
        if (pa->is_autofilled) {
            float penalty = a_won ? AUTOFILL_POST_WIN_PENALTY : AUTOFILL_POST_LOSS_PENALTY;
            pa->hidden_factor -= penalty;
            pa->hidden_factor  = clampf(pa->hidden_factor, HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX);
        }
        if (pb->is_autofilled) {
            float penalty = b_won ? AUTOFILL_POST_WIN_PENALTY : AUTOFILL_POST_LOSS_PENALTY;
            pb->hidden_factor -= penalty;
            pb->hidden_factor  = clampf(pb->hidden_factor, HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX);
        }

        /* Soft reset check */
        apply_soft_reset(pa);
        apply_soft_reset(pb);

        /* Engagement phase progression */
        /* --- team_a player (pa) --- */
        if (a_won) {
            if (pa->engagement_phase == PHASE_WIN_STREAK) {
                pa->phase_progress++;
            } else if (pa->engagement_phase == PHASE_LOSE_STREAK) {
                /* Premature exit from cold streak on a win (50% chance) */
                if (randf() < 0.5f) {
                    pa->engagement_phase = PHASE_NEUTRAL;
                    pa->phase_progress   = 0;
                }
            }
        } else {
            if (pa->engagement_phase == PHASE_LOSE_STREAK) {
                pa->phase_progress++;
            } else if (pa->engagement_phase == PHASE_WIN_STREAK) {
                /* Premature exit from hot streak on a loss (50% chance) */
                if (randf() < 0.5f) {
                    pa->engagement_phase = PHASE_NEUTRAL;
                    pa->phase_progress   = 0;
                }
            }
        }

        /* --- team_b player (pb) --- */
        if (b_won) {
            if (pb->engagement_phase == PHASE_WIN_STREAK) {
                pb->phase_progress++;
            } else if (pb->engagement_phase == PHASE_LOSE_STREAK) {
                if (randf() < 0.5f) {
                    pb->engagement_phase = PHASE_NEUTRAL;
                    pb->phase_progress   = 0;
                }
            }
        } else {
            if (pb->engagement_phase == PHASE_LOSE_STREAK) {
                pb->phase_progress++;
            } else if (pb->engagement_phase == PHASE_WIN_STREAK) {
                if (randf() < 0.5f) {
                    pb->engagement_phase = PHASE_NEUTRAL;
                    pb->phase_progress   = 0;
                }
            }
        }
    }
}

/* =========================================================
 * Matchmaking
 * ========================================================= */

/* =========================================================
 * Autofill system
 * ========================================================= */

/*
 * get_base_autofill_risk — base autofill probability (%) for a given role.
 *
 * Higher values for contested roles (ADC, Top), lower for protected or
 * less-desired roles (Support, Jungle).
 */
float get_base_autofill_risk(int role) {
    switch (role) {
        case ROLE_SUPPORT: return AUTOFILL_RISK_SUPPORT;
        case ROLE_JUNGLE:  return AUTOFILL_RISK_JUNGLE;
        case ROLE_MID:     return AUTOFILL_RISK_MID;
        case ROLE_TOP:     return AUTOFILL_RISK_TOP;
        case ROLE_ADC:     return AUTOFILL_RISK_ADC;
        default:           return AUTOFILL_RISK_TOP;
    }
}

/*
 * calculate_autofill_chance — compute effective autofill probability (%).
 *
 * Adds AUTOFILL_EOMM_BONUS when the player is in a NEGATIVE hidden state
 * (losing streak) to drive the EOMM engagement loop.
 */
float calculate_autofill_chance(const Player *p, int role) {
    float chance = get_base_autofill_risk(role);
    if (p->hidden_state == STATE_NEGATIVE) {
        chance += AUTOFILL_EOMM_BONUS;
    }
    return chance;
}

/*
 * should_autofill — dice-roll against the autofill chance.
 * Returns 1 if the player should be autofilled this game, 0 otherwise.
 */
int should_autofill(const Player *p, int role) {
    float chance = calculate_autofill_chance(p, role);
    return (randf() * 100.0f < chance) ? 1 : 0;
}

/*
 * assign_autofill_role — force the player into a role outside their two
 * preferred roles (prefRoles[0] and prefRoles[1]).
 *
 * Immediately applies:
 *   - current_role set to a non-preferred role
 *   - is_autofilled = 1
 *   - tilt_level = AUTOFILL_TILT_LEVEL (2)
 *   - hidden_factor -= AUTOFILL_FACTOR_PENALTY (0.05)
 */
void assign_autofill_role(Player *p) {
    int candidates[ROLE_COUNT];
    int count = 0;
    for (int r = 0; r < ROLE_COUNT; r++) {
        if (r != p->prefRoles[0] && r != p->prefRoles[1]) {
            candidates[count++] = r;
        }
    }
    if (count > 0) {
        p->current_role = candidates[rand() % count];
    } else {
        /* Unreachable with ROLE_COUNT=5 and 2 distinct prefRoles,
         * but kept as a defensive fallback if ROLE_COUNT is ever reduced. */
        p->current_role = (p->prefRoles[0] + 1) % ROLE_COUNT;
    }
    p->is_autofilled  = 1;
    p->tilt_level     = AUTOFILL_TILT_LEVEL;
    p->hidden_factor -= AUTOFILL_FACTOR_PENALTY;
    p->hidden_factor  = clampf(p->hidden_factor, HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX);
    /* Autofill angers the player: reduce tilt_resistance so they tilt more easily */
    p->perf.tilt_resistance -= AUTOFILL_TILT_PENALTY;
    p->perf.tilt_resistance  = clampf(p->perf.tilt_resistance, 0.0f, 1.0f);
}

/*
 * create_matches_random — game 0: fully random teams.
 */
static void create_matches_random(Player *players, int n,
                                  Match *matches, int *num_matches) {
    shuffle_players(players, n);
    int nm = n / MATCH_SIZE;
    *num_matches = nm;
    for (int m = 0; m < nm; m++) {
        int base = m * MATCH_SIZE;
        for (int i = 0; i < TEAM_SIZE; i++) {
            matches[m].team_a[i] = &players[base + i];
            matches[m].team_b[i] = &players[base + TEAM_SIZE + i];
        }
        matches[m].winner = -1;
    }
}

/*
 * create_matches_eomm — EOMM matchmaking for games after the first.
 *
 * Strategy per match:
 *   team_a (designed to lose): fill from NEGATIVE (tilt) pool
 *   team_b (designed to win) : fill from POSITIVE/NEUTRAL (healthy) pool
 *   smurfs: placed 70% in team_b, 30% in team_a
 *   hardstuck: weighted towards team_a
 *   remaining slots: filled from any unassigned players
 *   MMR balance: fallback if visible spread > MMR_BALANCE_TOLERANCE
 */
static void create_matches_eomm(Player *players, int n,
                                 Match *matches, int *num_matches) {
    int nm = n / MATCH_SIZE;
    *num_matches = nm;

    /* Refresh hidden state for all players and reset per-game autofill flags */
    for (int i = 0; i < n; i++) {
        update_hidden_state(&players[i]);
        players[i].is_autofilled = 0;
        players[i].current_role  = players[i].prefRoles[0];
    }

    int *assigned = (int *)calloc(n, sizeof(int));
    if (!assigned) return;

    /* Working pools — sized generously */
    Player **tilt_pool    = (Player **)malloc(sizeof(Player *) * n);
    Player **healthy_pool = (Player **)malloc(sizeof(Player *) * n);
    Player **any_pool     = (Player **)malloc(sizeof(Player *) * n);

    if (!tilt_pool || !healthy_pool || !any_pool) {
        free(tilt_pool); free(healthy_pool); free(any_pool);
        free(assigned);
        return;
    }

    for (int m = 0; m < nm; m++) {
        Match *match = &matches[m];
        match->winner = -1;
        for (int i = 0; i < TEAM_SIZE; i++) {
            match->team_a[i] = NULL;
            match->team_b[i] = NULL;
        }

        /* Categorise unassigned players */
        int tilt_n = 0, healthy_n = 0;
        for (int i = 0; i < n; i++) {
            if (assigned[i]) continue;
            Player *p = &players[i];
            if (p->hidden_state == STATE_NEGATIVE) {
                tilt_pool[tilt_n++] = p;
            } else {
                healthy_pool[healthy_n++] = p;
            }
        }

        shuffle_ptrs(tilt_pool,    tilt_n);
        shuffle_ptrs(healthy_pool, healthy_n);

        int a_count = 0, b_count = 0;

        /* 🚀 SIMPLIFIED MATCHMAKING (PHASE 0 REMOVED)
         * 
         * Instead of skill-aware team composition, use:
         *   1. Tilt-based assignment (negative → team_a)
         *   2. Engagement phase bias (from any_pool)
         *   3. MMR balance as fallback
         *
         * This allows natural skill expression early, while EOMM weight
         * (0 until game 50) ensures no artificial suppression.
         */

        /* --- Fill team_a with tilt players (max TEAM_SIZE) --- */
        for (int t = 0; t < tilt_n && a_count < TEAM_SIZE; t++) {
            match->team_a[a_count++] = tilt_pool[t];
            assigned[tilt_pool[t] - players] = 1;
        }

        /* --- Fill team_b with healthy players (max TEAM_SIZE) --- */
        for (int h = 0; h < healthy_n && b_count < TEAM_SIZE; h++) {
            match->team_b[b_count++] = healthy_pool[h];
            assigned[healthy_pool[h] - players] = 1;
        }

        /* --- Fill remaining slots from any unassigned player --- */
        int any_n = 0;
        float avg_mmr = 0.0f;
        for (int i = 0; i < n; i++) {
            if (!assigned[i]) {
                any_pool[any_n++] = &players[i];
                avg_mmr += players[i].visible_mmr;
            }
        }
        if (any_n > 0) avg_mmr /= (float)any_n;
        shuffle_ptrs(any_pool, any_n);

        /* First pass: use engagement-phase bias to steer LOSE_STREAK →
         * team_a and WIN_STREAK → team_b before falling back to balance.
         * Higher bias (LOSE_STREAK) → team_a (losing side).
         * Lower/negative bias (WIN_STREAK) → team_b (winning side). */
        for (int ai = 0; ai < any_n; ai++) {
            Player *p = any_pool[ai];
            if (assigned[p - players]) continue;
            float bias = calculate_matchmaking_bias(p,
                             p->visible_mmr - avg_mmr);
            /* Positive/high bias → prefer team_a (losing side) */
            if (bias > 0.0f && a_count < TEAM_SIZE) {
                match->team_a[a_count++] = p;
                assigned[p - players] = 1;
            } else if (bias <= 0.0f && b_count < TEAM_SIZE) {
                match->team_b[b_count++] = p;
                assigned[p - players] = 1;
            }
        }
        /* Second pass: fill any still-empty slots by balance */
        for (int ai = 0; ai < any_n; ai++) {
            if (a_count >= TEAM_SIZE && b_count >= TEAM_SIZE) break;
            Player *p = any_pool[ai];
            if (assigned[p - players]) continue;
            if (a_count <= b_count && a_count < TEAM_SIZE) {
                match->team_a[a_count++] = p;
            } else if (b_count < TEAM_SIZE) {
                match->team_b[b_count++] = p;
            } else if (a_count < TEAM_SIZE) {
                match->team_a[a_count++] = p;
            }
            assigned[p - players] = 1;
        }

        /* Autofill role assignment: for each player in this match, check
         * whether the system should force them off their preferred role. */
        for (int i = 0; i < TEAM_SIZE; i++) {
            if (match->team_a[i] && should_autofill(match->team_a[i],
                                                     match->team_a[i]->prefRoles[0])) {
                assign_autofill_role(match->team_a[i]);
            }
            if (match->team_b[i] && should_autofill(match->team_b[i],
                                                     match->team_b[i]->prefRoles[0])) {
                assign_autofill_role(match->team_b[i]);
            }
        }

        /* Log MMR imbalance warning */
        float min_mmr = 1e9f, max_mmr = -1.0f;
        for (int i = 0; i < TEAM_SIZE; i++) {
            if (match->team_a[i]) {
                float v = match->team_a[i]->visible_mmr;
                if (v < min_mmr) min_mmr = v;
                if (v > max_mmr) max_mmr = v;
            }
            if (match->team_b[i]) {
                float v = match->team_b[i]->visible_mmr;
                if (v < min_mmr) min_mmr = v;
                if (v > max_mmr) max_mmr = v;
            }
        }
        if (min_mmr <= max_mmr && (max_mmr - min_mmr) > MMR_BALANCE_TOLERANCE) {
            fprintf(stderr,
                "[EOMM] Match %d: MMR spread %.0f > %.0f (may appear unbalanced)\n",
                m, max_mmr - min_mmr, MMR_BALANCE_TOLERANCE);
        }
    }

    free(tilt_pool);
    free(healthy_pool);
    free(any_pool);
    free(assigned);
}

void create_matches(Player *players, int n, Match *matches,
                    int *num_matches, int game_number) {
    if (game_number == 0) {
        /* Game 0: fully random */
        create_matches_random(players, n, matches, num_matches);
    } else if (game_number < 50) {
        /* CALIBRATION PHASE (games 1-49): pure random/MMR-based
         * (no tilt-state manipulation, no engagement phase steering)
         * Use random matchmaking to allow natural skill expression
         */
        create_matches_random(players, n, matches, num_matches);
    } else {
        /* POST-CALIBRATION (game 50+): full EOMM with weight ramping */
        create_matches_eomm(players, n, matches, num_matches);
    }
}

/* =========================================================
 * Analytics
 * ========================================================= */

void compute_stats(const Player *players, int n, SkillStats stats[3]) {
    /* Initialise */
    for (int s = 0; s < 3; s++) {
        stats[s].skill_level      = (SkillLevel)s;
        stats[s].player_count     = 0;
        stats[s].total_wins       = 0;
        stats[s].total_games      = 0;
        stats[s].avg_mmr          = 0.0f;
        stats[s].avg_hidden_factor = 0.0f;
        stats[s].troll_count      = 0;
        stats[s].tilt2_count      = 0;
    }

    for (int i = 0; i < n; i++) {
        const Player *p = &players[i];
        int s = (int)p->skill_level;
        stats[s].player_count++;
        stats[s].total_wins       += p->wins;
        stats[s].total_games      += p->total_games;
        stats[s].avg_mmr          += p->visible_mmr;
        stats[s].avg_hidden_factor += p->hidden_factor;
        if (p->is_troll_pick)     stats[s].troll_count++;
        if (p->tilt_level == 2)   stats[s].tilt2_count++;
    }

    for (int s = 0; s < 3; s++) {
        if (stats[s].player_count > 0) {
            stats[s].avg_mmr           /= (float)stats[s].player_count;
            stats[s].avg_hidden_factor /= (float)stats[s].player_count;
        }
    }
}

static const char *skill_name(SkillLevel s) {
    switch (s) {
        case SKILL_SMURF:     return "Smurf    ";
        case SKILL_HARDSTUCK: return "Hardstuck";
        default:              return "Normal   ";
    }
}

void print_stats(const SkillStats stats[3]) {
    printf("%-12s %6s %6s %8s %8s %8s %7s %7s\n",
           "Type", "Count", "Games", "Wins", "WR%", "AvgMMR",
           "Troll", "Tilt2");
    printf("---------------------------------------------------------------\n");
    for (int s = 0; s < 3; s++) {
        const SkillStats *st = &stats[s];
        float wr = (st->total_games > 0)
                   ? (100.0f * (float)st->total_wins / (float)st->total_games)
                   : 0.0f;
        printf("%-12s %6d %6d %8d %7.1f%% %8.0f %7d %7d\n",
               skill_name(st->skill_level),
               st->player_count, st->total_games, st->total_wins, wr,
               st->avg_mmr, st->troll_count, st->tilt2_count);
    }
}

void print_final_report(const Player *players, int n, int total_games) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║          EOMM SIMULATION — FINAL REPORT                  ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("  Players: %d  |  Games simulated: %d\n\n", n, total_games);

    SkillStats stats[3];
    compute_stats(players, n, stats);
    print_stats(stats);

    /* MMR extremes */
    float min_mmr = players[0].visible_mmr, max_mmr = players[0].visible_mmr;
    float sum_mmr = 0.0f;
    for (int i = 0; i < n; i++) {
        float v = players[i].visible_mmr;
        sum_mmr += v;
        if (v < min_mmr) min_mmr = v;
        if (v > max_mmr) max_mmr = v;
    }
    printf("\n  MMR spread  — min: %.0f  avg: %.0f  max: %.0f\n",
           min_mmr, sum_mmr / (float)n, max_mmr);

    /* Troll and tilt summary */
    int total_troll = 0, total_tilt2 = 0;
    for (int i = 0; i < n; i++) {
        if (players[i].is_troll_pick) total_troll++;
        if (players[i].tilt_level == 2) total_tilt2++;
    }
    printf("  Troll picks — players currently trolling: %d / %d (%.1f%%)\n",
           total_troll, n, 100.0f * (float)total_troll / (float)n);
    printf("  Heavy tilt  — players at tilt level 2   : %d / %d (%.1f%%)\n",
           total_tilt2, n, 100.0f * (float)total_tilt2 / (float)n);

    /* Top 5 MMR gainers */
    printf("\n  Top 5 MMR gainers (final visible MMR):\n");
    /* Simple selection sort for top 5 — n is small */
    int shown_idx[5] = {-1, -1, -1, -1, -1};
    for (int k = 0; k < 5 && k < n; k++) {
        float best = -1.0f;
        int   best_i = -1;
        for (int i = 0; i < n; i++) {
            int already = 0;
            for (int j = 0; j < k; j++) if (shown_idx[j] == i) already = 1;
            if (!already && players[i].visible_mmr > best) {
                best   = players[i].visible_mmr;
                best_i = i;
            }
        }
        if (best_i >= 0) {
            shown_idx[k] = best_i;
            const Player *p = &players[best_i];
            float wr = (p->total_games > 0)
                       ? (100.0f * (float)p->wins / (float)p->total_games) : 0.0f;
            printf("    %s  MMR=%.0f  WR=%.1f%%  Skill=%s\n",
                   p->name, p->visible_mmr, wr, skill_name(p->skill_level));
        }
    }
    printf("\n");
}

/* =========================================================
 * Inflation control (harmless MMR recalibration)
 * ========================================================= */

void apply_inflation_control(Player *players, int n) {
    /* Gentle recenter: shift all MMR back toward START_MMR by 0.5% each cycle */
    float drift = 0.005f;  /* 0.5% drift per inflation control cycle */
    for (int i = 0; i < n; i++) {
        players[i].visible_mmr = players[i].visible_mmr * (1.0f - drift) 
                                 + START_MMR * drift;
    }
}
