# EOMM Conversation Recap

## Key Decisions Made:

1. **Reset Modes**:
   - **7 Parties** reset mode vs **14 Parties** reset mode discussed. 7 Parties favored for scalability and ease in matchmaking.

2. **Hidden MMR State Categorization**:
   - Categorized into 3 states: **NEGATIVE**, **NEUTRAL**, and **POSITIVE**. This helps in better matchmaking decisions based on historical performance.

3. **Champion Pool System**:
   - Introduced a flexible champion pool system that allows players to adapt based on matchmaking conditions and their historical performance.

4. **Team Composition Bias**:
   - Implemented a **30% randomness** factor to encourage diversity in team compositions, aiming for more engaging matches.

5. **LoL Standard LP**:
   - Adopted a similar LP system to League of Legends to standardize player rank and match outcomes, enhancing familiarity among players.

6. **Real-World Smurf Data**:
   - Analyzed data from **AssBlaster78**, showing **12 losses** with a **good KDA**, reinforcing the need for accurate skill tier assessments.

---

## Expected Outcomes:
- **Bob**: 58% winrate expected based on past performance metrics.
- **Alice**: 42% winrate expected, adjustments recommended based on matchmaking feedback.

---

## Proof Narrative:
- Analyzed data shows that transparent matchmaking leads to clearer expectations and satisfaction among players.

---

## Technical Implementation Details:
- Constants used in calculation:
   - `RANDOMNESS_FACTOR = 0.3`
   - `WINRATE_BOB = 0.58`
   - `WINRATE_ALICE = 0.42`

```python
# Example Code Snippet
class Matchmaking:
    def __init__(self):
        self.randomness_factor = 0.3

    def calculate_win_probability(self, player):
        # Calculation logic here
        return win_probability
```

---

## Next Steps Roadmap:
1.   Refine matchmaking algorithm with feedback from initial testing.
2.   Implement champion pool system with variable adjustments.
3.   Gather further data on player behavior under new reset modes.
4.   Develop documentation for players to understand the changes.
5.   Schedule follow-up discussions to evaluate impact and adjust strategies as needed.

---

Date Updated: 2026-03-21 03:00:09 UTC
