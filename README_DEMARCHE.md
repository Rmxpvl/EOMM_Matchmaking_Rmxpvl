# README_DEMARCHE — Démarche, décisions & specs (Streamlit / EOMM)

Date de travail : **2026-03-19**
Repo : **Rmxpvl/EOMM_Matchmaking_Rmxpv**
Objectif : **déployer sur Streamlit (option 2) — backend/logique d’abord**.

---

## 0) Contexte

- Le repo mélange :
  - un backend Python (Flask + Streamlit),
  - un moteur C (`c_engine`).
- On veut avancer vite sur une simulation Streamlit, tout en gardant Flask pour du local/dev.

---

## 1) Déploiement : choix Streamlit (Option 2)

### Option retenue
**Option 2 : Streamlit = UI + exécution de la logique backend** (pas d’API séparée obligatoire).

### Pourquoi
Streamlit Cloud lance typiquement `streamlit run <fichier>.py` et n’est pas un hébergement d’API Flask “classique”.
Donc pour une première étape “déploiement rapide”, on garde la logique en Python et Streamlit pilote la simulation.

---

## 2) Constats initiaux sur le backend Python

### A) Serveur(s) en doublon
Le dossier `backend/` contenait plusieurs fichiers Flask avec endpoints proches mais incohérents :
- `backend/app.py`
- `backend/api.py`
- `backend/main.py`

Décision : **unifier** à terme (un seul point d’entrée Flask si besoin), et surtout ne pas dépendre de Flask pour Streamlit (option 2).

### B) Dépendances
L’historique avait des incohérences `requirements.txt` (FastAPI/SQLAlchemy vs code Flask).
Décision : rester sur Flask et maintenir `backend/requirements.txt` cohérent.

---

## 3) Problème de matchmaking : 1v1 au lieu de 5v5

### Problème observé
- Ajout de 10 joueurs → génération de **5 matchs 1v1**.

### Comportement attendu
- Une partie se lance **uniquement à 10 joueurs** :
  - **5 joueurs** dans une équipe
  - **5 joueurs** dans l’autre
- Si on a 12 joueurs :
  - **10** matchés
  - **2** restent en **file d’attente** (queue)

Décision : matchmaking “par blocs de 10”, avec **queue** conservée.

---

## 4) Simulation : fin de match et remise en queue

Règle :
- Quand un match se termine, ses joueurs reviennent dans la file d’attente.

Règle additionnelle :
- On veut simuler des joueurs qui ne jouent pas pendant **7 jours et plus**.
- Pour la simulation : on remet ces joueurs dans la file d’attente **et** on met à jour une variable :
  - **“nombre de jours sans avoir joué”** (conceptuellement existante).

Remarque :
- Dans l’état actuel du Python minimal, cette variable n’apparaît pas.
- Dans l’état actuel de `c_engine/src/player.h`, il n’y a pas non plus ce champ.
Décision : **l’introduire côté simulation Python** (ou la dériver via `lastMatchTime`).

---

## 5) Données essentielles re-fournies (spec EOMM)

### 5.1 Population & dynamique
- **500 joueurs**
- Top **2 roles** / top **3 champions** dynamiques
- HiddenMMR dynamique + soft-reset
- Reset complet après **7 jours IRL**
- Placement initial aléatoire sur la première vague
- Matchmaking multi-équipes avec :
  - **effectiveMMR = visibleMMR × hiddenFactor**
- Chaînes win/loss **forcées** mais crédibles (winrate global ~50%)
- Gestion des smurfs et des joueurs faibles (tester extrêmes)

---

## 6) Structure du joueur (C) — source de vérité conceptuelle

```c
typedef struct {
    char *username;
    float visibleMMR;
    float hiddenMMR;
    float neutralMMR;

    int currentRole;
    int roleHistory[5];
    int roleGames[5];

    char *currentChampion;
    char *champHistory[20];
    int champGames[20];

    int pingCount;
    int deaths;
    int clickRate;

    time_t lastMatchTime;  // pour reset après 7 jours IRL

    int totalWins;
    int totalLosses;

} Player;
```

---

## 7) Calcul du hiddenFactor (fourni)

```c
float calculateHiddenFactor(Player *p, float avgPing, float avgClicks) {
    float factor = 1.0;

    // --- Rôle ---
    int top2Roles[2];
    updateTop2Roles(p, top2Roles);
    int roleOK = 0;
    for (int i = 0; i < 2; i++)
        if (p->currentRole == top2Roles[i]) roleOK = 1;
    if (!roleOK) factor -= 0.1;

    // --- Champions ---
    char *top3Champ[3];
    updateTop3Champions(p, top3Champ);
    int champOK = 0;
    for (int i = 0; i < 3; i++)
        if (top3Champ[i] && strcmp(p->currentChampion, top3Champ[i]) == 0)
            champOK = 1;
    if (!champOK) factor -= 0.2;

    // --- Activité ---
    if (p->pingCount > avgPing) factor -= 0.05 * (p->pingCount - avgPing);
    if (p->deaths > 3) factor -= 0.05 * (p->deaths - 3);
    if (p->clickRate > avgClicks) factor -= 0.05 * (p->clickRate - avgClicks);
    else if (p->clickRate < avgClicks * 0.5) factor -= 0.1;

    if (factor < 0.5) factor = 0.5;

    return factor;
}
```

---

## 8) Reset complet (7 jours IRL) + soft reset

### Reset complet
```c
void resetHiddenMMR(Player *p) {
    time_t now = time(NULL);
    if (difftime(now, p->lastMatchTime) >= 604800) { // 7 jours IRL
        p->hiddenMMR = p->neutralMMR;
        p->lastMatchTime = now;
    }
}
```

### Soft reset
```c
void softResetHiddenMMR(Player *p) {
    if (p->totalWins + p->totalLosses % 8 == 0) {
        float adjustment = 0.05;
        p->hiddenMMR = p->hiddenMMR * (1.0 - adjustment) + p->visibleMMR * adjustment;
    }
}
```

⚠️ Point d’attention : priorité des opérateurs → intention probable :
`if (((wins + losses) % 8) == 0)`.

---

## 9) Placement initial (première vague)

```c
void placeInitialTeams(Player pool[], int numPlayers, int teamSize) {
    shuffle(pool, numPlayers);
    for (int i = 0; i < numPlayers; i += teamSize) {
        createTeam(&pool[i], teamSize);
    }
}
```

---

## 10) Placement basé sur effectiveMMR

```c
float effectiveMMR(Player *p) {
    return p->visibleMMR * calculateHiddenFactor(p, AVG_PING, AVG_CLICKS);
}

void createMatch(Player pool[], int numPlayers, int teamSize) {
    sortByEffectiveMMR(pool, numPlayers);
    for (int i = 0; i < numPlayers; i += teamSize) {
        assignTeamWithBias(&pool[i], teamSize);
    }
}
```

- `assignTeamWithBias` = coeur de la manipulation (streaks win/loss forcées, crédibles).

---

## 11) Simulation multi-parties (boucle)

```c
for (int game = 0; game < NUM_GAMES; game++) {
    for (int i = 0; i < 500; i++) {
        resetHiddenMMR(&players[i]);
    }

    createMatch(players, 500, 5);

    for (int i = 0; i < NUM_TEAMS; i++) {
        simulateGame(&teams[i]);
    }

    for (int i = 0; i < 500; i++) {
        updateMMR(&players[i]);
        softResetHiddenMMR(&players[i]);
    }
}
```

---

## 12) Points clés (résumé)

- Première vague : **random placement**
- Collecte : top roles / top champions / performance
- EOMM : effectiveMMR + biais pour win/loss chains
- Crédibilité : ~50% winrate global, visibleMMR équilibré
- Extrêmes : smurfs + joueurs faibles
- Long terme : soft reset + reset complet 7 jours IRL

---

## 13) Actions à implémenter côté Streamlit (Option 2)

- Init 500 joueurs (avec profils extrêmes)
- Queue + matchmaking 5v5 par paquets de 10
- Simulation résultat match + update (visibleMMR/hiddenMMR)
- Remise en queue après match
- Gestion inactivité :
  - timestamp `lastMatchTime` et/ou compteur “jours sans jouer”
  - reset complet hiddenMMR après 7 jours IRL

---

## 14) Branches / organisation

- Une branche de travail a été créée : `streamlit-backend-flask-fixes`

---

## 15) Questions ouvertes à trancher (pour coder exactement)

1. Rôles : nombre et codage (0..4 ?)
2. Champions : liste et taille (20/50/160 ?)
3. Temps : 7 jours IRL strict vs temps simulé en Streamlit
4. Champ “jours sans jouer” :
   - dérivé de `lastMatchTime`
   - ou champ explicite (plus simple en simulation)