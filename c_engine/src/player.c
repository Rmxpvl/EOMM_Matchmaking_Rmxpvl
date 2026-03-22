// player.c

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct {
    int id;
    char name[50];
    int level;
    int experience;
} Player;

// Function to initialize a player
Player* initialize_player(int id, const char* name) {
    Player *new_player = (Player *)malloc(sizeof(Player));
    new_player->id = id;
    snprintf(new_player->name, sizeof(new_player->name), "%s", name);
    new_player->level = 1;
    new_player->experience = 0;
    return new_player;
}

// Function to update player experience and level
void update_player_stats(Player *player, int experience_gain) {
    player->experience += experience_gain;
    // Level up logic
    if (player->experience >= 100) { // Example level-up threshold
        player->level++;
        player->experience = 0; // Reset experience after leveling up
    }
}

// Utility function to display player information
void display_player_info(Player *player) {
    printf("Player ID: %d\n", player->id);
    printf("Name: %s\n", player->name);
    printf("Level: %d\n", player->level);
    printf("Experience: %d\n", player->experience);
}

// Free player memory
void free_player(Player *player) {
    free(player);
}