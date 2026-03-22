import re

with open('src/eomm_system.c', 'r') as f:
    content = f.read()

# Augmente la pénalité de perte
old = '#define FACTOR_LOSS_PENALTY     0.20f  /* factor lost on a loss */'
new = '#define FACTOR_LOSS_PENALTY     0.30f  /* factor lost on a loss */ /* INCREASED from 0.20 */'

content = content.replace(old, new)

with open('src/eomm_system.c', 'w') as f:
    f.write(content)

print("✓ Increased FACTOR_LOSS_PENALTY from 0.20 to 0.30")

