/*
 * simulator_structure.c
 *
 * Simulateur EOMM (prototype) - pool 300 joueurs, parties uniquement 5v5.
 *
 * Objectifs de simulation:
 * - Tous les joueurs commencent au même MMR visible (ex: 1500)
 * - Game 1: matchmaking totalement random (tout le monde est "inconnu")
 * - Games 1..10: phase de placement (MMR delta x2)
 * - Après 10 games: MMR delta normal
 * - Chaque joueur a un profil latent (préférences) non observé: 2 rôles + 3 champions
 * - Chaque game, il y a une proba de sortir du schéma (off-schema):
 *     pOff = baseOff + loseStreak*loseStreakStep (cap)
 *   => picks hors préférences + comportements plus "tilt"
 *
 * Hidden factor:
 * - pénalise si rôle/champion actuel hors top observé (seulement quand top connu)
 * - pénalise pings/chat/deaths/clickrate via historiques
 *   (chat via baseline EMA)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef N_PLAYERS
#define N_PLAYERS 300
#endif

#define TEAM_SIZE 5
#define MATCH_SIZE (TEAM_SIZE * 2)

#define HISTORY_MAX 50
#define WINDOW_RECENT 10

/* Pools */
#define ROLE_POOL 5
#define CHAMP_POOL 20

/* Placement */
#define PLACEMENT_GAMES 10

/* MMR */
#define START_MMR 1500.0f
#define K_FACTOR_NORMAL 25.0f

/* Off-schema / tilt */
#define OFF_BASE 0.15f
#define OFF_STEP_PER_LOSS 0.05f
#define OFF_CAP 0.90f

/* Penalités hidden factor */
#define PENALTY_ROLE_NOT_TOP2          0.10f
#define PENALTY_CHAMP_NOT_TOP3         0.20f
#define PENALTY_INGAME_PINGS_ABOVE_AVG 0.05f
#define PENALTY_CHAT_ABOVE_BASELINE    0.05f
#define PENALTY_DEATH_ABOVE_AVG        0.05f
#define PENALTY_CLICK_ABOVE_AVG        0.05f
#define PENALTY_CLICK_BELOW_HALF       0.10f

/* Clamp */
#define HIDDEN_FACTOR_MIN 0.50f

/* Reset */
#define FULL_RESET_SECONDS (7 * 24 * 3600)
#define SOFT_RESET_PERIOD_GAMES 8
#define SOFT_RESET_HIDDEN_W 0.95f
#define SOFT_RESET_VISIBLE_W 0.05f

/* Baseline chat EMA */
#define CHAT_BASELINE_EMA_ALPHA 0.10f

/* ========================= */
/* EOMM system constants     */
/* ========================= */
#define N_SMURFS                        30
/* N_TOTAL_PLAYERS documents the intended pool size (must equal N_PLAYERS). */
#define N_TOTAL_PLAYERS                 300
#define TILT_TEAM_MIN                   3
#define HEALTHY_TEAM_MIN                3
#define MAX_SMURFS_PER_TEAM             1
#define VISIBLE_MMR_TOLERANCE           200.0f
#define SMURF_WINNING_CHANCE            70
#define SMURF_LOSING_REINFORCED_CHANCE  20

/* Hidden MMR state: reflects tilt/health of a player */
typedef enum {
    HMR_NEGATIVE = -1,  /* in tilt / degraded state */
    HMR_NEUTRAL  =  0,  /* normal state             */
    HMR_POSITIVE =  1   /* healthy / hot streak     */
} HiddenMMRState;

/* Smurf placement outcome (weighted random 70/20/10) */
typedef enum {
    SMURF_WINNING_TEAM       = 0, /* 70%: place in winning team                        */
    SMURF_LOSING_REINFORCED  = 1, /* 20%: losing team, replace 1 heavy-tilt with neutral*/
    SMURF_LOSING_STANDARD    = 2  /* 10%: losing team, standard 3 heavy-tilt            */
} SmurfPlacement;

typedef struct {
    char name[16];

    float visibleMMR;
    float hiddenMMR;
    float neutralMMR;
    time_t lastMatchTime;

    int wins;
    int losses;
    int totalGames;

    /* Current picks (observables) */
    int currentRole;                 /* 1..ROLE_POOL */
    char currentChampion[16];        /* "ChampXX" */

    /* Observed tops inferred from counts */
    int topRoles[2];
    int topRolesCount;               /* 0..2 */

    char topChampions[3][16];
    int topChampionsCount;           /* 0..3 */

    int roleFreq[ROLE_POOL + 1];     /* index 1..ROLE_POOL */
    int champFreq[CHAMP_POOL + 1];   /* index 1..CHAMP_POOL */

    /* Latent preferences (not observed) */
    int prefRoles[2];
    int prefChampIds[3];

    /* Tilt state */
    int loseStreak;

    /* EOMM fields */
    int            is_smurf;         /* 1 if this player is a smurf, 0 otherwise  */
    HiddenMMRState hidden_mmr_state; /* current hidden MMR category               */
    int            tilt_level;       /* 0=none, 1=light tilt, 2=heavy tilt        */

    /* Histories (observables per match) */
    int ingamePingCountHistory[HISTORY_MAX];
    int chatUsageHistory[HISTORY_MAX];
    int deathHistory[HISTORY_MAX];
    int clickRateHistory[HISTORY_MAX];
    int historyCount;

    /* Chat baseline EMA */
    float chatBaselineAvg;
    int   chatBaselineInit;
} Player;

typedef struct {
    Player *teamA[TEAM_SIZE];
    Player *teamB[TEAM_SIZE];
    float sumEffectiveA;
    float sumEffectiveB;
    int winner; /* 0: A, 1: B */
} Match;

/* =========================
 * Helpers
 * ========================= */
static int min_int(int a, int b) { return (a < b) ? a : b; }

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static int rand_range(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (rand() % (hi - lo + 1));
}

static float rand01(void) {
    return (float)rand() / (float)RAND_MAX;
}

static void champNameFromId(int champId, char out[16]) {
    snprintf(out, 16, "Champ%02d", champId);
}

static int champIdFromName(const char *name) {
    int id = 0;
    if (sscanf(name, "Champ%02d", &id) == 1) return id;
    return 0;
}

static void shufflePlayers(Player *players, int n) {
    for (int i = n - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        Player tmp = players[i];
        players[i] = players[j];
        players[j] = tmp;
    }
}

static void shufflePlayerPtrs(Player **arr, int n) {
    for (int i = n - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        Player *tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

static void recent_window(const Player *p, int *start, int *count) {
    int n = p->historyCount;
    if (n <= 0) {
        *start = 0;
        *count = 0;
        return;
    }
    int c = min_int(WINDOW_RECENT, n);
    *start = n - c;
    *count = c;
}

/* =========================
 * Baseline chat EMA + History
 * ========================= */
static void updateChatBaselineEMA(Player *p, int chatUsage) {
    float x = (float)chatUsage;
    if (!p->chatBaselineInit) {
        p->chatBaselineAvg = x;
        p->chatBaselineInit = 1;
        return;
    }

    p->chatBaselineAvg =
        (1.0f - CHAT_BASELINE_EMA_ALPHA) * p->chatBaselineAvg +
        CHAT_BASELINE_EMA_ALPHA * x;
}

static void pushHistory(Player *p, int ingamePingCount, int chatUsage, int deaths, int clickRate) {
    if (p->historyCount < HISTORY_MAX) {
        int idx = p->historyCount;
        p->ingamePingCountHistory[idx] = ingamePingCount;
        p->chatUsageHistory[idx] = chatUsage;
        p->deathHistory[idx] = deaths;
        p->clickRateHistory[idx] = clickRate;
        p->historyCount++;
    } else {
        for (int i = 1; i < HISTORY_MAX; i++) {
            p->ingamePingCountHistory[i - 1] = p->ingamePingCountHistory[i];
            p->chatUsageHistory[i - 1] = p->chatUsageHistory[i];
            p->deathHistory[i - 1] = p->deathHistory[i];
            p->clickRateHistory[i - 1] = p->clickRateHistory[i];
        }
        p->ingamePingCountHistory[HISTORY_MAX - 1] = ingamePingCount;
        p->chatUsageHistory[HISTORY_MAX - 1] = chatUsage;
        p->deathHistory[HISTORY_MAX - 1] = deaths;
        p->clickRateHistory[HISTORY_MAX - 1] = clickRate;
    }

    updateChatBaselineEMA(p, chatUsage);
}

/* =========================
 * Reset hidden MMR (kept)
 * ========================= */
static void resetHiddenMMR(Player *p) {
    time_t now = time(NULL);
    if (p->lastMatchTime == 0) { p->lastMatchTime = now; return; }
    if (difftime(now, p->lastMatchTime) >= (double)FULL_RESET_SECONDS) {
        p->hiddenMMR = p->neutralMMR;
        p->lastMatchTime = now;
    }
}

static void softResetHiddenMMR(Player *p) {
    if ((p->totalGames % SOFT_RESET_PERIOD_GAMES) == 0 && p->totalGames > 0) {
        p->hiddenMMR = SOFT_RESET_HIDDEN_W * p->hiddenMMR + SOFT_RESET_VISIBLE_W * p->visibleMMR;
    }
}

/* =========================
 * Inference of top roles/champs
 * ========================= */
static void recomputeTopRoles(Player *p) {
    int best1 = 0, best2 = 0;
    int f1 = -1, f2 = -1;

    for (int r = 1; r <= ROLE_POOL; r++) {
        int f = p->roleFreq[r];
        if (f > f1) {
            f2 = f1; best2 = best1;
            f1 = f;  best1 = r;
        } else if (f > f2) {
            f2 = f; best2 = r;
        }
    }

    p->topRolesCount = 0;
    if (best1 != 0 && f1 > 0) p->topRoles[p->topRolesCount++] = best1;
    if (best2 != 0 && best2 != best1 && f2 > 0 && p->topRolesCount < 2) p->topRoles[p->topRolesCount++] = best2;
}

static void recomputeTopChamps(Player *p) {
    int best[3] = {0,0,0};
    int freq[3] = {-1,-1,-1};

    for (int c = 1; c <= CHAMP_POOL; c++) {
        int f = p->champFreq[c];
        for (int k = 0; k < 3; k++) {
            if (f > freq[k]) {
                for (int s = 2; s > k; s--) { freq[s] = freq[s-1]; best[s] = best[s-1]; }
                freq[k] = f;
                best[k] = c;
                break;
            }
        }
    }

    p->topChampionsCount = 0;
    for (int k = 0; k < 3; k++) {
        if (best[k] != 0 && freq[k] > 0) {
            champNameFromId(best[k], p->topChampions[p->topChampionsCount]);
            p->topChampionsCount++;
        }
    }
}

static void observePick(Player *p) {
    if (p->currentRole >= 1 && p->currentRole <= ROLE_POOL) {
        p->roleFreq[p->currentRole] += 1;
    }

    int champId = champIdFromName(p->currentChampion);
    if (champId >= 1 && champId <= CHAMP_POOL) {
        p->champFreq[champId] += 1;
    }

    recomputeTopRoles(p);
    recomputeTopChamps(p);
}

/* =========================
 * Hidden factor / Effective MMR
 * ========================= */
static float calculateHiddenFactor(Player *p) {
    float factor = 1.0f;

    /* Role penalty only once top2 is known */
    if (p->topRolesCount >= 2) {
        int roleOK = 0;
        for (int i = 0; i < 2; i++) {
            if (p->currentRole == p->topRoles[i]) { roleOK = 1; break; }
        }
        if (!roleOK) factor -= PENALTY_ROLE_NOT_TOP2;
    }

    /* Champ penalty only once top3 is known */
    if (p->topChampionsCount >= 3) {
        int champOK = 0;
        for (int i = 0; i < 3; i++) {
            if (strcmp(p->currentChampion, p->topChampions[i]) == 0) { champOK = 1; break; }
        }
        if (!champOK) factor -= PENALTY_CHAMP_NOT_TOP3;
    }

    int start, count;
    recent_window(p, &start, &count);
    if (count > 0) {
        int last = p->historyCount - 1;

        /* Pings/emotes */
        {
            int sum = 0;
            for (int i = start; i < start + count; i++) sum += p->ingamePingCountHistory[i];
            float avg = (float)sum / (float)count;
            if ((float)p->ingamePingCountHistory[last] > avg) factor -= PENALTY_INGAME_PINGS_ABOVE_AVG;
        }

        /* Chat vs baseline EMA */
        {
            float baseline = p->chatBaselineInit ? p->chatBaselineAvg : 0.0f;
            if (p->chatBaselineInit && (float)p->chatUsageHistory[last] > baseline) {
                factor -= PENALTY_CHAT_ABOVE_BASELINE;
            }
        }

        /* Death */
        {
            int sum = 0;
            for (int i = start; i < start + count; i++) sum += p->deathHistory[i];
            float avg = (float)sum / (float)count;
            if ((float)p->deathHistory[last] > avg) factor -= PENALTY_DEATH_ABOVE_AVG;
        }

        /* Click rate */
        {
            int sum = 0;
            for (int i = start; i < start + count; i++) sum += p->clickRateHistory[i];
            float avg = (float)sum / (float)count;
            float lastClick = (float)p->clickRateHistory[last];

            if (lastClick > avg) factor -= PENALTY_CLICK_ABOVE_AVG;
            else if (avg > 0.0f && lastClick < avg * 0.5f) factor -= PENALTY_CLICK_BELOW_HALF;
        }
    }

    if (factor < HIDDEN_FACTOR_MIN) factor = HIDDEN_FACTOR_MIN;
    return factor;
}

static float effectiveMMR(Player *p) {
    return p->visibleMMR * calculateHiddenFactor(p);
}

/* =========================
 * EOMM Core Functions
 * ========================= */

/*
 * calculateHiddenMMRState - Determine a player's hidden MMR category.
 * Also updates p->tilt_level (0=none, 1=light, 2=heavy).
 *
 * Rules:
 *   - loseStreak >= 3 OR factor <= 0.70  => heavy tilt  => NEGATIVE
 *   - loseStreak >  0 OR factor <  0.90  => light tilt  => NEGATIVE
 *   - factor >= 0.95 and loseStreak == 0 => POSITIVE
 *   - otherwise                          => NEUTRAL
 */
static HiddenMMRState calculateHiddenMMRState(Player *p) {
    float factor = calculateHiddenFactor(p);

    if (p->loseStreak >= 3 || factor <= 0.70f) {
        p->tilt_level = 2;
        return HMR_NEGATIVE;
    }
    if (p->loseStreak > 0 || factor < 0.90f) {
        p->tilt_level = 1;
        return HMR_NEGATIVE;
    }
    p->tilt_level = 0;
    return (factor >= 0.95f) ? HMR_POSITIVE : HMR_NEUTRAL;
}

/*
 * selectTiltPlayers - Fill `out` with up to `maxOut` players that have
 * negative hidden MMR from `pool`.  Heavy tilt (level 2) is preferred
 * over light tilt (level 1) to maximize the losing-team effect.
 */
static int selectTiltPlayers(Player **pool, int poolSize, Player **out, int maxOut) {
    int count = 0;
    for (int pass = 2; pass >= 1 && count < maxOut; pass--) {
        for (int i = 0; i < poolSize && count < maxOut; i++) {
            if (pool[i]->hidden_mmr_state == HMR_NEGATIVE && pool[i]->tilt_level == pass) {
                out[count++] = pool[i];
            }
        }
    }
    return count;
}

/*
 * selectHealthyPlayers - Fill `out` with up to `maxOut` players that have
 * neutral or positive hidden MMR from `pool`.
 */
static int selectHealthyPlayers(Player **pool, int poolSize, Player **out, int maxOut) {
    int count = 0;
    for (int i = 0; i < poolSize && count < maxOut; i++) {
        if (pool[i]->hidden_mmr_state != HMR_NEGATIVE) {
            out[count++] = pool[i];
        }
    }
    return count;
}

/*
 * placeSmurf - Weighted random smurf placement.
 *   70% -> SMURF_WINNING_TEAM
 *   20% -> SMURF_LOSING_REINFORCED  (losing team with lighter tilt composition)
 *   10% -> SMURF_LOSING_STANDARD    (losing team with normal heavy-tilt composition)
 */
static SmurfPlacement placeSmurf(void) {
    int roll = rand_range(1, 100);
    if (roll <= SMURF_WINNING_CHANCE)
        return SMURF_WINNING_TEAM;
    if (roll <= SMURF_WINNING_CHANCE + SMURF_LOSING_REINFORCED_CHANCE)
        return SMURF_LOSING_REINFORCED;
    return SMURF_LOSING_STANDARD;
}

/*
 * validateTeamMMRBalance - Return 1 if the max visible-MMR difference between
 * any two players in the match is within VISIBLE_MMR_TOLERANCE, 0 otherwise.
 */
static int validateTeamMMRBalance(const Match *m) {
    float minMMR =  1e9f;
    float maxMMR = -1.0f;
    for (int i = 0; i < TEAM_SIZE; i++) {
        if (m->teamA[i]) {
            float v = m->teamA[i]->visibleMMR;
            if (v < minMMR) minMMR = v;
            if (v > maxMMR) maxMMR = v;
        }
        if (m->teamB[i]) {
            float v = m->teamB[i]->visibleMMR;
            if (v < minMMR) minMMR = v;
            if (v > maxMMR) maxMMR = v;
        }
    }
    if (minMMR > maxMMR) return 1; /* minMMR=1e9f > maxMMR=-1.0f means no players assigned */
    return (maxMMR - minMMR) <= VISIBLE_MMR_TOLERANCE;
}

/*
 * createMatchWithEOMM - Build matches using the full EOMM strategy:
 *
 *  Losing team  (teamA): 3 tilt players  (negative hidden MMR)
 *  Winning team (teamB): 3 healthy players (neutral/positive hidden MMR)
 *  Smurf       (opt.)  : 1 per team max, weighted placement 70/20/10
 *  Random      (4 left): distributed to balance each team to 5 players
 *
 * Visible MMR balance (±200) is validated per match; teams that cannot
 * satisfy the constraint fall back to any available unassigned player.
 */
static void createMatchWithEOMM(Player *players, int nPlayers, Match *matches, int *numMatches) {
    int nm = nPlayers / MATCH_SIZE;
    *numMatches = nm;

    /* Pre-compute hidden MMR state for every player once per round */
    for (int i = 0; i < nPlayers; i++) {
        players[i].hidden_mmr_state = calculateHiddenMMRState(&players[i]);
    }

    int *assigned = (int*)calloc(nPlayers, sizeof(int));
    if (!assigned) return;

    /* Pool arrays reused across matches to avoid repeated stack allocation */
    Player **smurfPool  = (Player**)malloc(sizeof(Player*) * nPlayers);
    Player **tiltPool   = (Player**)malloc(sizeof(Player*) * nPlayers);
    Player **healthyPool = (Player**)malloc(sizeof(Player*) * nPlayers);
    Player **remaining  = (Player**)malloc(sizeof(Player*) * nPlayers);

    if (!smurfPool || !tiltPool || !healthyPool || !remaining) {
        free(smurfPool); free(tiltPool); free(healthyPool); free(remaining);
        free(assigned);
        return;
    }

    for (int m = 0; m < nm; m++) {
        Match *match = &matches[m];
        match->sumEffectiveA = 0.0f;
        match->sumEffectiveB = 0.0f;
        match->winner = -1;
        for (int i = 0; i < TEAM_SIZE; i++) {
            match->teamA[i] = NULL;
            match->teamB[i] = NULL;
        }

        /* ---- Categorise available (unassigned) players ---- */
        int smurfN = 0, tiltN = 0, healthyN = 0;

        for (int i = 0; i < nPlayers; i++) {
            if (assigned[i]) continue;
            Player *p = &players[i];
            if (p->is_smurf) {
                smurfPool[smurfN++] = p;
            } else if (p->hidden_mmr_state == HMR_NEGATIVE) {
                tiltPool[tiltN++] = p;
            } else {
                healthyPool[healthyN++] = p;
            }
        }

        /* Shuffle pools to introduce randomness within each category */
        shufflePlayerPtrs(smurfPool,   smurfN);
        shufflePlayerPtrs(tiltPool,    tiltN);
        shufflePlayerPtrs(healthyPool, healthyN);

        int aCount = 0, bCount = 0;
        int smurfInA = 0;

        /* ---- Smurf placement (max 1 smurf per team) ---- */
        Player *selectedSmurf = (smurfN > 0) ? smurfPool[0] : NULL;
        SmurfPlacement smurfPos = SMURF_WINNING_TEAM;

        if (selectedSmurf) {
            smurfPos = placeSmurf();
            assigned[selectedSmurf - players] = 1;

            if (smurfPos == SMURF_WINNING_TEAM) {
                match->teamB[bCount++] = selectedSmurf;
            } else {
                match->teamA[aCount++] = selectedSmurf;
                smurfInA = 1;
            }
        }

        /* ---- Fill losing team (teamA) with tilt players ---- */
        /*
         * Reinforced placement: the smurf takes the 3rd heavy-tilt slot.
         * Only 2 tilt players are explicitly selected; the remaining slot
         * is filled from the random pool (which may be neutral/lighter).
         */
        int tiltNeeded = TILT_TEAM_MIN;
        if (smurfInA && smurfPos == SMURF_LOSING_REINFORCED) {
            tiltNeeded = 2;
        }

        Player *tiltSelected[TILT_TEAM_MIN];
        int tiltSelectedN = selectTiltPlayers(tiltPool, tiltN, tiltSelected, tiltNeeded);

        for (int t = 0; t < tiltSelectedN; t++) {
            match->teamA[aCount++] = tiltSelected[t];
            assigned[tiltSelected[t] - players] = 1;
        }

        /* ---- Fill winning team (teamB) with healthy players ---- */
        Player *healthySelected[HEALTHY_TEAM_MIN];
        int healthySelectedN = selectHealthyPlayers(healthyPool, healthyN, healthySelected, HEALTHY_TEAM_MIN);

        for (int h = 0; h < healthySelectedN; h++) {
            match->teamB[bCount++] = healthySelected[h];
            assigned[healthySelected[h] - players] = 1;
        }

        /* ---- Distribute remaining 4 players randomly ---- */
        /* Collect all unassigned players; shuffle for unpredictability */
        int remainingN = 0;
        for (int i = 0; i < nPlayers; i++) {
            if (!assigned[i]) remaining[remainingN++] = &players[i];
        }
        shufflePlayerPtrs(remaining, remainingN);

        /* Distribute evenly: favour the team with fewer players first.
         * The OR condition stops the loop once both teams reach TEAM_SIZE;
         * the inner guards ensure neither count ever exceeds TEAM_SIZE. */
        for (int r = 0; r < remainingN && (aCount < TEAM_SIZE || bCount < TEAM_SIZE); r++) {
            if (aCount < bCount && aCount < TEAM_SIZE) {
                match->teamA[aCount++] = remaining[r];
            } else if (bCount < TEAM_SIZE) {
                match->teamB[bCount++] = remaining[r];
            } else if (aCount < TEAM_SIZE) {
                match->teamA[aCount++] = remaining[r];
            }
            assigned[remaining[r] - players] = 1;
        }

        /* ---- Verify visible MMR balance (informational; logged to stderr) ---- */
        if (!validateTeamMMRBalance(match)) {
            fprintf(stderr, "[EOMM] Match %d: visible MMR spread > %.0f (teams may appear unbalanced)\n",
                    m, VISIBLE_MMR_TOLERANCE);
        }
    }

    free(smurfPool);
    free(tiltPool);
    free(healthyPool);
    free(remaining);
    free(assigned);
}

/* =========================
 * Matchmaking helpers
 * ========================= */
static void placeInitialTeams(Player *players, int nPlayers, Match *matches, int *numMatches) {
    shufflePlayers(players, nPlayers);

    int nm = nPlayers / MATCH_SIZE;
    *numMatches = nm;

    for (int m = 0; m < nm; m++) {
        matches[m].sumEffectiveA = 0.0f;
        matches[m].sumEffectiveB = 0.0f;
        matches[m].winner = -1;

        int base = m * MATCH_SIZE;
        for (int i = 0; i < TEAM_SIZE; i++) {
            matches[m].teamA[i] = &players[base + i];
            matches[m].teamB[i] = &players[base + TEAM_SIZE + i];
        }
    }
}

/*
 * createMatchAdvanced - Entry point for post-game-1 matchmaking.
 * Delegates to createMatchWithEOMM so that all matches after the
 * random first game use the full EOMM engagement-optimized logic.
 */
static void createMatchAdvanced(Player *players, int nPlayers, Match *matches, int *numMatches) {
    createMatchWithEOMM(players, nPlayers, matches, numMatches);
}

/* =========================
 * MMR update (placements x2)
 * ========================= */
static float mmr_k_for_player(const Player *p) {
    /* totalGames is incremented after match signals but before applying MMR in this sim;
       here, we interpret "first 10 games" as totalGames < 10 at time of applying MMR. */
    if (p->totalGames < PLACEMENT_GAMES) return 2.0f * K_FACTOR_NORMAL;
    return K_FACTOR_NORMAL;
}

static void apply_mmr_result(Player *p, int didWin) {
    float K = mmr_k_for_player(p);
    if (didWin) p->visibleMMR += K;
    else p->visibleMMR -= K;

    if (p->visibleMMR < 0.0f) p->visibleMMR = 0.0f;
}

/* =========================
 * Latent prefs init
 * ========================= */
static void initUniquePairRoles(int out[2]) {
    out[0] = rand_range(1, ROLE_POOL);
    do { out[1] = rand_range(1, ROLE_POOL); } while (out[1] == out[0]);
}

static void initUniqueTripleChamps(int out[3]) {
    out[0] = rand_range(1, CHAMP_POOL);
    do { out[1] = rand_range(1, CHAMP_POOL); } while (out[1] == out[0]);
    do { out[2] = rand_range(1, CHAMP_POOL); } while (out[2] == out[0] || out[2] == out[1]);
}

/* =========================
 * Picks based on off-schema
 * ========================= */
static int pickRole(const Player *p, int offSchema) {
    if (!offSchema) {
        return p->prefRoles[rand_range(0, 1)];
    }

    int r;
    do {
        r = rand_range(1, ROLE_POOL);
    } while (r == p->prefRoles[0] || r == p->prefRoles[1]);
    return r;
}

static int pickChampId(const Player *p, int offSchema) {
    if (!offSchema) {
        return p->prefChampIds[rand_range(0, 2)];
    }

    int c;
    do {
        c = rand_range(1, CHAMP_POOL);
    } while (c == p->prefChampIds[0] || c == p->prefChampIds[1] || c == p->prefChampIds[2]);
    return c;
}

/* =========================
 * Signals: when offSchema -> amplify tilt signals
 * ========================= */
static void simulateSignals(int offSchema, int *outPings, int *outChat, int *outDeaths, int *outClicks) {
    int pings  = rand_range(0, 20);
    int chat   = rand_range(0, 10);
    int deaths = rand_range(0, 12);
    int clicks = rand_range(80, 380);

    if (offSchema) {
        pings  += rand_range(5, 25);
        chat   += rand_range(5, 20);
        deaths += rand_range(2, 10);
        clicks -= rand_range(10, 80);
    }

    if (clicks < 10) clicks = 10;

    *outPings = pings;
    *outChat = chat;
    *outDeaths = deaths;
    *outClicks = clicks;
}

/* =========================
 * One simulation step for all matches
 * ========================= */
static void simulateMatchAndUpdatePlayers(Match *matches, int numMatches) {
    /* Same simple winner logic as before: compare avg effective to globalAvg + opponent */
    float globalSum = 0.0f;
    int globalCount = 0;

    for (int m = 0; m < numMatches; m++) {
        for (int i = 0; i < TEAM_SIZE; i++) {
            globalSum += effectiveMMR(matches[m].teamA[i]);
            globalSum += effectiveMMR(matches[m].teamB[i]);
            globalCount += 2;
        }
    }
    float globalAvg = (globalCount > 0) ? (globalSum / (float)globalCount) : 0.0f;

    for (int m = 0; m < numMatches; m++) {
        float sumA = 0.0f, sumB = 0.0f;

        for (int i = 0; i < TEAM_SIZE; i++) {
            sumA += effectiveMMR(matches[m].teamA[i]);
            sumB += effectiveMMR(matches[m].teamB[i]);
        }

        matches[m].sumEffectiveA = sumA;
        matches[m].sumEffectiveB = sumB;

        float avgA = sumA / (float)TEAM_SIZE;
        float avgB = sumB / (float)TEAM_SIZE;

        int winner = (avgA >= globalAvg && avgA >= avgB) ? 0 : 1;
        matches[m].winner = winner;

        for (int i = 0; i < TEAM_SIZE; i++) {
            Player *pa = matches[m].teamA[i];
            Player *pb = matches[m].teamB[i];

            /* Off-schema probability depends on loseStreak */
            float pOffA = clampf(OFF_BASE + OFF_STEP_PER_LOSS * (float)pa->loseStreak, 0.0f, OFF_CAP);
            float pOffB = clampf(OFF_BASE + OFF_STEP_PER_LOSS * (float)pb->loseStreak, 0.0f, OFF_CAP);

            int offA = (rand01() < pOffA) ? 1 : 0;
            int offB = (rand01() < pOffB) ? 1 : 0;

            /* Choose picks */
            pa->currentRole = pickRole(pa, offA);
            pb->currentRole = pickRole(pb, offB);

            int champA = pickChampId(pa, offA);
            int champB = pickChampId(pb, offB);
            champNameFromId(champA, pa->currentChampion);
            champNameFromId(champB, pb->currentChampion);

            /* Observe picks for inferred tops */
            observePick(pa);
            observePick(pb);

            /* Signals */
            int pingsA, chatA, deathsA, clicksA;
            int pingsB, chatB, deathsB, clicksB;
            simulateSignals(offA, &pingsA, &chatA, &deathsA, &clicksA);
            simulateSignals(offB, &pingsB, &chatB, &deathsB, &clicksB);

            pushHistory(pa, pingsA, chatA, deathsA, clicksA);
            pushHistory(pb, pingsB, chatB, deathsB, clicksB);

            /* Increment games */
            pa->totalGames++;
            pb->totalGames++;

            int aWon = (winner == 0);
            int bWon = (winner == 1);

            if (aWon) { pa->wins++; pb->losses++; }
            else { pb->wins++; pa->losses++; }

            /* loseStreak update */
            if (aWon) pa->loseStreak = 0; else pa->loseStreak += 1;
            if (bWon) pb->loseStreak = 0; else pb->loseStreak += 1;

            /* MMR update */
            apply_mmr_result(pa, aWon);
            apply_mmr_result(pb, bWon);

            time_t now = time(NULL);
            pa->lastMatchTime = now;
            pb->lastMatchTime = now;

            softResetHiddenMMR(pa);
            softResetHiddenMMR(pb);
        }
    }
}

/* =========================
 * Init player
 * ========================= */
static void initPlayer(Player *p, int idx) {
    snprintf(p->name, sizeof(p->name), "Player%03d", idx + 1);

    p->visibleMMR = START_MMR;
    p->neutralMMR = START_MMR;
    p->hiddenMMR = START_MMR;

    p->lastMatchTime = time(NULL);

    p->wins = 0;
    p->losses = 0;
    p->totalGames = 0;

    initUniquePairRoles(p->prefRoles);
    initUniqueTripleChamps(p->prefChampIds);

    p->currentRole = 0;
    memset(p->currentChampion, 0, sizeof(p->currentChampion));

    p->topRoles[0] = 0;
    p->topRoles[1] = 0;
    p->topRolesCount = 0;

    memset(p->topChampions, 0, sizeof(p->topChampions));
    p->topChampionsCount = 0;

    for (int r = 0; r <= ROLE_POOL; r++) p->roleFreq[r] = 0;
    for (int c = 0; c <= CHAMP_POOL; c++) p->champFreq[c] = 0;

    p->loseStreak = 0;

    /* EOMM fields */
    p->is_smurf         = 0;
    p->hidden_mmr_state = HMR_NEUTRAL;
    p->tilt_level       = 0;

    p->historyCount = 0;
    for (int i = 0; i < HISTORY_MAX; i++) {
        p->ingamePingCountHistory[i] = 0;
        p->chatUsageHistory[i] = 0;
        p->deathHistory[i] = 0;
        p->clickRateHistory[i] = 0;
    }

    p->chatBaselineAvg = 0.0f;
    p->chatBaselineInit = 0;
}

/* =========================
 * Main (optionnel)
 * ========================= */
#ifdef BUILD_SIMULATOR_MAIN
int main(void) {
    srand((unsigned int)time(NULL));

    Player players[N_PLAYERS];
    for (int i = 0; i < N_PLAYERS; i++) initPlayer(&players[i], i);

    /* Randomly select N_SMURFS distinct players to be smurfs
     * (N_SMURFS / N_PLAYERS = 10% of playerbase).
     * Uses a partial Fisher-Yates shuffle on an index array to avoid bias. */
    {
        int idx[N_PLAYERS];
        for (int i = 0; i < N_PLAYERS; i++) idx[i] = i;
        for (int i = 0; i < N_SMURFS; i++) {
            int j = i + rand() % (N_PLAYERS - i);
            int tmp = idx[i]; idx[i] = idx[j]; idx[j] = tmp;
            players[idx[i]].is_smurf = 1;
        }
    }

    Match matches[N_PLAYERS / MATCH_SIZE];
    int numMatches = 0;

    int NUM_GAMES = 50;

    for (int g = 0; g < NUM_GAMES; g++) {
        for (int i = 0; i < N_PLAYERS; i++) resetHiddenMMR(&players[i]);

        /* Game 1 random, then advanced placement */
        if (g == 0) placeInitialTeams(players, N_PLAYERS, matches, &numMatches);
        else createMatchAdvanced(players, N_PLAYERS, matches, &numMatches);

        simulateMatchAndUpdatePlayers(matches, numMatches);
    }

    printf("Simulation finished. Sample players:\n");
    for (int i = 0; i < 10; i++) {
        Player *p = &players[i];
        printf("%s mmr=%.0f factor=%.2f chatBaseline=%.2f loseStreak=%d W=%d L=%d G=%d "
               "topRolesCount=%d topChampsCount=%d smurf=%d tilt=%d\n",
               p->name, p->visibleMMR, calculateHiddenFactor(p), p->chatBaselineAvg, p->loseStreak,
               p->wins, p->losses, p->totalGames, p->topRolesCount, p->topChampionsCount,
               p->is_smurf, p->tilt_level);
    }

    /* EOMM smurf winrate summary */
    printf("\nSmurf winrate summary (%d smurfs):\n", N_SMURFS);
    for (int i = 0; i < N_PLAYERS; i++) {
        Player *p = &players[i];
        if (!p->is_smurf) continue;
        float wr = (p->totalGames > 0) ? (100.0f * (float)p->wins / (float)p->totalGames) : 0.0f;
        printf("  %s mmr=%.0f W=%d L=%d G=%d winrate=%.1f%%\n",
               p->name, p->visibleMMR, p->wins, p->losses, p->totalGames, wr);
    }

    return 0;
}
#endif
