# Radio de secours hors ligne

Un chat local hébergé sur un ESP32 : quand Internet ou le réseau mobile ne fonctionne plus, les téléphones à portée peuvent encore échanger des messages, sans application à installer.

| | |
|---|---|
| **Parcours** | B. Projet libre, cours IoT & Arduino |
| **Équipe** | Nathan, Mael, Lukas, Lucas, Ilan |
| **Démo** | Vendredi 20 novembre 2026, 10 minutes |

## À quoi ça sert ?

Les applications de messagerie passent par Internet. Quand le réseau tombe, plus personne ne peut se parler, alors que le WiFi des téléphones fonctionne toujours. Cas d'usage :

- **manifestations, festivals, grands rassemblements** : des milliers de téléphones sur les mêmes antennes saturent le réseau mobile, et il devient impossible de retrouver son groupe ou de prévenir quelqu'un ;
- **zones isolées** : montagne, refuge, zone blanche, ou antennes coupées après une catastrophe.

Notre objectif est un objet autonome qui recrée un moyen de communication local, sans Internet ni application à installer : il suffit de se connecter au WiFi « Radio-Secours » et d'ouvrir la page affichée sur l'écran.

## Comment ça marche

1. On met le hub sous tension : l'ESP32 principal crée le réseau WiFi « Radio-Secours » et la LED verte s'allume.
2. L'écran LCD affiche l'adresse du site.
3. Chaque utilisateur se connecte au WiFi « Radio-Secours » et ouvre http://192.168.4.1 dans son navigateur : la LED jaune s'allume.
4. Il choisit un pseudo et écrit : la LED rouge clignote pendant le transfert, le buzzer bipe et la LED bleue s'allume à chaque message reçu.
5. Le bouton coupe ou rétablit le son (buzzer et notifications des téléphones), et le potentiomètre règle le volume du buzzer.

## Fonctionnalités

Le chat texte constitue le socle du projet. Le talkie-walkie (messages vocaux) est un bonus, réalisé si le socle est terminé.

### Socle

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
| Démarrage | Mise sous tension | Toutes éteintes | Point d'accès WiFi prêt → Hub en attente |
| Hub en attente | Point d'accès prêt, aucun appareil | Verte | Un appareil se connecte → Appareils connectés |
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

Tout le matériel du socle vient du lot fourni ; l'Arduino Uno et son shield Ethernet ne servent qu'au bonus.

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

## Câblage

![Schéma de câblage](wiring.png)

- Chaque LED : broche → résistance 220 Ω → anode (patte longue) → cathode (patte courte) → GND.
- Bouton : une patte sur GPIO 4, l'autre sur GND (pull-up interne activé dans le code).
- Potentiomètre : extrémités sur 3,3 V et GND, curseur sur GPIO 34.
- Buzzer : GPIO 18 et GND.
- Écran LCD : SDA sur GPIO 21, SCL sur GPIO 22, plus l'alimentation et GND.
- Rouge = alimentation, noir = masse ; tous les GND sont communs.

## Reproduire le projet

1. Installer l'IDE Arduino 2.x et le support des cartes ESP32.
2. Installer la bibliothèque de l'écran Grove LCD RGB Backlight (Seeed Studio).
3. Câbler le montage selon `wiring.png`.
4. Ouvrir `[nom_du_projet].ino`, choisir la carte ESP32 et le bon port, puis téléverser.
5. Se connecter au WiFi « Radio-Secours » avec un téléphone et ouvrir l'adresse affichée sur l'écran.

## Nos choix techniques et pourquoi

| Choix | Pourquoi |
|---|---|
| ESP32 plutôt qu'un Arduino Uno | WiFi intégré : il crée le point d'accès et héberge la page du chat |
| Potentiomètre sur GPIO 34 (ADC1) | Les entrées analogiques ADC2 ne fonctionnent pas quand le WiFi est actif |
| Alimentation du potentiomètre en 3,3 V | Les broches de l'ESP32 travaillent en 3,3 V et ne supportent pas le 5 V |
| Résistances de 220 Ω | (3,3 − 2) / 220 ≈ 6 mA pour une LED rouge, sous la limite de 20 mA par broche |
| `millis()` au lieu de `delay()` | Le hub reste réactif aux messages pendant que les LED clignotent |
| Anti-rebond sur le bouton | Un appui = un seul passage muet / son |
| Machine à états (`enum` + `switch`) | Un état = une situation observable, lisible sur les LED |
| Écran LCD | Afficher l'adresse du site pour que chacun sache où se connecter |
| ESP32 relais | Proposé par le PO |

## Évolution du projet : essayé, abandonné, modifié

- **Idée abandonnée** : un speed test du débit WiFi / Ethernet, remplacé par le chat de secours.
- **Écran LCD** : d'abord écarté car jugé incompatible, puis réintégré quand nous avons trouvé comment le brancher.
- **Potentiomètre** : il réglait d'abord la luminosité des LED ; il règle maintenant le volume du buzzer.
- **Buzzer** : ajouté pour signaler chaque message reçu par l'ESP32.
- **LED bleue** : s'allume quand un message est reçu.
- **ESP32 relais** : ajouté sur proposition du PO.

## Tests

| # | Cas | Action | Attendu | Observé |
|---|---|---|---|---|
| 1 | Démarrage | Brancher le hub | LED éteintes, puis LED verte quand le WiFi est prêt ; adresse affichée sur l'écran | |
| 2 | Connexion | Connecter un téléphone au WiFi | LED jaune allumée, page accessible | |
| 3 | Message | Envoyer un message | LED rouge qui clignote, bip, LED bleue allumée, message visible sur les autres téléphones | |
| 4 | Mode muet | Appuyer sur le bouton | Plus de bip ni de son sur les téléphones | |
| 5 | Appuis rapides | Appuyer 10 fois en 3 s | Un seul changement par appui | |
| 6 | Volume | Tourner le potentiomètre jusqu'aux butées | Volume qui suit, sans valeur absurde | |
| 7 | Déconnexion | Déconnecter tous les téléphones | LED jaune éteinte, retour à « Hub en attente » | |

## Bilan

**Ce qui marche :** [à compléter]

**Ce qui reste (bonus) :** talkie-walkie léger (messages vocaux) ; sauvegarde des messages sur carte SD.

**Ce qu'on referait autrement :** [à compléter]

## Risques, limites et améliorations possibles

- **Portée** : le WiFi de l'ESP32 couvre une salle ou un couloir (quelques dizaines de mètres), pas un bâtiment entier.
- **Nombre d'utilisateurs** : l'ESP32 accepte environ 10 appareils connectés en même temps.
- **Mémoire** : les messages sont perdus au redémarrage, sauf avec le bonus carte SD.
- **Sécurité** : réseau ouvert ; on pourrait ajouter un mot de passe WiFi et valider les messages reçus.
- **Talkie (bonus)** : un vocal enregistré sur iPhone peut ne pas se lire sur Android et inversement ; le stockage limite à une vingtaine de vocaux courts.
- **Pas de direct** : la diffusion en temps réel exige une connexion sécurisée (HTTPS) que l'ESP32 gère mal ; le talkie fonctionne donc avec quelques secondes de décalage.
