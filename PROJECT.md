# Fiche projet – Radio de secours hors ligne

| | |
|---|---|
| **Projet** | Radio de secours hors ligne |
| **Parcours** | B. Projet libre |
| **Équipe** | Nathan, Mael, Lukas, Lucas, Ilan |
| **Démo** | Vendredi 20 novembre 2026, 10 minutes |

## Présentation

L'ESP32 principal sert de hub : il crée son propre réseau WiFi, les téléphones s'y connectent et échangent des messages dans un chat hébergé sur une mini page web, sans Internet. Quatre LED affichent l'état du hub en temps réel (activité, appareils connectés, trafic de données, message reçu). Un écran LCD affiche l'adresse du site, et un buzzer, dont le volume se règle au potentiomètre, bipe à chaque message reçu. Un second ESP32, le relais, fonctionne de façon autonome à côté du hub.

## Problème et objectif

Lors d'une panne d'Internet ou du réseau mobile, les applications de messagerie ne fonctionnent plus :

- **manifestations et grands rassemblements** : des milliers de téléphones sur les mêmes antennes, le réseau mobile sature ;
- **zones isolées** : montagne, refuge, zone blanche, ou antennes coupées après une catastrophe.

Notre objectif est un objet autonome qui recrée un moyen de communication local, sans Internet ni application à installer. Il suffit de se connecter au WiFi « Radio-Secours » et d'ouvrir la page affichée sur l'écran.

## Fonctionnalités

Le chat texte constitue le socle du projet. Le talkie-walkie (messages vocaux) est un bonus, réalisé si le socle est terminé.

### Socle (obligatoire)

- **Hub WiFi autonome** : l'ESP32 principal crée le point d'accès « Radio-Secours » ; les téléphones s'y connectent et ouvrent la page sur http://192.168.4.1.
- **Chat texte** : choix d'un pseudo, envoi de messages, historique des derniers messages, nombre d'appareils connectés, son de notification joué par la page sur chaque téléphone.
- **Écran LCD** : affiche l'adresse du site pour que chacun sache où se connecter.
- **LED d'activité (verte)** : allumée fixe tant que le hub fonctionne ; elle s'éteint en cas de plantage ou de redémarrage.
- **LED de connexion (jaune)** : allumée dès qu'au moins un appareil est connecté au réseau, éteinte sinon.
- **LED de trafic (rouge)** : clignote quand l'ESP32 reçoit ou envoie des données ; plus le débit est élevé, plus elle clignote vite.
- **LED de message (bleue)** : s'allume quand un message est reçu.
- **Buzzer** : bip court à chaque message reçu par l'ESP32.
- **Bouton MUET / SON** : active ou coupe le mode silencieux (plus de bip du buzzer ni de son sur les téléphones).
- **Potentiomètre** : règle le volume du buzzer.

### Ajout sur proposition du PO

- **ESP32 relais** : un second ESP32, autonome, qui fonctionne seul à côté du hub principal. Il n'a pas d'écran : l'écran LCD est sur l'ESP32 principal.

### Bonus (si on a le temps)

- **Talkie-walkie léger** : bouton « Parler » qui ouvre l'enregistreur du téléphone, envoi du vocal à l'ESP32 (20 derniers conservés) et lecture automatique chez les autres. L'envoi d'un vocal fait clignoter fortement la LED de trafic.
- **Sauvegarde sur carte SD** via l'Arduino et son shield Ethernet (nécessite un routeur sans Internet).

## Machine à états

| État | Entrée dans l'état | LED | Sortie de l'état |
|---|---|---|---|
| Démarrage | Mise sous tension | Toutes éteintes | Point d'accès WiFi prêt |
| Hub en attente | Point d'accès prêt, aucun appareil | Verte | Un appareil se connecte |
| Appareils connectés | Au moins un appareil connecté | Verte + jaune | Données échangées → Trafic, ou plus aucun appareil → Hub en attente |
| Trafic | Message, page ou vocal en cours de transfert | Verte + jaune + rouge qui clignote selon le débit | Fin du transfert → Appareils connectés |

Indépendamment de l'état : à chaque message reçu, le buzzer bipe et la LED bleue s'allume. Le mode silencieux, activé par le bouton, coupe le buzzer et le son de notification sur les téléphones.

## Conformité au socle commun

| Attendu | Notre réponse |
|---|---|
| 2 entrées dont 1 analogique | Potentiomètre (analogique), bouton, plus les messages reçus en WiFi |
| 2 sorties dont 1 PWM ou son | 4 LED en PWM, buzzer (son), écran LCD |
| Plusieurs états | Machine à états ci-dessus (4 états) |
| Schéma de câblage lisible | `wiring.png` |
| Démo de 10 minutes | Le jury rejoint le chat avec ses téléphones |

## Matériel

| Composant | Qté | Rôle | Broche ESP32 principal |
|---|---|---|---|
| ESP32 (WiFi + Bluetooth) | 1 | Hub : point d'accès WiFi, serveur web, logique du chat | — |
| ESP32 relais | 1 | Second ESP32 autonome (proposé par le PO) | — |
| Écran Grove LCD RGB Backlight V4.0 (I2C) | 1 | Affiche l'adresse du site | SDA GPIO 21, SCL GPIO 22 |
| LED verte | 1 | Activité du hub | GPIO 25 |
| LED jaune | 1 | Appareils connectés | GPIO 26 |
| LED rouge | 1 | Trafic de données (clignote selon le débit) | GPIO 27 |
| LED bleue | 1 | Message reçu | GPIO 32 |
| Résistances 220 Ω | 4 | Protection des LED | — |
| Buzzer passif | 1 | Bip à chaque message reçu | GPIO 18 |
| Bouton poussoir | 1 | Mode muet / son | GPIO 4 (pull-up interne) |
| Potentiomètre 10 kΩ | 1 | Volume du buzzer | GPIO 34, alimenté en 3,3 V |
| Breadboard + fils | 1 | Montage | — |
| Téléphones des utilisateurs | — | Affichage du chat et son de notification | WiFi |
| Arduino Uno + shield Ethernet (SD) | 1 | Bonus : sauvegarde sur carte SD | — |

Le potentiomètre est sur GPIO 34 parce que les entrées analogiques ADC2 de l'ESP32 ne fonctionnent pas quand le WiFi est actif.

## Risques et limites

- **Portée** : le WiFi de l'ESP32 couvre une salle ou un couloir (quelques dizaines de mètres), pas un bâtiment entier.
- **Nombre d'utilisateurs** : l'ESP32 accepte environ 10 appareils connectés en même temps.
- **Mémoire** : les messages sont perdus au redémarrage, sauf avec le bonus carte SD.
- **Talkie (bonus)** : un vocal enregistré sur iPhone peut ne pas se lire sur Android et inversement ; le stockage limite à une vingtaine de vocaux courts.
- **Pas de direct** : la diffusion en temps réel exige une connexion sécurisée (HTTPS) que l'ESP32 gère mal ; le talkie fonctionne donc avec quelques secondes de décalage.

## Évolution du projet

- **Idée abandonnée** : un speed test du débit WiFi / Ethernet, remplacé par le chat de secours.
- **Écran LCD** : d'abord écarté car jugé incompatible, puis réintégré quand l'équipe a trouvé comment le brancher.
- **Potentiomètre** : il réglait d'abord la luminosité des LED ; il règle maintenant le volume du buzzer.
- **Buzzer** : ajouté pour signaler chaque message reçu par l'ESP32.
- **LED bleue** : s'allume quand un message est reçu.
- **ESP32 relais** : ajouté sur proposition du PO.
