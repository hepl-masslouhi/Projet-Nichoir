# Importation des bibliothèques nécessaires
import paho.mqtt.client as mqtt  # Pour communiquer avec un broker MQTT
import os                        # Pour gérer les fichiers et dossiers
from datetime import datetime    # Pour obtenir la date et l'heure actuelles

# Dossier où les images reçues seront sauvegardées
IMAGE_FOLDER = "/home/pious/Pictures"

# Création du dossier s'il n'existe pas déjà
os.makedirs(IMAGE_FOLDER, exist_ok=True)

# Fonction qui sera appelée à chaque fois qu'un message est reçu
def on_message(client, userdata, msg):
    """
    Cette fonction reçoit le message MQTT et sauvegarde son contenu comme image.
    """
    # Création d'un timestamp pour nommer l'image de façon unique
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

    # Construction du chemin complet du fichier
    filename = os.path.join(IMAGE_FOLDER, f"image_{timestamp}.png")

    # Écriture du contenu du message dans le fichier
    with open(filename, "wb") as f:
        f.write(msg.payload)

    # Confirmation dans la console
    print(f"Image sauvegardée : {filename}")

# Création d'un client MQTT
client = mqtt.Client()

# Attachement de la fonction de rappel pour les messages
client.on_message = on_message

# Connexion au broker MQTT (adresse IP et port)
client.connect("172.20.10.4", 1883)

# Abonnement au topic où les images sont publiées
client.subscribe("esp32/image")

# Boucle infinie pour maintenir le client connecté et écouter les messages
client.loop_forever()

