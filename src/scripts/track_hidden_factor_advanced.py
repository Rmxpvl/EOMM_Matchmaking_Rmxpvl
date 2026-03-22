import json
import sys

with open('match_history.json', 'r') as f:
    data = json.load(f)

matches = data['matches']

HIDDEN_FACTOR_START = 1.00
HIDDEN_FACTOR_MIN = 0.50
HIDDEN_FACTOR_MAX = 1.20
FACTOR_WIN_BONUS = 0.02
FACTOR_LOSS_PENALTY = 0.05
TROLL_PENALTY_BASE = 0.10
SOFT_RESET_INTERVAL = 14

# Track player state
player_state = {}

def clamp(v, min_v, max_v):
    return max(min_v, min(max_v, v))

def init_player(pid):
    if pid not in player_state:
        player_state[pid] = {
            'hidden_factor': HIDDEN_FACTOR_START,
            'games_played': 0,
            'wins': 0,
            'lose_streak': 0
        }

for match_idx, match in enumerate(matches):
    for team, team_data in [('team_a', match['team_a']), ('team_b', match['team_b'])]:
        winner = match['winner']
        won = (team == 'team_a' and winner == 0) or (team == 'team_b' and winner == 1)
        troll_count = match.get('troll_count_a' if team == 'team_a' else 'troll_count_b', 0)
        
        for player in team_data:
            pid = player['id']
            init_player(pid)
            state = player_state[pid]
            
            # Soft reset every 14 games
            if state['games_played'] > 0 and state['games_played'] % SOFT_RESET_INTERVAL == 0:
                state['hidden_factor'] = HIDDEN_FACTOR_START
                state['lose_streak'] = 0
            
            # Update hidden factor
            if won:
                state['hidden_factor'] += FACTOR_WIN_BONUS
                state['wins'] += 1
                state['lose_streak'] = 0
            else:
                state['hidden_factor'] -= FACTOR_LOSS_PENALTY
                state['lose_streak'] += 1
            
            # Troll penalty
            if troll_count > 0:
                state['hidden_factor'] -= TROLL_PENALTY_BASE * troll_count
            
            # Clamp
            state['hidden_factor'] = clamp(state['hidden_factor'], HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX)
            state['games_played'] += 1

# Display
print("=== FINAL HIDDEN FACTORS (with soft reset + trolls) ===\n")
sorted_players = sorted(player_state.items(), key=lambda x: x[1]['hidden_factor'], reverse=True)

print("TOP 10 (Hot Streak):")
for i, (pid, state) in enumerate(sorted_players[:10], 1):
    wr = 100 * state['wins'] / max(1, state['games_played'])
    print(f"{i}. Player {pid:04d}: HF={state['hidden_factor']:.3f}, WR={wr:.1f}%, Games={state['games_played']}")

print("\nBOTTOM 10 (Tilted):")
for i, (pid, state) in enumerate(sorted_players[-10:], 1):
    wr = 100 * state['wins'] / max(1, state['games_played'])
    print(f"{i}. Player {pid:04d}: HF={state['hidden_factor']:.3f}, WR={wr:.1f}%, Games={state['games_played']}")
