import re

with open('src/eomm_system.c', 'r') as f:
    content = f.read()

# Trouver la section calculate_mmr_change
old = '''    /* Ranked play (after placement) */
    if (games_played >= PLACEMENT_GAMES) {
        k = K_FACTOR_RANKED;
    }'''

new = '''    /* Ranked play (after placement) */
    if (games_played >= PLACEMENT_GAMES) {
        k = K_FACTOR_RANKED;
        
        /* Hardstuck players gain/lose less LP */
        if (p->skill_level == SKILL_HARDSTUCK) {
            k *= 0.7f;  /* 30% reduction in LP gains/losses */
        }
    }'''

content = content.replace(old, new)

with open('src/eomm_system.c', 'w') as f:
    f.write(content)

print("✓ Added 0.7x K-factor for hardstuck players")

