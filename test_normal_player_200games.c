// test_normal_player_200games.c

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Function to simulate a normal player playing games with EOMM mechanics
void simulate_normal_player(int games) {
    int wins = 0;
    int losses = 0;

    // Seed the random number generator
    srand(time(NULL));

    for (int i = 0; i < games; i++) {
        // Simulate win or loss with a 50% chance
        if (rand() % 2 == 0) {
            wins++;
        } else {
            losses++;
        }
    }

    printf("Total Games: %d\n", games);
    printf("Wins: %d\n", wins);
    printf("Losses: %d\n", losses);
}

int main() {
    int total_games = 200;
    simulate_normal_player(total_games);
    return 0;
}