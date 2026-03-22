import re

with open('src/eomm_system.c', 'r') as f:
    content = f.read()

old = '''        case SKILL_HARDSTUCK:
            lo = 0.01f; hi = 0.12f;  /* DRASTIC REDUCTION */
            adj_delta = +0.04f; /* minimal boost */
            break;'''

new = '''        case SKILL_HARDSTUCK:
            lo = 0.08f; hi = 0.28f;  /* Balanced: better variance, target ~42-46% WR */
            adj_delta = +0.06f; /* modest boost */
            break;'''

content = content.replace(old, new)

with open('src/eomm_system.c', 'w') as f:
    f.write(content)

print("✓ Expanded hardstuck range to [0.08, 0.28]")

