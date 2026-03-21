#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
    int visible_mmr;
    float win_rate;
    int wins;
    int losses;
} SimplePlayer;

float randf(void) {
    return (float)rand() / (float)RAND_MAX;
}

int main() {
    srand(time(NULL));
    
    SimplePlayer p;
    p.visible_mmr = 1000;
    p.win_rate = 0.55f;  // FORCE 55%
    p.wins = 0;
    p.losses = 0;
    
    printf("=== TEST: Joueur 55%% WR sur 200 games ===\n\n");
    printf("Initial MMR: %d\n", p.visible_mmr);
    printf("Expected WR: 55%%\n\n");
    
    /* Simulation simple sans EOMM */
    for (int i = 1; i <= 200; i++) {
        int did_win = (randf() < p.win_rate);
        
        if (did_win) {
            p.visible_mmr += 28;  /* K=28 */
            p.wins++;
        } else {
            p.visible_mmr -= 28;
            p.losses++;
        }
    }
    
    float actual_wr = (float)p.wins / 200.0f * 100.0f;
    int mmr_change = p.visible_mmr - 1000;
    int expected_mmr = 1000 + (28 * (p.wins - p.losses));
    
    printf("Results:\n");
    printf("─────────────────────────────────────────\n");
    printf("Wins:           %d\n", p.wins);
    printf("Losses:         %d\n", p.losses);
    printf("Actual WR:      %.1f%%\n", actual_wr);
    printf("─────────────────────────────────────────\n");
    printf("Final MMR:      %d\n", p.visible_mmr);
    printf("MMR Change:     %+d\n", mmr_change);
    printf("Expected MMR:   %d\n", expected_mmr);
    printf("─────────────────────────────────────────\n");
    
    /* Analysis */
    if (p.wins > p.losses) {
        printf("\n✅ CORRECT: 55%% WR joueur a GAGNÉ du MMR\n");
        printf("   %d wins - %d losses = +%d (×28 = %+d MMR)\n", 
               p.wins, p.losses, p.wins - p.losses, 28 * (p.wins - p.losses));
    } else {
        printf("\n❌ ERROR: 55%% WR joueur a PERDU du MMR!\n");
        printf("   Ceci serait un BUG CRITIQUE!\n");
    }
    
    return 0;
}