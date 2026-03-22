import re

with open('src/eomm_system.c', 'r') as f:
    content = f.read()

# Find the function
old = '''    /* MODIFICATION 1: hidden_factor NOW directly affects WR */
    /* This creates the streak feedback loop:
       - Winning players (HF > 1.0) get higher WR
       - Losing players (HF < 1.0) get lower WR
       - Creates self-reinforcing streaks */
    wr *= p->hidden_factor;

    if (p->hidden_state == STATE_NEGATIVE) {
        float fragility = 1.0f - p->perf.tilt_resistance;
        wr -= fragility * 0.05f; /* up to -5% for zero tilt_resistance */
    }

    return clampf(wr, 0.25f, 0.75f);'''

new = '''    if (p->hidden_state == STATE_NEGATIVE) {
        float fragility = 1.0f - p->perf.tilt_resistance;
        wr -= fragility * 0.05f; /* up to -5% for zero tilt_resistance */
    }

    wr = clampf(wr, 0.25f, 0.75f);

    /* MODIFICATION 1: hidden_factor NOW affects WR AFTER clamping */
    /* Blend: keep 50% of base WR, add 50% of hidden_factor boost */
    /* This creates streak feedback without destroying skill hierarchy */
    float hf_blend = 0.5f + 0.5f * p->hidden_factor;  /* ranges [0.75, 1.1] */
    wr *= hf_blend;

    return clampf(wr, 0.25f, 0.75f);'''

content = content.replace(old, new)

with open('src/eomm_system.c', 'w') as f:
    f.write(content)

print("✓ Fixed hidden_factor blending")

