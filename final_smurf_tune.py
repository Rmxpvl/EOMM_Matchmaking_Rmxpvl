import re

with open('src/eomm_system.c', 'r') as f:
    content = f.read()

old = '''        case SKILL_SMURF:
            lo = 0.65f; hi = 0.82f;  /* REDUCED: was [0.70, 0.90] */
            adj_delta = -0.08f; /* one weaker stat */
            break;'''

new = '''        case SKILL_SMURF:
            lo = 0.62f; hi = 0.78f;  /* FURTHER REDUCED */
            adj_delta = -0.08f; /* one weaker stat */
            break;'''

content = content.replace(old, new)

with open('src/eomm_system.c', 'w') as f:
    f.write(content)

print("✓ Reduced smurf peak [0.62, 0.78]")

