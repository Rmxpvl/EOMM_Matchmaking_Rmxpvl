import re

with open('src/eomm_system.c', 'r') as f:
    content = f.read()

# Réduire la range smurf
old = '''        case SKILL_SMURF:
            lo = 0.70f; hi = 0.90f;
            adj_delta = -0.10f; /* one weaker stat */
            break;'''

new = '''        case SKILL_SMURF:
            lo = 0.65f; hi = 0.82f;  /* REDUCED: was [0.70, 0.90] */
            adj_delta = -0.08f; /* one weaker stat */
            break;'''

content = content.replace(old, new)

# Retirer le boost smurf qu'on a ajouté
old2 = '''    wr = clampf(wr, 0.25f, 0.75f);

    /* Smurf skill bonus: +3% to their win rate */
    if (p->skill_level == SKILL_SMURF) {
        wr += 0.03f;
    }

    /* MODIFICATION 1: hidden_factor NOW affects WR AFTER clamping */'''

new2 = '''    wr = clampf(wr, 0.25f, 0.75f);

    /* MODIFICATION 1: hidden_factor NOW affects WR AFTER clamping */'''

content = content.replace(old2, new2)

with open('src/eomm_system.c', 'w') as f:
    f.write(content)

print("✓ Reduced smurf peak [0.65, 0.82] + removed +3% bonus")

