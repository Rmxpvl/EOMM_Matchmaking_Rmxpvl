# EOMM_Matchmaking_Rmxpv

Projet perso autour d’un **EOMM (Engagement Optimized Matchmaking)** : simulation + implémentation d’un moteur de matchmaking où l’objectif n’est pas seulement “l’équité”, mais aussi la **rétention/engagement** via des mécanismes de **hidden MMR**, **effective MMR**, et (potentiellement) des **streaks win/loss** “crédibles”.

> Contexte actuel (au 2026-03-19) : le repo mélange un **backend Python** (Flask + Streamlit) et un **moteur C** (`c_engine`). La démarche visée est de **déployer rapidement sur Streamlit Cloud** en mode “Streamlit = UI + exécution de la logique” (sans dépendre d’une API séparée dans un premier temps). Voir `README_DEMARCHE.md` pour les décisions détaillées.

---

## Objectifs principaux

- **Simuler une population** (ex: ~500 joueurs) avec profils variés (smurfs, faibles, joueurs moyens…).
- Mettre en place un **matchmaking 5v5** :
  - Un match ne démarre **que lorsqu’on a 10 joueurs** disponibles.
  - Si plus de 10 joueurs sont en file, on match par **blocs de 10** et on conserve une **queue**.
- Utiliser une MMR “à 2 couches” :
  - `visibleMMR` (ce que le joueur “voit”)
  - `hiddenMMR` (utilisé pour piloter/ajuster le matchmaking)
- Introduire une **effectiveMMR** du type :
  - `effectiveMMR = visibleMMR × hiddenFactor`
- Gérer une dynamique long-terme :
  - **Reset complet** du hiddenMMR après **7 jours** d’inactivité (IRL, basé sur `lastMatchTime`)
  - **Soft reset** périodique (ex: toutes les X parties) pour recoller progressivement au visibleMMR

---

## Architecture (high-level)

- `backend/`
  - Contient la partie Python (Flask/Streamlit) pour piloter la simulation et l’UI.
  - Objectif court terme : **Streamlit** exécute la logique de simulation (option “déploiement simple”).
- `c_engine/`
  - Contient un moteur en C (structures & logique “source de vérité conceptuelle” côté simulation).
- `frontend/`
  - Présent dans l’arborescence (à compléter selon les besoins).
- `docs/`
  - Documentation additionnelle (si ajoutée).
- `README_DEMARCHE.md`
  - Document de travail : constats, décisions, specs EOMM, points ouverts.
- `engine_structure.md`
  - Notes de structure (générique) sur l’“engine” et le build.

---

## Spécifications de simulation / matchmaking (résumé)

### Matchmaking 5v5 + file d’attente
- On alimente une **queue** de joueurs.
- Dès que la queue atteint **10 joueurs**, on crée 1 match :
  - 5 vs 5
- Les joueurs restants (ex: 12 joueurs → 2 restants) restent en queue.

### Fin de match
- Quand un match se termine, **les joueurs reviennent dans la file d’attente**.

### Inactivité (7 jours)
- Si un joueur n’a pas joué depuis **≥ 7 jours**, on déclenche un **reset** (ex: `hiddenMMR = neutralMMR`) en se basant sur `lastMatchTime` (ou un compteur dérivé “jours sans jouer”).

---

## Concepts clés (EOMM)

- **hiddenFactor** : facteur multiplicatif (>= 0.5) qui pénalise certains comportements/écarts (rôle non-main, champion non-main, “activité” type ping/clicks/deaths, etc.).
- **effectiveMMR** : MMR utilisée pour trier/placer les joueurs avant assignment d’équipes.
- **Bias/streaks** : point central possible de l’EOMM : ajuster subtilement la composition des équipes pour pousser des séquences win/loss “plausibles” tout en gardant un winrate global ~50% (objectif de crédibilité).

---

## Build / exécution (état actuel)

Le repo contient des fichiers de build génériques :
- `CMakeLists.txt` (cible C++ exemple via `src/main.cpp`, etc.)
- `Makefile` (cible `main` via `src/*.cpp`)

> Remarque : la partie build C/C++ semble encore “placeholder” au root (les chemins `src/main.cpp` etc. ne reflètent pas forcément `c_engine/`). Il faudra aligner la config build avec la vraie arborescence du moteur.

---

## Documentation & références internes

- Démarche / décisions / spec : **`README_DEMARCHE.md`**
- Notes de structure : **`engine_structure.md`**
- Backend : **`backend/README.md`**

---

## Roadmap (prochaine étape réaliste)

- [ ] Implémenter la simulation Streamlit “end-to-end” :
  - init joueurs
  - queue
  - matchmaking 5v5 par blocs de 10
  - fin de match + remise en queue
  - update MMR (visible/hidden) + reset (7 jours) + soft reset
- [ ] Unifier/clarifier les points d’entrée backend (si Flask reste utile en local).
- [ ] Aligner le `c_engine/` avec une API claire (et/ou binding Python si besoin).

---

## Contribuer

Voir `CONTRIBUTING.md`.
