#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_PLAYERS 100
#define NUM_GAMES 200

typedef struct {
    int id;
    int skillLevel;
    int wins;
    int losses;
} Player;

void initializePlayers(Player players[]) {
    for (int i = 0; i < NUM_PLAYERS; i++) {
        players[i].id = i;
        players[i].skillLevel = rand() % 100; // Random skill level between 0 and 99
        players[i].wins = 0;
        players[i].losses = 0;
    }
}

void simulateGame(Player *player1, Player *player2) {
    int result = rand() % (player1->skillLevel + player2->skillLevel);
    if (result < player1->skillLevel) {
        player1->wins++;
        player2->losses++;
    } else {
        player1->losses++;
        player2->wins++;
    }
}

void simulateEOMM() {
    Player players[NUM_PLAYERS];
    initializePlayers(players);

    for (int i = 0; i < NUM_GAMES; i++) {
        int player1Index = rand() % NUM_PLAYERS;
        int player2Index;
        do {
            player2Index = rand() % NUM_PLAYERS;
        } while (player2Index == player1Index); // Ensure different players
        
        simulateGame(&players[player1Index], &players[player2Index]);
    }

    // Output results
    for (int i = 0; i < NUM_PLAYERS; i++) {
        printf("Player %d: Wins = %d, Losses = %d\n", players[i].id, players[i].wins, players[i].losses);
    }
}

int main() {
    srand(time(NULL)); // Seed for random number generation
    simulateEOMM();
    return 0;
}