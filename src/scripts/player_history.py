import json
import sys

# Load match history
with open('match_history.json', 'r') as f:
    data = json.load(f)

matches = data['matches']

# Get player ID from command line
if len(sys.argv) < 2:
    print("Usage: python3 player_history.py <player_id>")
    sys.exit(1)

player_id = int(sys.argv[1])

# Find all matches for this player
player_matches = []
for match in matches:
    in_team_a = any(p['id'] == player_id for p in match['team_a'])
    in_team_b = any(p['id'] == player_id for p in match['team_b'])
    
    if in_team_a or in_team_b:
        player_matches.append({
            'match_id': match['match_id'],
            'team': 'A' if in_team_a else 'B',
            'opponent_team': 'B' if in_team_a else 'A',
            'won': (match['winner'] == 0 and in_team_a) or (match['winner'] == 1 and in_team_b),
            'team_a_power': match['team_a_power'],
            'team_b_power': match['team_b_power'],
            'troll_count': match.get('troll_count_a' if in_team_a else 'troll_count_b', 0)
        })

# Display results
print(f"\n=== PLAYER {player_id:04d} HISTORY ===")
print(f"Total matches: {len(player_matches)}")

wins = sum(1 for m in player_matches if m['won'])
losses = len(player_matches) - wins
wr = 100 * wins / max(1, len(player_matches))

print(f"Record: {wins}-{losses} ({wr:.1f}%)")
print(f"\nMatch history:")
print("-" * 80)

for i, match in enumerate(player_matches, 1):
    result = "WIN" if match['won'] else "LOSS"
    team_power = match['team_a_power'] if match['team'] == 'A' else match['team_b_power']
    opp_power = match['team_b_power'] if match['team'] == 'A' else match['team_a_power']
    
    print(f"{i}. Match {match['match_id']:04d} [{result}] Team {match['team']} ({team_power:.1f}) vs Team {match['opponent_team']} ({opp_power:.1f}) | Trolls: {match['troll_count']}")

print("-" * 80)
