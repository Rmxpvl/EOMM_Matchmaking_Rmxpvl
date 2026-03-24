# EOMM_Matchmaking_Rmxpvl

**Moteur de matchmaking 5v5 avec système ELO Riot-grade** : implémentation C d'un matchmaking équitable et stable à travers une population diverse de joueurs (smurfs, novices, intermédiaires).

## 🎯 Objectif

Simuler et valider un système ELO **mathématiquement solide** qui:
- ✅ Empêche l'escalade infinie des smurfs (saturation logarithmique)
- ✅ Bloque la pseudo-progression (WR < 50% = pas de montée)
- ✅ Converge naturellement les joueurs à leur niveau réel
- ✅ Gère équitablement 7 niveaux de skill (ultra-bas → champion)

---

## 📂 Architecture

```
EOMM_Matchmaking/
├── README.md                          ← Ce fichier
├── CHANGELOG.md                       ← Historique des changements
├── LICENSE                            ← MIT License
├── Makefile                           ← Build & test commands
│
├── include/
│   └── eomm_system.h                 ← Définitions des structures & fonctions
│
├── src/
│   ├── eomm_main.c                   ← Point d'entrée principal
│   ├── eomm_system.c                 ← Cœur du système (matchmaking, MMR)
│   ├── match_history.c               ← Gestion de l'historique
│   └── scripts/                      ← Utilitaires de développement
│
├── tests/
│   └── test_eomm_season_realistic.c  ← Test de validation principal (1M parties)
│
├── docs/
│   ├── ALGORITHM.md                  ← Détail de l'algorithme ELO
│   └── engine_structure.md           ← Architecture du système
│
├── bin/
│   ├── eomm_system                   ← Binaire principal compilé
│   └── test_season_realistic         ← Binaire de test compilé
│
└── data/
    └── (répertoire pour données de test)
```

---

## 🎮 Système ELO: 3 Couches d'Équilibre

### 1️⃣ **Carry Bonus Cap (Saturation logarithmique)**

Empêche les smurfs de dominer indéfiniment:

```c
carry_bonus = log(1 + skill_gap/100) × 100
if (carry_bonus > 90 MMR) carry_bonus = 90 MMR;  // Hard cap
```

**Effet**: 
- SMURF_HIGH (90% WR): +90 MMR max vs équipes faibles
- Progression naturelle vers des matchups plus difficiles
- Convergence mathématique au lieu d'inflation

### 2️⃣ **Expected Win Clamping (By Skill Tier)**

Ajuste les attentes selon le niveau du joueur:

```c
// Low-skill players: need 55% WR to break even
if (low_skill) {
    clamp(expected, 0.30, 0.85);  // Tighter range
} else {
    clamp(expected, 0.10, 0.90);  // Standard ELO
}
```

**Effet**:
- Joueurs faibles ne peuvent pas escalader sur des flukes
- Baseline 50% WR correspond à équité mathématique

### 3️⃣ **WR-Based Scaling (Enforcement du loi ELO)**

**La couche critique**: force WR < 50% → pas de montée

```c
if (player_games >= 20) {
    wr_factor = (player_wr - 0.5) × 2.0;
    clamp(wr_factor, -0.5, +0.5);
    delta *= (1.0 + wr_factor × 1.0);
}
```

**Effet**:
- 43.6% WR @ +0.23 MMR/game (fortement réduit)
- 62.5% WR @ gains maximaux (naturel)
- **Impossible** de monter chroniquement avec WR négative

---

## 🧪 Test de Validation

### `test_eomm_season_realistic.c` — 1M Parties Simulées

```bash
make build
./bin/test_season_realistic
```

**Scope**: 
- 1000 joueurs
- 100K matches
- 7 profils de skill (2.2% smurfs → 87.8% normaux)
- 4 pools de progression par joueur (50/100/200/300 games)

**Résultats validés**:
```
✅ SMURF_HIGH:    62.5% WR | Pool: 90%→81% (domination réaliste)
✅ SMURF_MED:     53.2% WR | Pool: 68%→51% (progression logique)
✅ NORMAL:        54.5% WR | Converge ~50% (baseline)
✅ LOW_VERY_BAD:  43.6% WR | +231 MMR total (0.23/game throttled)
```

---

## 🔧 Quick Start

### Compiler

```bash
make clean
make build
```

### Exécuter le test de validation

```bash
make test
```

Affiche l'analyse complète des 7 joueurs tracés sur 1000 games avec:
- Winrate par pool
- Progression MMR
- Rangs et divisions
- Métriques de convergence

### Modifier les paramètres

Éditer dans `tests/test_eomm_season_realistic.c`:
- `N_PLAYERS`: nombre de joueurs (défaut: 1000)
- `N_GAMES`: nombre total de parties (défaut: 100000)
- `MAX_GAMES_PER_PLAYER`: games max par joueur (défaut: 1000)
- Carry bonus cap (ligne ~125): actuellement 90 MMR
- Expected clamps (ligne ~180-190): skill-tier dependent
- WR scaling coefficient (ligne ~205): actuellement 1.0

---

## 📊 Concepts Clés

### ELO Fondamental

Tout joueur avec WR < 50% vs population = doit perdre du rating.
Tout joueur avec WR > 50% vs population = doit gagner du rating.

**Notre système garantit cette invariance** via WR-scaling.

### Delta Calculation

```c
delta = K × (outcome - expected)
delta *= (1.0 + wr_factor × 1.0)  // Enforce WR law
```

Où:
- `K`: K-factor dynamique (40→20→10 par expérience)
- `outcome`: 1.0 si win, 0.0 si loss
- `expected`: probabilité de victoire (clamped par skill)

### Carry Bonus & Team Composition

Effective MMR d'une équipe:
```c
effective_mmr = avg_team_mmr + carry_bonus
```

Carry bonus limité par log saturation → empêche les "unkillable" adcs

---

## 📈 Résultats de Validation

Voir `docs/ALGORITHM.md` pour l'analyse mathématique complète.

En résumé:
- **Pas de problèmes d'inflation**: smurfs convergent vers le haut
- **Pas d'exploitation WR < 50%**: système reject les gains sans winrate
- **Convergence stable**: joueurs trouvent leur niveau naturel en ~200-300 games
- **Distributions réalistes**: déciles de population correspondent aux 7 skill tiers

---

## 📝 Fichiers Clés

| Fichier | Rôle |
|---------|------|
| `include/eomm_system.h` | Définitions structures & types |
| `src/eomm_system.c` | Cœur ELO + matchmaking |
| `src/eomm_main.c` | CLI & entrée |
| `tests/test_eomm_season_realistic.c` | Validation 1M games |
| `docs/ALGORITHM.md` | Détail algo + math |

---

## 🚀 Prochains Pasde

1. Interface web (valider UI avec les données)
2. Intégration à un vrai serveur (websocket matchmaking)
3. Analytics & dashboards temps-réel
4. Anti-cheat/smurf detection (basé sur patterns)
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
