// HiddenMMR calculation implementation

#include <stdio.h>
#include <math.h>

#define BASE_MMR 1500
#define CHAT_TOXICITY_PENALTY 0.3
#define PING_SPAM_FACTOR 0.1
#define TILT_FACTOR 0.2

typedef struct {
    float chat_toxicity;
    int ping_spam_count;
    int tilt_level;
    // Add other behavioral factors as needed
} PlayerBehavior;

float calculate_penalty(PlayerBehavior behavior) {
    float penalty = 0.0;

    // Calculate penalty for chat toxicity
    penalty += behavior.chat_toxicity * CHAT_TOXICITY_PENALTY;

    // Calculate penalty for ping spam
    penalty += behavior.ping_spam_count * PING_SPAM_FACTOR;

    // Calculate penalty for tilt
    penalty += behavior.tilt_level * TILT_FACTOR;

    return penalty;
}

float calculate_hidden_mmr(PlayerBehavior behavior) {
    float penalty = calculate_penalty(behavior);
    float hidden_mmr = BASE_MMR - penalty;
    
    // Ensure hidden MMR does not drop below 0
    if (hidden_mmr < 0) {
        hidden_mmr = 0;
    }

    return hidden_mmr;
}

int main() {
    PlayerBehavior player = {0.1, 5, 3}; // Sample values for testing
    float hidden_mmr = calculate_hidden_mmr(player);

    printf("Calculated Hidden MMR: %.2f\n", hidden_mmr);
    return 0;
}