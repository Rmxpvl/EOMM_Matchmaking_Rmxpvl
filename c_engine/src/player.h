// Player structure definition

#ifndef PLAYER_H
#define PLAYER_H

#include <vector>
#include <string>

struct Player {
    std::string username;  // Player's username
    int toxicity;          // Player's toxicity score
    std::vector<std::string> chat_history; // Player's chat history
    // Add other fields as necessary
};

#endif // PLAYER_H
