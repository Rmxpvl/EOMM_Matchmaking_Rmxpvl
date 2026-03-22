import re

with open('src/eomm_system.c', 'r') as f:
    content = f.read()

old = '''void update_tilt(Player *p, int did_win) {
    if (did_win) {
        p->win_streak++;
        p->lose_streak = 0;
        
        /* MODIFICATION 2: Increase win bonus 4x
           OLD: +0.02
           NEW: +0.08 (scale with streak length for super-linearity) */
        float bonus = FACTOR_WIN_BONUS;
        if (p->win_streak > 3) {
            bonus *= 1.5f;  /* 1.5x multiplier on streak */
        }
        p->hidden_factor += bonus;
        
        // ...
    } else {
        p->lose_streak++;
        p->win_streak = 0;
        
        /* MODIFICATION 3: Increase loss penalty 4x
           OLD: -0.05
           NEW: -0.20 (scale with streak length for super-linearity) */
        float penalty = FACTOR_LOSS_PENALTY;
        if (p->lose_streak > 3) {
            penalty *= 1.5f;  /* 1.5x multiplier on streak */
        }
        p->hidden_factor -= penalty;
        
        // ...
    }
    
    p->hidden_factor = clampf(p->hidden_factor, HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX);
    update_hidden_state(p);
}'''

new = '''void update_tilt(Player *p, int did_win) {
    if (did_win) {
        p->win_streak++;
        p->lose_streak = 0;
        
        /* MODIFICATION 2: Increase win bonus 4x
           OLD: +0.02
           NEW: +0.08 (scale with streak length for super-linearity) */
        float bonus = FACTOR_WIN_BONUS;
        if (p->win_streak > 3) {
            bonus *= 1.5f;  /* 1.5x multiplier on streak */
        }
        p->hidden_factor += bonus;
        
        /* Reduce tilt on win */
        if (p->tilt_level > 0) p->tilt_level--;
        
        /* Reset consecutive trolls on clean win */
        if (!p->is_troll_pick) {
            p->consecutive_trolls = 0;
        }
    } else {
        p->lose_streak++;
        p->win_streak = 0;
        
        /* MODIFICATION 3: Increase loss penalty 4x
           OLD: -0.05
           NEW: -0.20 (scale with streak length for super-linearity) */
        float penalty = FACTOR_LOSS_PENALTY;
        if (p->lose_streak > 3) {
            penalty *= 1.5f;  /* 1.5x multiplier on streak */
        }
        p->hidden_factor -= penalty;
        
        /* Increase tilt on loss */
        if (p->lose_streak >= 3) {
            p->tilt_level = 2;
        } else if (p->lose_streak >= 1) {
            p->tilt_level = 1;
        }
        
        /* Anger resets arrogance: reset consecutive trolls */
        p->consecutive_trolls = 0;
    }
    
    p->hidden_factor = clampf(p->hidden_factor, HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX);
    update_hidden_state(p);
}'''

content = content.replace(old, new)

with open('src/eomm_system.c', 'w') as f:
    f.write(content)

print("✓ Fixed update_tilt() function")

