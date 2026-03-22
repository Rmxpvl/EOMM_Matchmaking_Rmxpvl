#ifndef MATCHMAKING_H
#define MATCHMAKING_H

#include <stdio.h>
#include <stdlib.h>

#include "lol_tiers.h"

typedef struct {
    int player_id;
    float elo_rating;
    int rank;
    int wins;
    int losses;
    float playtime_hours;
    /* LoL rank system */
    TierEnum tier;
    int division;   /* 0=I, 1=II, 2=III, 3=IV */
    /* Role preferences (0-indexed: ROLE_TOP..ROLE_SUPPORT from lol_tiers.h) */
    int primary_role;
    int secondary_role;
} Player;

typedef struct {
    Player player1;
    Player player2;
    float compatibility_score;
    float skill_difference;
} Match;

Match* find_best_match(Player* players, int player_count, Player target);
float calculate_compatibility(Player p1, Player p2);
float calculate_skill_difference(Player p1, Player p2);

#endif