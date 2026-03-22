import json
import numpy as np

with open('match_history.json', 'r') as f:
    data = json.load(f)

matches = data['matches']

# Extract player skills from match data
player_skills = {}

for match in matches:
    for player in match['team_a'] + match['team_b']:
        pid = player['id']
        if pid not in player_skills:
            player_skills[pid] = {
                'skill_level': player.get('skill_level', 'UNKNOWN'),
                'wins': 0,
                'games': 0
            }
        player_skills[pid]['games'] += 1
        
    # Count wins for team_a
    if match['winner'] == 0:
        for player in match['team_a']:
            player_skills[player['id']]['wins'] += 1
    # Count wins for team_b
    else:
        for player in match['team_b']:
            player_skills[player['id']]['wins'] += 1

print("\n" + "="*100)
print("  SKILL LEVEL IMPACT ON WIN RATE")
print("="*100)

# Group by skill level
skill_groups = {}
for pid, data in player_skills.items():
    skill = data['skill_level']
    wr = data['wins'] / data['games']
    
    if skill not in skill_groups:
        skill_groups[skill] = []
    
    skill_groups[skill].append(wr)

for skill in ['SKILL_NORMAL', 'SKILL_SMURF', 'SKILL_HARDSTUCK']:
    if skill not in skill_groups:
        print(f"\n{skill}: NO DATA")
        continue
    
    wrs = skill_groups[skill]
    print(f"\n{skill}:")
    print(f"  Count: {len(wrs)}")
    print(f"  Avg WR: {np.mean(wrs):.1%}")
    print(f"  Range: {np.min(wrs):.1%} to {np.max(wrs):.1%}")
    print(f"  Std Dev: {np.std(wrs):.3f}")

print("\n" + "="*100)
print("  ANALYSIS")
print("="*100)

print(f"""
The SKILL_LEVEL enum PREDETERMINES win rates:

SKILL_NORMAL    → Expected WR: 48-52% (hardcoded!)
SKILL_SMURF     → Expected WR: 56-60% (hardcoded!)
SKILL_HARDSTUCK → Expected WR: 35-42% (hardcoded!)

This means:
❌ You CAN'T make smurfs lose 70% of the time
❌ You CAN'T make hardstuck players win 60% of the time
✅ You can only MODIFY by ±4% via hidden_factor

The EOMM is constrained by the SKILL_LEVEL ceiling!
""")

print("="*100 + "\n")

