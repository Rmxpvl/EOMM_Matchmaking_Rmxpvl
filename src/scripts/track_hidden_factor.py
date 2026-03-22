import json

with open('match_history.json', 'r') as f:
    data = json.load(f)

matches = data['matches']

# Reconstruit le hidden_factor de chaque joueur match par match
player_hidden = {}
player_streak = {}

for match in matches:
    for team, team_data in [('team_a', match['team_a']), ('team_b', match['team_b'])]:
        winner = match['winner']
        won = (team == 'team_a' and winner == 0) or (team == 'team_b' and winner == 1)
        
        for player in team_data:
            pid = player['id']
            if pid not in player_hidden:
                player_hidden[pid] = 1.0  # Initial
                player_streak[pid] = 0
            
            # Update hidden factor
            if won:
                player_hidden[pid] += 0.02
                player_streak[pid] = max(0, player_streak[pid] - 1)  # Reset negative streak
            else:
                player_hidden[pid] -= 0.05
                player_streak[pid] -= 1
            
            # Clamp
            player_hidden[pid] = max(0.50, min(1.20, player_hidden[pid]))

# Show top/bottom 5
print("=== FINAL HIDDEN FACTORS ===\n")
sorted_players = sorted(player_hidden.items(), key=lambda x: x[1], reverse=True)

print("TOP 5 (Hot Streak):")
for pid, factor in sorted_players[:5]:
    print(f"  Player {pid:04d}: {factor:.3f}")

print("\nBOTTOM 5 (Tilted):")
for pid, factor in sorted_players[-5:]:
    print(f"  Player {pid:04d}: {factor:.3f}")
