/* ============================================================
 * MINIMAL TEST: 1 Player per Skill Category + 4 Pools
 * 
 * Simple focused test:
 * - 5 players total (one per skill category)
 * - Track 4 pools: 50, 100, 200, 300 games
 * - No randomness in player selection (always same 5)
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#define N_PLAYERS 100           /* Small pool to ensure constant matchmaking with our 5 */
#define MAX_GAMES_PER_PLAYER 300
#define NUM_TRACKED 5

typedef enum {
    SKILL_LOW_EXTREME = 0,
    SKILL_LOW_VERY_BAD = 1,
    SKILL_NORMAL = 2,
    SKILL_SMURF_MED = 3,
    SKILL_SMURF_HIGH = 4,
} SkillLevel;

typedef struct {
    int id;
    SkillLevel skill_level;
    float mmr_raw;
    int total_games;
    int wins;
    int outcomes[MAX_GAMES_PER_PLAYER];
    float mmr_timeline[MAX_GAMES_PER_PLAYER + 1];
    int actual_game_count;
} Player;

static float randf(void) {
    return (float)rand() / (float)RAND_MAX;
}

float calculate_expected(float mmr_a, float mmr_b) {
    float diff = (mmr_a - mmr_b) / 400.0f;
    return 1.0f / (1.0f + powf(10.0f, -diff));
}

float get_dynamic_K(int games_played) {
    if (games_played < 100)  return 40.0f;
    if (games_played < 500)  return 20.0f;
    return 10.0f;
}

void update_mmr(Player *p, float opponent_mmr, int did_win) {
    float expected = calculate_expected(p->mmr_raw, opponent_mmr);
    if (expected > 0.98f) expected = 0.98f;
    if (expected < 0.02f) expected = 0.02f;
    
    float outcome = did_win ? 1.0f : 0.0f;
    float K = get_dynamic_K(p->total_games);
    float delta = K * (outcome - expected);
    p->mmr_raw += delta;
    if (p->mmr_raw < 0.0f) p->mmr_raw = 0.0f;
}

void init_player(Player *p, int id, SkillLevel skill) {
    p->id = id;
    p->skill_level = skill;
    p->total_games = 0;
    p->wins = 0;
    p->actual_game_count = 0;
    
    switch (skill) {
        case SKILL_LOW_EXTREME:
            p->mmr_raw = 200.0f;
            break;
        case SKILL_LOW_VERY_BAD:
            p->mmr_raw = 550.0f;
            break;
        case SKILL_NORMAL:
            p->mmr_raw = 950.0f;
            break;
        case SKILL_SMURF_MED:
            p->mmr_raw = 1200.0f;
            break;
        case SKILL_SMURF_HIGH:
            p->mmr_raw = 1450.0f;
            break;
    }
    
    p->mmr_timeline[0] = p->mmr_raw;
    memset(p->outcomes, 0, sizeof(p->outcomes));
}

void init_players_filler(Player *players) {
    /* Initialize filler players for matchmaking (not tracked) */
    for (int i = NUM_TRACKED; i < N_PLAYERS; i++) {
        SkillLevel skill = (SkillLevel)(i % 5);
        init_player(&players[i], i, skill);
    }
}

const char* skill_label(SkillLevel skill) {
    switch (skill) {
        case SKILL_LOW_EXTREME:    return "💔💔💔 LOW_EXTREME";
        case SKILL_LOW_VERY_BAD:   return "💔💔 LOW_VERY_BAD";
        case SKILL_NORMAL:         return "📊 NORMAL";
        case SKILL_SMURF_MED:      return "🔥🔥 SMURF_MED";
        case SKILL_SMURF_HIGH:     return "🔥🔥🔥 SMURF_HIGH";
        default:                   return "?";
    }
}

void calculate_pool_stats(Player *p, int pool_size, float *wr_out, float *mmr_start_out, float *mmr_end_out) {
    if (pool_size > p->actual_game_count) {
        pool_size = p->actual_game_count;
    }
    if (pool_size <= 0) {
        *wr_out = 0.0f;
        *mmr_start_out = p->mmr_timeline[0];
        *mmr_end_out = p->mmr_raw;
        return;
    }
    
    int wins = 0;
    for (int i = 0; i < pool_size; i++) {
        wins += p->outcomes[i];
    }
    
    *wr_out = 100.0f * wins / (float)pool_size;
    *mmr_start_out = p->mmr_timeline[0];
    *mmr_end_out = p->mmr_timeline[pool_size];
}

int main(void) {
    srand(42);
    
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  1 PLAYER PER CATEGORY + 4 POOLS TEST                      ║\n");
    printf("║  Categories: LOW_EXTREME, LOW_VERY_BAD, NORMAL,            ║\n");
    printf("║              SMURF_MED, SMURF_HIGH                          ║\n");
    printf("║  Pools:      50, 100, 200, 300 games                       ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    Player *players = malloc(sizeof(Player) * N_PLAYERS);
    
    /* Initialize our 5 tracked players (one per category) */
    init_player(&players[0], 0, SKILL_LOW_EXTREME);
    init_player(&players[1], 1, SKILL_LOW_VERY_BAD);
    init_player(&players[2], 2, SKILL_NORMAL);
    init_player(&players[3], 3, SKILL_SMURF_MED);
    init_player(&players[4], 4, SKILL_SMURF_HIGH);
    
    /* Initialize filler players for matchmaking */
    init_players_filler(players);
    
    printf("Running simulation: 300+ games per player...\n");
    printf("This may take a moment...\n\n");
    
    /* Simulation */
    int round = 0;
    while (true) {
        int players_needing_games = 0;
        for (int i = 0; i < N_PLAYERS; i++) {
            if (players[i].total_games < MAX_GAMES_PER_PLAYER) {
                players_needing_games++;
            }
        }
        if (players_needing_games == 0) break;
        
        /* Shuffle players */
        int indices[N_PLAYERS];
        for (int i = 0; i < N_PLAYERS; i++) indices[i] = i;
        for (int i = N_PLAYERS - 1; i > 0; --i) {
            int j = rand() % (i + 1);
            int tmp = indices[i];
            indices[i] = indices[j];
            indices[j] = tmp;
        }
        
        /* Create matches */
        int matches = N_PLAYERS / 10;
        for (int m = 0; m < matches; m++) {
            Player *team_a[5];
            Player *team_b[5];
            
            for (int i = 0; i < 5; i++) {
                team_a[i] = &players[indices[m * 10 + i]];
                team_b[i] = &players[indices[m * 10 + 5 + i]];
            }
            
            /* Calculate team MMRs */
            float mmr_a = 0.0f, mmr_b = 0.0f;
            for (int i = 0; i < 5; i++) {
                mmr_a += team_a[i]->mmr_raw;
                mmr_b += team_b[i]->mmr_raw;
            }
            mmr_a /= 5.0f;
            mmr_b /= 5.0f;
            
            /* Determine winner */
            float win_prob_a = calculate_expected(mmr_a, mmr_b);
            float random_variance = (mmr_a < mmr_b) ? 0.18f : (mmr_a > mmr_b) ? 0.08f : 0.12f;
            win_prob_a += (randf() - 0.5f) * 2.0f * random_variance;
            win_prob_a = (win_prob_a < 0.0f) ? 0.0f : (win_prob_a > 1.0f) ? 1.0f : win_prob_a;
            
            int winner = (randf() < win_prob_a) ? 0 : 1;
            
            /* Update players */
            for (int i = 0; i < 5; i++) {
                Player *pa = team_a[i];
                Player *pb = team_b[i];
                
                if (pa->actual_game_count >= MAX_GAMES_PER_PLAYER || 
                    pb->actual_game_count >= MAX_GAMES_PER_PLAYER) {
                    continue;
                }
                
                int a_won = (winner == 0);
                int b_won = (winner == 1);
                
                pa->outcomes[pa->actual_game_count] = a_won ? 1 : 0;
                pb->outcomes[pb->actual_game_count] = b_won ? 1 : 0;
                
                pa->total_games++;
                pb->total_games++;
                if (a_won) { pa->wins++; pb->wins = pb->total_games - pb->wins - 1; }
                else       { pb->wins++; pa->wins = pa->total_games - pa->wins - 1; }
                
                update_mmr(pa, mmr_b, a_won);
                update_mmr(pb, mmr_a, b_won);
                
                pa->mmr_timeline[pa->actual_game_count + 1] = pa->mmr_raw;
                pb->mmr_timeline[pb->actual_game_count + 1] = pb->mmr_raw;
                
                pa->actual_game_count++;
                pb->actual_game_count++;
            }
        }
        
        round++;
        if (round % 50 == 0) {
            printf("  ✓ Round %d complete (%d players still playing)\n", round, players_needing_games);
        }
    }
    
    printf("\n✅ Simulation complete\n\n");
    
    /* ═════════════════════════════════════════════════════════════ */
    /* RESULTS: 5 TRACKED PLAYERS WITH POOL STATS */
    /* ═════════════════════════════════════════════════════════════ */
    
    printf("═════════════════════════════════════════════════════════════\n");
    printf("📊 RESULTS: 1 PLAYER PER CATEGORY - 4 POOL ANALYSIS\n");
    printf("═════════════════════════════════════════════════════════════\n\n");
    
    int pool_sizes[] = {50, 100, 200, 300};
    
    for (int t = 0; t < NUM_TRACKED; t++) {
        Player *p = &players[t];
        
        printf("╔═ %s (ID: #%d) ═╗\n", skill_label(p->skill_level), p->id);
        printf("├─ TOTAL SEASON: %d games | %d wins (%.1f%%) | MMR: %.0f → %.0f [%+.0f]\n",
               p->actual_game_count,
               p->wins,
               (p->actual_game_count > 0) ? (100.0f * p->wins / (float)p->actual_game_count) : 0.0f,
               p->mmr_timeline[0],
               p->mmr_raw,
               p->mmr_raw - p->mmr_timeline[0]);
        printf("│\n");
        
        for (int i = 0; i < 4; i++) {
            float wr, mmr_start, mmr_end;
            calculate_pool_stats(p, pool_sizes[i], &wr, &mmr_start, &mmr_end);
            
            if (pool_sizes[i] <= p->actual_game_count) {
                printf("│ POOL %d (%d games): %.1f%% WR | MMR: %.0f → %.0f [%+.0f]\n",
                       i + 1, pool_sizes[i],
                       wr,
                       mmr_start,
                       mmr_end,
                       mmr_end - mmr_start);
            }
        }
        
        printf("└─\n\n");
    }
    
    free(players);
    return 0;
}
