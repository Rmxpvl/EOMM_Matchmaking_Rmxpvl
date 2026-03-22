// Full EOMM Simulation for 100 Players Over 200 Games

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_PLAYERS 100
#define NUM_GAMES 200

typedef struct {
    int mmr;       // Matchmaking Rating
    int wins;
    int losses;
} Player;

Player players[NUM_PLAYERS];

void initialize_players() {
    for (int i = 0; i < NUM_PLAYERS; i++) {
        players[i].mmr = 1500;  // Starting MMR
        players[i].wins = 0;
        players[i].losses = 0;
    }
}

void simulate_game(int player1, int player2) {
    // Randomly decide the winner
    int winner = rand() % 2;
    if (winner == 0) {
        players[player1].wins++;
        players[player2].losses++;
    } else {
        players[player1].losses++;
        players[player2].wins++;
    }
    
    // Update MMR based on results
    int mmr_change = 10;  // MMR change per game
    if (winner == 0) {
        players[player1].mmr += mmr_change;
        players[player2].mmr -= mmr_change;
    } else {
        players[player1].mmr -= mmr_change;
        players[player2].mmr += mmr_change;
    }
}

void matchmaking() {
    srand(time(0));  // Seed random number generator
    for (int game = 0; game < NUM_GAMES; game++) {
        int player1 = rand() % NUM_PLAYERS;
        int player2;
        do {
            player2 = rand() % NUM_PLAYERS;
        } while(player1 == player2);  // Ensure players are not the same

        simulate_game(player1, player2);
    }
}

void print_results() {
    for (int i = 0; i < NUM_PLAYERS; i++) {
        printf("Player %d: MMR=%d, Wins=%d, Losses=%d\n", i, players[i].mmr, players[i].wins, players[i].losses);
    }
}

int main() {
    initialize_players();
    matchmaking();
    print_results();
    return 0;
}