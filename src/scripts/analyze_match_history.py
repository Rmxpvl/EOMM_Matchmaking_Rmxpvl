import json
import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict

# Load match history
with open('match_history.json', 'r') as f:
    data = json.load(f)

matches = data['matches']
print(f"Total matches: {len(matches)}")

# Stats par joueur
player_stats = defaultdict(lambda: {
    'wins': 0,
    'losses': 0,
    'matches': 0,
    'team_a_count': 0,
    'team_b_count': 0,
    'troll_count': 0
})

for match in matches:
    winner = match['winner']  # 0 = team_a, 1 = team_b
    
    for player in match['team_a']:
        pid = player['id']
        player_stats[pid]['matches'] += 1
        player_stats[pid]['team_a_count'] += 1
        if winner == 0:
            player_stats[pid]['wins'] += 1
        else:
            player_stats[pid]['losses'] += 1
    
    for player in match['team_b']:
        pid = player['id']
        player_stats[pid]['matches'] += 1
        player_stats[pid]['team_b_count'] += 1
        if winner == 1:
            player_stats[pid]['wins'] += 1
        else:
            player_stats[pid]['losses'] += 1

# Affiche top 10 joueurs
print("\n=== TOP 10 JOUEURS (Win Rate) ===")
sorted_players = sorted(player_stats.items(), key=lambda x: x[1]['wins'] / max(1, x[1]['matches']), reverse=True)
for i, (pid, stats) in enumerate(sorted_players[:10], 1):
    wr = 100 * stats['wins'] / max(1, stats['matches'])
    print(f"{i}. Player {pid:04d}: {stats['wins']}-{stats['losses']} ({wr:.1f}%) — {stats['matches']} games")

print("\n=== BOTTOM 10 JOUEURS (Win Rate) ===")
for i, (pid, stats) in enumerate(sorted_players[-10:], 1):
    wr = 100 * stats['wins'] / max(1, stats['matches'])
    print(f"{i}. Player {pid:04d}: {stats['wins']}-{stats['losses']} ({wr:.1f}%) — {stats['matches']} games")

# Win rate distribution
win_rates = [s['wins'] / max(1, s['matches']) for s in player_stats.values()]
print(f"\n=== WIN RATE STATS ===")
print(f"Mean: {np.mean(win_rates):.3f}")
print(f"Median: {np.median(win_rates):.3f}")
print(f"Std Dev: {np.std(win_rates):.3f}")
print(f"Min: {np.min(win_rates):.3f}")
print(f"Max: {np.max(win_rates):.3f}")

# Plot win rate distribution
plt.figure(figsize=(12, 6))
plt.hist(win_rates, bins=30, edgecolor='black', alpha=0.7)
plt.axvline(np.mean(win_rates), color='red', linestyle='--', linewidth=2, label=f'Mean: {np.mean(win_rates):.3f}')
plt.axvline(0.5, color='green', linestyle='--', linewidth=2, label='50%')
plt.xlabel('Win Rate')
plt.ylabel('Number of Players')
plt.title('Win Rate Distribution')
plt.legend()
plt.grid(True, alpha=0.3)
plt.savefig('win_rate_distribution.png', dpi=150, bbox_inches='tight')
print("\n✓ Graph saved: win_rate_distribution.png")

# Troll impact analysis
troll_matches = {'0_trolls': {'wins': 0, 'total': 0},
                 '1_troll': {'wins': 0, 'total': 0},
                 '2_trolls': {'wins': 0, 'total': 0}}

for match in matches:
    troll_a = match.get('troll_count_a', 0)
    troll_b = match.get('troll_count_b', 0)
    winner = match['winner']
    
    # Team A
    key_a = f"{troll_a}_trolls" if troll_a <= 2 else "2_trolls"
    troll_matches[key_a]['total'] += 1
    if winner == 0:
        troll_matches[key_a]['wins'] += 1
    
    # Team B
    key_b = f"{troll_b}_trolls" if troll_b <= 2 else "2_trolls"
    troll_matches[key_b]['total'] += 1
    if winner == 1:
        troll_matches[key_b]['wins'] += 1

print(f"\n=== TROLL IMPACT ===")
for key in sorted(troll_matches.keys()):
    data = troll_matches[key]
    if data['total'] > 0:
        wr = 100 * data['wins'] / data['total']
        print(f"{key}: {wr:.1f}% win rate ({data['wins']}/{data['total']} matches)")

# Plot troll impact
fig, ax = plt.subplots(figsize=(10, 6))
categories = list(troll_matches.keys())
win_rates_troll = [100 * troll_matches[cat]['wins'] / max(1, troll_matches[cat]['total']) for cat in categories]
colors = ['green' if wr >= 50 else 'red' for wr in win_rates_troll]
bars = ax.bar(categories, win_rates_troll, color=colors, alpha=0.7, edgecolor='black')
ax.axhline(50, color='gray', linestyle='--', linewidth=2, label='50% (neutral)')
ax.set_ylabel('Win Rate (%)')
ax.set_title('Troll Impact on Win Rate')
ax.set_ylim([40, 60])
ax.legend()
ax.grid(True, alpha=0.3, axis='y')
for bar, wr in zip(bars, win_rates_troll):
    height = bar.get_height()
    ax.text(bar.get_x() + bar.get_width()/2., height,
            f'{wr:.1f}%', ha='center', va='bottom')
plt.savefig('troll_impact.png', dpi=150, bbox_inches='tight')
print("✓ Graph saved: troll_impact.png")

print("\n=== ANALYSIS COMPLETE ===")
