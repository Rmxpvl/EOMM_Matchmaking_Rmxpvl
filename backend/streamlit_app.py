from backend.matchmaking import matchmaking_5v5_with_queue

# ...

if st.button("Run matchmaking"):
    matches, remaining = matchmaking_5v5_with_queue(st.session_state.players, team_size=5)

    if len(matches) == 0:
        st.warning("Pas assez de joueurs pour lancer une partie 5v5 (min 10).")
    else:
        st.success(f"{len(matches)} partie(s) 5v5 lancée(s).")

    st.subheader("Matches")
    st.json(matches)

    st.subheader("Queue (en attente)")
    st.session_state.players = remaining
    st.json(st.session_state.players)