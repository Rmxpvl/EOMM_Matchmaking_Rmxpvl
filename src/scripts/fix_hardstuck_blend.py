import re

with open('src/eomm_system.c', 'r') as f:
    content = f.read()

old = '''    float hf_blend;
    switch (p->skill_level) {
        case SKILL_SMURF:
            /* Smurfs: 60% base + 40% HF → less volatile */
            hf_blend = 0.6f + 0.4f * p->hidden_factor;  /* ranges [0.8, 1.08] */
            break;
        case SKILL_HARDSTUCK:
            /* Hardstuck: 40% base + 60% HF → more volatile, streaky */
            hf_blend = 0.4f + 0.6f * p->hidden_factor;  /* ranges [0.7, 1.12] */
            break;
        default: /* SKILL_NORMAL */
            /* Normal: 50% base + 50% HF → balanced */
            hf_blend = 0.5f + 0.5f * p->hidden_factor;  /* ranges [0.75, 1.1] */
            break;
    }'''

new = '''    float hf_blend;
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

content = content.replace(old, new)

with open('src/eomm_system.c', 'w') as f:
    f.write(content)

print("✓ Changed hardstuck blend to 70% base + 30% HF")

