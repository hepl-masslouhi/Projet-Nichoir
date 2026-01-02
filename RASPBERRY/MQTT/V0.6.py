#!/usr/bin/env python3
import paho.mqtt.client as mqtt
import os
import time
from datetime import datetime
import threading

IMAGE_FOLDER = "/home/pious/Pictures"
BATTERY_FOLDER = "/home/pious/Batterie"

# Création des dossiers s'ils n'existent pas
os.makedirs(IMAGE_FOLDER, exist_ok=True)
os.makedirs(BATTERY_FOLDER, exist_ok=True)

# Mémoire pour stocker les chunks
image_chunks = {}
expected_chunks = None
timeout_timer = None
IMAGE_TIMEOUT = 10  # secondes

def save_battery(vbat):
    """Sauvegarde la tension dans un fichier horodaté"""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = os.path.join(BATTERY_FOLDER, f"batterie_{timestamp}.txt")
    with open(filename, "w") as f:
        f.write(f"{vbat:.2f} V\n")
    print(f"[SAVE] Batterie sauvegardée : {filename}")

def finalize_image():
    """Sauvegarde une image même incomplète après timeout"""
    global image_chunks, expected_chunks
    if expected_chunks is not None and len(image_chunks) > 0:
        missing = [i for i in range(expected_chunks) if i not in image_chunks]
        print(f"[TIMEOUT] Image incomplète, {len(image_chunks)}/{expected_chunks} chunks reçus. Manquants : {missing}")
        image_data = b''.join(image_chunks[i] for i in sorted(image_chunks.keys()))
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = os.path.join(IMAGE_FOLDER, f"image_{timestamp}.jpg")
        with open(filename, "wb") as f:
            f.write(image_data)
        print(f"[SAVE] Image partielle sauvegardée : {filename}")
        image_chunks = {}
        expected_chunks = None

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
                timeout_timer.cancel()  # annuler le timeout
            image_data = b''.join(image_chunks[i] for i in range(expected_chunks))
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = os.path.join(IMAGE_FOLDER, f"image_{timestamp}.jpg")
            with open(filename, "wb") as f:
                f.write(image_data)
            print(f"[SAVE] Image sauvegardée : {filename}")
            image_chunks = {}
            expected_chunks = None

    elif msg.topic == "esp32/batterie":
        try:
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

# Attente réseau + reconnexion
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
