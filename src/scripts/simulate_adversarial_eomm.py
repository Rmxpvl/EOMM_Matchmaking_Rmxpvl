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
FACTOR_LOSS_PENALTY = 0.10
SOFT_RESET_INTERVAL = 14

def clamp(v, min_v, max_v):
    return max(min_v, min(max_v, v))

def simulate_adversarial_matchmaking():
    """
    ADVERSARIAL EOMM:
    - Tilted players (HF < 0.75) get EASY opponents (HF > 0.95)
    - Hot streak (HF > 0.95) get HARD opponents (HF < 0.75)
    - This creates win streaks for tilted and lose streaks for hot!
    """
    player_state = {}
    player_skill = {}  # Track actual skill from match history
    
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
            player_skill[pid] = 0.5  # Default skill
    
    # Initialize all players
    for match in matches:
        for player in match['team_a'] + match['team_b']:
            init_player(player['id'])
    
    # Extract actual player skill from match history
    for match in matches:
        winner = match['winner']
        for player in match['team_a']:
            if winner == 0:
                player_skill[player['id']] += 1
        for player in match['team_b']:
            if winner == 1:
                player_skill[player['id']] += 1
    
    for pid in player_skill:
        player_skill[pid] /= 100  # Normalize
    
    for match_idx, match in enumerate(matches):
        all_players = match['team_a'] + match['team_b']
        
        # ADVERSARIAL ASSIGNMENT:
        # Separate by hidden_factor
        tilted = []  # HF < 0.75 (struggling)
        hot = []     # HF > 0.95 (winning)
        neutral = [] # 0.75 <= HF <= 0.95 (neutral)
        
        for player in all_players:
            pid = player['id']
            hf = player_state[pid]['hidden_factor']
            if hf < 0.75:
                tilted.append(player)
            elif hf > 0.95:
                hot.append(player)
            else:
                neutral.append(player)
        
        # MATCHMAKING LOGIC:
        # Team A: Mix tilted (need easy wins) + hot (need hard losses)
        # Team B: Mix hot (need easy losses) + tilted (need hard wins)
        # Actually, reverse: Put tilted vs hot to give tilted easy wins!
        
        team_a = tilted + neutral[len(tilted):]  # Tilted gets advantage
        team_b = hot + neutral[len(hot):]        # Hot gets handicap
        
        winner = match['winner']
        troll_count_a = match.get('troll_count_a', 0)
        troll_count_b = match.get('troll_count_b', 0)
        
        # Simulate
        for player in team_a:
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
        
        for player in team_b:
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
print("Simulating with ADVERSARIAL MATCHMAKING...")
adv_result = simulate_adversarial_matchmaking()

# Analyze
good_players = [(pid, state) for pid, state in adv_result.items() 
                if state['wins'] / state['games_played'] >= 0.55]
bad_players = [(pid, state) for pid, state in adv_result.items() 
               if state['wins'] / state['games_played'] <= 0.45]

good_wins = [s[1]['max_win_streak'] for s in good_players]
good_loses = [s[1]['max_lose_streak'] for s in good_players]
bad_wins = [s[1]['max_win_streak'] for s in bad_players]
bad_loses = [s[1]['max_lose_streak'] for s in bad_players]

print("\n" + "="*100)
print("  ADVERSARIAL MATCHMAKING RESULTS")
print("="*100)
print(f"\nGOOD PLAYERS (WR >= 55%): {len(good_players)}")
print(f"  Avg Max Win Streak: {np.mean(good_wins):.1f}  |  Avg Max Lose Streak: {np.mean(good_loses):.1f}")
print(f"\nPOOR PLAYERS (WR <= 45%): {len(bad_players)}")
print(f"  Avg Max Win Streak: {np.mean(bad_wins):.1f}  |  Avg Max Lose Streak: {np.mean(bad_loses):.1f}")
print(f"\n📊 WIN STREAK IMPACT:")
print(f"  Good has {np.mean(good_wins):.1f} vs Bad has {np.mean(bad_wins):.1f}")
print(f"  Ratio: {(np.mean(good_wins) / np.mean(bad_wins)):.2f}x (WANT: >2.0x)")
print(f"\n📊 LOSE STREAK IMPACT:")
print(f"  Good has {np.mean(good_loses):.1f} vs Bad has {np.mean(bad_loses):.1f}")
print(f"  Ratio: {(np.mean(bad_loses) / np.mean(good_loses)):.2f}x (WANT: >2.0x)")

print("\n" + "="*100 + "\n")

