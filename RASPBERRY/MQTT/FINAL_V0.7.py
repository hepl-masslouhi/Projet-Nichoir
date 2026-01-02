#!/usr/bin/env python3
import paho.mqtt.client as mqtt
import os
import time
import threading
from datetime import datetime
import socket
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker
from table import Images, Tensions  # Assure-toi que les tables existent

# ---------- Configuration BDD ----------
engine = create_engine("mysql+pymysql://pious:pious@172.20.10.3/mqtt")
Session = sessionmaker(bind=engine)
session = Session()

# ---------- Dossiers locaux ----------
IMAGE_FOLDER = "/home/pious/Pictures"
BATTERY_FOLDER = "/home/pious/Batterie"
os.makedirs(IMAGE_FOLDER, exist_ok=True)
os.makedirs(BATTERY_FOLDER, exist_ok=True)

# ---------- Variables globales ----------
image_chunks = {}
expected_chunks = None
timeout_timer = None
IMAGE_TIMEOUT = 20  # secondes

# ---------- Fonctions ----------
def save_battery(vbat):
    """Sauvegarde la tension localement et dans la BDD"""
    timestamp = datetime.now()
    filename = os.path.join(BATTERY_FOLDER, f"batterie_{timestamp.strftime('%Y%m%d_%H%M%S')}.txt")
    with open(filename, "w") as f:
        f.write(f"{vbat:.2f} V\n")
    print(f"[SAVE] Batterie locale sauvegardée : {filename}")

    # Sauvegarde dans la BDD
    new_tension = Tensions(tension=f"{vbat:.2f}", date=timestamp.strftime("%Y-%m-%d %H:%M:%S"))
    session.add(new_tension)
    session.commit()
    print(f"[SAVE] Batterie enregistrée en BDD : {vbat:.2f} V")

def save_image_to_db(image_data):
    """Sauvegarde une image localement et dans la BDD"""
    timestamp = datetime.now()
    filename = os.path.join(IMAGE_FOLDER, f"image_{timestamp.strftime('%Y%m%d_%H%M%S')}.jpg")
    with open(filename, "wb") as f:
        f.write(image_data)
    print(f"[SAVE] Image sauvegardée localement : {filename}")

    # Sauvegarde dans la BDD
    new_image = Images(image_path=filename, date=timestamp.strftime("%Y-%m-%d %H:%M:%S"))
    session.add(new_image)
    session.commit()
    print(f"[SAVE] Image enregistrée en BDD : {filename}")

def finalize_image():
    """Sauvegarde une image même incomplète après timeout"""
    global image_chunks, expected_chunks
    if expected_chunks is not None and len(image_chunks) > 0:
        missing = [i for i in range(expected_chunks) if i not in image_chunks]
        print(f"[TIMEOUT] Image incomplète, {len(image_chunks)}/{expected_chunks} chunks reçus. Manquants : {missing}")
        image_data = b''.join(image_chunks[i] for i in sorted(image_chunks.keys()))
        save_image_to_db(image_data)
        image_chunks = {}
        expected_chunks = None

# ---------- MQTT callback ----------
def on_message(client, userdata, msg):
    global image_chunks, expected_chunks, timeout_timer

    if msg.topic == "esp32/image":
        payload = msg.payload
        chunk_id = (payload[0] << 8) | payload[1]
        total_chunks = (payload[2] << 8) | payload[3]
        data = payload[4:]

        if chunk_id == 0:
            image_chunks = {}
            expected_chunks = total_chunks
            print(f"[INFO] Nouvelle image : {total_chunks} chunks")
            # Démarrer le timer de timeout
            if timeout_timer:
                timeout_timer.cancel()
            timeout_timer = threading.Timer(IMAGE_TIMEOUT, finalize_image)
            timeout_timer.start()

        image_chunks[chunk_id] = data
        print(f"[RX] Chunk {chunk_id + 1}/{total_chunks}")

        if expected_chunks is not None and len(image_chunks) == expected_chunks:
            print("[OK] Image complète reçue")
            if timeout_timer:
                timeout_timer.cancel()
            image_data = b''.join(image_chunks[i] for i in range(expected_chunks))
            save_image_to_db(image_data)
            image_chunks = {}
            expected_chunks = None

    elif msg.topic == "esp32/batterie":
        try:
        # Décoder en UTF-8, ignorer les caractères invalides, et supprimer les espaces / retours à la ligne
            #vbat_str = msg.payload.decode('utf-8', errors='ignore').strip()
            vbat = float(msg.payload.decode())
            print(f"[BAT] Tension reçue : {vbat:.2f} V")
            save_battery(vbat)
        except Exception as e:
            print(f"[ERR] Batterie invalide : {e}")

# ---------- MQTT ----------
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_message = on_message

BROKER = "172.20.10.3"
PORT = 1883

# Reconnexion en boucle
while True:
    try:
        print("[MQTT] Tentative de connexion...")
        client.connect(BROKER, PORT, 60)
        break
    except Exception as e:
        print(f"[MQTT] Broker inaccessible, retry dans 5s : {e}")
        time.sleep(5)

client.subscribe("esp32/image")
client.subscribe("esp32/batterie")

print("[MQTT] Connecté et en écoute")
client.loop_forever()
