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
│   └── scripts/            ← helpers Python (calibration, analyse, simulation)
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

## Build / exécution

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
