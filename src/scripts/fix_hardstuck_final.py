import re

with open('src/eomm_system.c', 'r') as f:
    content = f.read()

# Revert blend à 50/50 pour tous
old = '''    float hf_blend;
    switch (p->skill_level) {
        case SKILL_SMURF:
            /* Smurfs: 60% base + 40% HF → less volatile */
            hf_blend = 0.6f + 0.4f * p->hidden_factor;  /* ranges [0.8, 1.08] */
            break;
        case SKILL_HARDSTUCK:
            /* Hardstuck: 70% base + 30% HF → MUCH less volatile, skill matters more */
            hf_blend = 0.7f + 0.3f * p->hidden_factor;  /* ranges [0.85, 1.06] */
            break;
        default: /* SKILL_NORMAL */
            /* Normal: 50% base + 50% HF → balanced */
            hf_blend = 0.5f + 0.5f * p->hidden_factor;  /* ranges [0.75, 1.1] */
            break;
    }'''

new = '''    float hf_blend = 0.5f + 0.5f * p->hidden_factor;  /* ranges [0.75, 1.1] */'''

content = content.replace(old, new)

# Réduire hardstuck range DRASTIQUEMENT
old2 = '''        case SKILL_HARDSTUCK:
            lo = 0.02f; hi = 0.18f;  /* REDUCED AGAIN: was [0.05, 0.25] */
            adj_delta = +0.06f; /* one stronger stat */
            break;'''

new2 = '''        case SKILL_HARDSTUCK:
            lo = 0.01f; hi = 0.12f;  /* DRASTIC REDUCTION */
            adj_delta = +0.04f; /* minimal boost */
            break;'''

content = content.replace(old2, new2)

# Réduire FACTOR_LOSS_PENALTY back
old3 = '#define FACTOR_LOSS_PENALTY     0.30f  /* factor lost on a loss */ /* INCREASED from 0.20 */'
new3 = '#define FACTOR_LOSS_PENALTY     0.20f  /* factor lost on a loss */'

content = content.replace(old3, new3)

with open('src/eomm_system.c', 'w') as f:
    f.write(content)

print("✓ Reverted blend, drastically reduced hardstuck [0.01, 0.12]")

