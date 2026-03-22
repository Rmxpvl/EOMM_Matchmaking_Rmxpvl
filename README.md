# EOMM_Matchmaking_Rmxpvl

Projet perso autour d'un **EOMM (Engagement Optimized Matchmaking)** : simulation + implémentation d'un moteur de matchmaking où l'objectif n'est pas seulement "l'équité", mais aussi la **rétention/engagement** via des mécanismes de **hidden MMR**, **effective MMR**, et (potentiellement) des **streaks win/loss** "crédibles".

---

## Objectifs principaux

- **Simuler une population** (ex: ~500 joueurs) avec profils variés (smurfs, faibles, joueurs moyens…).
- Mettre en place un **matchmaking 5v5** :
  - Un match ne démarre **que lorsqu'on a 10 joueurs** disponibles.
  - Si plus de 10 joueurs sont en file, on match par **blocs de 10** et on conserve une **queue**.
- Utiliser une MMR "à 2 couches" :
  - `visibleMMR` (ce que le joueur "voit")
  - `hiddenMMR` (utilisé pour piloter/ajuster le matchmaking)
- Introduire une **effectiveMMR** du type :
  - `effectiveMMR = visibleMMR × hiddenFactor`
- Gérer une dynamique long-terme :
  - **Reset complet** du hiddenMMR après **7 jours** d'inactivité (IRL, basé sur `lastMatchTime`)
  - **Soft reset** périodique (ex: toutes les X parties) pour recoller progressivement au visibleMMR

---

## Architecture

```
EOMM_Matchmaking_Rmxpvl/
├── README.md
├── CHANGELOG.md
├── LICENSE
├── Makefile
├── .gitignore
├── include/
│   └── eomm_system.h       ← header principal
├── src/
│   ├── eomm_system.c       ← cœur du moteur C
│   ├── eomm_main.c         ← point d'entrée
│   ├── match_history.c     ← historique des matchs
│   └── scripts/
│       └── eomm_tools.py   ← unified Python CLI (analyze, simulate, track, fix…)
├── tests/
│   ├── test_autofill_system.c
│   ├── test_debug_autofill.c
│   ├── test_coefficient_analysis.c
│   ├── test_performance_stats.c
│   ├── test_50_games_detailed.c
│   ├── test_3_separate_simulations.c
│   ├── test_eomm_auto_200games.c
│   ├── test_full_eomm_200games.c
│   └── test_full_eomm_200games_final.c
├── docs/
│   ├── engine_structure.md
│   ├── CONVERSATION_RECAP.md
│   ├── README_DEMARCHE.md
│   └── assets/
│       ├── aggressive_eomm_comparison.png
│       ├── hidden_factor_vs_winrate.png
│       └── win_rate_distribution.png
└── data/
    └── match_history.json  ← (gitignored, ~8MB)
```

---

## Spécifications de simulation / matchmaking (résumé)

### Matchmaking 5v5 + file d'attente
- On alimente une **queue** de joueurs.
- Dès que la queue atteint **10 joueurs**, on crée 1 match :
  - 5 vs 5
- Les joueurs restants (ex: 12 joueurs → 2 restants) restent en queue.

### Fin de match
- Quand un match se termine, **les joueurs reviennent dans la file d'attente**.

### Inactivité (7 jours)
- Si un joueur n'a pas joué depuis **≥ 7 jours**, on déclenche un **reset** (ex: `hiddenMMR = neutralMMR`) en se basant sur `lastMatchTime`.

---

## Concepts clés (EOMM)

- **hiddenFactor** : facteur multiplicatif (>= 0.5) qui pénalise certains comportements/écarts.
- **effectiveMMR** : MMR utilisée pour trier/placer les joueurs avant assignment d'équipes.
- **Bias/streaks** : ajuster subtilement la composition des équipes pour pousser des séquences win/loss "plausibles" tout en gardant un winrate global ~50%.

---

## Python Tools CLI

All Python analysis, simulation, calibration, and debugging utilities are consolidated
in a single file: **`src/scripts/eomm_tools.py`**

```bash
# Show all available commands
python3 src/scripts/eomm_tools.py --help

# Or use Makefile shortcuts
make analyze        # Analyze match history and win-rate distribution
make simulate       # Compare aggressive EOMM configurations
make track          # Track hidden factors with soft reset + troll penalties
make report         # EOMM engagement mechanics summary report
```

### Available commands

| Category | Command | Description |
|----------|---------|-------------|
| **Analyze** | `analyze-match` | Match history, win-rate distribution, troll impact |
| | `analyze-missing-streaks` | Deep streak pattern analysis |
| | `analyze-skill-ceiling` | Skill level impact on win rate |
| | `analyze-streaks` | Win/loss streak statistics |
| **Fix** | `fix-hardstuck-blend` | Change hardstuck HF blend ratio |
| | `fix-hardstuck-final` | Revert blend + drastic range reduction |
| | `fix-hardstuck-perf` | Reduce hardstuck performance range |
| | `fix-hardstuck-v2` | Further range reduction + 2x tilt penalty |
| | `fix-hardstuck-wr` | Per-skill hf_blend in winrate calc |
| | `fix-update-tilt` | Patch update_tilt() with tilt/arrogance |
| | `fix-wr` | Patch hidden_factor blending |
| **Simulate** | `simulate-adversarial` | Adversarial matchmaking simulation |
| | `simulate-aggressive` | Compare 4 aggressive EOMM configs |
| | `simulate-active` | HF-balanced matchmaking simulation |
| **Track** | `track-hidden` | Simple hidden factor tracker |
| | `track-hidden-advanced` | HF with soft reset + troll penalties |
| **Visualize** | `visualize` | Hidden factor vs win-rate scatter plot |
| **Calibrate** | `boost-smurf` | Revert hardstuck + add smurf bonus |
| | `calibrate-smurf` | Reduce smurf peak range |
| | `final-analysis` | Rubber-banding correlation analysis |
| | `final-report` | Engagement mechanics summary |
| | `final-smurf-tune` | Further reduce smurf peak |
| **Patch** | `patch-calculate-wr` | Patch calculate_actual_winrate() |
| | `player-history --player-id <ID>` | Match history for a player |
| | `reduce-hardstuck-kfactor` | Add 0.7x K-factor for hardstuck |
| | `lower-hardstuck-kfactor` | Lower hardstuck K-factor to 0.5x |
| | `increase-loss-penalty` | Increase FACTOR_LOSS_PENALTY to 0.30 |
| | `stabilize-hardstuck` | Expand hardstuck range [0.08, 0.28] |
| | `implement-forced-streaks` | Adversarial streak-extension sim |

### Examples

```bash
# Analyze match data
python3 src/scripts/eomm_tools.py analyze-match

# Run a simulation
python3 src/scripts/eomm_tools.py simulate-aggressive

# Look up a specific player's history
python3 src/scripts/eomm_tools.py player-history --player-id 42

# Apply a C source patch
python3 src/scripts/eomm_tools.py fix-wr
```

---



```bash
# Compiler le moteur EOMM
make

# Lancer le moteur
make run

# Lancer tous les tests
make test

# Compiler un test spécifique
make test_autofill
make test_eomm_auto_200games
make test_full_eomm_200games
make test_full_eomm_200games_final

# Nettoyer les artefacts
make clean
```

---

## Documentation & références internes

- Démarche / décisions / spec : **`docs/README_DEMARCHE.md`**
- Notes de structure : **`docs/engine_structure.md`**
- Récap conversations : **`docs/CONVERSATION_RECAP.md`**

---

## Contribuer

Voir `CONTRIBUTING.md`.
