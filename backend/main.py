from flask import Flask, request, jsonify

app = Flask(__name__)

@app.route('/matchmaking', methods=['POST'])
def matchmaking():
    data = request.json
    return jsonify({
        'message': 'Matchmaking logic goes here!',
        'data': data
    })

@app.route('/status', methods=['GET'])
def status():
    return jsonify({'status': 'API is running'})

if __name__ == '__main__':
    app.run(debug=True)