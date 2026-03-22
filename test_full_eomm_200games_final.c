#include <stdio.h>
#include "eomm_system.c"

int main() {
    int players[100];
    // Assign player types: 10 smurfs, 10 hardstuck, 80 normal
    for (int i = 0; i < 10; i++) players[i] = SMURF;
    for (int i = 10; i < 20; i++) players[i] = HARDSTUCK;
    for (int i = 20; i < 100; i++) players[i] = NORMAL;

    for (int game = 0; game < 200; game++) {
        simulate_game(players);
    }

    printf("Simulation of 200 games complete.\n");
    return 0;
}