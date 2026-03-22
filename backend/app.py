from flask import Flask, request, jsonify

app = Flask(__name__)

# In-memory data structures for example purposes
players = []
matches_history = []

@app.route('/players', methods=['POST'])
def add_player():
    player = request.json
    players.append(player)
    return jsonify(player), 201

@app.route('/players', methods=['GET'])
def get_players():
    return jsonify(players)

@app.route('/matches', methods=['POST'])
def create_match():
    match = request.json
    match_id = len(matches_history) + 1
    matches_history.append({"id": match_id, **match})
    return jsonify(matches_history[-1]), 201

@app.route('/matches', methods=['GET'])
def get_matches():
    return jsonify(matches_history)

@app.route('/matchmaking', methods=['POST'])
def matchmaking():
    # Simple matchmaking logic placeholder
    if len(players) < 2:
        return jsonify({'message': 'Not enough players for matchmaking.'}), 400
    match = {'player1': players[0], 'player2': players[1]}
    return jsonify(match), 201

if __name__ == '__main__':
    app.run(debug=True)