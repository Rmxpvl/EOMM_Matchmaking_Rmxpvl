import re

with open('src/eomm_system.c', 'r') as f:
    content = f.read()

# Revert hardstuck à [0.01, 0.12]
old = '''        case SKILL_HARDSTUCK:
            lo = 0.08f; hi = 0.28f;  /* Balanced: better variance, target ~42-46% WR */
            adj_delta = +0.06f; /* modest boost */
            break;'''

new = '''        case SKILL_HARDSTUCK:
            lo = 0.01f; hi = 0.12f;  /* Tight range for low WR */
            adj_delta = +0.04f; /* minimal boost */
            break;'''

content = content.replace(old, new)

# Ajouter boost smurf dans calculate_actual_winrate
old2 = '''    wr = clampf(wr, 0.25f, 0.75f);

    /* MODIFICATION 1: hidden_factor NOW affects WR AFTER clamping */'''

new2 = '''    wr = clampf(wr, 0.25f, 0.75f);

    /* Smurf skill bonus: +3% to their win rate */
    if (p->skill_level == SKILL_SMURF) {
        wr += 0.03f;
    }

    /* MODIFICATION 1: hidden_factor NOW affects WR AFTER clamping */'''

content = content.replace(old2, new2)

with open('src/eomm_system.c', 'w') as f:
    f.write(content)

print("✓ Reverted hardstuck [0.01, 0.12] + Added +3% smurf bonus")

