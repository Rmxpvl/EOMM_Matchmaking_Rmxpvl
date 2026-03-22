import json
import numpy as np
import matplotlib.pyplot as plt

with open('match_history.json', 'r') as f:
    data = json.load(f)

matches = data['matches']

HIDDEN_FACTOR_START = 1.00
HIDDEN_FACTOR_MIN = 0.50
HIDDEN_FACTOR_MAX = 1.20
FACTOR_WIN_BONUS = 0.02
FACTOR_LOSS_PENALTY = 0.05
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
            'losses': 0,
            'match_sequence': [],  # Track win/loss sequence
            'current_streak': ('start', 0),
            'all_streaks': [],
        }

for match in matches:
    for team, team_data in [('team_a', match['team_a']), ('team_b', match['team_b'])]:
        winner = match['winner']
        won = (team == 'team_a' and winner == 0) or (team == 'team_b' and winner == 1)
        
        for player in team_data:
            pid = player['id']
            init_player(pid)
            state = player_state[pid]
            
            if won:
                state['wins'] += 1
                state['match_sequence'].append('W')
            else:
                state['losses'] += 1
                state['match_sequence'].append('L')
            
            state['games_played'] += 1

# Analyze streaks in detail
def get_all_streaks(sequence):
    """Extract all consecutive win/loss streaks"""
    if not sequence:
        return []
    
    streaks = []
    current_char = sequence[0]
    current_length = 1
    
    for char in sequence[1:]:
        if char == current_char:
            current_length += 1
        else:
            streaks.append((current_char, current_length))
            current_char = char
            current_length = 1
    
    streaks.append((current_char, current_length))
    return streaks

print("\n" + "="*100)
print("  STREAK PATTERN ANALYSIS - SHOULD SHOW STRONG WIN/LOSS CHAINS")
print("="*100)

# Categorize players
elite = []
good = []
neutral = []
bad = []
terrible = []

for pid, state in player_state.items():
    wr = state['wins'] / state['games_played']
    streaks = get_all_streaks(state['match_sequence'])
    max_win = max([s[1] for s in streaks if s[0] == 'W'], default=0)
    max_loss = max([s[1] for s in streaks if s[0] == 'L'], default=0)
    
    entry = (pid, wr, max_win, max_loss, streaks, state['match_sequence'])
    
    if wr >= 0.60:
        elite.append(entry)
    elif wr >= 0.55:
        good.append(entry)
    elif wr <= 0.40:
        terrible.append(entry)
    elif wr <= 0.45:
        bad.append(entry)
    else:
        neutral.append(entry)

def analyze_category(name, players):
    if not players:
        print(f"\n{name}: NO PLAYERS")
        return
    
    print(f"\n{name}")
    print("-" * 100)
    
    max_wins = [p[2] for p in players]
    max_losses = [p[3] for p in players]
    wrs = [p[1] for p in players]
    
    print(f"Count: {len(players)}")
    print(f"Avg WR: {np.mean(wrs):.1%}")
    print(f"\nStreak Statistics:")
    print(f"  Max Win Streak - Mean: {np.mean(max_wins):.1f}, Std: {np.std(max_wins):.1f}, Range: {np.min(max_wins)}-{np.max(max_wins)}")
    print(f"  Max Loss Streak - Mean: {np.mean(max_losses):.1f}, Std: {np.std(max_losses):.1f}, Range: {np.min(max_losses)}-{np.max(max_losses)}")
    
    # Show most interesting patterns
    print(f"\nMost interesting patterns:")
    
    # Best win streak example
    best_win = max(players, key=lambda x: x[2])
    print(f"  Best Win Streak (Player {best_win[0]:04d}):")
    print(f"    Sequence: {''.join(best_win[5][:30])}...")
    print(f"    Max Win: {best_win[2]}, Max Loss: {best_win[3]}, WR: {best_win[1]:.1%}")
    
    # Best loss streak example
    worst_loss = max(players, key=lambda x: x[3])
    print(f"  Longest Loss Streak (Player {worst_loss[0]:04d}):")
    print(f"    Sequence: {''.join(worst_loss[5][:30])}...")
    print(f"    Max Win: {worst_loss[2]}, Max Loss: {worst_loss[3]}, WR: {worst_loss[1]:.1%}")

analyze_category("🏆 ELITE (WR >= 60%)", elite)
analyze_category("✅ GOOD (WR >= 55%)", good)
analyze_category("➡️  NEUTRAL (45-55%)", neutral)
analyze_category("❌ BAD (WR <= 45%)", bad)
analyze_category("💀 TERRIBLE (WR <= 40%)", terrible)

# Global streak pattern analysis
print("\n" + "="*100)
print("  WHAT SHOULD HAPPEN (IDEAL EOMM WITH STREAKS)")
print("="*100)

print(f"""
CURRENT PATTERN (too random):
  Elite: W W L W W L W L W...  (Random scattered losses)
  Bad: L L W L L W L L L...    (Random scattered wins)
  
DESIRED PATTERN (with artificial streaks):
  Elite: W W W W W L L L L L W W W W W L L L L L...
         └─ 5 wins ─┘ └─ 5 losses ─┘ └─ 5 wins ─┘
         
  Bad: L L L L L W W W W W L L L L L W W W W W...
       └─ 5 losses ─┘ └─ 5 wins ─┘ └─ 5 losses ─┘

This creates:
✅ Engagement loops (frustration → relief → repeat)
✅ Apparent balance (each player gets wins AND losses)
✅ Real hierarchy maintained (elite wins 60%+ overall, bad wins 40%-)
✅ Streaks are "scripted" by matchmaker decisions
""")

# Calculate streak efficiency
print("\n" + "="*100)
print("  STREAK EFFICIENCY METRIC")
print("="*100)

def calculate_streak_efficiency(sequence):
    """
    Measure how 'clumped' wins/losses are.
    Higher = more consecutive W's and L's (good for streaks)
    Lower = more alternating W/L (bad, too random)
    """
    if len(sequence) < 2:
        return 0.5
    
    changes = sum(1 for i in range(len(sequence)-1) if sequence[i] != sequence[i+1])
    max_changes = len(sequence) - 1
    
    # Efficiency = 1 - (changes / max_changes)
    # Perfect alternation (WLWLWL): changes = 99, efficiency = 0
    # Perfect streaks (WWWWW...LLLLL): changes = 1, efficiency ≈ 1
    
    efficiency = 1 - (changes / max_changes) if max_changes > 0 else 0.5
    return efficiency

all_efficiencies = []
elite_eff = []
bad_eff = []

for pid, state in player_state.items():
    eff = calculate_streak_efficiency(state['match_sequence'])
    all_efficiencies.append(eff)
    
    wr = state['wins'] / state['games_played']
    if wr >= 0.60:
        elite_eff.append(eff)
    elif wr <= 0.45:
        bad_eff.append(eff)

print(f"\nGlobal Streak Efficiency: {np.mean(all_efficiencies):.3f}")
print(f"  (0.0 = perfect alternation WLWLWL..., 1.0 = perfect streaks WWWW...LLLL...)")
print(f"  Current: {np.mean(all_efficiencies):.3f} - {'TOO RANDOM' if np.mean(all_efficiencies) < 0.4 else 'OK'}")

if elite_eff and bad_eff:
    print(f"\nElite players streak efficiency: {np.mean(elite_eff):.3f}")
    print(f"Bad players streak efficiency: {np.mean(bad_eff):.3f}")
    print(f"Difference: {abs(np.mean(elite_eff) - np.mean(bad_eff)):.3f}")
    print(f"  → Should be HIGH difference (elite have more artificial streaks)")

print("\n" + "="*100 + "\n")

