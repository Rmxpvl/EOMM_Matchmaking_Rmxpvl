import json
from collections import defaultdict

with open('match_history.json', 'r') as f:
    data = json.load(f)

matches = data['matches']

# Track streaks for each player
player_streaks = defaultdict(lambda: {'current': 0, 'max_win': 0, 'max_loss': 0, 'current_type': None})

for match in matches:
    team_a = [p['id'] for p in match['team_a']]
    team_b = [p['id'] for p in match['team_b']]
    winner = match['winner']
    
    # Team A results
    for player_id in team_a:
        won = (winner == 0)
        ps = player_streaks[player_id]
        
        if won:
            if ps['current_type'] == 'W':
                ps['current'] += 1
            else:
                ps['current'] = 1
                ps['current_type'] = 'W'
            ps['max_win'] = max(ps['max_win'], ps['current'])
        else:
            if ps['current_type'] == 'L':
                ps['current'] += 1
            else:
                ps['current'] = 1
                ps['current_type'] = 'L'
            ps['max_loss'] = max(ps['max_loss'], ps['current'])
    
    # Team B results
    for player_id in team_b:
        won = (winner == 1)
        ps = player_streaks[player_id]
        
        if won:
            if ps['current_type'] == 'W':
                ps['current'] += 1
            else:
                ps['current'] = 1
                ps['current_type'] = 'W'
            ps['max_win'] = max(ps['max_win'], ps['current'])
        else:
            if ps['current_type'] == 'L':
                ps['current'] += 1
            else:
                ps['current'] = 1
                ps['current_type'] = 'L'
            ps['max_loss'] = max(ps['max_loss'], ps['current'])

# Print results
print("╔════════════════════════════════════════════╗")
print("║      STREAK ANALYSIS                       ║")
print("╚════════════════════════════════════════════╝")
print()

# Find players with best/worst streaks
top_win_streaks = sorted(player_streaks.items(), key=lambda x: x[1]['max_win'], reverse=True)[:5]
top_loss_streaks = sorted(player_streaks.items(), key=lambda x: x[1]['max_loss'], reverse=True)[:5]

print("Top 5 Win Streaks:")
print("Player         Max Win Streak")
print("-" * 40)
for player_id, stats in top_win_streaks:
    print(f"Player{player_id:04d}      {stats['max_win']}")

print()
print("Top 5 Loss Streaks:")
print("Player         Max Loss Streak")
print("-" * 40)
for player_id, stats in top_loss_streaks:
    print(f"Player{player_id:04d}      {stats['max_loss']}")

# Calculate average streak length
avg_max_win = sum(s['max_win'] for s in player_streaks.values()) / len(player_streaks)
avg_max_loss = sum(s['max_loss'] for s in player_streaks.values()) / len(player_streaks)

print()
print(f"Average Max Win Streak:  {avg_max_win:.2f}")
print(f"Average Max Loss Streak: {avg_max_loss:.2f}")

# Streak efficiency (measure of how "clumpy" wins/losses are)
efficiency = avg_max_win / (avg_max_win + avg_max_loss)
print(f"Streak Efficiency:       {efficiency:.3f}")
print(f"  (0.5 = random, 0.7+ = clustered)")

