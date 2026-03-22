import re

with open('src/eomm_system.c', 'r') as f:
    content = f.read()

# Find and replace calculate_actual_winrate
old = r'''float calculate_actual_winrate\(const Player \*p\) \{
    /\* Equal-weight average of all eight performance stats \*/
    float avg = \(p->perf\.mechanical_skill
               \+ p->perf\.decision_making
               \+ p->perf\.map_awareness
               \+ p->perf\.tilt_resistance
               \+ p->perf\.champion_pool_depth
               \+ p->perf\.champion_proficiency
               \+ p->perf\.wave_management
               \+ p->perf\.teamfight_positioning\) / 8\.0f;

    /\* Map \[0, 1\] performance to \[0\.25, 0\.75\] win rate \*/
    float wr = 0\.25f \+ avg \* 0\.50f;

    /\* Tilt penalty: emotionally fragile players drop further when negative \*/
    if \(p->hidden_state == STATE_NEGATIVE\) \{
        float fragility = 1\.0f - p->perf\.tilt_resistance;
        wr -= fragility \* 0\.05f; /\* up to -5% for zero tilt_resistance \*/
    \}

    return clampf\(wr, 0\.25f, 0\.75f\);
\}'''

new = '''float calculate_actual_winrate(const Player *p) {
    /* Equal-weight average of all eight performance stats */
    float avg = (p->perf.mechanical_skill
               + p->perf.decision_making
               + p->perf.map_awareness
               + p->perf.tilt_resistance
               + p->perf.champion_pool_depth
               + p->perf.champion_proficiency
               + p->perf.wave_management
               + p->perf.teamfight_positioning) / 8.0f;

    /* Map [0, 1] performance to [0.25, 0.75] win rate */
    float wr = 0.25f + avg * 0.50f;

    /* MODIFICATION 1: hidden_factor NOW directly affects WR */
    /* This creates the streak feedback loop:
       - Winning players (HF > 1.0) get higher WR
       - Losing players (HF < 1.0) get lower WR
       - Creates self-reinforcing streaks */
    wr *= p->hidden_factor;

    if (p->hidden_state == STATE_NEGATIVE) {
        float fragility = 1.0f - p->perf.tilt_resistance;
        wr -= fragility * 0.05f; /* up to -5% for zero tilt_resistance */
    }

    return clampf(wr, 0.25f, 0.75f);
}'''

if re.search(old, content):
    content = re.sub(old, new, content)
    print("✓ Patched calculate_actual_winrate() with regex")
else:
    print("⚠ Regex didn't match, trying manual approach...")
    # Find the function and replace it manually
    start = content.find("float calculate_actual_winrate(const Player *p) {")
    if start != -1:
        # Find the closing brace
        brace_count = 0
        end = start
        for i in range(start, len(content)):
            if content[i] == '{':
                brace_count += 1
            elif content[i] == '}':
                brace_count -= 1
                if brace_count == 0:
                    end = i + 1
                    break
        
        content = content[:start] + new + content[end:]
        print("✓ Patched calculate_actual_winrate() manually")
    else:
        print("✗ Could not find calculate_actual_winrate()")

with open('src/eomm_system.c', 'w') as f:
    f.write(content)

