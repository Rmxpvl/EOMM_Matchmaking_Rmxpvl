from __future__ import annotations

from typing import List, Dict, Any


def matchmaking_5v5(players: List[Dict[str, Any]], team_size: int = 5) -> List[Dict[str, Any]]:
    """
    Create 5v5 matches (2 teams of size `team_size`).
    - Requires total players to be a multiple of (2 * team_size).
    - For 10 players => 1 match: team_a (5) vs team_b (5)
    - For 20 players => 2 matches, etc.
    """
    if team_size <= 0:
        raise ValueError("team_size must be > 0")

    match_size = 2 * team_size  # 10 for 5v5

    if len(players) < match_size:
        return []

    if len(players) % match_size != 0:
        raise ValueError(f"Must have a multiple of {match_size} players for {team_size}v{team_size}")

    matches: List[Dict[str, Any]] = []
    for start in range(0, len(players), match_size):
        block = players[start : start + match_size]
        team_a = block[:team_size]
        team_b = block[team_size:]
        matches.append({"team_a": team_a, "team_b": team_b})
    return matches