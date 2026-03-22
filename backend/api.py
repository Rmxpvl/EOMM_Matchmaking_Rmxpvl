from flask import Flask, jsonify, request
from flask_cors import CORS

app = Flask(__name__)
CORS(app)

# In-memory databases for players and matches
database = {
    'players': [],
    'matches': []
}

# Endpoint for player management
@app.route('/api/players', methods=['POST'])
def add_player():
    player_data = request.json
    database['players'].append(player_data)
    return jsonify({'message': 'Player added', 'player': player_data}), 201

@app.route('/api/players', methods=['GET'])
def get_players():
    return jsonify(database['players']), 200

@app.route('/api/players/<player_id>', methods=['DELETE'])
def delete_player(player_id):
    global database
    database['players'] = [p for p in database['players'] if p['id'] != player_id]
    return jsonify({'message': 'Player deleted'}), 200

# Endpoint for matches history
@app.route('/api/matches', methods=['GET'])
def get_matches():
    return jsonify(database['matches']), 200

# Sample matchmaking algorithm
@app.route('/api/matchmaking', methods=['POST'])
def matchmaking():
    players = request.json['players']
    # Simplified matchmaking logic (e.g., pairing players based on skill)
    if len(players) % 2 != 0:
        return jsonify({'message': 'Must have an even number of players'}), 400
    matches = [{'player1': players[i], 'player2': players[i + 1]} for i in range(0, len(players), 2)]
    database['matches'].extend(matches)
    return jsonify({'matches': matches}), 201

# Statistics endpoint
@app.route('/api/statistics', methods=['GET'])
def get_statistics():
    total_players = len(database['players'])
    total_matches = len(database['matches'])
    return jsonify({'total_players': total_players, 'total_matches': total_matches}), 200

# Endpoint for real-time updates (Flask-SocketIO can be integrated here)
@app.route('/api/updates', methods=['GET'])
def get_updates():
    # Mock updates, in real scenario would use WebSockets or similar for live updates
    return jsonify({'update': 'This is a mock update'}), 200

if __name__ == '__main__':
    app.run(debug=True)
