import paho.mqtt.client as mqtt
import os
from datetime import datetime

IMAGE_FOLDER = "/home/pious/Pictures"
os.makedirs(IMAGE_FOLDER, exist_ok=True)

# Mémoire pour stocker les chunks
image_chunks = {}
expected_chunks = None

# Callback quand un message MQTT est reçu
def on_message(client, userdata, msg):
    global image_chunks, expected_chunks

    topic = msg.topic

    if topic == "esp32/image":
        payload = msg.payload

        # Lecture de l'en-tête
        chunk_id = (payload[0] << 8) | payload[1]
        total_chunks = (payload[2] << 8) | payload[3]
        data = payload[4:]

        # Initialisation pour une nouvelle image
        if chunk_id == 0:
            image_chunks = {}
            expected_chunks = total_chunks
            print(f"[INFO] Nouvelle image : {total_chunks} chunks")

        # Stockage du chunk
        image_chunks[chunk_id] = data
        print(f"[RX] Chunk {chunk_id+1}/{total_chunks}")

        # Vérification : image complète ?
        if expected_chunks is not None and len(image_chunks) == expected_chunks:
            print("[OK] Image complète reçue")

            # Reconstruction de l'image
            image_data = b''.join(image_chunks[i] for i in range(expected_chunks))

            # Sauvegarde
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = os.path.join(IMAGE_FOLDER, f"image_{timestamp}.jpg")

            with open(filename, "wb") as f:
                f.write(image_data)

            print(f"[SAVE] Image sauvegardée : {filename}")

            # Reset pour la prochaine image
            image_chunks = {}
            expected_chunks = None

    elif topic == "esp32/batterie":
        # Lecture tension batterie simple
        try:
            vbat = float(msg.payload.decode())
            print(f"[BAT] Tension reçue : {vbat:.2f} V")
        except Exception as e:
            print(f"[ERR] Impossible de lire la batterie : {e}")

# MQTT
client = mqtt.Client()
client.on_message = on_message

client.connect("172.20.10.3", 1883)

# S'abonner aux topics image et batterie
client.subscribe("esp32/image")
client.subscribe("esp32/batterie")

print("[MQTT] En attente d'images et de batterie...")
client.loop_forever()
