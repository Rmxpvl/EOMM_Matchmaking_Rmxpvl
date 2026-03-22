import json
import matplotlib.pyplot as plt

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

for match in matches:
    for team, team_data in [('team_a', match['team_a']), ('team_b', match['team_b'])]:
        winner = match['winner']
        won = (team == 'team_a' and winner == 0) or (team == 'team_b' and winner == 1)
        troll_count = match.get('troll_count_a' if team == 'team_a' else 'troll_count_b', 0)
        
        for player in team_data:
            pid = player['id']
            init_player(pid)
            state = player_state[pid]
            
            if state['games_played'] > 0 and state['games_played'] % SOFT_RESET_INTERVAL == 0:
                state['hidden_factor'] = HIDDEN_FACTOR_START
                state['lose_streak'] = 0
            
            if won:
                state['hidden_factor'] += FACTOR_WIN_BONUS
                state['wins'] += 1
                state['lose_streak'] = 0
            else:
                state['hidden_factor'] -= FACTOR_LOSS_PENALTY
                state['lose_streak'] += 1
            
            if troll_count > 0:
                state['hidden_factor'] -= TROLL_PENALTY_BASE * troll_count
            
            state['hidden_factor'] = clamp(state['hidden_factor'], HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX)
            state['games_played'] += 1

# Plot
hf_list = []
wr_list = []
for pid, state in player_state.items():
    wr = 100 * state['wins'] / max(1, state['games_played'])
    hf_list.append(state['hidden_factor'])
    wr_list.append(wr)

plt.figure(figsize=(12, 8))
plt.scatter(hf_list, wr_list, alpha=0.6, s=100, edgecolors='black')
plt.xlabel('Hidden Factor', fontsize=12)
plt.ylabel('Win Rate (%)', fontsize=12)
plt.title('EOMM Rubber Banding: Hidden Factor vs Win Rate', fontsize=14)
plt.grid(True, alpha=0.3)
plt.axhline(50, color='green', linestyle='--', alpha=0.5, label='50% WR')
plt.axvline(1.0, color='red', linestyle='--', alpha=0.5, label='Neutral HF')
plt.legend()
plt.tight_layout()
plt.savefig('hidden_factor_vs_winrate.png', dpi=150, bbox_inches='tight')
print("✓ Graph saved: hidden_factor_vs_winrate.png")
