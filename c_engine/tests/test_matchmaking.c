#include <stdio.h>
#include <assert.h>
#include "../include/matchmaking.h"

void test_calculate_compatibility() {
    Player p1 = {1, 2000.0f, 1, 100, 50, 500.0f};
    Player p2 = {2, 2100.0f, 2, 95, 55, 450.0f};
    
    float score = calculate_compatibility(p1, p2);
    
    printf("Test: calculate_compatibility\n");
    printf("Player 1 ELO: %.2f, Player 2 ELO: %.2f\n", p1.elo_rating, p2.elo_rating);
    printf("Compatibility Score: %.2f\n", score);
    
    assert(score >= 0.0f && score <= 100.0f);
    printf("✓ Test passed!\n\n");
}

void test_skill_difference() {
    Player p1 = {1, 2000.0f, 1, 100, 50, 500.0f};
    Player p2 = {2, 1800.0f, 3, 80, 60, 400.0f};
    
    float diff = calculate_skill_difference(p1, p2);
    
    printf("Test: calculate_skill_difference\n");
    printf("Player 1 ELO: %.2f, Player 2 ELO: %.2f\n", p1.elo_rating, p2.elo_rating);
    printf("Skill Difference: %.2f\n", diff);
    
    assert(diff == 200.0f);
    printf("✓ Test passed!\n\n");
}

void test_find_best_match() {
    Player players[3] = {
        {1, 1900.0f, 1, 100, 50, 500.0f},
        {2, 2000.0f, 2, 95, 55, 450.0f},
        {3, 2150.0f, 3, 90, 60, 400.0f}
    };
    
    Player target = {0, 2000.0f, 1, 100, 50, 500.0f};
    
    Match* match = find_best_match(players, 3, target);
    
    printf("Test: find_best_match\n");
    printf("Target ELO: %.2f\n", target.elo_rating);
    
    if (match != NULL) {
        printf("Best Match Found:\n");
        printf("  Player: %d (ELO: %.2f)\n", match->player2.player_id, match->player2.elo_rating);
        printf("  Compatibility: %.2f%%\n", match->compatibility_score);
        printf("  Skill Difference: %.2f\n", match->skill_difference);
        
        assert(match->player2.player_id == 1);
        assert(match->compatibility_score > 0.0f);
        
        free(match);
        printf("✓ Test passed!\n\n");
    }
}

int main() {
    printf("=== EOMM Matchmaking Unit Tests ===\n\n");
    
    test_calculate_compatibility();
    test_skill_difference();
    test_find_best_match();
    
    printf("=== All tests passed! ===\n");
    return 0;
}