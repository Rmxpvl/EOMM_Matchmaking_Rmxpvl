# Chat Baseline EMA Configuration
#define CHAT_BASELINE_EMA_ALPHA 0.10f

// Existing struct for Player, assuming Player is defined in the same file.

typedef struct {
    // Existing members...
    float chatBaselineAvg; // New member for chat baseline average
} Player;

// Function to initialize player
void initPlayer(Player *player) {
    // Existing initialization...
    player->chatBaselineAvg = 0.0f; // Initialize chat baseline average
}

// Function to record chat usage
void pushHistory(Player *player, float chatUsage) {
    // Existing logic to record chat usage...
    if (player->chatBaselineAvg == 0.0f) {
        player->chatBaselineAvg = chatUsage; // Set chatBaselineAvg to first value if baseline is 0
    } else {
        player->chatBaselineAvg += (CHAT_BASELINE_EMA_ALPHA * (chatUsage - player->chatBaselineAvg)); // Update using EMA
    }
}

// Existing calculateHiddenFactor function update...
float calculateHiddenFactor(const Player *player) {
    // Logic to calculate hidden factor...
    float chatPenalty = lastChatUsageHistory - player->chatBaselineAvg; // Compare against chatBaselineAvg
    // Keep all other behavior unchanged
    // ...
}