# Projet Nichoir Connecté 🐦
<img width="620" height="414" alt="Image" src="https://github.com/user-attachments/assets/aefba0d8-88c2-43fa-afcf-ec8ca74970dd" />
## Description

Dans le cadre du cours **SmartCities**, nous devons réaliser un projet de type **nichoir connecté**. L’objectif est de concevoir un système autonome capable de **capturer des images**, **mesurer des tensions** et **transmettre ces données** pour analyse et visualisation.

Pour ce projet, nous utilisons un **ESP32-CAM** 📷 pour capturer les images et mesurer les tensions, et un **Raspberry Pi** 🖥️ pour stocker, traiter et afficher ces données sur un site web. La communication entre les modules se fait via le protocole **MQTT**, permettant une transmission fiable et en temps réel.

Ce projet combine les notions de **IoT**, de **traitement d’image** et de **capteurs**, et vise à créer un système complet pour l’observation des oiseaux dans un environnement naturel.  
Le système est alimenté par une **batterie rechargeable** 🔋, rechargée par un **panneau solaire** ☀️, pour un fonctionnement autonome en extérieur.

---

## Architecture du projet

- **ESP32-CAM** : capture des images et mesure la tension de la batterie  
- **Capteur PIR** : détecte les mouvements à l’intérieur du nichoir pour déclencher la caméra et la LED flash  
- **LED flash** : éclaire l’intérieur du nichoir
- **PCB personnalisé** : regroupe le PIR, la LED et les connexions avec l’ESP32-CAM pour un montage compact et sécurisé  
- **Raspberry Pi** : stocke les images, analyse les données et les rend accessibles via un site web  
- **Communication MQTT** : assure le transfert des images et des mesures entre l’ESP32 et le Raspberry Pi  
- **Boîtier protecteur** : protège l’électronique des intempéries et des animaux  
- **Alimentation autonome** : batterie rechargeable avec panneau solaire pour un fonctionnement continu  

---

## Répertoires

- [ESP32](./ESP32/) : code Arduino pour la capture d’images, lecture du capteur et envoi MQTT  
- [RaspberryPi](./RaspberryPi/) : scripts Python pour réception MQTT, stockage et affichage sur site web  
- [Boîtier](./Boitier/) : fichiers liés au boîtier  
- [PCB](./PCB/) : fichiers et schémas du PCB pour le montage PIR + LED  

---

## ESP32-CAM 📷

Pour le projet, nous utilisons le module **TimerCamera**, basé sur l’ESP32. Il permet de capturer des images et de les transmettre via Wi-Fi vers le Raspberry Pi.

### Caractéristiques principales

- Microcontrôleur : **ESP32-D0WDQ6-V3**  
- Caméra : **OV3660**, 3MP, DFOV 66.5°  
- Résolution maximale : 2048 × 1536  
- Mémoire : 8MB PSRAM, 4MB Flash  
- Batterie rechargeable possible via port dédié  
- Connectivité : **Wi-Fi** pour transmission d’images 

<img width="500" height="500" alt="Image" src="https://github.com/user-attachments/assets/7f696215-18ed-42f8-b4d0-89aa8737adf9" />

### Développement

- Programmation en **Arduino IDE (C/C++)**
![Image](https://github.com/user-attachments/assets/3ae2f8f3-50c1-431d-bfd9-ee0a236af56a)

---

## Raspberry Pi 🖥️

Le **Raspberry Pi** sert de serveur central pour stocker les images et mesures, et pour fournir l’interface web.

### Fonctions principales

- Stockage des images et des mesures  
- Traitement des images et visualisation en temps réel  
- Serveur web local avec Flask 
- Réception des données via **MQTT**  

<img width="623" height="401" alt="Image" src="https://github.com/user-attachments/assets/e2beb3de-28a1-4ba5-be4a-76e60e3efe2a" />

### Développement

- Programmation en **Python** via **Visual Studio Code**
<img width="100" height="100" alt="Image" src="https://github.com/user-attachments/assets/c8c247fe-ab5e-43ec-9325-5a7f1a45c9ba" />

---

## PCB et Boîtier

- **PCB personnalisé** : permet de connecter le PIR, la LED flash et l’ESP32-CAM via un connecteur, réduisant l’encombrement et sécurisant les connexions  
- **Boîtier** : protège tout le système des intempéries et des animaux, avec ouverture pour entretien et alimentation  
- **Gestion de l’énergie** : intégration de la batterie et du panneau solaire pour un fonctionnement autonome  

---

## Site Web

Le site web permet de :

- Visualiser les images capturées par l’ESP32-CAM 📷  
- Afficher les mesures de tension en temps réel ⚡   
- Intégrer des graphiques dynamiques pour l’évolution des tensions 📈  

---

## Communication MQTT

- Protocole léger et efficace pour l’IoT  
- **ESP32-CAM** publie les images et mesures sur un **broker MQTT**  
- **Raspberry Pi** s’abonne aux topics pour recevoir et stocker les données  
- Permet une architecture **temps réel** et **scalable**
