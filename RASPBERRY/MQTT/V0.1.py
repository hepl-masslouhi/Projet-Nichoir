# -*- coding: utf-8 -*-

import paho.mqtt.client as mqtt
from datetime import datetime
import os

from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker
from table import Images, Tensions

# --- Configuration de la base de données ---
engine = create_engine("mysql+pymysql://pious:pious@172.20.10.4/mqtt")
Session = sessionmaker(bind=engine)
session = Session()

# --- Dossier où les images seront sauvegardées ---
IMAGE_FOLDER = "/home/pious/Pictures"
os.makedirs(IMAGE_FOLDER, exist_ok=True)

# Buffer temporaire pour reconstruire l'image en binaire
image_buffer = bytearray()


def on_message(client, userdata, msg):
    global image_buffer

    # Si le message est "END", l'image est complète
    if msg.payload == b"END":
        print("Dernier chunk reçu : reconstruction de l'image...")

        # Nom du fichier
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"image_{timestamp}.jpg"
        filepath = os.path.join(IMAGE_FOLDER, filename)

        # Sauvegarde du fichier
        with open(filepath, "wb") as f:
            f.write(image_buffer)

        print(f"Image sauvegardée : {filepath}")

        # Enregistrement en base
        new_image = Images(
            image_path=filepath,
            date=datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        )
        session.add(new_image)
        session.commit()

        print("Image enregistrée dans la base de données.")

        # Reset du buffer
        image_buffer = bytearray()
        return

    # Sinon : chunk binaire
    image_buffer.extend(msg.payload)
    print(f"Chunk reçu : {len(msg.payload)} octets — total accumulé : {len(image_buffer)} octets")


# --- Configuration MQTT ---
client = mqtt.Client()
client.on_message = on_message

client.connect("172.20.10.4")
client.subscribe("esp32/image")

print("En attente des images...")
client.loop_forever()
