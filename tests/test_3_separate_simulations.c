#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/eomm_system.h"

#define NUM_ADVERSARIES 499
#define GAMES_PER_SHOWCASE 50
#define SHOWCASE_COUNT 3

typedef struct {
    int player_id;
    int opponent_encounters[NUM_ADVERSARIES];
    int games_played;
    int wins;
    int losses;
    float mmr_start;
    float mmr_end;
    int tilt_max;
    int autofill_count;
    int win_streak_max;
    int loss_streak_max;
    float hidden_factor_final;
} ShowcaseStats;

int main(void) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  EOMM TEST: 3 Separate Simulations (CORRECTED)             ║\n");
    printf("║  Each showcase plays 50 games vs 499 fresh adversaries     ║\n");
    printf("║  Distribution: 15%% Smurf | 50%% Normal | 35%% Hardstuck    ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    srand((unsigned int)time(NULL));
    
    const char* skill_names[] = {"SMURF", "NORMAL", "HARDSTUCK"};
    SkillLevel showcase_skills[SHOWCASE_COUNT] = {SKILL_SMURF, SKILL_NORMAL, SKILL_HARDSTUCK};
    ShowcaseStats stats[SHOWCASE_COUNT];
    
    // Run 3 separate simulations
    for (int sim = 0; sim < SHOWCASE_COUNT; sim++) {
        printf("\n");
        printf("════════════════════════════════════════════════════════════\n");
        printf("SIMULATION %d: %s SHOWCASE PLAYER\n", sim + 1, skill_names[sim]);
        printf("════════════════════════════════════════════════════════════\n\n");
        
        // ✅ CREATE FRESH SHOWCASE PLAYER FOR THIS SIMULATION
        Player showcase;
        init_player(&showcase, 0, showcase_skills[sim]);
        float showcase_mmr_start = showcase.visible_mmr;
        
        // ✅ CREATE 499 FRESH ADVERSARIES (NOT REUSED FROM PREVIOUS SIMS)
        Player *adversaries = (Player *)malloc(NUM_ADVERSARIES * sizeof(Player));
        if (adversaries == NULL) {
            fprintf(stderr, "Memory allocation failed!\n");
            return 1;
        }
        
        printf("Creating 499 FRESH adversaries with realistic distribution...\n");
        printf("  * ~75 Smurfs (15%%)\n");
        printf("  * ~250 Normal (50%%)\n");
        printf("  * ~175 Hardstuck (35%%)\n\n");
        
        int smurf_count = 0, normal_count = 0, hardstuck_count = 0;
        
        for (int i = 0; i < NUM_ADVERSARIES; i++) {
            SkillLevel skill;
            int rand_skill = rand() % 100;
            
            if (rand_skill < 15) {
                skill = SKILL_SMURF;
                smurf_count++;
            } else if (rand_skill < 65) {
                skill = SKILL_NORMAL;
                normal_count++;
            } else {
                skill = SKILL_HARDSTUCK;
                hardstuck_count++;
            }
            
            // ✅ INITIALIZE EACH ADVERSARY FRESHLY
            init_player(&adversaries[i], i + 1, skill);
        }
        
        printf("Created: %d Smurfs | %d Normal | %d Hardstuck\n\n", 
               smurf_count, normal_count, hardstuck_count);
        
        // ✅ INITIALIZE STATS FOR THIS SIMULATION
        stats[sim].player_id = 0;
        stats[sim].games_played = 0;
        stats[sim].wins = 0;
        stats[sim].losses = 0;
        stats[sim].mmr_start = showcase_mmr_start;
        stats[sim].tilt_max = 0;
        stats[sim].autofill_count = 0;
        stats[sim].win_streak_max = 0;
        stats[sim].loss_streak_max = 0;
        stats[sim].hidden_factor_final = 0.0f;
        memset(stats[sim].opponent_encounters, 0, sizeof(stats[sim].opponent_encounters));
        
        printf("Running 50 games for %s player...\n", skill_names[sim]);
        
        // ✅ PLAY 50 GAMES
        for (int game = 0; game < GAMES_PER_SHOWCASE; game++) {
            Match match;
            match.winner = -1;
            
            // ✅ SHOWCASE PLAYER IN TEAM A (position 0)
            match.team_a[0] = &showcase;
            
            // ✅ SELECT 4 UNIQUE TEAMMATES (no duplicates, no showcase player)
            int teammate_indices[4];
            int teammate_count = 0;
            
            while (teammate_count < 4) {
                int candidate = rand() % NUM_ADVERSARIES;
                int already_selected = 0;
                
                // Check if already selected for this game
                for (int i = 0; i < teammate_count; i++) {
                    if (teammate_indices[i] == candidate) {
                        already_selected = 1;
                        break;
                    }
                }
                
                if (!already_selected) {
                    teammate_indices[teammate_count] = candidate;
                    match.team_a[teammate_count + 1] = &adversaries[candidate];
                    teammate_count++;
                }
            }
            
            // ✅ SELECT 5 UNIQUE OPPONENTS (no duplicates, no teammates, no showcase)
            int opponent_indices[5];
            int opponent_count = 0;
            
            while (opponent_count < 5) {
                int candidate = rand() % NUM_ADVERSARIES;
                int already_used = 0;
                
                // Check if teammate
                for (int i = 0; i < 4; i++) {
                    if (teammate_indices[i] == candidate) {
                        already_used = 1;
                        break;
                    }
                }
                
                // Check if already opponent in this game
                for (int i = 0; i < opponent_count; i++) {
                    if (opponent_indices[i] == candidate) {
                        already_used = 1;
                        break;
                    }
                }
                
                if (!already_used) {
                    opponent_indices[opponent_count] = candidate;
                    match.team_b[opponent_count] = &adversaries[candidate];
                    
                    // ✅ TRACK OPPONENT ENCOUNTER
                    stats[sim].opponent_encounters[candidate]++;
                    
                    opponent_count++;
                }
            }
            
            // ✅ SIMULATE MATCH
            determine_troll_picks(&match);
            simulate_match(&match);
            update_players_after_match(&match);
            
            // ✅ UPDATE SHOWCASE STATS (ONLY SHOWCASE PERSISTS)
            stats[sim].games_played++;
            stats[sim].wins = showcase.wins;
            stats[sim].losses = showcase.losses;
            stats[sim].mmr_end = showcase.visible_mmr;
            stats[sim].tilt_max = (showcase.tilt_level > stats[sim].tilt_max) ? showcase.tilt_level : stats[sim].tilt_max;
            stats[sim].autofill_count = showcase.is_autofilled;
            stats[sim].win_streak_max = (showcase.win_streak > stats[sim].win_streak_max) ? showcase.win_streak : stats[sim].win_streak_max;
            stats[sim].loss_streak_max = (showcase.lose_streak > stats[sim].loss_streak_max) ? showcase.lose_streak : stats[sim].loss_streak_max;
            stats[sim].hidden_factor_final = showcase.hidden_factor;
        }
        
        printf("\n✅ Simulation %d complete!\n\n", sim + 1);
        
        // ✅ PRINT RESULTS FOR THIS SIMULATION
        float wr = (stats[sim].games_played > 0) ? (100.0f * (float)stats[sim].wins / (float)stats[sim].games_played) : 0.0f;
        float mmr_delta = stats[sim].mmr_end - stats[sim].mmr_start;
        
        int unique_opponents = 0;
        int faced_twice = 0;
        int faced_thrice = 0;
        int max_encounters = 0;
        
        for (int i = 0; i < NUM_ADVERSARIES; i++) {
            if (stats[sim].opponent_encounters[i] > 0) {
                unique_opponents++;
                if (stats[sim].opponent_encounters[i] >= 2) faced_twice++;
                if (stats[sim].opponent_encounters[i] >= 3) faced_thrice++;
                if (stats[sim].opponent_encounters[i] > max_encounters) {
                    max_encounters = stats[sim].opponent_encounters[i];
                }
            }
        }
        
        printf("RESULTS FOR %s PLAYER:\n", skill_names[sim]);
        printf("──────────────────────────────────────────────────────────\n");
        printf("Performance Stats:\n");
        printf("  * Games Played: %d\n", stats[sim].games_played);
        printf("  * Win Rate: %.1f%% (%dW-%dL)\n", wr, stats[sim].wins, stats[sim].losses);
        printf("  * MMR Progression: %.0f -> %.0f (delta: %+.0f)\n", 
               stats[sim].mmr_start, stats[sim].mmr_end, mmr_delta);
        printf("  * Hidden Factor: %.3f\n", stats[sim].hidden_factor_final);
        printf("  * Tilt Level: %d (max reached: %d)\n", showcase.tilt_level, stats[sim].tilt_max);
        printf("  * Autofilled: %d times (%.1f%%)\n", stats[sim].autofill_count,
               (100.0f * (float)stats[sim].autofill_count / (float)stats[sim].games_played));
        printf("\n");
        
        printf("Streak Stats:\n");
        printf("  * Current Win Streak: %d (max: %d)\n", showcase.win_streak, stats[sim].win_streak_max);
        printf("  * Current Loss Streak: %d (max: %d)\n", showcase.lose_streak, stats[sim].loss_streak_max);
        printf("\n");
        
        printf("Opponent Diversity:\n");
        printf("  * Unique opponents faced: %d / 499\n", unique_opponents);
        printf("  * Faced 2+ times: %d opponents\n", faced_twice);
        printf("  * Faced 3+ times: %d opponents\n", faced_thrice);
        printf("  * Max times vs same opponent: %d\n", max_encounters);
        printf("\n");
        
        // ✅ FREE ADVERSARIES MEMORY FOR THIS SIMULATION
        free(adversaries);
    }
    
    // ✅ FINAL COMPARISON
    printf("\n");
    printf("════════════════════════════════════════════════════════════\n");
    printf("FINAL COMPARISON: All 3 Showcase Players\n");
    printf("════════════════════════════════════════════════════════════\n\n");
    
    for (int i = 0; i < SHOWCASE_COUNT; i++) {
        float wr = (100.0f * (float)stats[i].wins / (float)stats[i].games_played);
        float mmr_delta = stats[i].mmr_end - stats[i].mmr_start;
        
        printf("%s Player:\n", skill_names[i]);
        printf("  * Win Rate: %.1f%%\n", wr);
        printf("  * MMR Delta: %+.0f\n", mmr_delta);
        printf("  * Hidden Factor: %.3f\n", stats[i].hidden_factor_final);
        printf("  * Max Tilt: %d\n", stats[i].tilt_max);
        printf("  * Max Win Streak: %d\n", stats[i].win_streak_max);
        printf("  * Max Loss Streak: %d\n\n", stats[i].loss_streak_max);
    }
    
    // ✅ CALCULATE DIFFERENCES
    float smurf_wr = (100.0f * (float)stats[0].wins / (float)stats[0].games_played);
    float normal_wr = (100.0f * (float)stats[1].wins / (float)stats[1].games_played);
    float hardstuck_wr = (100.0f * (float)stats[2].wins / (float)stats[2].games_played);
    
    printf("Win Rate Gaps:\n");
    printf("  * Smurf vs Normal: %+.1f%%\n", smurf_wr - normal_wr);
    printf("  * Smurf vs Hardstuck: %+.1f%%\n", smurf_wr - hardstuck_wr);
    printf("  * Normal vs Hardstuck: %+.1f%%\n", normal_wr - hardstuck_wr);
    printf("\n");
    
    printf("════════════════════════════════════════════════════════════\n");
    printf("MMR Deltas:\n");
    printf("  * Smurf: %+.0f\n", stats[0].mmr_end - stats[0].mmr_start);
    printf("  * Normal: %+.0f\n", stats[1].mmr_end - stats[1].mmr_start);
    printf("  * Hardstuck: %+.0f\n", stats[2].mmr_end - stats[2].mmr_start);
    printf("════════════════════════════════════════════════════════════\n\n");
    
    printf("═══════════════════════════════════════════════════════════\n");
    printf("✅ All simulations complete - EOMM validation done!\n");
    printf("═══════════════════════════════════════════════════════════\n");
    
    return 0;
}