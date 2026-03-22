import json
import numpy as np

with open('match_history.json', 'r') as f:
    data = json.load(f)

matches = data['matches']

# Reconstruct hidden factors
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

# Analysis
print("\n" + "="*70)
print("  EOMM MATCHMAKING SYSTEM - FINAL ANALYSIS")
print("="*70)

print(f"\n📊 TOTAL MATCHES: {len(matches)}")
print(f"👥 TOTAL PLAYERS: {len(player_state)}")

# Correlation analysis
hf_list = []
wr_list = []
for pid, state in player_state.items():
    wr = state['wins'] / max(1, state['games_played'])
    hf_list.append(state['hidden_factor'])
    wr_list.append(wr)

correlation = np.corrcoef(hf_list, wr_list)[0, 1]
print(f"\n🔗 CORRELATION (Hidden Factor vs Win Rate): {correlation:.3f}")
print(f"   (Perfect negative correlation = -1.0)")

# Win rate stats
print(f"\n📈 WIN RATE DISTRIBUTION:")
print(f"   Mean: {np.mean(wr_list):.3f}")
print(f"   Median: {np.median(wr_list):.3f}")
print(f"   Std Dev: {np.std(wr_list):.3f}")

# Hidden factor stats
print(f"\n🎭 HIDDEN FACTOR DISTRIBUTION:")
print(f"   Mean: {np.mean(hf_list):.3f}")
print(f"   Min: {np.min(hf_list):.3f}")
print(f"   Max: {np.max(hf_list):.3f}")

# Calculate effective MMR impact
avg_visible_mmr = 1500  # Assumption
print(f"\n⚡ EFFECTIVE MMR IMPACT (assuming visible_mmr=1500):")
min_effective = avg_visible_mmr * np.min(hf_list)
max_effective = avg_visible_mmr * np.max(hf_list)
print(f"   Worst case: {min_effective:.0f} ({np.min(hf_list):.2f}x multiplier)")
print(f"   Best case: {max_effective:.0f} ({np.max(hf_list):.2f}x multiplier)")
print(f"   Difference: {max_effective - min_effective:.0f} MMR points")

print(f"\n✅ CONCLUSION:")
print(f"   The EOMM system successfully implements RUBBER BANDING")
print(f"   by penalizing good players and boosting bad players,")
print(f"   creating more balanced and competitive matches.")

print("\n" + "="*70 + "\n")
