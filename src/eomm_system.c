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
 * get_compensation_bonus — returns the win-probability bonus for a player
 * who has suffered COMPENSATION_THRESHOLD or more consecutive losses.
 *
 * Gradual escalation:
 *   7  losses → +15%
 *   8  losses → +25%
 *   9  losses → +35%
 *  10+ losses → +50% (capped at COMPENSATION_MAX_BONUS)
 */
static inline float get_compensation_bonus(int lose_streak) {
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
     *   HARDSTUCK : [0.10, 0.35]
     */
    float lo, hi, adj_delta;
    switch (skill) {
        case SKILL_SMURF:
            lo = 0.70f; hi = 0.90f;
            adj_delta = -0.10f; /* one weaker stat */
            break;
        case SKILL_HARDSTUCK:
            lo = 0.10f; hi = 0.35f;
            adj_delta = +0.10f; /* one stronger stat */
            break;
        default: /* SKILL_NORMAL */
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
        p->hidden_factor += FACTOR_WIN_BONUS;
        p->hidden_factor  = clampf(p->hidden_factor, HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX);
        if (p->tilt_level > 0) p->tilt_level--;
        if (!p->is_troll_pick) {
            /* Clean win: reset consecutive troll counter */
            p->consecutive_trolls = 0;
        }
    } else {
        p->lose_streak++;
        p->win_streak = 0;
        p->hidden_factor -= FACTOR_LOSS_PENALTY;
        p->hidden_factor  = clampf(p->hidden_factor, HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX);
        /* Set tilt level from lose streak */
        if (p->lose_streak >= 3) {
            p->tilt_level = 2;
        } else {
            p->tilt_level = 1;
        }
        p->consecutive_trolls = 0;
    }

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
 * MMR update
 * ========================================================= */

/*
 * update_mmr — apply Elo-style K-factor delta.
 * During the placement phase (total_games < PLACEMENT_GAMES) K = 30.
 * After placement K = 25.
 */
void update_mmr(Player *p, int did_win) {
    float K = (p->total_games < PLACEMENT_GAMES) ? K_FACTOR_PLACEMENT : K_FACTOR_RANKED;
    if (did_win) p->visible_mmr += K;
    else         p->visible_mmr -= K;
    if (p->visible_mmr < 0.0f) p->visible_mmr = 0.0f;
}

/*
 * effective_mmr — the value used for team power calculation.
 *   effectiveMMR = visible_mmr * hidden_factor
 */
float effective_mmr(const Player *p) {
    return p->visible_mmr * p->hidden_factor;
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

    /* Tilt penalty: emotionally fragile players drop further when negative */
    if (p->hidden_state == STATE_NEGATIVE) {
        float fragility = 1.0f - p->perf.tilt_resistance;
        wr -= fragility * 0.05f; /* up to -5% for zero tilt_resistance */
    }

    return clampf(wr, 0.25f, 0.75f);
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
int simulate_match(Match *m) {
    float power_a = 0.0f, power_b = 0.0f;

    for (int i = 0; i < TEAM_SIZE; i++) {
        power_a += effective_mmr(m->team_a[i]) * calculate_actual_winrate(m->team_a[i]);
        power_b += effective_mmr(m->team_b[i]) * calculate_actual_winrate(m->team_b[i]);
    }

    float total = power_a + power_b;
    float win_prob_a = (total > 0.0f) ? (power_a / total) : 0.5f;

    /* Apply compensation bonus for team_a players on a long losing streak.
     * Only team_a is checked because the EOMM matchmaker deliberately places
     * tilted/losing players on team_a; team_b is the "stronger" side by
     * design, so compensating team_b would cancel the EOMM intent. */
    float max_bonus = 0.0f;
    for (int i = 0; i < TEAM_SIZE; i++) {
        float bonus = get_compensation_bonus(m->team_a[i]->lose_streak);
        if (bonus > max_bonus) max_bonus = bonus;
    }
    if (max_bonus > 0.0f) {
        win_prob_a = clampf(win_prob_a * (1.0f + max_bonus), 0.05f, 0.95f);
    }

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

        /* MMR */
        update_mmr(pa, a_won);
        update_mmr(pb, b_won);

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

        /* --- Fill team_a (losing side) with tilt players (max TEAM_SIZE) --- */
        for (int t = 0; t < tilt_n && a_count < TEAM_SIZE; t++) {
            /* Hardstuck players are preferred for the losing slot */
            if (tilt_pool[t]->skill_level == SKILL_HARDSTUCK ||
                tilt_pool[t]->skill_level == SKILL_NORMAL) {
                match->team_a[a_count++] = tilt_pool[t];
                assigned[tilt_pool[t] - players] = 1;
            }
        }
        /* Fill remaining team_a slots from tilt pool (any type) */
        for (int t = 0; t < tilt_n && a_count < TEAM_SIZE; t++) {
            if (!assigned[tilt_pool[t] - players]) {
                match->team_a[a_count++] = tilt_pool[t];
                assigned[tilt_pool[t] - players] = 1;
            }
        }

        /* --- Fill team_b (winning side) with healthy players (max TEAM_SIZE) --- */
        for (int h = 0; h < healthy_n && b_count < TEAM_SIZE; h++) {
            match->team_b[b_count++] = healthy_pool[h];
            assigned[healthy_pool[h] - players] = 1;
        }

        /* --- Fill remaining slots from any unassigned player --- */
        int any_n = 0;
        for (int i = 0; i < n; i++) {
            if (!assigned[i]) any_pool[any_n++] = &players[i];
        }
        shuffle_ptrs(any_pool, any_n);

        int ai = 0;
        while ((a_count < TEAM_SIZE || b_count < TEAM_SIZE) && ai < any_n) {
            Player *p = any_pool[ai++];
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
        create_matches_random(players, n, matches, num_matches);
    } else {
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
