import json
import numpy as np
import random

with open('match_history.json', 'r') as f:
    data = json.load(f)

matches = data['matches']

HIDDEN_FACTOR_START = 1.00
HIDDEN_FACTOR_MIN = 0.50
HIDDEN_FACTOR_MAX = 1.20
FACTOR_WIN_BONUS = 0.02
FACTOR_LOSS_PENALTY = 0.10
SOFT_RESET_INTERVAL = 14

def clamp(v, min_v, max_v):
    return max(min_v, min(max_v, v))

def simulate_active_matchmaking():
    """Simulate EOMM with intelligent team assignment"""
    player_state = {}
    
    def init_player(pid):
        if pid not in player_state:
            player_state[pid] = {
                'hidden_factor': HIDDEN_FACTOR_START,
                'games_played': 0,
                'wins': 0,
                'max_win_streak': 0,
                'max_lose_streak': 0,
                'win_streak': 0,
                'lose_streak': 0,
            }
    
    # Initialize all players first
    for match in matches:
        for player in match['team_a'] + match['team_b']:
            init_player(player['id'])
    
    for match_idx, match in enumerate(matches):
        all_players_in_match = match['team_a'] + match['team_b']
        
        # ACTIVE MATCHMAKING: Sort players by hidden_factor
        # Goal: Balance teams based on HF
        sorted_by_hf = sorted(all_players_in_match, 
                             key=lambda p: player_state[p['id']]['hidden_factor'])
        
        # Assign to teams: Alternating to balance
        team_a_optimized = sorted_by_hf[::2]  # Low HF players (good/tilted)
        team_b_optimized = sorted_by_hf[1::2] # High HF players (bad/boosted)
        
        winner = match['winner']
        troll_count_a = match.get('troll_count_a', 0)
        troll_count_b = match.get('troll_count_b', 0)
        
        # Simulate match with optimized teams
        for player in team_a_optimized:
            pid = player['id']
            state = player_state[pid]
            
            if state['games_played'] > 0 and state['games_played'] % SOFT_RESET_INTERVAL == 0:
                state['hidden_factor'] = HIDDEN_FACTOR_START
                state['win_streak'] = 0
                state['lose_streak'] = 0
            
            won = (winner == 0)
            
            if won:
                state['hidden_factor'] += FACTOR_WIN_BONUS
                state['wins'] += 1
                if state['lose_streak'] > 0:
                    state['max_lose_streak'] = max(state['max_lose_streak'], state['lose_streak'])
                    state['lose_streak'] = 0
                state['win_streak'] += 1
                state['max_win_streak'] = max(state['max_win_streak'], state['win_streak'])
            else:
                state['hidden_factor'] -= FACTOR_LOSS_PENALTY
                if state['win_streak'] > 0:
                    state['max_win_streak'] = max(state['max_win_streak'], state['win_streak'])
                    state['win_streak'] = 0
                state['lose_streak'] += 1
                state['max_lose_streak'] = max(state['max_lose_streak'], state['lose_streak'])
            
            if troll_count_a > 0:
                state['hidden_factor'] -= 0.15 * troll_count_a
            
            state['hidden_factor'] = clamp(state['hidden_factor'], HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX)
            state['games_played'] += 1
        
        for player in team_b_optimized:
            pid = player['id']
            state = player_state[pid]
            
            if state['games_played'] > 0 and state['games_played'] % SOFT_RESET_INTERVAL == 0:
                state['hidden_factor'] = HIDDEN_FACTOR_START
                state['win_streak'] = 0
                state['lose_streak'] = 0
            
            won = (winner == 1)
            
            if won:
                state['hidden_factor'] += FACTOR_WIN_BONUS
                state['wins'] += 1
                if state['lose_streak'] > 0:
                    state['max_lose_streak'] = max(state['max_lose_streak'], state['lose_streak'])
                    state['lose_streak'] = 0
                state['win_streak'] += 1
                state['max_win_streak'] = max(state['max_win_streak'], state['win_streak'])
            else:
                state['hidden_factor'] -= FACTOR_LOSS_PENALTY
                if state['win_streak'] > 0:
                    state['max_win_streak'] = max(state['max_win_streak'], state['win_streak'])
                    state['win_streak'] = 0
                state['lose_streak'] += 1
                state['max_lose_streak'] = max(state['max_lose_streak'], state['lose_streak'])
            
            if troll_count_b > 0:
                state['hidden_factor'] -= 0.15 * troll_count_b
            
            state['hidden_factor'] = clamp(state['hidden_factor'], HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX)
            state['games_played'] += 1
    
    # Close final streaks
    for pid, state in player_state.items():
        state['max_win_streak'] = max(state['max_win_streak'], state['win_streak'])
        state['max_lose_streak'] = max(state['max_lose_streak'], state['lose_streak'])
    
    return player_state

# Run simulation
print("Simulating with ACTIVE MATCHMAKING...")
active_result = simulate_active_matchmaking()

# Analyze
good_players = [(pid, state) for pid, state in active_result.items() 
                if state['wins'] / state['games_played'] >= 0.55]
bad_players = [(pid, state) for pid, state in active_result.items() 
               if state['wins'] / state['games_played'] <= 0.45]

good_wins = [s[1]['max_win_streak'] for s in good_players]
bad_wins = [s[1]['max_win_streak'] for s in bad_players]

print("\n" + "="*80)
print("  ACTIVE MATCHMAKING RESULTS")
print("="*80)
print(f"\nGOOD PLAYERS (WR >= 55%): {len(good_players)}")
print(f"  Avg Max Win Streak: {np.mean(good_wins):.1f}")
print(f"\nPOOR PLAYERS (WR <= 45%): {len(bad_players)}")
print(f"  Avg Max Win Streak: {np.mean(bad_wins):.1f}")
print(f"\n📊 DIFFERENCE:")
print(f"  Good has {(np.mean(good_wins) / np.mean(bad_wins) * 100):.0f}% of Bad's win streak")
print(f"  Ratio: {np.mean(good_wins) / np.mean(bad_wins):.2f}x")

print("\n" + "="*80 + "\n")

