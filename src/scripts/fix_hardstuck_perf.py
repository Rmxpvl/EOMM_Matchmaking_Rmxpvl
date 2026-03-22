import re

with open('src/eomm_system.c', 'r') as f:
    content = f.read()

# Cherche et remplace la section init_player hardstuck
old = '''    switch (skill) {
        case SKILL_SMURF:
            lo = 0.70f; hi = 0.90f;
            adj_delta = -0.10f; /* one weaker stat */
            break;
        case SKILL_HARDSTUCK:
            lo = 0.10f; hi = 0.35f;
            adj_delta = +0.10f; /* one stronger stat */
            break;'''

new = '''    switch (skill) {
        case SKILL_SMURF:
            lo = 0.70f; hi = 0.90f;
            adj_delta = -0.10f; /* one weaker stat */
            break;
        case SKILL_HARDSTUCK:
            lo = 0.05f; hi = 0.25f;  /* REDUCED: was [0.10, 0.35] */
            adj_delta = +0.08f; /* one stronger stat */
            break;'''

content = content.replace(old, new)

# Aussi update la comment
old2 = '''   HARDSTUCK : [0.10, 0.35]'''
new2 = '''   HARDSTUCK : [0.05, 0.25]'''
content = content.replace(old2, new2)

with open('src/eomm_system.c', 'w') as f:
    f.write(content)

print("✓ Reduced hardstuck performance range [0.05, 0.25]")

