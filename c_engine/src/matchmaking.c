#include "../include/matchmaking.h"
#include <math.h>
#include <string.h>

float calculate_skill_difference(Player p1, Player p2) {
    return fabs(p1.elo_rating - p2.elo_rating);
}

float calculate_compatibility(Player p1, Player p2) {
    float skill_diff = calculate_skill_difference(p1, p2);
    
    // Lower skill difference = higher compatibility
    float compatibility = 100.0f - (skill_diff / 10.0f);
    
    // Ensure compatibility is within bounds
    if (compatibility < 0.0f) compatibility = 0.0f;
    if (compatibility > 100.0f) compatibility = 100.0f;
    
    return compatibility;
}

Match* find_best_match(Player* players, int player_count, Player target) {
    if (players == NULL || player_count <= 0) {
        return NULL;
    }
    
    Match* best_match = NULL;
    float best_score = -1.0f;
    
    for (int i = 0; i < player_count; i++) {
        if (players[i].player_id == target.player_id) {
            continue;
        }
        
        float compatibility = calculate_compatibility(target, players[i]);
        
        if (compatibility > best_score) {
            best_score = compatibility;
            
            if (best_match == NULL) {
                best_match = (Match*)malloc(sizeof(Match));
            }
            
            best_match->player1 = target;
            best_match->player2 = players[i];
            best_match->compatibility_score = compatibility;
            best_match->skill_difference = calculate_skill_difference(target, players[i]);
        }
    }
    
    return best_match;
}