import json
import numpy as np
import matplotlib.pyplot as plt

with open('match_history.json', 'r') as f:
    data = json.load(f)

matches = data['matches']

# Define different parameter sets
CONFIGS = {
    'original': {
        'name': 'Original EOMM',
        'FACTOR_WIN_BONUS': 0.02,
        'FACTOR_LOSS_PENALTY': 0.05,
        'TROLL_PENALTY_BASE': 0.10,
        'SOFT_RESET_INTERVAL': 14,
    },
    'aggressive_v1': {
        'name': 'Aggressive v1 (2x harder)',
        'FACTOR_WIN_BONUS': 0.02,
        'FACTOR_LOSS_PENALTY': 0.10,  # 2x
        'TROLL_PENALTY_BASE': 0.15,   # 1.5x
        'SOFT_RESET_INTERVAL': 14,
    },
    'aggressive_v2': {
        'name': 'Aggressive v2 (5x harder)',
        'FACTOR_WIN_BONUS': 0.01,
        'FACTOR_LOSS_PENALTY': 0.15,  # 3x
        'TROLL_PENALTY_BASE': 0.20,   # 2x
        'SOFT_RESET_INTERVAL': 21,    # Longer reset
    },
    'aggressive_v3': {
        'name': 'Aggressive v3 (Extreme)',
        'FACTOR_WIN_BONUS': 0.01,
        'FACTOR_LOSS_PENALTY': 0.20,  # 4x
        'TROLL_PENALTY_BASE': 0.25,   # 2.5x
        'SOFT_RESET_INTERVAL': 28,    # Even longer
    },
}

HIDDEN_FACTOR_START = 1.00
HIDDEN_FACTOR_MIN = 0.50
HIDDEN_FACTOR_MAX = 1.20

def clamp(v, min_v, max_v):
    return max(min_v, min(max_v, v))

def simulate_with_config(config):
    player_state = {}
    
    def init_player(pid):
        if pid not in player_state:
            player_state[pid] = {
                'hidden_factor': HIDDEN_FACTOR_START,
                'games_played': 0,
                'wins': 0,
                'lose_streak': 0,
                'win_streak': 0,
                'max_win_streak': 0,
                'max_lose_streak': 0,
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
                
                # Soft reset
                if state['games_played'] > 0 and state['games_played'] % config['SOFT_RESET_INTERVAL'] == 0:
                    state['hidden_factor'] = HIDDEN_FACTOR_START
                    state['lose_streak'] = 0
                    state['win_streak'] = 0
                
                # Update streaks
                if won:
                    state['hidden_factor'] += config['FACTOR_WIN_BONUS']
                    state['wins'] += 1
                    if state['lose_streak'] > 0:
                        state['max_lose_streak'] = max(state['max_lose_streak'], state['lose_streak'])
                        state['lose_streak'] = 0
                    state['win_streak'] += 1
                    state['max_win_streak'] = max(state['max_win_streak'], state['win_streak'])
                else:
                    state['hidden_factor'] -= config['FACTOR_LOSS_PENALTY']
                    if state['win_streak'] > 0:
                        state['max_win_streak'] = max(state['max_win_streak'], state['win_streak'])
                        state['win_streak'] = 0
                    state['lose_streak'] += 1
                    state['max_lose_streak'] = max(state['max_lose_streak'], state['lose_streak'])
                
                # Troll penalty
                if troll_count > 0:
                    state['hidden_factor'] -= config['TROLL_PENALTY_BASE'] * troll_count
                
                # Clamp
                state['hidden_factor'] = clamp(state['hidden_factor'], HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX)
                state['games_played'] += 1
    
    # Close final streaks
    for pid, state in player_state.items():
        state['max_win_streak'] = max(state['max_win_streak'], state['win_streak'])
        state['max_lose_streak'] = max(state['max_lose_streak'], state['lose_streak'])
    
    return player_state

# Run all simulations
results = {}
for config_key, config in CONFIGS.items():
    print(f"Simulating {config['name']}...")
    results[config_key] = simulate_with_config(config)

# Analyze results
print("\n" + "="*100)
print("  AGGRESSIVE EOMM COMPARISON")
print("="*100)

for config_key, config in CONFIGS.items():
    player_state = results[config_key]
    
    # Categorize
    good_players = []
    bad_players = []
    
    for pid, state in player_state.items():
        wr = state['wins'] / max(1, state['games_played'])
        if wr >= 0.55:
            good_players.append((pid, state, wr))
        elif wr <= 0.45:
            bad_players.append((pid, state, wr))
    
    if not good_players or not bad_players:
        continue
    
    good_max_wins = [s[1]['max_win_streak'] for s in good_players]
    good_max_loses = [s[1]['max_lose_streak'] for s in good_players]
    good_hf = [s[1]['hidden_factor'] for s in good_players]
    
    bad_max_wins = [s[1]['max_win_streak'] for s in bad_players]
    bad_max_loses = [s[1]['max_lose_streak'] for s in bad_players]
    bad_hf = [s[1]['hidden_factor'] for s in bad_players]
    
    print(f"\n{config['name']}")
    print("-" * 100)
    print(f"GOOD PLAYERS (WR >= 55%): {len(good_players)}")
    print(f"  Avg Max Win Streak: {np.mean(good_max_wins):.1f}  |  Avg Hidden Factor: {np.mean(good_hf):.3f}")
    print(f"POOR PLAYERS (WR <= 45%): {len(bad_players)}")
    print(f"  Avg Max Win Streak: {np.mean(bad_max_wins):.1f}  |  Avg Hidden Factor: {np.mean(bad_hf):.3f}")
    print(f"\n📊 DIFFERENCE:")
    print(f"  Good vs Bad Win Streak: {np.mean(good_max_wins) - np.mean(bad_max_wins):+.1f} (Good has {(np.mean(good_max_wins) / np.mean(bad_max_wins) * 100):.0f}% of Bad)")
    print(f"  Good vs Bad Hidden Factor: {np.mean(good_hf) - np.mean(bad_hf):+.3f}")
    
    # Correlation
    all_hf = [state['hidden_factor'] for state in player_state.values()]
    all_max_wins = [state['max_win_streak'] for state in player_state.values()]
    all_max_loses = [state['max_lose_streak'] for state in player_state.values()]
    
    corr_win = np.corrcoef(all_hf, all_max_wins)[0, 1]
    corr_lose = np.corrcoef(all_hf, all_max_loses)[0, 1]
    print(f"\n🔗 CORRELATION (HF vs Streaks):")
    print(f"  vs Max Win Streak: {corr_win:.3f}")
    print(f"  vs Max Lose Streak: {corr_lose:.3f}")

print("\n" + "="*100 + "\n")

# Create comparison plots
fig, axes = plt.subplots(2, 2, figsize=(16, 12))
fig.suptitle('Aggressive EOMM Configurations Comparison', fontsize=16, fontweight='bold')

for idx, (config_key, config) in enumerate(CONFIGS.items()):
    player_state = results[config_key]
    
    good_players = [(pid, state, state['wins'] / state['games_played']) 
                    for pid, state in player_state.items() 
                    if state['wins'] / state['games_played'] >= 0.55]
    bad_players = [(pid, state, state['wins'] / state['games_played']) 
                   for pid, state in player_state.items() 
                   if state['wins'] / state['games_played'] <= 0.45]
    
    if not good_players or not bad_players:
        continue
    
    ax = axes[idx // 2, idx % 2]
    
    good_wins = [s[1]['max_win_streak'] for s in good_players]
    good_loses = [s[1]['max_lose_streak'] for s in good_players]
    bad_wins = [s[1]['max_win_streak'] for s in bad_players]
    bad_loses = [s[1]['max_lose_streak'] for s in bad_players]
    
    x = np.arange(2)
    width = 0.35
    
    bars1 = ax.bar(x - width/2, [np.mean(good_wins), np.mean(bad_wins)], width, label='Max Win Streak', alpha=0.8)
    bars2 = ax.bar(x + width/2, [np.mean(good_loses), np.mean(bad_loses)], width, label='Max Lose Streak', alpha=0.8)
    
    ax.set_ylabel('Average Streak Length')
    ax.set_title(config['name'])
    ax.set_xticks(x)
    ax.set_xticklabels(['Good Players\n(WR≥55%)', 'Bad Players\n(WR≤45%)'])
    ax.legend()
    ax.grid(True, alpha=0.3, axis='y')
    ax.set_ylim([0, 10])
    
    # Add value labels on bars
    for bars in [bars1, bars2]:
        for bar in bars:
            height = bar.get_height()
            ax.text(bar.get_x() + bar.get_width()/2., height,
                   f'{height:.1f}', ha='center', va='bottom', fontsize=9)

plt.tight_layout()
plt.savefig('aggressive_eomm_comparison.png', dpi=150, bbox_inches='tight')
print("✓ Graph saved: aggressive_eomm_comparison.png")

