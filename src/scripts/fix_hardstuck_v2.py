import re

with open('src/eomm_system.c', 'r') as f:
    content = f.read()

# Réduire encore la range hardstuck
old = '''        case SKILL_HARDSTUCK:
            lo = 0.05f; hi = 0.25f;  /* REDUCED: was [0.10, 0.35] */
            adj_delta = +0.08f; /* one stronger stat */
            break;'''

new = '''        case SKILL_HARDSTUCK:
            lo = 0.02f; hi = 0.18f;  /* REDUCED AGAIN: was [0.05, 0.25] */
            adj_delta = +0.06f; /* one stronger stat */
            break;'''

content = content.replace(old, new)

# Augmenter le tilt penalty pour hardstuck dans calculate_actual_winrate
old2 = '''    if (p->hidden_state == STATE_NEGATIVE) {
        float fragility = 1.0f - p->perf.tilt_resistance;
        wr -= fragility * 0.05f; /* up to -5% for zero tilt_resistance */
    }'''

new2 = '''    if (p->hidden_state == STATE_NEGATIVE) {
        float fragility = 1.0f - p->perf.tilt_resistance;
        float penalty = fragility * 0.05f; /* base penalty */
        
        /* Hardstuck players suffer more when tilted */
        if (p->skill_level == SKILL_HARDSTUCK) {
            penalty *= 2.0f;  /* 2x penalty for hardstuck */
        }
        wr -= penalty;
    }'''

content = content.replace(old2, new2)

with open('src/eomm_system.c', 'w') as f:
    f.write(content)

print("✓ Further reduced hardstuck range [0.02, 0.18] + 2x tilt penalty")

