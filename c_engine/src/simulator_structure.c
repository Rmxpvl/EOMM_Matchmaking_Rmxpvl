/*
 * simulator_structure.c
 *
 * Simulateur EOMM (prototype) - pool 300 joueurs, parties uniquement 5v5.
 * HiddenMMR factor basé sur rôle/champion + signaux comportementaux:
 * - pings/emotes non textuels in-game (?, !, etc.)
 * - utilisation du chat écrit (nb messages) comparée à une baseline par joueur (EMA)
 * - deaths
 * - click rate
 *
 * Notes:
 * - Historique: on garde 50 matchs, et on calcule les moyennes sur les 10 derniers (ou moins si <10).
 * - Option A (EMA): la baseline chat est mise à jour en continu.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* =========================
 * Paramètres globaux
 * ========================= */
#ifndef N_PLAYERS
#define N_PLAYERS 300
#endif

#define TEAM_SIZE 5
#define MATCH_SIZE (TEAM_SIZE * 2)

#define HISTORY_MAX 50
#define WINDOW_RECENT 10

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

/* Option A: baseline chat EMA */
#define CHAT_BASELINE_EMA_ALPHA 0.10f

/* =========================
 * Structures
 * ========================= */
typedef struct {
    char name[16];            /* Player001.. */
    float visibleMMR;         /* MMR affiché */
    float hiddenMMR;          /* MMR caché (peut être utilisé plus tard) */
    float neutralMMR;         /* reset complet */
    time_t lastMatchTime;     /* reset IRL */

    int wins;
    int losses;
    int totalGames;

    int currentRole;
    int topRoles[2];

    char currentChampion[16];
    char topChampions[3][16];

    int ingamePingCountHistory[HISTORY_MAX]; /* pings/emotes non textuels */
    int chatUsageHistory[HISTORY_MAX];       /* nb messages écrits */
    int deathHistory[HISTORY_MAX];
    int clickRateHistory[HISTORY_MAX];
    int historyCount;

    /* Baseline per-player (Option A): EMA du nb de messages */
    float chatBaselineAvg;
    int   chatBaselineInit; /* 0 tant que pas initialisé */
} Player;

typedef struct {
    Player *teamA[TEAM_SIZE];
    Player *teamB[TEAM_SIZE];
    float sumEffectiveA;
    float sumEffectiveB;
    int winner; /* 0: A, 1: B */
} Match;

/* =========================
 * Utilitaires
 * ========================= */
static int min_int(int a, int b) { return (a < b) ? a : b; }

static void shufflePlayers(Player *players, int n) {
    for (int i = n - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        Player tmp = players[i];
        players[i] = players[j];
        players[j] = tmp;
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
 * Hidden factor / Effective MMR
 * ========================= */
static float calculateHiddenFactor(Player *p) {
    float factor = 1.0f;

    /* Rôle: top 2 */
    int roleOK = 0;
    for (int i = 0; i < 2; i++) {
        if (p->currentRole == p->topRoles[i]) { roleOK = 1; break; }
    }
    if (!roleOK) factor -= PENALTY_ROLE_NOT_TOP2;

    /* Champion: top 3 */
    int champOK = 0;
    for (int i = 0; i < 3; i++) {
        if (strcmp(p->currentChampion, p->topChampions[i]) == 0) { champOK = 1; break; }
    }
    if (!champOK) factor -= PENALTY_CHAMP_NOT_TOP3;

    /* Comparaison au AVG des X derniers */
    int start, count;
    recent_window(p, &start, &count);
    if (count > 0) {
        int last = p->historyCount - 1;

        /* In-game pings/emotes */
        {
            int sum = 0;
            for (int i = start; i < start + count; i++) sum += p->ingamePingCountHistory[i];
            float avg = (float)sum / (float)count;
            if ((float)p->ingamePingCountHistory[last] > avg) factor -= PENALTY_INGAME_PINGS_ABOVE_AVG;
        }

        /* Chat écrit: comparaison à la baseline EMA (Option A) */
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
 * Reset hidden MMR
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
 * Historique + Baseline EMA
 * ========================= */
static void updateChatBaselineEMA(Player *p, int chatUsage) {
    float x = (float)chatUsage;
    if (!p->chatBaselineInit) {
        p->chatBaselineAvg = x;
        p->chatBaselineInit = 1;
        return;
    }

    /* EMA: baseline = (1-a)*baseline + a*x */
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

    /* Update baseline chat (Option A) */
    updateChatBaselineEMA(p, chatUsage);
}

/* =========================
 * Team placement
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

static int cmp_effective_desc(const void *a, const void *b) {
    Player *pa = *(Player**)a;
    Player *pb = *(Player**)b;
    float ea = effectiveMMR(pa);
    float eb = effectiveMMR(pb);
    if (ea < eb) return 1;
    if (ea > eb) return -1;
    return 0;
}

static void createMatchAdvanced(Player *players, int nPlayers, Match *matches, int *numMatches) {
    int nm = nPlayers / MATCH_SIZE;
    *numMatches = nm;

    Player **sorted = (Player**)malloc(sizeof(Player*) * nPlayers);
    for (int i = 0; i < nPlayers; i++) sorted[i] = &players[i];
    qsort(sorted, nPlayers, sizeof(Player*), cmp_effective_desc);

    for (int m = 0; m < nm; m++) {
        matches[m].sumEffectiveA = 0.0f;
        matches[m].sumEffectiveB = 0.0f;
        matches[m].winner = -1;

        for (int i = 0; i < TEAM_SIZE; i++) { matches[m].teamA[i] = NULL; matches[m].teamB[i] = NULL; }

        int aCount = 0, bCount = 0;
        int base = m * MATCH_SIZE;

        /* Snake: A B B A ... */
        for (int k = 0; k < MATCH_SIZE; k++) {
            Player *p = sorted[base + k];
            int mod = k % 4;
            int pickToA = (mod == 0 || mod == 3);

            if (pickToA) {
                if (aCount < TEAM_SIZE) matches[m].teamA[aCount++] = p;
                else matches[m].teamB[bCount++] = p;
            } else {
                if (bCount < TEAM_SIZE) matches[m].teamB[bCount++] = p;
                else matches[m].teamA[aCount++] = p;
            }
        }
    }

    free(sorted);
}

/* =========================
 * Simulation
 * ========================= */
static int rand_range(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (rand() % (hi - lo + 1));
}

static void simulateGame(Match *matches, int numMatches) {
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

            /* Signaux simulés (ranges à calibrer) */
            int ingamePingsA = rand_range(0, 40);
            int chatA        = rand_range(0, 25);
            int deathsA      = rand_range(0, 20);
            int clickA       = rand_range(50, 400);

            int ingamePingsB = rand_range(0, 40);
            int chatB        = rand_range(0, 25);
            int deathsB      = rand_range(0, 20);
            int clickB       = rand_range(50, 400);

            pushHistory(pa, ingamePingsA, chatA, deathsA, clickA);
            pushHistory(pb, ingamePingsB, chatB, deathsB, clickB);

            pa->totalGames++;
            pb->totalGames++;

            if (winner == 0) { pa->wins++; pb->losses++; }
            else { pb->wins++; pa->losses++; }

            time_t now = time(NULL);
            pa->lastMatchTime = now;
            pb->lastMatchTime = now;

            softResetHiddenMMR(pa);
            softResetHiddenMMR(pb);
        }
    }
}

/* =========================
 * Init joueurs
 * ========================= */
static void initPlayer(Player *p, int idx) {
    snprintf(p->name, sizeof(p->name), "Player%03d", idx + 1);

    p->visibleMMR = (float)rand_range(800, 2400);
    p->neutralMMR = p->visibleMMR;
    p->hiddenMMR = p->visibleMMR;

    p->lastMatchTime = time(NULL);

    p->wins = 0;
    p->losses = 0;
    p->totalGames = 0;

    p->topRoles[0] = rand_range(1, 5);
    p->topRoles[1] = rand_range(1, 5);
    p->currentRole = rand_range(1, 5);

    snprintf(p->topChampions[0], sizeof(p->topChampions[0]), "Champ%02d", rand_range(1, 20));
    snprintf(p->topChampions[1], sizeof(p->topChampions[1]), "Champ%02d", rand_range(1, 20));
    snprintf(p->topChampions[2], sizeof(p->topChampions[2]), "Champ%02d", rand_range(1, 20));
    snprintf(p->currentChampion, sizeof(p->currentChampion), "Champ%02d", rand_range(1, 20));

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

    Match matches[N_PLAYERS / MATCH_SIZE];
    int numMatches = 0;

    int NUM_GAMES = 50;

    placeInitialTeams(players, N_PLAYERS, matches, &numMatches);

    for (int g = 0; g < NUM_GAMES; g++) {
        for (int i = 0; i < N_PLAYERS; i++) resetHiddenMMR(&players[i]);

        if (g >= 5) createMatchAdvanced(players, N_PLAYERS, matches, &numMatches);
        else placeInitialTeams(players, N_PLAYERS, matches, &numMatches);

        simulateGame(matches, numMatches);
    }

    printf("Simulation finished. Sample players:\n");
    for (int i = 0; i < 5; i++) {
        Player *p = &players[i];
        printf("%s visible=%.0f hidden=%.0f factor=%.2f chatBaseline=%.2f W=%d L=%d G=%d\n",
               p->name, p->visibleMMR, p->hiddenMMR, calculateHiddenFactor(p), p->chatBaselineAvg,
               p->wins, p->losses, p->totalGames);
    }

    return 0;
}
#endif
