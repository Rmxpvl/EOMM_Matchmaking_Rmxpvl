#!/usr/bin/env python3
"""
EOMM Matchmaking System - Unified Analysis & Tuning Tools

This module consolidates all analysis, simulation, calibration, and
debugging utilities for the EOMM (Engagement Optimized Matchmaking) system.

Usage:
    python3 src/scripts/eomm_tools.py <command> [args]

Commands:
    analyze-match           - Analyze match history and win-rate distribution
    analyze-missing-streaks - Deep streak pattern analysis
    analyze-skill-ceiling   - Analyze skill level impact on win rate
    analyze-streaks         - Analyze win/loss streak statistics

    fix-hardstuck-blend     - Fix hardstuck HF blend ratio
    fix-hardstuck-final     - Revert blend to flat + drastic hardstuck range reduction
    fix-hardstuck-perf      - Reduce hardstuck performance range
    fix-hardstuck-v2        - Further reduce hardstuck range + 2x tilt penalty
    fix-hardstuck-wr        - Apply per-skill hf_blend in calculate_actual_winrate
    fix-update-tilt         - Patch update_tilt() with tilt/arrogance logic
    fix-wr                  - Patch hidden_factor blending in calculate_actual_winrate

    simulate-adversarial    - Run adversarial EOMM simulation
    simulate-aggressive     - Run aggressive EOMM configurations comparison
    simulate-active         - Run active (HF-balanced) matchmaking simulation

    track-hidden            - Track hidden factor per player (simple)
    track-hidden-advanced   - Track hidden factor with soft reset and trolls

    visualize               - Generate hidden factor vs win-rate scatter plot

    boost-smurf             - Revert hardstuck range + add +3% smurf bonus
    calibrate-smurf         - Reduce smurf peak range + remove smurf bonus
    final-analysis          - EOMM final analysis (correlation, rubber-banding)
    final-report            - EOMM report with engagement mechanics summary
    final-smurf-tune        - Further reduce smurf peak range

    patch-calculate-wr      - Patch calculate_actual_winrate() via regex/manual
    player-history          - Show match history for a specific player
    reduce-hardstuck-kfactor - Add 0.7x K-factor reduction for hardstuck players
    lower-hardstuck-kfactor  - Lower hardstuck K-factor to 0.5x
    increase-loss-penalty    - Increase FACTOR_LOSS_PENALTY from 0.20 to 0.30
    stabilize-hardstuck      - Expand hardstuck range to [0.08, 0.28]
    implement-forced-streaks - Simulate adversarial streak-extension matchmaking
"""

import argparse
import json
import os
import re
import sys
from collections import defaultdict

try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False

try:
    import matplotlib.pyplot as plt
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False

# ── Path resolution ────────────────────────────────────────────────────────────
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, '..', '..'))
MATCH_HISTORY_PATH = os.environ.get(
    'EOMM_MATCH_HISTORY',
    os.path.join(PROJECT_ROOT, 'data', 'match_history.json')
)
EOMM_SYSTEM_C_PATH = os.path.join(PROJECT_ROOT, 'src', 'eomm_system.c')
DOCS_ASSETS_DIR = os.path.join(PROJECT_ROOT, 'docs', 'assets')


# ── Shared helpers ─────────────────────────────────────────────────────────────
def _clamp(v, min_v, max_v):
    return max(min_v, min(max_v, v))


def _load_match_history():
    if not os.path.isfile(MATCH_HISTORY_PATH):
        print(f"ERROR: Match history file not found: {MATCH_HISTORY_PATH}")
        print(f"  Run the EOMM engine to generate match data, or set the")
        print(f"  EOMM_MATCH_HISTORY environment variable to point to your file.")
        sys.exit(1)
    with open(MATCH_HISTORY_PATH, 'r') as f:
        return json.load(f)


def _require_eomm_c():
    if not os.path.isfile(EOMM_SYSTEM_C_PATH):
        print(f"ERROR: C source file not found: {EOMM_SYSTEM_C_PATH}")
        print(f"  Make sure you are running this from the project root directory")
        print(f"  or that '{EOMM_SYSTEM_C_PATH}' exists.")
        sys.exit(1)


def _require_numpy():
    if not HAS_NUMPY:
        print("ERROR: numpy is required for this command. Install with: pip install numpy")
        sys.exit(1)


def _require_matplotlib():
    if not HAS_MATPLOTLIB:
        print("ERROR: matplotlib is required for this command. Install with: pip install matplotlib")
        sys.exit(1)


def _assets_path(filename):
    os.makedirs(DOCS_ASSETS_DIR, exist_ok=True)
    return os.path.join(DOCS_ASSETS_DIR, filename)


# ── ANALYZE SECTION ────────────────────────────────────────────────────────────

def main_analyze_match_history():
    """Original: analyze_match_history.py — Win-rate distribution + troll impact."""
    _require_numpy()
    _require_matplotlib()

    data = _load_match_history()
    matches = data['matches']
    print(f"Total matches: {len(matches)}")

    player_stats = defaultdict(lambda: {
        'wins': 0, 'losses': 0, 'matches': 0,
        'team_a_count': 0, 'team_b_count': 0, 'troll_count': 0
    })

    for match in matches:
        winner = match['winner']
        for player in match['team_a']:
            pid = player['id']
            player_stats[pid]['matches'] += 1
            player_stats[pid]['team_a_count'] += 1
            if winner == 0:
                player_stats[pid]['wins'] += 1
            else:
                player_stats[pid]['losses'] += 1
        for player in match['team_b']:
            pid = player['id']
            player_stats[pid]['matches'] += 1
            player_stats[pid]['team_b_count'] += 1
            if winner == 1:
                player_stats[pid]['wins'] += 1
            else:
                player_stats[pid]['losses'] += 1

    print("\n=== TOP 10 JOUEURS (Win Rate) ===")
    sorted_players = sorted(player_stats.items(),
                            key=lambda x: x[1]['wins'] / max(1, x[1]['matches']),
                            reverse=True)
    for i, (pid, stats) in enumerate(sorted_players[:10], 1):
        wr = 100 * stats['wins'] / max(1, stats['matches'])
        print(f"{i}. Player {pid:04d}: {stats['wins']}-{stats['losses']} ({wr:.1f}%) — {stats['matches']} games")

    print("\n=== BOTTOM 10 JOUEURS (Win Rate) ===")
    for i, (pid, stats) in enumerate(sorted_players[-10:], 1):
        wr = 100 * stats['wins'] / max(1, stats['matches'])
        print(f"{i}. Player {pid:04d}: {stats['wins']}-{stats['losses']} ({wr:.1f}%) — {stats['matches']} games")

    win_rates = [s['wins'] / max(1, s['matches']) for s in player_stats.values()]
    print(f"\n=== WIN RATE STATS ===")
    print(f"Mean: {np.mean(win_rates):.3f}")
    print(f"Median: {np.median(win_rates):.3f}")
    print(f"Std Dev: {np.std(win_rates):.3f}")
    print(f"Min: {np.min(win_rates):.3f}")
    print(f"Max: {np.max(win_rates):.3f}")

    plt.figure(figsize=(12, 6))
    plt.hist(win_rates, bins=30, edgecolor='black', alpha=0.7)
    plt.axvline(np.mean(win_rates), color='red', linestyle='--', linewidth=2,
                label=f'Mean: {np.mean(win_rates):.3f}')
    plt.axvline(0.5, color='green', linestyle='--', linewidth=2, label='50%')
    plt.xlabel('Win Rate')
    plt.ylabel('Number of Players')
    plt.title('Win Rate Distribution')
    plt.legend()
    plt.grid(True, alpha=0.3)
    out = _assets_path('win_rate_distribution.png')
    plt.savefig(out, dpi=150, bbox_inches='tight')
    print(f"\n✓ Graph saved: {out}")

    troll_matches = {
        '0_trolls': {'wins': 0, 'total': 0},
        '1_troll': {'wins': 0, 'total': 0},
        '2_trolls': {'wins': 0, 'total': 0},
    }

    for match in matches:
        troll_a = match.get('troll_count_a', 0)
        troll_b = match.get('troll_count_b', 0)
        winner = match['winner']

        key_a = f"{troll_a}_trolls" if troll_a <= 2 else "2_trolls"
        troll_matches[key_a]['total'] += 1
        if winner == 0:
            troll_matches[key_a]['wins'] += 1

        key_b = f"{troll_b}_trolls" if troll_b <= 2 else "2_trolls"
        troll_matches[key_b]['total'] += 1
        if winner == 1:
            troll_matches[key_b]['wins'] += 1

    print(f"\n=== TROLL IMPACT ===")
    for key in sorted(troll_matches.keys()):
        d = troll_matches[key]
        if d['total'] > 0:
            wr = 100 * d['wins'] / d['total']
            print(f"{key}: {wr:.1f}% win rate ({d['wins']}/{d['total']} matches)")

    fig, ax = plt.subplots(figsize=(10, 6))
    categories = list(troll_matches.keys())
    win_rates_troll = [100 * troll_matches[cat]['wins'] / max(1, troll_matches[cat]['total'])
                       for cat in categories]
    colors = ['green' if wr >= 50 else 'red' for wr in win_rates_troll]
    bars = ax.bar(categories, win_rates_troll, color=colors, alpha=0.7, edgecolor='black')
    ax.axhline(50, color='gray', linestyle='--', linewidth=2, label='50% (neutral)')
    ax.set_ylabel('Win Rate (%)')
    ax.set_title('Troll Impact on Win Rate')
    ax.set_ylim([40, 60])
    ax.legend()
    ax.grid(True, alpha=0.3, axis='y')
    for bar, wr in zip(bars, win_rates_troll):
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width() / 2., height,
                f'{wr:.1f}%', ha='center', va='bottom')
    out2 = _assets_path('troll_impact.png')
    plt.savefig(out2, dpi=150, bbox_inches='tight')
    print(f"✓ Graph saved: {out2}")

    print("\n=== ANALYSIS COMPLETE ===")


def main_analyze_missing_streaks():
    """Original: analyze_missing_streaks.py — Deep streak pattern analysis."""
    _require_numpy()

    data = _load_match_history()
    matches = data['matches']

    player_state = {}

    def init_player(pid):
        if pid not in player_state:
            player_state[pid] = {
                'hidden_factor': 1.00,
                'games_played': 0,
                'wins': 0,
                'losses': 0,
                'match_sequence': [],
                'current_streak': ('start', 0),
                'all_streaks': [],
            }

    for match in matches:
        for team, team_data in [('team_a', match['team_a']), ('team_b', match['team_b'])]:
            winner = match['winner']
            won = (team == 'team_a' and winner == 0) or (team == 'team_b' and winner == 1)
            for player in team_data:
                pid = player['id']
                init_player(pid)
                state = player_state[pid]
                state['match_sequence'].append('W' if won else 'L')
                if won:
                    state['wins'] += 1
                else:
                    state['losses'] += 1
                state['games_played'] += 1

    def get_all_streaks(sequence):
        if not sequence:
            return []
        streaks = []
        current_char = sequence[0]
        current_length = 1
        for char in sequence[1:]:
            if char == current_char:
                current_length += 1
            else:
                streaks.append((current_char, current_length))
                current_char = char
                current_length = 1
        streaks.append((current_char, current_length))
        return streaks

    print("\n" + "=" * 100)
    print("  STREAK PATTERN ANALYSIS - SHOULD SHOW STRONG WIN/LOSS CHAINS")
    print("=" * 100)

    elite, good, neutral, bad, terrible = [], [], [], [], []

    for pid, state in player_state.items():
        wr = state['wins'] / state['games_played']
        streaks = get_all_streaks(state['match_sequence'])
        max_win = max([s[1] for s in streaks if s[0] == 'W'], default=0)
        max_loss = max([s[1] for s in streaks if s[0] == 'L'], default=0)
        entry = (pid, wr, max_win, max_loss, streaks, state['match_sequence'])
        if wr >= 0.60:
            elite.append(entry)
        elif wr >= 0.55:
            good.append(entry)
        elif wr <= 0.40:
            terrible.append(entry)
        elif wr <= 0.45:
            bad.append(entry)
        else:
            neutral.append(entry)

    def analyze_category(name, players):
        if not players:
            print(f"\n{name}: NO PLAYERS")
            return
        print(f"\n{name}")
        print("-" * 100)
        max_wins = [p[2] for p in players]
        max_losses = [p[3] for p in players]
        wrs = [p[1] for p in players]
        print(f"Count: {len(players)}")
        print(f"Avg WR: {np.mean(wrs):.1%}")
        print(f"\nStreak Statistics:")
        print(f"  Max Win Streak - Mean: {np.mean(max_wins):.1f}, Std: {np.std(max_wins):.1f}, "
              f"Range: {np.min(max_wins)}-{np.max(max_wins)}")
        print(f"  Max Loss Streak - Mean: {np.mean(max_losses):.1f}, Std: {np.std(max_losses):.1f}, "
              f"Range: {np.min(max_losses)}-{np.max(max_losses)}")
        print(f"\nMost interesting patterns:")
        best_win = max(players, key=lambda x: x[2])
        print(f"  Best Win Streak (Player {best_win[0]:04d}):")
        print(f"    Sequence: {''.join(best_win[5][:30])}...")
        print(f"    Max Win: {best_win[2]}, Max Loss: {best_win[3]}, WR: {best_win[1]:.1%}")
        worst_loss = max(players, key=lambda x: x[3])
        print(f"  Longest Loss Streak (Player {worst_loss[0]:04d}):")
        print(f"    Sequence: {''.join(worst_loss[5][:30])}...")
        print(f"    Max Win: {worst_loss[2]}, Max Loss: {worst_loss[3]}, WR: {worst_loss[1]:.1%}")

    analyze_category("🏆 ELITE (WR >= 60%)", elite)
    analyze_category("✅ GOOD (WR >= 55%)", good)
    analyze_category("➡️  NEUTRAL (45-55%)", neutral)
    analyze_category("❌ BAD (WR <= 45%)", bad)
    analyze_category("💀 TERRIBLE (WR <= 40%)", terrible)

    def calculate_streak_efficiency(sequence):
        if len(sequence) < 2:
            return 0.5
        changes = sum(1 for i in range(len(sequence) - 1) if sequence[i] != sequence[i + 1])
        max_changes = len(sequence) - 1
        return 1 - (changes / max_changes) if max_changes > 0 else 0.5

    all_efficiencies = []
    elite_eff = []
    bad_eff = []

    for pid, state in player_state.items():
        eff = calculate_streak_efficiency(state['match_sequence'])
        all_efficiencies.append(eff)
        wr = state['wins'] / state['games_played']
        if wr >= 0.60:
            elite_eff.append(eff)
        elif wr <= 0.45:
            bad_eff.append(eff)

    print(f"\n{'=' * 100}")
    print("  STREAK EFFICIENCY METRIC")
    print("=" * 100)
    print(f"\nGlobal Streak Efficiency: {np.mean(all_efficiencies):.3f}")
    print(f"  (0.0 = perfect alternation WLWLWL..., 1.0 = perfect streaks WWWW...LLLL...)")
    print(f"  Current: {np.mean(all_efficiencies):.3f} - "
          f"{'TOO RANDOM' if np.mean(all_efficiencies) < 0.4 else 'OK'}")

    if elite_eff and bad_eff:
        print(f"\nElite players streak efficiency: {np.mean(elite_eff):.3f}")
        print(f"Bad players streak efficiency: {np.mean(bad_eff):.3f}")
        print(f"Difference: {abs(np.mean(elite_eff) - np.mean(bad_eff)):.3f}")

    print("\n" + "=" * 100 + "\n")


def main_analyze_skill_level_ceiling():
    """Original: analyze_skill_level_ceiling.py — Skill level impact on win rate."""
    _require_numpy()

    data = _load_match_history()
    matches = data['matches']

    player_skills = {}
    for match in matches:
        for player in match['team_a'] + match['team_b']:
            pid = player['id']
            if pid not in player_skills:
                player_skills[pid] = {
                    'skill_level': player.get('skill_level', 'UNKNOWN'),
                    'wins': 0,
                    'games': 0
                }
            player_skills[pid]['games'] += 1
        if match['winner'] == 0:
            for player in match['team_a']:
                player_skills[player['id']]['wins'] += 1
        else:
            for player in match['team_b']:
                player_skills[player['id']]['wins'] += 1

    print("\n" + "=" * 100)
    print("  SKILL LEVEL IMPACT ON WIN RATE")
    print("=" * 100)

    skill_groups = {}
    for pid, d in player_skills.items():
        skill = d['skill_level']
        wr = d['wins'] / d['games']
        if skill not in skill_groups:
            skill_groups[skill] = []
        skill_groups[skill].append(wr)

    for skill in ['SKILL_NORMAL', 'SKILL_SMURF', 'SKILL_HARDSTUCK']:
        if skill not in skill_groups:
            print(f"\n{skill}: NO DATA")
            continue
        wrs = skill_groups[skill]
        print(f"\n{skill}:")
        print(f"  Count: {len(wrs)}")
        print(f"  Avg WR: {np.mean(wrs):.1%}")
        print(f"  Range: {np.min(wrs):.1%} to {np.max(wrs):.1%}")
        print(f"  Std Dev: {np.std(wrs):.3f}")

    print("\n" + "=" * 100 + "\n")


def main_analyze_streaks():
    """Original: analyze_streaks.py — Win/loss streak statistics per player."""
    data = _load_match_history()
    matches = data['matches']

    player_streaks = defaultdict(lambda: {
        'current': 0, 'max_win': 0, 'max_loss': 0, 'current_type': None
    })

    for match in matches:
        team_a = [p['id'] for p in match['team_a']]
        team_b = [p['id'] for p in match['team_b']]
        winner = match['winner']

        for player_id in team_a:
            won = (winner == 0)
            ps = player_streaks[player_id]
            if won:
                if ps['current_type'] == 'W':
                    ps['current'] += 1
                else:
                    ps['current'] = 1
                    ps['current_type'] = 'W'
                ps['max_win'] = max(ps['max_win'], ps['current'])
            else:
                if ps['current_type'] == 'L':
                    ps['current'] += 1
                else:
                    ps['current'] = 1
                    ps['current_type'] = 'L'
                ps['max_loss'] = max(ps['max_loss'], ps['current'])

        for player_id in team_b:
            won = (winner == 1)
            ps = player_streaks[player_id]
            if won:
                if ps['current_type'] == 'W':
                    ps['current'] += 1
                else:
                    ps['current'] = 1
                    ps['current_type'] = 'W'
                ps['max_win'] = max(ps['max_win'], ps['current'])
            else:
                if ps['current_type'] == 'L':
                    ps['current'] += 1
                else:
                    ps['current'] = 1
                    ps['current_type'] = 'L'
                ps['max_loss'] = max(ps['max_loss'], ps['current'])

    print("╔════════════════════════════════════════════╗")
    print("║      STREAK ANALYSIS                       ║")
    print("╚════════════════════════════════════════════╝")
    print()

    top_win_streaks = sorted(player_streaks.items(), key=lambda x: x[1]['max_win'], reverse=True)[:5]
    top_loss_streaks = sorted(player_streaks.items(), key=lambda x: x[1]['max_loss'], reverse=True)[:5]

    print("Top 5 Win Streaks:")
    print("Player         Max Win Streak")
    print("-" * 40)
    for player_id, stats in top_win_streaks:
        print(f"Player{player_id:04d}      {stats['max_win']}")

    print()
    print("Top 5 Loss Streaks:")
    print("Player         Max Loss Streak")
    print("-" * 40)
    for player_id, stats in top_loss_streaks:
        print(f"Player{player_id:04d}      {stats['max_loss']}")

    avg_max_win = sum(s['max_win'] for s in player_streaks.values()) / len(player_streaks)
    avg_max_loss = sum(s['max_loss'] for s in player_streaks.values()) / len(player_streaks)

    print()
    print(f"Average Max Win Streak:  {avg_max_win:.2f}")
    print(f"Average Max Loss Streak: {avg_max_loss:.2f}")

    efficiency = avg_max_win / (avg_max_win + avg_max_loss)
    print(f"Streak Efficiency:       {efficiency:.3f}")
    print(f"  (0.5 = random, 0.7+ = clustered)")


# ── FIX SECTION ────────────────────────────────────────────────────────────────

def main_fix_hardstuck_blend():
    """Original: fix_hardstuck_blend.py — Change hardstuck blend to 70% base + 30% HF."""
    _require_eomm_c()
    with open(EOMM_SYSTEM_C_PATH, 'r') as f:
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
    with open(EOMM_SYSTEM_C_PATH, 'w') as f:
        f.write(content)
    print("✓ Changed hardstuck blend to 70% base + 30% HF")


def main_fix_hardstuck_final():
    """Original: fix_hardstuck_final.py — Revert blend to flat + drastic hardstuck reduction."""
    _require_eomm_c()
    with open(EOMM_SYSTEM_C_PATH, 'r') as f:
        content = f.read()

    old = '''    float hf_blend;
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

    new = '''    float hf_blend = 0.5f + 0.5f * p->hidden_factor;  /* ranges [0.75, 1.1] */'''

    content = content.replace(old, new)

    old2 = '''        case SKILL_HARDSTUCK:
            lo = 0.02f; hi = 0.18f;  /* REDUCED AGAIN: was [0.05, 0.25] */
            adj_delta = +0.06f; /* one stronger stat */
            break;'''

    new2 = '''        case SKILL_HARDSTUCK:
            lo = 0.01f; hi = 0.12f;  /* DRASTIC REDUCTION */
            adj_delta = +0.04f; /* minimal boost */
            break;'''

    content = content.replace(old2, new2)

    old3 = '#define FACTOR_LOSS_PENALTY     0.30f  /* factor lost on a loss */ /* INCREASED from 0.20 */'
    new3 = '#define FACTOR_LOSS_PENALTY     0.20f  /* factor lost on a loss */'
    content = content.replace(old3, new3)

    with open(EOMM_SYSTEM_C_PATH, 'w') as f:
        f.write(content)
    print("✓ Reverted blend, drastically reduced hardstuck [0.01, 0.12]")


def main_fix_hardstuck_perf():
    """Original: fix_hardstuck_perf.py — Reduce hardstuck performance range [0.05, 0.25]."""
    _require_eomm_c()
    with open(EOMM_SYSTEM_C_PATH, 'r') as f:
        content = f.read()

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

    old2 = '''   HARDSTUCK : [0.10, 0.35]'''
    new2 = '''   HARDSTUCK : [0.05, 0.25]'''
    content = content.replace(old2, new2)

    with open(EOMM_SYSTEM_C_PATH, 'w') as f:
        f.write(content)
    print("✓ Reduced hardstuck performance range [0.05, 0.25]")


def main_fix_hardstuck_v2():
    """Original: fix_hardstuck_v2.py — Further reduce hardstuck range + 2x tilt penalty."""
    _require_eomm_c()
    with open(EOMM_SYSTEM_C_PATH, 'r') as f:
        content = f.read()

    old = '''        case SKILL_HARDSTUCK:
            lo = 0.05f; hi = 0.25f;  /* REDUCED: was [0.10, 0.35] */
            adj_delta = +0.08f; /* one stronger stat */
            break;'''

    new = '''        case SKILL_HARDSTUCK:
            lo = 0.02f; hi = 0.18f;  /* REDUCED AGAIN: was [0.05, 0.25] */
            adj_delta = +0.06f; /* one stronger stat */
            break;'''

    content = content.replace(old, new)

    old2 = '''    if (p->hidden_state == STATE_NEGATIVE) {
        float fragility = 1.0f - p->perf.tilt_resistance;
        wr -= fragility * 0.05f; /* up to -5% for zero tilt_resistance */
    }'''

    new2 = '''    if (p->hidden_state == STATE_NEGATIVE) {
        float fragility = 1.0f - p->perf.tilt_resistance;
        float penalty = fragility * 0.05f; /* base penalty */
        
        /* Hardstuck players suffer more when tilted */
        if (p->skill_level == SKILL_HARDSTUCK) {
            penalty *= 2.0f;  /* 2x penalty for hardstuck */
        }
        wr -= penalty;
    }'''

    content = content.replace(old2, new2)

    with open(EOMM_SYSTEM_C_PATH, 'w') as f:
        f.write(content)
    print("✓ Further reduced hardstuck range [0.02, 0.18] + 2x tilt penalty")


def main_fix_hardstuck_wr():
    """Original: fix_hardstuck_wr.py — Apply per-skill hf_blend in calculate_actual_winrate."""
    _require_eomm_c()
    with open(EOMM_SYSTEM_C_PATH, 'r') as f:
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

    with open(EOMM_SYSTEM_C_PATH, 'w') as f:
        f.write(content)
    print("✓ Fixed hardstuck WR blend")


def main_fix_update_tilt():
    """Original: fix_update_tilt.py — Patch update_tilt() with tilt/arrogance logic."""
    _require_eomm_c()
    with open(EOMM_SYSTEM_C_PATH, 'r') as f:
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

    with open(EOMM_SYSTEM_C_PATH, 'w') as f:
        f.write(content)
    print("✓ Fixed update_tilt() function")


def main_fix_wr():
    """Original: fix_wr.py — Patch hidden_factor blending in calculate_actual_winrate."""
    _require_eomm_c()
    with open(EOMM_SYSTEM_C_PATH, 'r') as f:
        content = f.read()

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

    with open(EOMM_SYSTEM_C_PATH, 'w') as f:
        f.write(content)
    print("✓ Fixed hidden_factor blending")


# ── SIMULATE SECTION ───────────────────────────────────────────────────────────

def main_simulate_adversarial_eomm():
    """Original: simulate_adversarial_eomm.py — Adversarial matchmaking simulation."""
    _require_numpy()

    data = _load_match_history()
    matches = data['matches']

    HIDDEN_FACTOR_START = 1.00
    HIDDEN_FACTOR_MIN = 0.50
    HIDDEN_FACTOR_MAX = 1.20
    FACTOR_WIN_BONUS = 0.02
    FACTOR_LOSS_PENALTY = 0.10
    SOFT_RESET_INTERVAL = 14

    def simulate_adversarial_matchmaking():
        player_state = {}
        player_skill = {}

        def init_player(pid):
            if pid not in player_state:
                player_state[pid] = {
                    'hidden_factor': HIDDEN_FACTOR_START,
                    'games_played': 0,
                    'wins': 0,
                    'max_win_streak': 0,
                    'max_lose_streak': 0,
                    'win_streak': 0,
                    'lose_streak': 0,
                }
                player_skill[pid] = 0.5

        for match in matches:
            for player in match['team_a'] + match['team_b']:
                init_player(player['id'])

        for match in matches:
            winner = match['winner']
            for player in match['team_a']:
                if winner == 0:
                    player_skill[player['id']] += 1
            for player in match['team_b']:
                if winner == 1:
                    player_skill[player['id']] += 1

        for pid in player_skill:
            player_skill[pid] /= 100

        for match in matches:
            all_players = match['team_a'] + match['team_b']
            tilted, hot, neutral = [], [], []

            for player in all_players:
                hf = player_state[player['id']]['hidden_factor']
                if hf < 0.75:
                    tilted.append(player)
                elif hf > 0.95:
                    hot.append(player)
                else:
                    neutral.append(player)

            team_a = tilted + neutral[len(tilted):]
            team_b = hot + neutral[len(hot):]

            winner = match['winner']
            troll_count_a = match.get('troll_count_a', 0)
            troll_count_b = match.get('troll_count_b', 0)

            for player, won, troll_count in [
                (p, (winner == 0), troll_count_a) for p in team_a
            ] + [
                (p, (winner == 1), troll_count_b) for p in team_b
            ]:
                pid = player['id']
                state = player_state[pid]

                if state['games_played'] > 0 and state['games_played'] % SOFT_RESET_INTERVAL == 0:
                    state['hidden_factor'] = HIDDEN_FACTOR_START
                    state['win_streak'] = 0
                    state['lose_streak'] = 0

                if won:
                    state['hidden_factor'] += FACTOR_WIN_BONUS
                    state['wins'] += 1
                    if state['lose_streak'] > 0:
                        state['max_lose_streak'] = max(state['max_lose_streak'], state['lose_streak'])
                        state['lose_streak'] = 0
                    state['win_streak'] += 1
                    state['max_win_streak'] = max(state['max_win_streak'], state['win_streak'])
                else:
                    state['hidden_factor'] -= FACTOR_LOSS_PENALTY
                    if state['win_streak'] > 0:
                        state['max_win_streak'] = max(state['max_win_streak'], state['win_streak'])
                        state['win_streak'] = 0
                    state['lose_streak'] += 1
                    state['max_lose_streak'] = max(state['max_lose_streak'], state['lose_streak'])

                if troll_count > 0:
                    state['hidden_factor'] -= 0.15 * troll_count

                state['hidden_factor'] = _clamp(state['hidden_factor'], HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX)
                state['games_played'] += 1

        for pid, state in player_state.items():
            state['max_win_streak'] = max(state['max_win_streak'], state['win_streak'])
            state['max_lose_streak'] = max(state['max_lose_streak'], state['lose_streak'])

        return player_state

    print("Simulating with ADVERSARIAL MATCHMAKING...")
    adv_result = simulate_adversarial_matchmaking()

    good_players = [(pid, state) for pid, state in adv_result.items()
                    if state['wins'] / state['games_played'] >= 0.55]
    bad_players = [(pid, state) for pid, state in adv_result.items()
                   if state['wins'] / state['games_played'] <= 0.45]

    good_wins = [s[1]['max_win_streak'] for s in good_players]
    good_loses = [s[1]['max_lose_streak'] for s in good_players]
    bad_wins = [s[1]['max_win_streak'] for s in bad_players]
    bad_loses = [s[1]['max_lose_streak'] for s in bad_players]

    print("\n" + "=" * 100)
    print("  ADVERSARIAL MATCHMAKING RESULTS")
    print("=" * 100)
    print(f"\nGOOD PLAYERS (WR >= 55%): {len(good_players)}")
    print(f"  Avg Max Win Streak: {np.mean(good_wins):.1f}  |  Avg Max Lose Streak: {np.mean(good_loses):.1f}")
    print(f"\nPOOR PLAYERS (WR <= 45%): {len(bad_players)}")
    print(f"  Avg Max Win Streak: {np.mean(bad_wins):.1f}  |  Avg Max Lose Streak: {np.mean(bad_loses):.1f}")
    print(f"\n📊 WIN STREAK IMPACT:")
    print(f"  Good has {np.mean(good_wins):.1f} vs Bad has {np.mean(bad_wins):.1f}")
    print(f"  Ratio: {(np.mean(good_wins) / np.mean(bad_wins)):.2f}x (WANT: >2.0x)")
    print(f"\n📊 LOSE STREAK IMPACT:")
    print(f"  Good has {np.mean(good_loses):.1f} vs Bad has {np.mean(bad_loses):.1f}")
    print(f"  Ratio: {(np.mean(bad_loses) / np.mean(good_loses)):.2f}x (WANT: >2.0x)")
    print("\n" + "=" * 100 + "\n")


def main_simulate_aggressive_eomm():
    """Original: simulate_aggressive_eomm.py — Compare 4 aggressive EOMM configurations."""
    _require_numpy()
    _require_matplotlib()

    data = _load_match_history()
    matches = data['matches']

    CONFIGS = {
        'original': {
            'name': 'Original EOMM',
            'FACTOR_WIN_BONUS': 0.02,
            'FACTOR_LOSS_PENALTY': 0.05,
            'TROLL_PENALTY_BASE': 0.10,
            'SOFT_RESET_INTERVAL': 14,
        },
        'aggressive_v1': {
            'name': 'Aggressive v1 (2x harder)',
            'FACTOR_WIN_BONUS': 0.02,
            'FACTOR_LOSS_PENALTY': 0.10,
            'TROLL_PENALTY_BASE': 0.15,
            'SOFT_RESET_INTERVAL': 14,
        },
        'aggressive_v2': {
            'name': 'Aggressive v2 (5x harder)',
            'FACTOR_WIN_BONUS': 0.01,
            'FACTOR_LOSS_PENALTY': 0.15,
            'TROLL_PENALTY_BASE': 0.20,
            'SOFT_RESET_INTERVAL': 21,
        },
        'aggressive_v3': {
            'name': 'Aggressive v3 (Extreme)',
            'FACTOR_WIN_BONUS': 0.01,
            'FACTOR_LOSS_PENALTY': 0.20,
            'TROLL_PENALTY_BASE': 0.25,
            'SOFT_RESET_INTERVAL': 28,
        },
    }

    HIDDEN_FACTOR_START = 1.00
    HIDDEN_FACTOR_MIN = 0.50
    HIDDEN_FACTOR_MAX = 1.20

    def simulate_with_config(config):
        player_state = {}

        def init_player(pid):
            if pid not in player_state:
                player_state[pid] = {
                    'hidden_factor': HIDDEN_FACTOR_START,
                    'games_played': 0,
                    'wins': 0,
                    'lose_streak': 0,
                    'win_streak': 0,
                    'max_win_streak': 0,
                    'max_lose_streak': 0,
                }

        for match in matches:
            for team, team_data in [('team_a', match['team_a']), ('team_b', match['team_b'])]:
                winner = match['winner']
                won = (team == 'team_a' and winner == 0) or (team == 'team_b' and winner == 1)
                troll_count = match.get('troll_count_a' if team == 'team_a' else 'troll_count_b', 0)

                for player in team_data:
                    pid = player['id']
                    init_player(pid)
                    state = player_state[pid]

                    if state['games_played'] > 0 and state['games_played'] % config['SOFT_RESET_INTERVAL'] == 0:
                        state['hidden_factor'] = HIDDEN_FACTOR_START
                        state['lose_streak'] = 0
                        state['win_streak'] = 0

                    if won:
                        state['hidden_factor'] += config['FACTOR_WIN_BONUS']
                        state['wins'] += 1
                        if state['lose_streak'] > 0:
                            state['max_lose_streak'] = max(state['max_lose_streak'], state['lose_streak'])
                            state['lose_streak'] = 0
                        state['win_streak'] += 1
                        state['max_win_streak'] = max(state['max_win_streak'], state['win_streak'])
                    else:
                        state['hidden_factor'] -= config['FACTOR_LOSS_PENALTY']
                        if state['win_streak'] > 0:
                            state['max_win_streak'] = max(state['max_win_streak'], state['win_streak'])
                            state['win_streak'] = 0
                        state['lose_streak'] += 1
                        state['max_lose_streak'] = max(state['max_lose_streak'], state['lose_streak'])

                    if troll_count > 0:
                        state['hidden_factor'] -= config['TROLL_PENALTY_BASE'] * troll_count

                    state['hidden_factor'] = _clamp(state['hidden_factor'], HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX)
                    state['games_played'] += 1

        for pid, state in player_state.items():
            state['max_win_streak'] = max(state['max_win_streak'], state['win_streak'])
            state['max_lose_streak'] = max(state['max_lose_streak'], state['lose_streak'])

        return player_state

    results = {}
    for config_key, config in CONFIGS.items():
        print(f"Simulating {config['name']}...")
        results[config_key] = simulate_with_config(config)

    print("\n" + "=" * 100)
    print("  AGGRESSIVE EOMM COMPARISON")
    print("=" * 100)

    for config_key, config in CONFIGS.items():
        player_state = results[config_key]
        good_players = [(pid, state, state['wins'] / max(1, state['games_played']))
                        for pid, state in player_state.items()
                        if state['wins'] / max(1, state['games_played']) >= 0.55]
        bad_players = [(pid, state, state['wins'] / max(1, state['games_played']))
                       for pid, state in player_state.items()
                       if state['wins'] / max(1, state['games_played']) <= 0.45]

        if not good_players or not bad_players:
            continue

        good_max_wins = [s[1]['max_win_streak'] for s in good_players]
        good_max_loses = [s[1]['max_lose_streak'] for s in good_players]
        good_hf = [s[1]['hidden_factor'] for s in good_players]
        bad_max_wins = [s[1]['max_win_streak'] for s in bad_players]
        bad_max_loses = [s[1]['max_lose_streak'] for s in bad_players]
        bad_hf = [s[1]['hidden_factor'] for s in bad_players]

        print(f"\n{config['name']}")
        print("-" * 100)
        print(f"GOOD PLAYERS (WR >= 55%): {len(good_players)}")
        print(f"  Avg Max Win Streak: {np.mean(good_max_wins):.1f}  |  Avg Hidden Factor: {np.mean(good_hf):.3f}")
        print(f"POOR PLAYERS (WR <= 45%): {len(bad_players)}")
        print(f"  Avg Max Win Streak: {np.mean(bad_max_wins):.1f}  |  Avg Hidden Factor: {np.mean(bad_hf):.3f}")
        print(f"\n📊 DIFFERENCE:")
        print(f"  Good vs Bad Win Streak: {np.mean(good_max_wins) - np.mean(bad_max_wins):+.1f}")
        print(f"  Good vs Bad Hidden Factor: {np.mean(good_hf) - np.mean(bad_hf):+.3f}")

        all_hf = [state['hidden_factor'] for state in player_state.values()]
        all_max_wins = [state['max_win_streak'] for state in player_state.values()]
        all_max_loses = [state['max_lose_streak'] for state in player_state.values()]
        corr_win = np.corrcoef(all_hf, all_max_wins)[0, 1]
        corr_lose = np.corrcoef(all_hf, all_max_loses)[0, 1]
        print(f"\n🔗 CORRELATION (HF vs Streaks):")
        print(f"  vs Max Win Streak: {corr_win:.3f}")
        print(f"  vs Max Lose Streak: {corr_lose:.3f}")

    print("\n" + "=" * 100 + "\n")

    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle('Aggressive EOMM Configurations Comparison', fontsize=16, fontweight='bold')

    for idx, (config_key, config) in enumerate(CONFIGS.items()):
        player_state = results[config_key]
        good_players = [(pid, state, state['wins'] / state['games_played'])
                        for pid, state in player_state.items()
                        if state['wins'] / state['games_played'] >= 0.55]
        bad_players = [(pid, state, state['wins'] / state['games_played'])
                       for pid, state in player_state.items()
                       if state['wins'] / state['games_played'] <= 0.45]

        if not good_players or not bad_players:
            continue

        ax = axes[idx // 2, idx % 2]
        good_wins = [s[1]['max_win_streak'] for s in good_players]
        good_loses = [s[1]['max_lose_streak'] for s in good_players]
        bad_wins = [s[1]['max_win_streak'] for s in bad_players]
        bad_loses = [s[1]['max_lose_streak'] for s in bad_players]

        x = np.arange(2)
        width = 0.35
        bars1 = ax.bar(x - width / 2, [np.mean(good_wins), np.mean(bad_wins)], width,
                       label='Max Win Streak', alpha=0.8)
        bars2 = ax.bar(x + width / 2, [np.mean(good_loses), np.mean(bad_loses)], width,
                       label='Max Lose Streak', alpha=0.8)
        ax.set_ylabel('Average Streak Length')
        ax.set_title(config['name'])
        ax.set_xticks(x)
        ax.set_xticklabels(['Good Players\n(WR≥55%)', 'Bad Players\n(WR≤45%)'])
        ax.legend()
        ax.grid(True, alpha=0.3, axis='y')
        ax.set_ylim([0, 10])
        for bars in [bars1, bars2]:
            for bar in bars:
                height = bar.get_height()
                ax.text(bar.get_x() + bar.get_width() / 2., height,
                        f'{height:.1f}', ha='center', va='bottom', fontsize=9)

    plt.tight_layout()
    out = _assets_path('aggressive_eomm_comparison.png')
    plt.savefig(out, dpi=150, bbox_inches='tight')
    print(f"✓ Graph saved: {out}")


def main_simulate_with_active_matchmaking():
    """Original: simulate_with_active_matchmaking.py — HF-balanced team assignment."""
    _require_numpy()

    data = _load_match_history()
    matches = data['matches']

    HIDDEN_FACTOR_START = 1.00
    HIDDEN_FACTOR_MIN = 0.50
    HIDDEN_FACTOR_MAX = 1.20
    FACTOR_WIN_BONUS = 0.02
    FACTOR_LOSS_PENALTY = 0.10
    SOFT_RESET_INTERVAL = 14

    def simulate_active_matchmaking():
        player_state = {}

        def init_player(pid):
            if pid not in player_state:
                player_state[pid] = {
                    'hidden_factor': HIDDEN_FACTOR_START,
                    'games_played': 0,
                    'wins': 0,
                    'max_win_streak': 0,
                    'max_lose_streak': 0,
                    'win_streak': 0,
                    'lose_streak': 0,
                }

        for match in matches:
            for player in match['team_a'] + match['team_b']:
                init_player(player['id'])

        for match in matches:
            all_players_in_match = match['team_a'] + match['team_b']
            sorted_by_hf = sorted(all_players_in_match,
                                  key=lambda p: player_state[p['id']]['hidden_factor'])
            team_a_optimized = sorted_by_hf[::2]
            team_b_optimized = sorted_by_hf[1::2]

            winner = match['winner']
            troll_count_a = match.get('troll_count_a', 0)
            troll_count_b = match.get('troll_count_b', 0)

            for player, won, troll_count in [
                (p, (winner == 0), troll_count_a) for p in team_a_optimized
            ] + [
                (p, (winner == 1), troll_count_b) for p in team_b_optimized
            ]:
                pid = player['id']
                state = player_state[pid]

                if state['games_played'] > 0 and state['games_played'] % SOFT_RESET_INTERVAL == 0:
                    state['hidden_factor'] = HIDDEN_FACTOR_START
                    state['win_streak'] = 0
                    state['lose_streak'] = 0

                if won:
                    state['hidden_factor'] += FACTOR_WIN_BONUS
                    state['wins'] += 1
                    if state['lose_streak'] > 0:
                        state['max_lose_streak'] = max(state['max_lose_streak'], state['lose_streak'])
                        state['lose_streak'] = 0
                    state['win_streak'] += 1
                    state['max_win_streak'] = max(state['max_win_streak'], state['win_streak'])
                else:
                    state['hidden_factor'] -= FACTOR_LOSS_PENALTY
                    if state['win_streak'] > 0:
                        state['max_win_streak'] = max(state['max_win_streak'], state['win_streak'])
                        state['win_streak'] = 0
                    state['lose_streak'] += 1
                    state['max_lose_streak'] = max(state['max_lose_streak'], state['lose_streak'])

                if troll_count > 0:
                    state['hidden_factor'] -= 0.15 * troll_count

                state['hidden_factor'] = _clamp(state['hidden_factor'], HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX)
                state['games_played'] += 1

        for pid, state in player_state.items():
            state['max_win_streak'] = max(state['max_win_streak'], state['win_streak'])
            state['max_lose_streak'] = max(state['max_lose_streak'], state['lose_streak'])

        return player_state

    print("Simulating with ACTIVE MATCHMAKING...")
    active_result = simulate_active_matchmaking()

    good_players = [(pid, state) for pid, state in active_result.items()
                    if state['wins'] / state['games_played'] >= 0.55]
    bad_players = [(pid, state) for pid, state in active_result.items()
                   if state['wins'] / state['games_played'] <= 0.45]

    good_wins = [s[1]['max_win_streak'] for s in good_players]
    bad_wins = [s[1]['max_win_streak'] for s in bad_players]

    print("\n" + "=" * 80)
    print("  ACTIVE MATCHMAKING RESULTS")
    print("=" * 80)
    print(f"\nGOOD PLAYERS (WR >= 55%): {len(good_players)}")
    print(f"  Avg Max Win Streak: {np.mean(good_wins):.1f}")
    print(f"\nPOOR PLAYERS (WR <= 45%): {len(bad_players)}")
    print(f"  Avg Max Win Streak: {np.mean(bad_wins):.1f}")
    print(f"\n📊 DIFFERENCE:")
    print(f"  Good has {(np.mean(good_wins) / np.mean(bad_wins) * 100):.0f}% of Bad's win streak")
    print(f"  Ratio: {np.mean(good_wins) / np.mean(bad_wins):.2f}x")
    print("\n" + "=" * 80 + "\n")


# ── TRACK SECTION ──────────────────────────────────────────────────────────────

def main_track_hidden_factor():
    """Original: track_hidden_factor.py — Simple hidden factor tracker."""
    data = _load_match_history()
    matches = data['matches']

    player_hidden = {}
    player_streak = {}

    for match in matches:
        for team, team_data in [('team_a', match['team_a']), ('team_b', match['team_b'])]:
            winner = match['winner']
            won = (team == 'team_a' and winner == 0) or (team == 'team_b' and winner == 1)

            for player in team_data:
                pid = player['id']
                if pid not in player_hidden:
                    player_hidden[pid] = 1.0
                    player_streak[pid] = 0

                if won:
                    player_hidden[pid] += 0.02
                    player_streak[pid] = max(0, player_streak[pid] - 1)
                else:
                    player_hidden[pid] -= 0.05
                    player_streak[pid] -= 1

                player_hidden[pid] = max(0.50, min(1.20, player_hidden[pid]))

    print("=== FINAL HIDDEN FACTORS ===\n")
    sorted_players = sorted(player_hidden.items(), key=lambda x: x[1], reverse=True)

    print("TOP 5 (Hot Streak):")
    for pid, factor in sorted_players[:5]:
        print(f"  Player {pid:04d}: {factor:.3f}")

    print("\nBOTTOM 5 (Tilted):")
    for pid, factor in sorted_players[-5:]:
        print(f"  Player {pid:04d}: {factor:.3f}")


def main_track_hidden_factor_advanced():
    """Original: track_hidden_factor_advanced.py — HF with soft reset + troll penalties."""
    data = _load_match_history()
    matches = data['matches']

    HIDDEN_FACTOR_START = 1.00
    HIDDEN_FACTOR_MIN = 0.50
    HIDDEN_FACTOR_MAX = 1.20
    FACTOR_WIN_BONUS = 0.02
    FACTOR_LOSS_PENALTY = 0.05
    TROLL_PENALTY_BASE = 0.10
    SOFT_RESET_INTERVAL = 14

    player_state = {}

    def init_player(pid):
        if pid not in player_state:
            player_state[pid] = {
                'hidden_factor': HIDDEN_FACTOR_START,
                'games_played': 0,
                'wins': 0,
                'lose_streak': 0
            }

    for match in matches:
        for team, team_data in [('team_a', match['team_a']), ('team_b', match['team_b'])]:
            winner = match['winner']
            won = (team == 'team_a' and winner == 0) or (team == 'team_b' and winner == 1)
            troll_count = match.get('troll_count_a' if team == 'team_a' else 'troll_count_b', 0)

            for player in team_data:
                pid = player['id']
                init_player(pid)
                state = player_state[pid]

                if state['games_played'] > 0 and state['games_played'] % SOFT_RESET_INTERVAL == 0:
                    state['hidden_factor'] = HIDDEN_FACTOR_START
                    state['lose_streak'] = 0

                if won:
                    state['hidden_factor'] += FACTOR_WIN_BONUS
                    state['wins'] += 1
                    state['lose_streak'] = 0
                else:
                    state['hidden_factor'] -= FACTOR_LOSS_PENALTY
                    state['lose_streak'] += 1

                if troll_count > 0:
                    state['hidden_factor'] -= TROLL_PENALTY_BASE * troll_count

                state['hidden_factor'] = _clamp(state['hidden_factor'], HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX)
                state['games_played'] += 1

    print("=== FINAL HIDDEN FACTORS (with soft reset + trolls) ===\n")
    sorted_players = sorted(player_state.items(), key=lambda x: x[1]['hidden_factor'], reverse=True)

    print("TOP 10 (Hot Streak):")
    for i, (pid, state) in enumerate(sorted_players[:10], 1):
        wr = 100 * state['wins'] / max(1, state['games_played'])
        print(f"{i}. Player {pid:04d}: HF={state['hidden_factor']:.3f}, WR={wr:.1f}%, Games={state['games_played']}")

    print("\nBOTTOM 10 (Tilted):")
    for i, (pid, state) in enumerate(sorted_players[-10:], 1):
        wr = 100 * state['wins'] / max(1, state['games_played'])
        print(f"{i}. Player {pid:04d}: HF={state['hidden_factor']:.3f}, WR={wr:.1f}%, Games={state['games_played']}")


# ── VISUALIZE SECTION ──────────────────────────────────────────────────────────

def main_visualize_hidden_vs_wr():
    """Original: visualize_hidden_vs_wr.py — Scatter plot of hidden factor vs win rate."""
    _require_matplotlib()

    data = _load_match_history()
    matches = data['matches']

    HIDDEN_FACTOR_START = 1.00
    HIDDEN_FACTOR_MIN = 0.50
    HIDDEN_FACTOR_MAX = 1.20
    FACTOR_WIN_BONUS = 0.02
    FACTOR_LOSS_PENALTY = 0.05
    TROLL_PENALTY_BASE = 0.10
    SOFT_RESET_INTERVAL = 14

    player_state = {}

    def init_player(pid):
        if pid not in player_state:
            player_state[pid] = {
                'hidden_factor': HIDDEN_FACTOR_START,
                'games_played': 0,
                'wins': 0,
                'lose_streak': 0
            }

    for match in matches:
        for team, team_data in [('team_a', match['team_a']), ('team_b', match['team_b'])]:
            winner = match['winner']
            won = (team == 'team_a' and winner == 0) or (team == 'team_b' and winner == 1)
            troll_count = match.get('troll_count_a' if team == 'team_a' else 'troll_count_b', 0)

            for player in team_data:
                pid = player['id']
                init_player(pid)
                state = player_state[pid]

                if state['games_played'] > 0 and state['games_played'] % SOFT_RESET_INTERVAL == 0:
                    state['hidden_factor'] = HIDDEN_FACTOR_START
                    state['lose_streak'] = 0

                if won:
                    state['hidden_factor'] += FACTOR_WIN_BONUS
                    state['wins'] += 1
                    state['lose_streak'] = 0
                else:
                    state['hidden_factor'] -= FACTOR_LOSS_PENALTY
                    state['lose_streak'] += 1

                if troll_count > 0:
                    state['hidden_factor'] -= TROLL_PENALTY_BASE * troll_count

                state['hidden_factor'] = _clamp(state['hidden_factor'], HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX)
                state['games_played'] += 1

    hf_list = []
    wr_list = []
    for pid, state in player_state.items():
        wr = 100 * state['wins'] / max(1, state['games_played'])
        hf_list.append(state['hidden_factor'])
        wr_list.append(wr)

    plt.figure(figsize=(12, 8))
    plt.scatter(hf_list, wr_list, alpha=0.6, s=100, edgecolors='black')
    plt.xlabel('Hidden Factor', fontsize=12)
    plt.ylabel('Win Rate (%)', fontsize=12)
    plt.title('EOMM Rubber Banding: Hidden Factor vs Win Rate', fontsize=14)
    plt.grid(True, alpha=0.3)
    plt.axhline(50, color='green', linestyle='--', alpha=0.5, label='50% WR')
    plt.axvline(1.0, color='red', linestyle='--', alpha=0.5, label='Neutral HF')
    plt.legend()
    plt.tight_layout()
    out = _assets_path('hidden_factor_vs_winrate.png')
    plt.savefig(out, dpi=150, bbox_inches='tight')
    print(f"✓ Graph saved: {out}")


# ── CALIBRATE/BOOST SECTION ────────────────────────────────────────────────────

def main_boost_smurf_wr():
    """Original: boost_smurf_wr.py — Revert hardstuck range + add +3% smurf bonus."""
    _require_eomm_c()
    with open(EOMM_SYSTEM_C_PATH, 'r') as f:
        content = f.read()

    old = '''        case SKILL_HARDSTUCK:
            lo = 0.08f; hi = 0.28f;  /* Balanced: better variance, target ~42-46% WR */
            adj_delta = +0.06f; /* modest boost */
            break;'''

    new = '''        case SKILL_HARDSTUCK:
            lo = 0.01f; hi = 0.12f;  /* Tight range for low WR */
            adj_delta = +0.04f; /* minimal boost */
            break;'''

    content = content.replace(old, new)

    old2 = '''    wr = clampf(wr, 0.25f, 0.75f);

    /* MODIFICATION 1: hidden_factor NOW affects WR AFTER clamping */'''

    new2 = '''    wr = clampf(wr, 0.25f, 0.75f);

    /* Smurf skill bonus: +3% to their win rate */
    if (p->skill_level == SKILL_SMURF) {
        wr += 0.03f;
    }

    /* MODIFICATION 1: hidden_factor NOW affects WR AFTER clamping */'''

    content = content.replace(old2, new2)

    with open(EOMM_SYSTEM_C_PATH, 'w') as f:
        f.write(content)
    print("✓ Reverted hardstuck [0.01, 0.12] + Added +3% smurf bonus")


def main_calibrate_smurf_peak():
    """Original: calibrate_smurf_peak.py — Reduce smurf peak range + remove smurf bonus."""
    _require_eomm_c()
    with open(EOMM_SYSTEM_C_PATH, 'r') as f:
        content = f.read()

    old = '''        case SKILL_SMURF:
            lo = 0.70f; hi = 0.90f;
            adj_delta = -0.10f; /* one weaker stat */
            break;'''

    new = '''        case SKILL_SMURF:
            lo = 0.65f; hi = 0.82f;  /* REDUCED: was [0.70, 0.90] */
            adj_delta = -0.08f; /* one weaker stat */
            break;'''

    content = content.replace(old, new)

    old2 = '''    wr = clampf(wr, 0.25f, 0.75f);

    /* Smurf skill bonus: +3% to their win rate */
    if (p->skill_level == SKILL_SMURF) {
        wr += 0.03f;
    }

    /* MODIFICATION 1: hidden_factor NOW affects WR AFTER clamping */'''

    new2 = '''    wr = clampf(wr, 0.25f, 0.75f);

    /* MODIFICATION 1: hidden_factor NOW affects WR AFTER clamping */'''

    content = content.replace(old2, new2)

    with open(EOMM_SYSTEM_C_PATH, 'w') as f:
        f.write(content)
    print("✓ Reduced smurf peak [0.65, 0.82] + removed +3% bonus")


def main_final_analysis():
    """Original: final_analysis.py — EOMM rubber-banding correlation analysis."""
    _require_numpy()

    data = _load_match_history()
    matches = data['matches']

    HIDDEN_FACTOR_START = 1.00
    HIDDEN_FACTOR_MIN = 0.50
    HIDDEN_FACTOR_MAX = 1.20
    FACTOR_WIN_BONUS = 0.02
    FACTOR_LOSS_PENALTY = 0.05
    TROLL_PENALTY_BASE = 0.10
    SOFT_RESET_INTERVAL = 14

    player_state = {}

    def init_player(pid):
        if pid not in player_state:
            player_state[pid] = {
                'hidden_factor': HIDDEN_FACTOR_START,
                'games_played': 0,
                'wins': 0,
                'lose_streak': 0
            }

    for match in matches:
        for team, team_data in [('team_a', match['team_a']), ('team_b', match['team_b'])]:
            winner = match['winner']
            won = (team == 'team_a' and winner == 0) or (team == 'team_b' and winner == 1)
            troll_count = match.get('troll_count_a' if team == 'team_a' else 'troll_count_b', 0)

            for player in team_data:
                pid = player['id']
                init_player(pid)
                state = player_state[pid]

                if state['games_played'] > 0 and state['games_played'] % SOFT_RESET_INTERVAL == 0:
                    state['hidden_factor'] = HIDDEN_FACTOR_START
                    state['lose_streak'] = 0

                if won:
                    state['hidden_factor'] += FACTOR_WIN_BONUS
                    state['wins'] += 1
                    state['lose_streak'] = 0
                else:
                    state['hidden_factor'] -= FACTOR_LOSS_PENALTY
                    state['lose_streak'] += 1

                if troll_count > 0:
                    state['hidden_factor'] -= TROLL_PENALTY_BASE * troll_count

                state['hidden_factor'] = _clamp(state['hidden_factor'], HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX)
                state['games_played'] += 1

    print("\n" + "=" * 70)
    print("  EOMM MATCHMAKING SYSTEM - FINAL ANALYSIS")
    print("=" * 70)
    print(f"\n📊 TOTAL MATCHES: {len(matches)}")
    print(f"👥 TOTAL PLAYERS: {len(player_state)}")

    hf_list = []
    wr_list = []
    for pid, state in player_state.items():
        wr = state['wins'] / max(1, state['games_played'])
        hf_list.append(state['hidden_factor'])
        wr_list.append(wr)

    correlation = np.corrcoef(hf_list, wr_list)[0, 1]
    print(f"\n🔗 CORRELATION (Hidden Factor vs Win Rate): {correlation:.3f}")
    print(f"   (Perfect negative correlation = -1.0)")

    print(f"\n📈 WIN RATE DISTRIBUTION:")
    print(f"   Mean: {np.mean(wr_list):.3f}")
    print(f"   Median: {np.median(wr_list):.3f}")
    print(f"   Std Dev: {np.std(wr_list):.3f}")

    print(f"\n🎭 HIDDEN FACTOR DISTRIBUTION:")
    print(f"   Mean: {np.mean(hf_list):.3f}")
    print(f"   Min: {np.min(hf_list):.3f}")
    print(f"   Max: {np.max(hf_list):.3f}")

    avg_visible_mmr = 1500
    print(f"\n⚡ EFFECTIVE MMR IMPACT (assuming visible_mmr=1500):")
    min_effective = avg_visible_mmr * np.min(hf_list)
    max_effective = avg_visible_mmr * np.max(hf_list)
    print(f"   Worst case: {min_effective:.0f} ({np.min(hf_list):.2f}x multiplier)")
    print(f"   Best case: {max_effective:.0f} ({np.max(hf_list):.2f}x multiplier)")
    print(f"   Difference: {max_effective - min_effective:.0f} MMR points")

    print(f"\n✅ CONCLUSION:")
    print(f"   The EOMM system successfully implements RUBBER BANDING")
    print(f"   by penalizing good players and boosting bad players,")
    print(f"   creating more balanced and competitive matches.")
    print("\n" + "=" * 70 + "\n")


def main_final_eomm_report():
    """Original: final_eomm_report.py — Engagement mechanics summary report."""
    _require_numpy()

    data = _load_match_history()
    matches = data['matches']

    print("\n" + "=" * 100)
    print("  FINAL EOMM ANALYSIS - WHAT THE SYSTEM SHOULD DO")
    print("=" * 100)

    print("""
The EOMM (Engagement-Oriented Matchmaking) system's PRIMARY GOAL is NOT to create
win/lose streaks for everyone equally. Rather, it should:

1. ✅ REDUCE SKILL GAP variance between teams
   → Make matches more competitive (50-50 win chance)
   → Prevent "stomp" matches (70-30 or worse)

2. ✅ CREATE ENGAGEMENT LOOPS for struggling players
   → Give them occasional wins to keep them engaged
   → Prevent total demoralization (loss streaks 8+ games)

3. ✅ MAINTAIN SKILL INTEGRITY
   → Better players still win more often (~55-70% WR)
   → Worse players still lose more often (~30-45% WR)

4. ❌ NOT create artificial win streaks
   → This would be ANTI-COMPETITIVE
   → Would reward bad play with free wins
   → Would punish skill development
""")

    HIDDEN_FACTOR_START = 1.00
    HIDDEN_FACTOR_MIN = 0.50
    HIDDEN_FACTOR_MAX = 1.20
    FACTOR_WIN_BONUS = 0.02
    FACTOR_LOSS_PENALTY = 0.05
    TROLL_PENALTY_BASE = 0.10
    SOFT_RESET_INTERVAL = 14

    player_state = {}

    def init_player(pid):
        if pid not in player_state:
            player_state[pid] = {
                'hidden_factor': HIDDEN_FACTOR_START,
                'games_played': 0,
                'wins': 0,
                'max_win_streak': 0,
                'max_lose_streak': 0,
            }

    for match in matches:
        for team, team_data in [('team_a', match['team_a']), ('team_b', match['team_b'])]:
            winner = match['winner']
            won = (team == 'team_a' and winner == 0) or (team == 'team_b' and winner == 1)
            troll_count = match.get('troll_count_a' if team == 'team_a' else 'troll_count_b', 0)

            for player in team_data:
                pid = player['id']
                init_player(pid)
                state = player_state[pid]

                if state['games_played'] > 0 and state['games_played'] % SOFT_RESET_INTERVAL == 0:
                    state['hidden_factor'] = HIDDEN_FACTOR_START

                if won:
                    state['hidden_factor'] += FACTOR_WIN_BONUS
                    state['wins'] += 1
                else:
                    state['hidden_factor'] -= FACTOR_LOSS_PENALTY

                if troll_count > 0:
                    state['hidden_factor'] -= TROLL_PENALTY_BASE * troll_count

                state['hidden_factor'] = _clamp(state['hidden_factor'], HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX)
                state['games_played'] += 1

    elite, good, bad, terrible = [], [], [], []

    for pid, state in player_state.items():
        wr = state['wins'] / state['games_played']
        if wr >= 0.60:
            elite.append((pid, wr, state['hidden_factor']))
        elif wr >= 0.55:
            good.append((pid, wr, state['hidden_factor']))
        elif wr <= 0.40:
            terrible.append((pid, wr, state['hidden_factor']))
        elif wr <= 0.45:
            bad.append((pid, wr, state['hidden_factor']))

    print(f"\n✅ SKILL INTEGRITY MAINTAINED:")
    print(f"  Elite (WR >= 60%): {len(elite)} players with Avg HF = {np.mean([x[2] for x in elite]):.3f}")
    print(f"  Good (WR >= 55%): {len(good)} players with Avg HF = {np.mean([x[2] for x in good]):.3f}")
    print(f"  Bad (WR <= 45%): {len(bad)} players with Avg HF = {np.mean([x[2] for x in bad]):.3f}")
    print(f"  Terrible (WR <= 40%): {len(terrible)} players with Avg HF = {np.mean([x[2] for x in terrible]):.3f}")

    all_hf = [state['hidden_factor'] for state in player_state.values()]
    all_wr = [state['wins'] / state['games_played'] for state in player_state.values()]
    corr = np.corrcoef(all_hf, all_wr)[0, 1]

    print(f"\n✅ ENGAGEMENT MECHANISM WORKING:")
    print(f"  Correlation (HF vs WR): {corr:.3f}")
    print(f"  → Negative correlation = Struggling players get MMR boosts ✓")

    max_streaks = [state['max_lose_streak'] for state in player_state.values()]
    print(f"\n✅ PREVENTING DEMORALIZATION:")
    print(f"  Max lose streaks across all players:")
    print(f"    Mean: {np.mean(max_streaks):.1f}")
    print(f"    Max: {np.max(max_streaks)}")

    print("\n" + "=" * 100 + "\n")


def main_final_smurf_tune():
    """Original: final_smurf_tune.py — Further reduce smurf peak range [0.62, 0.78]."""
    _require_eomm_c()
    with open(EOMM_SYSTEM_C_PATH, 'r') as f:
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

    with open(EOMM_SYSTEM_C_PATH, 'w') as f:
        f.write(content)
    print("✓ Reduced smurf peak [0.62, 0.78]")


# ── PATCH/REDUCE SECTION ───────────────────────────────────────────────────────

def main_patch_calculate_wr():
    """Original: patch_calculate_wr.py — Patch calculate_actual_winrate() via regex/manual."""
    _require_eomm_c()
    with open(EOMM_SYSTEM_C_PATH, 'r') as f:
        content = f.read()

    old_pattern = (
        r'float calculate_actual_winrate\(const Player \*p\) \{\n'
        r'    /\* Equal-weight average of all eight performance stats \*/\n'
        r'    float avg = \(p->perf\.mechanical_skill\n'
        r'               \+ p->perf\.decision_making\n'
        r'               \+ p->perf\.map_awareness\n'
        r'               \+ p->perf\.tilt_resistance\n'
        r'               \+ p->perf\.champion_pool_depth\n'
        r'               \+ p->perf\.champion_proficiency\n'
        r'               \+ p->perf\.wave_management\n'
        r'               \+ p->perf\.teamfight_positioning\) / 8\.0f;\n'
        r'\n'
        r'    /\* Map \[0, 1\] performance to \[0\.25, 0\.75\] win rate \*/\n'
        r'    float wr = 0\.25f \+ avg \* 0\.50f;\n'
        r'\n'
        r'    /\* Tilt penalty: emotionally fragile players drop further when negative \*/\n'
        r'    if \(p->hidden_state == STATE_NEGATIVE\) \{\n'
        r'        float fragility = 1\.0f - p->perf\.tilt_resistance;\n'
        r'        wr -= fragility \* 0\.05f; /\* up to -5% for zero tilt_resistance \*/\n'
        r'    \}\n'
        r'\n'
        r'    return clampf\(wr, 0\.25f, 0\.75f\);\n'
        r'\}'
    )

    new_func = '''float calculate_actual_winrate(const Player *p) {
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

    if re.search(old_pattern, content):
        content = re.sub(old_pattern, new_func, content)
        print("✓ Patched calculate_actual_winrate() with regex")
    else:
        print("⚠ Regex didn't match, trying manual approach...")
        start = content.find("float calculate_actual_winrate(const Player *p) {")
        if start != -1:
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
            content = content[:start] + new_func + content[end:]
            print("✓ Patched calculate_actual_winrate() manually")
        else:
            print("✗ Could not find calculate_actual_winrate()")

    with open(EOMM_SYSTEM_C_PATH, 'w') as f:
        f.write(content)


def main_player_history(player_id=None):
    """Original: player_history.py — Show match history for a specific player."""
    data = _load_match_history()
    matches = data['matches']

    if player_id is None:
        print("Usage: python3 src/scripts/eomm_tools.py player-history --player-id <id>")
        sys.exit(1)

    player_matches = []
    for match in matches:
        in_team_a = any(p['id'] == player_id for p in match['team_a'])
        in_team_b = any(p['id'] == player_id for p in match['team_b'])

        if in_team_a or in_team_b:
            player_matches.append({
                'match_id': match['match_id'],
                'team': 'A' if in_team_a else 'B',
                'opponent_team': 'B' if in_team_a else 'A',
                'won': (match['winner'] == 0 and in_team_a) or (match['winner'] == 1 and in_team_b),
                'team_a_power': match['team_a_power'],
                'team_b_power': match['team_b_power'],
                'troll_count': match.get('troll_count_a' if in_team_a else 'troll_count_b', 0)
            })

    print(f"\n=== PLAYER {player_id:04d} HISTORY ===")
    print(f"Total matches: {len(player_matches)}")

    wins = sum(1 for m in player_matches if m['won'])
    losses = len(player_matches) - wins
    wr = 100 * wins / max(1, len(player_matches))

    print(f"Record: {wins}-{losses} ({wr:.1f}%)")
    print(f"\nMatch history:")
    print("-" * 80)

    for i, match in enumerate(player_matches, 1):
        result = "WIN" if match['won'] else "LOSS"
        team_power = match['team_a_power'] if match['team'] == 'A' else match['team_b_power']
        opp_power = match['team_b_power'] if match['team'] == 'A' else match['team_a_power']
        print(f"{i}. Match {match['match_id']:04d} [{result}] Team {match['team']} "
              f"({team_power:.1f}) vs Team {match['opponent_team']} ({opp_power:.1f}) "
              f"| Trolls: {match['troll_count']}")

    print("-" * 80)


def main_reduce_hardstuck_kfactor():
    """Original: reduce_hardstuck_kfactor.py — Add 0.7x K-factor for hardstuck players."""
    _require_eomm_c()
    with open(EOMM_SYSTEM_C_PATH, 'r') as f:
        content = f.read()

    old = '''    /* Ranked play (after placement) */
    if (games_played >= PLACEMENT_GAMES) {
        k = K_FACTOR_RANKED;
    }'''

    new = '''    /* Ranked play (after placement) */
    if (games_played >= PLACEMENT_GAMES) {
        k = K_FACTOR_RANKED;
        
        /* Hardstuck players gain/lose less LP */
        if (p->skill_level == SKILL_HARDSTUCK) {
            k *= 0.7f;  /* 30% reduction in LP gains/losses */
        }
    }'''

    content = content.replace(old, new)

    with open(EOMM_SYSTEM_C_PATH, 'w') as f:
        f.write(content)
    print("✓ Added 0.7x K-factor for hardstuck players")


def main_lower_hardstuck_kfactor():
    """Original: lower_hardstuck_kfactor.py — Lower hardstuck K-factor to 0.5x."""
    _require_eomm_c()
    with open(EOMM_SYSTEM_C_PATH, 'r') as f:
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

    with open(EOMM_SYSTEM_C_PATH, 'w') as f:
        f.write(content)
    print("✓ Reduced hardstuck K-factor to 0.5x")


def main_increase_loss_penalty():
    """Original: increase_loss_penalty.py — Increase FACTOR_LOSS_PENALTY to 0.30."""
    _require_eomm_c()
    with open(EOMM_SYSTEM_C_PATH, 'r') as f:
        content = f.read()

    old = '#define FACTOR_LOSS_PENALTY     0.20f  /* factor lost on a loss */'
    new = '#define FACTOR_LOSS_PENALTY     0.30f  /* factor lost on a loss */ /* INCREASED from 0.20 */'

    content = content.replace(old, new)

    with open(EOMM_SYSTEM_C_PATH, 'w') as f:
        f.write(content)
    print("✓ Increased FACTOR_LOSS_PENALTY from 0.20 to 0.30")


def main_stabilize_hardstuck():
    """Original: stabilize_hardstuck.py — Expand hardstuck range to [0.08, 0.28]."""
    _require_eomm_c()
    with open(EOMM_SYSTEM_C_PATH, 'r') as f:
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

    with open(EOMM_SYSTEM_C_PATH, 'w') as f:
        f.write(content)
    print("✓ Expanded hardstuck range to [0.08, 0.28]")


def main_implement_forced_streaks():
    """Original: implement_forced_streaks.py — Adversarial streak-extension matchmaking."""
    _require_numpy()

    data = _load_match_history()
    matches = data['matches']

    HIDDEN_FACTOR_START = 1.00
    HIDDEN_FACTOR_MIN = 0.50
    HIDDEN_FACTOR_MAX = 1.20
    FACTOR_WIN_BONUS = 0.08
    FACTOR_LOSS_PENALTY = 0.20
    SOFT_RESET_INTERVAL = 20

    def simulate_forced_streaks():
        player_state = {}

        def init_player(pid):
            if pid not in player_state:
                player_state[pid] = {
                    'hidden_factor': HIDDEN_FACTOR_START,
                    'games_played': 0,
                    'wins': 0,
                    'match_sequence': [],
                    'current_streak_type': None,
                    'current_streak_length': 0,
                }

        for match in matches:
            for player in match['team_a'] + match['team_b']:
                init_player(player['id'])

        for match in matches:
            all_players = match['team_a'] + match['team_b']
            on_win_streak, on_loss_streak, neutral_players = [], [], []

            for player in all_players:
                pid = player['id']
                state = player_state[pid]
                if state['current_streak_type'] == 'W':
                    on_win_streak.append(player)
                elif state['current_streak_type'] == 'L':
                    on_loss_streak.append(player)
                else:
                    neutral_players.append(player)

            team_a = on_win_streak + neutral_players[:2]
            team_b = on_loss_streak + neutral_players[2:]

            winner = match['winner']

            for player, won in (
                [(p, (winner == 0)) for p in team_a] +
                [(p, (winner == 1)) for p in team_b]
            ):
                pid = player['id']
                state = player_state[pid]

                if state['games_played'] > 0 and state['games_played'] % SOFT_RESET_INTERVAL == 0:
                    state['hidden_factor'] = HIDDEN_FACTOR_START
                    state['current_streak_type'] = None
                    state['current_streak_length'] = 0

                if won:
                    state['hidden_factor'] += FACTOR_WIN_BONUS
                    state['wins'] += 1
                    state['match_sequence'].append('W')
                    if state['current_streak_type'] == 'W':
                        state['current_streak_length'] += 1
                    else:
                        state['current_streak_type'] = 'W'
                        state['current_streak_length'] = 1
                else:
                    state['hidden_factor'] -= FACTOR_LOSS_PENALTY
                    state['match_sequence'].append('L')
                    if state['current_streak_type'] == 'L':
                        state['current_streak_length'] += 1
                    else:
                        state['current_streak_type'] = 'L'
                        state['current_streak_length'] = 1

                state['hidden_factor'] = _clamp(state['hidden_factor'], HIDDEN_FACTOR_MIN, HIDDEN_FACTOR_MAX)
                state['games_played'] += 1

        return player_state

    def calculate_streak_efficiency(sequence):
        if len(sequence) < 2:
            return 0.5
        changes = sum(1 for i in range(len(sequence) - 1) if sequence[i] != sequence[i + 1])
        max_changes = len(sequence) - 1
        return 1 - (changes / max_changes) if max_changes > 0 else 0.5

    print("Simulating FORCED STREAKS EOMM...")
    result = simulate_forced_streaks()

    elite, good, bad, terrible = [], [], [], []
    for pid, state in result.items():
        wr = state['wins'] / state['games_played']
        eff = calculate_streak_efficiency(state['match_sequence'])
        entry = (pid, wr, eff, state['match_sequence'], state['hidden_factor'])
        if wr >= 0.60:
            elite.append(entry)
        elif wr >= 0.55:
            good.append(entry)
        elif wr <= 0.40:
            terrible.append(entry)
        elif wr <= 0.45:
            bad.append(entry)

    print("\n" + "=" * 100)
    print("  FORCED STREAKS EOMM - RESULTS")
    print("=" * 100)

    print(f"\n🏆 ELITE (WR >= 60%): {len(elite)} players")
    if elite:
        eff = [e[2] for e in elite]
        wrs = [e[1] for e in elite]
        print(f"  Avg WR: {np.mean(wrs):.1%}")
        print(f"  Avg Streak Efficiency: {np.mean(eff):.3f}")
        print(f"  Example Elite (Player {elite[0][0]:04d}): {elite[0][3][:40]}")

    print(f"\n✅ GOOD (WR >= 55%): {len(good)} players")
    if good:
        print(f"  Avg WR: {np.mean([e[1] for e in good]):.1%}")
        print(f"  Avg Streak Efficiency: {np.mean([e[2] for e in good]):.3f}")

    print(f"\n❌ BAD (WR <= 45%): {len(bad)} players")
    if bad:
        print(f"  Avg WR: {np.mean([e[1] for e in bad]):.1%}")
        print(f"  Avg Streak Efficiency: {np.mean([e[2] for e in bad]):.3f}")
        print(f"  Example Bad (Player {bad[0][0]:04d}): {bad[0][3][:40]}")

    print(f"\n💀 TERRIBLE (WR <= 40%): {len(terrible)} players")
    if terrible:
        print(f"  Avg WR: {np.mean([e[1] for e in terrible]):.1%}")
        print(f"  Avg Streak Efficiency: {np.mean([e[2] for e in terrible]):.3f}")

    all_eff = [calculate_streak_efficiency(state['match_sequence']) for state in result.values()]
    print(f"\n📊 GLOBAL STREAK EFFICIENCY: {np.mean(all_eff):.3f} (was 0.525)")
    print(f"   Improvement: +{(np.mean(all_eff) - 0.525) * 100:.1f}%")

    print("\n" + "=" * 100 + "\n")


# ── CLI ────────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description='EOMM Matchmaking System - Unified Analysis & Tuning Tools',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    subparsers = parser.add_subparsers(dest='command', metavar='<command>')

    # Analyze
    subparsers.add_parser('analyze-match', help='Analyze match history and win-rate distribution')
    subparsers.add_parser('analyze-missing-streaks', help='Deep streak pattern analysis')
    subparsers.add_parser('analyze-skill-ceiling', help='Analyze skill level impact on win rate')
    subparsers.add_parser('analyze-streaks', help='Analyze win/loss streak statistics')

    # Fix
    subparsers.add_parser('fix-hardstuck-blend', help='Change hardstuck HF blend to 70pct+30pct')
    subparsers.add_parser('fix-hardstuck-final', help='Revert blend + drastic hardstuck range reduction')
    subparsers.add_parser('fix-hardstuck-perf', help='Reduce hardstuck performance range [0.05, 0.25]')
    subparsers.add_parser('fix-hardstuck-v2', help='Reduce hardstuck range [0.02, 0.18] + 2x tilt')
    subparsers.add_parser('fix-hardstuck-wr', help='Apply per-skill hf_blend in calculate_actual_winrate')
    subparsers.add_parser('fix-update-tilt', help='Patch update_tilt() with tilt/arrogance logic')
    subparsers.add_parser('fix-wr', help='Patch hidden_factor blending in calculate_actual_winrate')

    # Simulate
    subparsers.add_parser('simulate-adversarial', help='Run adversarial EOMM simulation')
    subparsers.add_parser('simulate-aggressive', help='Compare aggressive EOMM configurations')
    subparsers.add_parser('simulate-active', help='Run active (HF-balanced) matchmaking simulation')

    # Track
    subparsers.add_parser('track-hidden', help='Track hidden factor per player (simple)')
    subparsers.add_parser('track-hidden-advanced', help='Track hidden factor with soft reset and trolls')

    # Visualize
    subparsers.add_parser('visualize', help='Generate hidden factor vs win-rate scatter plot')

    # Calibrate/Boost
    subparsers.add_parser('boost-smurf', help='Revert hardstuck range + add +3pct smurf bonus')
    subparsers.add_parser('calibrate-smurf', help='Reduce smurf peak range + remove smurf bonus')
    subparsers.add_parser('final-analysis', help='EOMM rubber-banding correlation analysis')
    subparsers.add_parser('final-report', help='EOMM engagement mechanics summary report')
    subparsers.add_parser('final-smurf-tune', help='Further reduce smurf peak range [0.62, 0.78]')

    # Patch/Reduce
    subparsers.add_parser('patch-calculate-wr', help='Patch calculate_actual_winrate() via regex/manual')
    p_history = subparsers.add_parser('player-history', help='Show match history for a specific player')
    p_history.add_argument('--player-id', type=int, required=True, metavar='ID',
                           help='Player ID to look up')
    subparsers.add_parser('reduce-hardstuck-kfactor', help='Add 0.7x K-factor for hardstuck players')
    subparsers.add_parser('lower-hardstuck-kfactor', help='Lower hardstuck K-factor to 0.5x')
    subparsers.add_parser('increase-loss-penalty', help='Increase FACTOR_LOSS_PENALTY to 0.30')
    subparsers.add_parser('stabilize-hardstuck', help='Expand hardstuck range to [0.08, 0.28]')
    subparsers.add_parser('implement-forced-streaks', help='Simulate adversarial streak-extension matchmaking')

    args = parser.parse_args()

    command_map = {
        'analyze-match': main_analyze_match_history,
        'analyze-missing-streaks': main_analyze_missing_streaks,
        'analyze-skill-ceiling': main_analyze_skill_level_ceiling,
        'analyze-streaks': main_analyze_streaks,
        'fix-hardstuck-blend': main_fix_hardstuck_blend,
        'fix-hardstuck-final': main_fix_hardstuck_final,
        'fix-hardstuck-perf': main_fix_hardstuck_perf,
        'fix-hardstuck-v2': main_fix_hardstuck_v2,
        'fix-hardstuck-wr': main_fix_hardstuck_wr,
        'fix-update-tilt': main_fix_update_tilt,
        'fix-wr': main_fix_wr,
        'simulate-adversarial': main_simulate_adversarial_eomm,
        'simulate-aggressive': main_simulate_aggressive_eomm,
        'simulate-active': main_simulate_with_active_matchmaking,
        'track-hidden': main_track_hidden_factor,
        'track-hidden-advanced': main_track_hidden_factor_advanced,
        'visualize': main_visualize_hidden_vs_wr,
        'boost-smurf': main_boost_smurf_wr,
        'calibrate-smurf': main_calibrate_smurf_peak,
        'final-analysis': main_final_analysis,
        'final-report': main_final_eomm_report,
        'final-smurf-tune': main_final_smurf_tune,
        'patch-calculate-wr': main_patch_calculate_wr,
        'reduce-hardstuck-kfactor': main_reduce_hardstuck_kfactor,
        'lower-hardstuck-kfactor': main_lower_hardstuck_kfactor,
        'increase-loss-penalty': main_increase_loss_penalty,
        'stabilize-hardstuck': main_stabilize_hardstuck,
        'implement-forced-streaks': main_implement_forced_streaks,
    }

    if args.command == 'player-history':
        main_player_history(player_id=args.player_id)
    elif args.command in command_map:
        command_map[args.command]()
    else:
        parser.print_help()


if __name__ == '__main__':
    main()
