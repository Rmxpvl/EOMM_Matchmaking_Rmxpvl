import json
import numpy as np
import random

with open('match_history.json', 'r') as f:
    data = json.load(f)

matches = data['matches']

HIDDEN_FACTOR_START = 1.00
HIDDEN_FACTOR_MIN = 0.50
HIDDEN_FACTOR_MAX = 1.20

# AGGRESSIVE PARAMETERS FOR STREAKING
FACTOR_WIN_BONUS = 0.08      # 4x more (+0.08 vs +0.02)
FACTOR_LOSS_PENALTY = 0.20   # 4x more (-0.20 vs -0.05)
SOFT_RESET_INTERVAL = 20     # Longer reset
STREAK_EXTENSION_CHANCE = 0.7  # 70% chance to extend current streak

def clamp(v, min_v, max_v):
    return max(min_v, min(max_v, v))

def get_all_streaks(sequence):
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

def simulate_forced_streaks():
    """EOMM that actively extends or breaks streaks"""
    player_state = {}
    
    def init_player(pid):
        if pid not in player_state:
            player_state[pid] = {
                'hidden_factor': HIDDEN_FACTOR_START,
                'games_played': 0,
                'wins': 0,
                'match_sequence': [],
                'current_streak_type': None,  # 'W' or 'L'
                'current_streak_length': 0,
            }
    
    # Initialize all players
    for match in matches:
        for player in match['team_a'] + match['team_b']:
            init_player(player['id'])
    
    for match_idx, match in enumerate(matches):
        all_players = match['team_a'] + match['team_b']
        
        # MATCHMAKING LOGIC: Assign teams based on current streaks
        # Goal: Extend streaks by pairing streak players favorably
        
        on_win_streak = []
        on_loss_streak = []
        neutral_players = []
        
        for player in all_players:
            pid = player['id']
            state = player_state[pid]
            
            if state['current_streak_type'] == 'W':
                on_win_streak.append(player)
            elif state['current_streak_type'] == 'L':
                on_loss_streak.append(player)
            else:
                neutral_players.append(player)
        
        # ADVERSARIAL PAIRING:
        # Put win-streak players AGAINST loss-streak players
        # → Win-streak players more likely to win (extend streak)
        # → Loss-streak players more likely to lose (extend streak)
        
        team_a = on_win_streak + neutral_players[:2]
        team_b = on_loss_streak + neutral_players[2:]
        
        winner = match['winner']
        
        # Simulate
        for player in team_a:
            pid = player['id']
            state = player_state[pid]
            
            if state['games_played'] > 0 and state['games_played'] % SOFT_RESET_INTERVAL == 0:
                state['hidden_factor'] = HIDDEN_FACTOR_START
                state['current_streak_type'] = None
                state['current_streak_length'] = 0
            
            won = (winner == 0)
            
            if won:
                state['hidden_factor'] += FACTOR_WIN_BONUS
                state['wins'] += 1
                state['match_sequence'].append('W')
                
                # Streak logic
                if state['current_streak_type'] == 'W':
                    state['current_streak_length'] += 1
                else:
                    state['current_streak_type'] = 'W'
                    state['current_streak_length'] = 1
            else:
                state['hidden_factor'] -= FACTOR_LOSS_PENALTY
                state['match_sequence'].append('L')
                
                if state['current_streak_type'] == 'L':
                    state['current_streak_length'] += 1
                else:
                    state['current_streak_type'] = 'L'
                    state['current_streak_length'] = 1
            
            state['hidden_factor'] = clamp(state['hidden_factor'], HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX)
            state['games_played'] += 1
        
        for player in team_b:
            pid = player['id']
            state = player_state[pid]
            
            if state['games_played'] > 0 and state['games_played'] % SOFT_RESET_INTERVAL == 0:
                state['hidden_factor'] = HIDDEN_FACTOR_START
                state['current_streak_type'] = None
                state['current_streak_length'] = 0
            
            won = (winner == 1)
            
            if won:
                state['hidden_factor'] += FACTOR_WIN_BONUS
                state['wins'] += 1
                state['match_sequence'].append('W')
                
                if state['current_streak_type'] == 'W':
                    state['current_streak_length'] += 1
                else:
                    state['current_streak_type'] = 'W'
                    state['current_streak_length'] = 1
            else:
                state['hidden_factor'] -= FACTOR_LOSS_PENALTY
                state['match_sequence'].append('L')
                
                if state['current_streak_type'] == 'L':
                    state['current_streak_length'] += 1
                else:
                    state['current_streak_type'] = 'L'
                    state['current_streak_length'] = 1
            
            state['hidden_factor'] = clamp(state['hidden_factor'], HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX)
            state['games_played'] += 1
    
    return player_state

# Run simulation
print("Simulating FORCED STREAKS EOMM...")
result = simulate_forced_streaks()

# Analyze
def calculate_streak_efficiency(sequence):
    if len(sequence) < 2:
        return 0.5
    changes = sum(1 for i in range(len(sequence)-1) if sequence[i] != sequence[i+1])
    max_changes = len(sequence) - 1
    efficiency = 1 - (changes / max_changes) if max_changes > 0 else 0.5
    return efficiency

elite = []
good = []
bad = []
terrible = []

for pid, state in result.items():
    wr = state['wins'] / state['games_played']
    eff = calculate_streak_efficiency(state['match_sequence'])
    
    entry = (pid, wr, eff, state['match_sequence'], state['hidden_factor'])
    
    if wr >= 0.60:
        elite.append(entry)
    elif wr >= 0.55:
        good.append(entry)
    elif wr <= 0.40:
        terrible.append(entry)
    elif wr <= 0.45:
        bad.append(entry)

print("\n" + "="*100)
print("  FORCED STREAKS EOMM - RESULTS")
print("="*100)

print(f"\n🏆 ELITE (WR >= 60%): {len(elite)} players")
if elite:
    eff = [e[2] for e in elite]
    wrs = [e[1] for e in elite]
    print(f"  Avg WR: {np.mean(wrs):.1%}")
    print(f"  Avg Streak Efficiency: {np.mean(eff):.3f}")
    print(f"  Example Elite (Player {elite[0][0]:04d}): {elite[0][3][:40]}")

print(f"\n✅ GOOD (WR >= 55%): {len(good)} players")
if good:
    eff = [e[2] for e in good]
    wrs = [e[1] for e in good]
    print(f"  Avg WR: {np.mean(wrs):.1%}")
    print(f"  Avg Streak Efficiency: {np.mean(eff):.3f}")

print(f"\n❌ BAD (WR <= 45%): {len(bad)} players")
if bad:
    eff = [e[2] for e in bad]
    wrs = [e[1] for e in bad]
    print(f"  Avg WR: {np.mean(wrs):.1%}")
    print(f"  Avg Streak Efficiency: {np.mean(eff):.3f}")
    print(f"  Example Bad (Player {bad[0][0]:04d}): {bad[0][3][:40]}")

print(f"\n💀 TERRIBLE (WR <= 40%): {len(terrible)} players")
if terrible:
    eff = [e[2] for e in terrible]
    wrs = [e[1] for e in terrible]
    print(f"  Avg WR: {np.mean(wrs):.1%}")
    print(f"  Avg Streak Efficiency: {np.mean(eff):.3f}")

# Global stats
all_eff = [calculate_streak_efficiency(state['match_sequence']) for state in result.values()]
print(f"\n📊 GLOBAL STREAK EFFICIENCY: {np.mean(all_eff):.3f} (was 0.525)")
print(f"   Improvement: +{(np.mean(all_eff) - 0.525) * 100:.1f}%")

print("\n" + "="*100 + "\n")

