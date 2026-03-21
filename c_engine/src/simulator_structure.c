/*
 * simulator_structure.c
 *
 * Simulateur EOMM (prototype) - pool 300 joueurs, parties uniquement 5v5.
 *
 * =========================================================================
 * OBJECTIF EOMM (Engagement-Optimized Matchmaking)
 * =========================================================================
 * L'objectif n'est PAS de créer des matchs parfaitement équilibrés, mais de
 * MAXIMISER L'ENGAGEMENT en contrôlant les cycles de winning/losing streaks.
 *
 * Le système:
 * - Vise ~50% de winrate global pour tous les joueurs sur le long terme
 * - Manipule la COMPOSITION d'équipes (pas les gains/pertes de LP)
 * - Crée des cycles naturels: périodes de victoires suivies de défaites forcées
 * - Montre que même les meilleurs joueurs ne peuvent échapper aux streaks
 * - Montre que même les mauvais joueurs ont des "moments de gloire"
 *
 * Winrates cibles:
 * - Excellents joueurs (smurfs):   ~58-62% (meilleur, mais pas dominant)
 * - Mauvais joueurs (hardstuck):   ~40-45% (pire, mais pas inexistant)
 * - Joueurs normaux:               ~50%    (cycles avec streaks naturels)
 *
 * IMPORTANT: Pas de récompense pour la performance individuelle (LoL standard).
 *            LP gains/losses sont fixes (+25/-25). Le biais vient uniquement
 *            de la composition d'équipes.
 * =========================================================================
 *
 * Objectifs de simulation:
 * - Tous les joueurs commencent au même MMR visible (ex: 1000)
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
 * - pénalise si champion hors pool de rôle du joueur (wrong role pick)
 * - pénalise pings/chat/deaths/clickrate via historiques
 *   (chat via baseline EMA)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../include/lol_champions.h"
#include "../include/lol_tiers.h"

#ifndef N_PLAYERS
#define N_PLAYERS 300
#endif

#define TEAM_SIZE 5
#define MATCH_SIZE (TEAM_SIZE * 2)

#define HISTORY_MAX 50
#define WINDOW_RECENT 10

/* Pools */
#define ROLE_POOL 5
#define CHAMP_POOL LOL_CHAMP_COUNT   /* 170 real LoL champions */

/* Placement */
#define PLACEMENT_GAMES 10

/* MMR */
#define START_MMR 1000.0f        /* all players begin at 1000 (Silver) */
#define K_FACTOR_NORMAL 25.0f
#define K_FACTOR_PLACEMENT 30.0f /* placement phase K-factor (faster calibration) */

/* Off-schema / tilt */
#define OFF_BASE 0.15f
#define OFF_STEP_PER_LOSS 0.05f
#define OFF_CAP 0.90f

/* Penalités hidden factor */
#define PENALTY_ROLE_NOT_TOP2          0.10f
#define PENALTY_CHAMP_NOT_TOP3         0.20f
#define PENALTY_CHAMP_WRONG_ROLE       0.25f  /* -25% if champion's role doesn't match player's top roles */
#define PENALTY_INGAME_PINGS_ABOVE_AVG 0.05f
#define PENALTY_CHAT_ABOVE_BASELINE    0.05f
#define PENALTY_DEATH_ABOVE_AVG        0.05f
#define PENALTY_CLICK_ABOVE_AVG        0.05f
#define PENALTY_CLICK_BELOW_HALF       0.10f

/* Clamp */
#define HIDDEN_FACTOR_MIN 0.50f

/* Troll pick / tilt-based champion selection */
#define BASE_TROLL_PROBABILITY  5.0f   /* base 5% chance to troll-pick */
#define TROLL_FACTOR_PER_LOSS   2.0f   /* +2% per loss streak increment */
#define TROLL_PROBABILITY_CAP  60.0f   /* maximum 60% troll probability  */
#define PENALTY_TROLL_PICK      0.20f  /* -20% hidden factor for troll pick */
#define TROLL_PICK_TILT_INCREASE 5     /* tilt increase when troll-pick detected */

/* Reset dual: RESET_SHORT (7 games) or RESET_MEDIUM (14 games) - TOTAL reset */
#define RESET_SHORT  7
#define RESET_MEDIUM 14
/* Change CURRENT_RESET_MODE to switch between the two reset modes */
#define CURRENT_RESET_MODE RESET_MEDIUM

/* Baseline chat EMA */
#define CHAT_BASELINE_EMA_ALPHA 0.10f

/* ========================= */
/* EOMM system constants     */
/* ========================= */
#define N_SMURFS                        30
#define N_HARDSTUCK                     30
/* N_TOTAL_PLAYERS documents the intended pool size (must equal N_PLAYERS). */
#define N_TOTAL_PLAYERS                 300
#define TILT_TEAM_MIN                   3
#define HEALTHY_TEAM_MIN                3
#define MAX_SMURFS_PER_TEAM             1
#define VISIBLE_MMR_TOLERANCE           200.0f
#define SMURF_WINNING_CHANCE            70
#define SMURF_LOSING_REINFORCED_CHANCE  20

/*
 * SkillLevel — permanent skill category assigned at player creation.
 * Determines base win-rate contribution to match outcome.
 */
typedef enum {
    SKILL_NORMAL    = 0,  /* 80% of players — 50% base WR */
    SKILL_SMURF     = 1,  /* 10% of players — 58% base WR */
    SKILL_HARDSTUCK = 2   /* 10% of players — 42% base WR */
} SkillLevel;

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
    char currentChampion[24];        /* real champion name (e.g. "Ahri") */

    /* Observed tops inferred from counts */
    int topRoles[2];
    int topRolesCount;               /* 0..2 */

    char topChampions[3][24];
    int topChampionsCount;           /* 0..3 */

    int roleFreq[ROLE_POOL + 1];     /* index 1..ROLE_POOL */
    int champFreq[CHAMP_POOL + 1];   /* index 1..CHAMP_POOL */

    /* Latent preferences (not observed) */
    int prefRoles[2];
    int prefChampIds[3];

    /* Tilt state */
    int loseStreak;
    int winStreak;

    /* EOMM fields */
    int            is_smurf;         /* 1 if this player is a smurf, 0 otherwise  */
    SkillLevel     skill_level;      /* NORMAL / SMURF / HARDSTUCK                */
    float          win_rate;         /* base win probability (0.50/0.58/0.42)     */
    HiddenMMRState hidden_mmr_state; /* current hidden MMR category               */
    int            tilt_level;       /* 0=none, 1=light tilt, 2=heavy tilt        */

    /* LoL tier / division (visible rank) */
    TierEnum tier;                   /* Iron..Master                              */
    int      division;               /* 0=I, 1=II, 2=III, 3=IV (Master=0)        */

    /* Role preferences (0-indexed: ROLE_TOP..ROLE_SUPPORT) */
    int primaryRole;                 /* preferred main role                       */
    int secondaryRole;               /* preferred secondary role                  */

    /* Troll-pick tracking */
    int   isTrollPick;               /* 1 if current pick is off-pool             */
    float trollProbability;          /* current troll probability (%)             */
    int   currentChampionId;         /* numeric id of champion picked this game   */
    int   currentChampionRole;       /* role played with currentChampionId (0..4) */

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
static const char *skillLabel(SkillLevel s) {
    switch (s) {
        case SKILL_SMURF:     return "smurf";
        case SKILL_HARDSTUCK: return "hardstuck";
        default:              return "normal";
    }
}
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

static void champNameFromId(int champId, char out[24]) {
    const char *n = lolChampName(champId);
    if (n[0] != '\0') {
        snprintf(out, 24, "%s", n);
    } else {
        snprintf(out, 24, "Champ%03d", champId);
    }
}

static int champIdFromName(const char *name) {
    int id = lolChampIdByName(name);
    if (id != 0) return id;
    /* Fallback: parse legacy "ChampXXX" format */
    sscanf(name, "Champ%03d", &id);
    return id;
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
    if (difftime(now, p->lastMatchTime) >= (double)(7 * 24 * 3600)) {
        p->hiddenMMR = p->neutralMMR;
        p->lastMatchTime = now;
    }
}

/*
 * softResetHiddenMMR - TOTAL reset of hidden MMR after CURRENT_RESET_MODE games.
 * Two modes available: RESET_SHORT (7) or RESET_MEDIUM (14).
 * TOTAL reset: returns to neutral state (not progressive 95%/5%).
 * Also resets loseStreak and tilt_level.
 */
static void softResetHiddenMMR(Player *p) {
    if ((p->totalGames % CURRENT_RESET_MODE) == 0 && p->totalGames > 0) {
        /* TOTAL reset: full return to neutral state (hidden factor = 1.0) */
        p->hidden_mmr_state = HMR_NEUTRAL;
        p->loseStreak = 0;
        p->tilt_level = 0;
        printf("[RESET] %s total reset at game %d (mode=%d)\n",
               p->name, p->totalGames, CURRENT_RESET_MODE);
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

    /* Troll pick penalty: off-pool pick detected */
    if (p->isTrollPick) {
        factor -= PENALTY_TROLL_PICK;
    }

    /* Champion wrong-role penalty: champion's primary role doesn't match player's top roles */
    if (p->topRolesCount >= 1) {
        int roleMatch = 0;
        for (int i = 0; i < p->topRolesCount; i++) {
            /* topRoles are 1-indexed; currentChampionRole is 0-indexed */
            if (p->currentChampionRole == (p->topRoles[i] - 1)) { roleMatch = 1; break; }
        }
        if (!roleMatch) factor -= PENALTY_CHAMP_WRONG_ROLE;
    }

    if (factor < HIDDEN_FACTOR_MIN) factor = HIDDEN_FACTOR_MIN;
    return factor;
}

static float effectiveMMR(Player *p) {
    return p->visibleMMR * calculateHiddenFactor(p);
}

/* =========================
 * Troll-pick / Champion selection
 * ========================= */

/*
 * calculateTrollProbability - return the current troll-pick probability (%).
 * Base 5% + 2% per loss-streak step, capped at 60%.
 */
static float calculateTrollProbability(Player *p) {
    float prob = BASE_TROLL_PROBABILITY + ((float)p->loseStreak * TROLL_FACTOR_PER_LOSS);
    if (prob > TROLL_PROBABILITY_CAP) prob = TROLL_PROBABILITY_CAP;
    return prob;
}

/*
 * buildPlayerChampionPool - Build a pool of viable champions based on observed roles.
 * Filters champions whose primary role matches the player's topRoles.
 * Returns the number of champions in outPool.
 */
static int buildPlayerChampionPool(const Player *p, int outPool[], int maxPool) {
    int count = 0;
    if (p->topRolesCount == 0) return 0;  /* No roles observed yet */

    for (int c = 1; c <= CHAMP_POOL && count < maxPool; c++) {
        int champRole = lolChampPrimaryRole(c);
        for (int r = 0; r < p->topRolesCount; r++) {
            /* topRoles are 1-indexed; champRole is 0-indexed */
            if (champRole == (p->topRoles[r] - 1)) {
                outPool[count++] = c;
                break;
            }
        }
    }
    return count;
}

/*
 * selectChampionWithPool - Pick a champion from the role pool (or troll outside it).
 * Uses buildPlayerChampionPool() to build a pool of viable champions based on the
 * player's top observed roles, then applies troll probability to decide if the
 * player steps outside the pool (tilt behaviour).
 */
static void selectChampionWithPool(Player *p) {
    float troll_prob = calculateTrollProbability(p);
    p->trollProbability = troll_prob;

    int rolePool[CHAMP_POOL];
    int rolePoolN = buildPlayerChampionPool(p, rolePool, CHAMP_POOL);

    int troll_roll = rand_range(1, 100);
    if ((float)troll_roll <= troll_prob || rolePoolN == 0) {
        /* TROLL PICK: champion outside role pool */
        int c;
        int tries = 0;
        do {
            c = rand_range(1, CHAMP_POOL);
            tries++;
            if (rolePoolN == 0) break;
            /* Try to pick a champion NOT in the role pool */
            int inPool = 0;
            for (int i = 0; i < rolePoolN; i++) {
                if (rolePool[i] == c) { inPool = 1; break; }
            }
            if (!inPool) break;
        } while (tries < 10);

        p->currentChampionId = c;
        p->isTrollPick = 1;
    } else {
        /* NORMAL PICK: champion from role pool */
        int idx = rand_range(0, rolePoolN - 1);
        p->currentChampionId = rolePool[idx];
        p->isTrollPick = 0;
    }

    /* Sync string name and champion role */
    champNameFromId(p->currentChampionId, p->currentChampion);
    p->currentChampionRole = lolChampPrimaryRole(p->currentChampionId);
}

/*
 * assignRole - set currentRole based on primary/secondary role preference.
 *
 * availableRoles[5]: 1 if the role (0..4) is still open in the team, 0 otherwise.
 * The assigned role slot is marked 0 in availableRoles so the caller can track
 * which roles have been taken when assigning multiple players in the same team.
 * Falls back to a random available role if neither preference is available.
 * currentRole is stored 1-indexed (1..ROLE_POOL) to match the rest of the code.
 */
static void assignRole(Player *p, int availableRoles[5]) {
    int chosen = -1;
    if (p->primaryRole >= 0 && p->primaryRole < 5 && availableRoles[p->primaryRole]) {
        chosen = p->primaryRole;
    } else if (p->secondaryRole >= 0 && p->secondaryRole < 5 && availableRoles[p->secondaryRole]) {
        chosen = p->secondaryRole;
    } else {
        /* Neither preferred role available: pick any open role at random */
        int choices[5];
        int n = 0;
        int r;
        for (r = 0; r < 5; r++) {
            if (availableRoles[r]) choices[n++] = r;
        }
        if (n > 0) {
            chosen = choices[rand_range(0, n - 1)];
        }
    }

    if (chosen >= 0) {
        p->currentRole = chosen + 1;   /* convert 0-indexed to 1-indexed */
        availableRoles[chosen] = 0;    /* mark role as taken in this team */
    } else {
        p->currentRole = rand_range(1, ROLE_POOL);
    }
}

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
 * selectHealthyPlayers - Select players for the winning team (teamB).
 * Composition with randomness: 70% chance of a "healthy" player (non-NEGATIVE),
 * 30% chance of a random player (may be NEGATIVE, for realism).
 * Guarantees a minimum of 2 healthy players in the final selection.
 *
 * healthyPool/healthyN : non-NEGATIVE players available (already shuffled)
 * anyPool/anyN         : other players (e.g. remaining tilt) for the 30% slot
 */
static int selectHealthyPlayers(Player **healthyPool, int healthyN,
                                  Player **anyPool, int anyN,
                                  Player **out, int maxOut) {
    int count = 0;
    int healthyCount = 0;
    int hIdx = 0;
    int aIdx = 0;

    for (int slot = 0; slot < maxOut; slot++) {
        /* Force a healthy player if the minimum of 2 has not yet been reached */
        int forceHealthy = (healthyCount < 2);
        int useRandom = (!forceHealthy && aIdx < anyN && rand_range(1, 100) <= 30);

        if (useRandom) {
            /* 30%: random player (may be NEGATIVE) */
            out[count++] = anyPool[aIdx++];
        } else if (hIdx < healthyN) {
            /* 70%: healthy player (non-NEGATIVE) */
            out[count++] = healthyPool[hIdx++];
            healthyCount++;
        } else if (aIdx < anyN) {
            /* No more healthy players available, fall back to any */
            out[count++] = anyPool[aIdx++];
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
    Player **anyPool    = (Player**)malloc(sizeof(Player*) * nPlayers);
    Player **remaining  = (Player**)malloc(sizeof(Player*) * nPlayers);

    if (!smurfPool || !tiltPool || !healthyPool || !anyPool || !remaining) {
        free(smurfPool); free(tiltPool); free(healthyPool); free(anyPool); free(remaining);
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
        /*
         * 70/30 composition: 70% healthy players (non-NEGATIVE), 30% random player.
         * Guarantees minimum 2 healthy players. anyPool contains remaining unassigned
         * tilt players for the 30% random slot (non-smurf players outside healthyPool).
         */
        int anyN = 0;
        for (int i = 0; i < nPlayers; i++) {
            if (!assigned[i] && !players[i].is_smurf &&
                players[i].hidden_mmr_state == HMR_NEGATIVE) {
                anyPool[anyN++] = &players[i];
            }
        }
        shufflePlayerPtrs(anyPool, anyN);

        Player *healthySelected[HEALTHY_TEAM_MIN];
        int healthySelectedN = selectHealthyPlayers(healthyPool, healthyN,
                                                     anyPool, anyN,
                                                     healthySelected, HEALTHY_TEAM_MIN);

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
    free(anyPool);
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
    if (p->totalGames < PLACEMENT_GAMES) return K_FACTOR_PLACEMENT;
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

        /* Per-team role availability (all 5 roles open at match start) */
        int availableRolesA[5] = {1, 1, 1, 1, 1};
        int availableRolesB[5] = {1, 1, 1, 1, 1};

        for (int i = 0; i < TEAM_SIZE; i++) {
            Player *pa = matches[m].teamA[i];
            Player *pb = matches[m].teamB[i];

            /* Off-schema probability depends on loseStreak (used for signals) */
            float pOffA = clampf(OFF_BASE + OFF_STEP_PER_LOSS * (float)pa->loseStreak, 0.0f, OFF_CAP);
            float pOffB = clampf(OFF_BASE + OFF_STEP_PER_LOSS * (float)pb->loseStreak, 0.0f, OFF_CAP);

            int offA = (rand01() < pOffA) ? 1 : 0;
            int offB = (rand01() < pOffB) ? 1 : 0;

            /* Role assignment using primary/secondary role preferences (per-team tracking) */
            assignRole(pa, availableRolesA);
            assignRole(pb, availableRolesB);

            /* Champion selection with role-pool logic (replaces pickChampion) */
            selectChampionWithPool(pa);
            selectChampionWithPool(pb);

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

            /* loseStreak / winStreak update with streak logging */
            if (aWon) {
                pa->loseStreak = 0;
                pa->winStreak++;
                if (pa->winStreak >= 3)
                    printf("[STREAK] %s winning streak: %d\n", pa->name, pa->winStreak);
            } else {
                pa->loseStreak++;
                pa->winStreak = 0;
                if (pa->loseStreak >= 3)
                    printf("[STREAK] %s losing streak: %d\n", pa->name, pa->loseStreak);
            }
            if (bWon) {
                pb->loseStreak = 0;
                pb->winStreak++;
                if (pb->winStreak >= 3)
                    printf("[STREAK] %s winning streak: %d\n", pb->name, pb->winStreak);
            } else {
                pb->loseStreak++;
                pb->winStreak = 0;
                if (pb->loseStreak >= 3)
                    printf("[STREAK] %s losing streak: %d\n", pb->name, pb->loseStreak);
            }

            /* Troll pick tilt feedback: once per game, not per factor computation */
            if (pa->isTrollPick) pa->tilt_level += TROLL_PICK_TILT_INCREASE;
            if (pb->isTrollPick) pb->tilt_level += TROLL_PICK_TILT_INCREASE;

            /* MMR update */
            apply_mmr_result(pa, aWon);
            apply_mmr_result(pb, bWon);

            /* Update visible tier/division from current MMR */
            pa->tier     = mmrToTier(pa->visibleMMR);
            pa->division = mmrToDivision(pa->visibleMMR, pa->tier);
            pb->tier     = mmrToTier(pb->visibleMMR);
            pb->division = mmrToDivision(pb->visibleMMR, pb->tier);

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
    p->winStreak  = 0;

    /* EOMM fields */
    p->is_smurf         = 0;
    p->skill_level      = SKILL_NORMAL;
    p->win_rate         = 0.50f;
    p->hidden_mmr_state = HMR_NEUTRAL;
    p->tilt_level       = 0;

    /* LoL tier / division (derived from starting MMR) */
    p->tier     = mmrToTier(p->visibleMMR);
    p->division = mmrToDivision(p->visibleMMR, p->tier);

    /* Role preferences: map latent prefRoles (1-indexed) to 0-indexed LoL roles */
    p->primaryRole   = p->prefRoles[0] - 1;   /* 1..5 => 0..4 */
    p->secondaryRole = p->prefRoles[1] - 1;

    /* Troll-pick state */
    p->isTrollPick       = 0;
    p->trollProbability  = BASE_TROLL_PROBABILITY;
    p->currentChampionId = 0;
    p->currentChampionRole = 0;

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

    /* Assign skill levels using a partial Fisher-Yates shuffle on an index array.
     * Distribution: 10% smurfs, 10% hardstuck, 80% normal. */
    {
        int idx[N_PLAYERS];
        for (int i = 0; i < N_PLAYERS; i++) idx[i] = i;

        /* Select N_SMURFS players */
        for (int i = 0; i < N_SMURFS; i++) {
            int j = i + rand() % (N_PLAYERS - i);
            int tmp = idx[i]; idx[i] = idx[j]; idx[j] = tmp;
            players[idx[i]].is_smurf    = 1;
            players[idx[i]].skill_level = SKILL_SMURF;
            players[idx[i]].win_rate    = 0.58f;
        }

        /* Select N_HARDSTUCK players from remaining */
        for (int i = N_SMURFS; i < N_SMURFS + N_HARDSTUCK; i++) {
            int j = i + rand() % (N_PLAYERS - i);
            int tmp = idx[i]; idx[i] = idx[j]; idx[j] = tmp;
            players[idx[i]].skill_level = SKILL_HARDSTUCK;
            players[idx[i]].win_rate    = 0.42f;
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
               "topRolesCount=%d topChampsCount=%d skill=%s tilt=%d\n",
               p->name, p->visibleMMR, calculateHiddenFactor(p), p->chatBaselineAvg, p->loseStreak,
               p->wins, p->losses, p->totalGames, p->topRolesCount, p->topChampionsCount,
               skillLabel(p->skill_level), p->tilt_level);
    }

    /* EOMM per-skill winrate summary */
    printf("\nSmurf winrate summary (%d smurfs):\n", N_SMURFS);
    for (int i = 0; i < N_PLAYERS; i++) {
        Player *p = &players[i];
        if (p->skill_level != SKILL_SMURF) continue;
        float wr = (p->totalGames > 0) ? (100.0f * (float)p->wins / (float)p->totalGames) : 0.0f;
        printf("  %s mmr=%.0f W=%d L=%d G=%d winrate=%.1f%%\n",
               p->name, p->visibleMMR, p->wins, p->losses, p->totalGames, wr);
    }

    printf("\nHardstuck winrate summary (%d hardstuck):\n", N_HARDSTUCK);
    for (int i = 0; i < N_PLAYERS; i++) {
        Player *p = &players[i];
        if (p->skill_level != SKILL_HARDSTUCK) continue;
        float wr = (p->totalGames > 0) ? (100.0f * (float)p->wins / (float)p->totalGames) : 0.0f;
        printf("  %s mmr=%.0f W=%d L=%d G=%d winrate=%.1f%%\n",
               p->name, p->visibleMMR, p->wins, p->losses, p->totalGames, wr);
    }

    return 0;
}
#endif
