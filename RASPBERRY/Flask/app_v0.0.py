from flask import Flask, render_template, url_for, send_from_directory
import os

app = Flask(__name__)

IMAGE_FOLDER = '/home/pious/Pictures'

@app.route('/')
def home():
    return render_template("index.html")  # page d'accueil

@app.route('/tensions')
def tensions():
    # Exemple : liste vide pour l'instant
    tensions_list = [
    "12/08 → 14.2V",
    "13/08 → 13.9V",
    "14/08 → 14.4V",
    "15/08 → 14.1V",
    "16/08 → 13.8V",
    "17/08 → 14.3V",
    "18/08 → 14.0V",
    "19/08 → 14.5V",
    "20/08 → 13.7V",
    "21/08 → 14.2V"
		]

    return render_template("tensions.html", tensions=tensions_list)

@app.route('/photos')
def photos():
    images = [img for img in os.listdir(IMAGE_FOLDER) if img.lower().endswith(('.png', '.jpg', '.jpeg', '.gif'))]
    images.sort(reverse=True)
    images = [url_for('serve_image', filename=img) for img in images]
    return render_template("gallery.html", images=images)

@app.route('/images/<filename>')
def serve_image(filename):
    return send_from_directory(IMAGE_FOLDER, filename)

if __name__ == "__main__":
    app.run(host='0.0.0.0', port=5000, debug=True)
