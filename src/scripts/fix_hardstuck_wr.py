import re

with open('src/eomm_system.c', 'r') as f:
    content = f.read()

old = '''    wr = clampf(wr, 0.25f, 0.75f);

    /* MODIFICATION 1: hidden_factor NOW affects WR AFTER clamping */
    /* Blend: keep 50% of base WR, add 50% of hidden_factor boost */
    /* This creates streak feedback without destroying skill hierarchy */
    float hf_blend = 0.5f + 0.5f * p->hidden_factor;  /* ranges [0.75, 1.1] */
    wr *= hf_blend;

    return clampf(wr, 0.25f, 0.75f);'''

new = '''    wr = clampf(wr, 0.25f, 0.75f);

    /* MODIFICATION 1: hidden_factor NOW affects WR AFTER clamping */
    /* Different blend intensity by skill level:
       - Smurfs: weaker blend (they're already strong)
       - Normal: medium blend
       - Hardstuck: stronger blend (streak effects matter more)
    */
    float hf_blend;
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
    }
    wr *= hf_blend;

    return clampf(wr, 0.25f, 0.75f);'''

content = content.replace(old, new)

with open('src/eomm_system.c', 'w') as f:
    f.write(content)

print("✓ Fixed hardstuck WR blend")

