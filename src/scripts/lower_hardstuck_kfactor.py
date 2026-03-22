import re

with open('src/eomm_system.c', 'r') as f:
    content = f.read()

old = '''        /* Hardstuck players gain/lose less LP */
        if (p->skill_level == SKILL_HARDSTUCK) {
            k *= 0.7f;  /* 30% reduction in LP gains/losses */
        }'''

new = '''        /* Hardstuck players gain/lose much less LP */
        if (p->skill_level == SKILL_HARDSTUCK) {
            k *= 0.5f;  /* 50% reduction in LP gains/losses */
        }'''

content = content.replace(old, new)

with open('src/eomm_system.c', 'w') as f:
    f.write(content)

print("✓ Reduced hardstuck K-factor to 0.5x")

