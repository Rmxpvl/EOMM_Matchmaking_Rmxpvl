from __future__ import annotations

import streamlit as st

from backend.matchmaking import pairwise_matchmaking


st.set_page_config(page_title="EOMM Matchmaking", layout="centered")
st.title("EOMM Matchmaking (prototype)")

st.write("Ajoute des joueurs, puis lance le matchmaking (pairing simple).")

if "players" not in st.session_state:
    st.session_state.players = []

with st.form("add_player"):
    col1, col2 = st.columns(2)
    with col1:
        pid = st.text_input("Player id", value="")
    with col2:
        name = st.text_input("Name (optionnel)", value="")

    submitted = st.form_submit_button("Ajouter")
    if submitted:
        if not pid.strip():
            st.error("Player id obligatoire.")
        else:
            st.session_state.players.append({"id": pid.strip(), "name": name.strip() or None})
            st.success("Joueur ajouté.")

st.subheader("Players")
st.json(st.session_state.players)

st.subheader("Matchmaking")

if st.button("Run matchmaking"):
    try:
        matches = pairwise_matchmaking(st.session_state.players)
        if not matches:
            st.warning("Pas assez de joueurs.")
        else:
            st.success(f"{len(matches)} match(s) généré(s).")
            st.json(matches)
    except ValueError as e:
        st.error(str(e))

if st.button("Reset"):
    st.session_state.players = []
    st.rerun()