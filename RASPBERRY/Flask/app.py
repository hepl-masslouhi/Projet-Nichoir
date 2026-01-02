from flask import Flask, render_template, url_for, send_from_directory
import os

app = Flask(__name__)

IMAGE_FOLDER = '/home/pious/Pictures'
BATTERY_FOLDER = '/home/pious/Batterie'

@app.route('/')
def home():
    return render_template("index.html")  # page d'accueil

@app.route('/tensions')
def tensions():
    tensions_list = []

    # Lire tous les fichiers dans le dossier Batterie
    if os.path.exists(BATTERY_FOLDER):
        files = sorted(os.listdir(BATTERY_FOLDER))  # tri par nom (timestamp)
        for filename in files:
            if filename.lower().endswith(".txt"):
                filepath = os.path.join(BATTERY_FOLDER, filename)
                try:
                    with open(filepath, "r") as f:
                        value = f.read().strip()
                        # Extraire la date depuis le nom du fichier
                        date_str = filename.replace("batterie_", "").replace(".txt", "")
                        tensions_list.append(f"{date_str} → {value}")
                except Exception as e:
                    print(f"[ERR] Lecture fichier {filename} : {e}")

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
