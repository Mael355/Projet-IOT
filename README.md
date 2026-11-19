# Radio de secours hors ligne

Un chat local hébergé sur un ESP32 : quand Internet ou le réseau mobile ne fonctionne plus, les téléphones à portée peuvent encore échanger des messages, sans application à installer.

Projet libre (parcours B) du cours IoT & Arduino. Équipe : Nathan, Mael, Lukas, Lucas, Ilan.

## À quoi ça sert ?

Les applications de messagerie passent par Internet. Quand le réseau tombe, plus personne ne peut se parler, alors que le WiFi des téléphones fonctionne toujours. Cas d'usage :

- **manifestations, festivals, grands rassemblements** : des milliers de téléphones sur les mêmes antennes saturent le réseau mobile, et il devient impossible de retrouver son groupe ou de prévenir quelqu'un ;
- **zones isolées** : montagne, refuge, zone blanche, ou antennes coupées après une catastrophe.

Notre objet recrée un réseau de communication local, indépendant d'Internet.

## Comment ça marche

1. On met le hub sous tension : l'ESP32 principal crée le réseau WiFi « Radio-Secours » et la LED verte s'allume.
2. L'écran LCD affiche l'adresse du site.
3. Chaque utilisateur se connecte au WiFi « Radio-Secours » et ouvre http://192.168.4.1 dans son navigateur : la LED jaune s'allume.
4. Il choisit un pseudo et écrit : la LED rouge clignote pendant le transfert, le buzzer bipe et la LED bleue s'allume à chaque message reçu.
5. Le bouton coupe ou rétablit le son (buzzer et notifications des téléphones), et le potentiomètre règle le volume du buzzer.

Un second ESP32, le relais, fonctionne de façon autonome à côté du hub (ajouté sur proposition du PO). Il n'a pas d'écran.

### États du système

| État | LED | Passe à l'état suivant quand… |
|---|---|---|
| Démarrage | toutes éteintes | le point d'accès WiFi est prêt |
| Hub en attente | verte | un appareil se connecte |
| Appareils connectés | verte + jaune | des données passent (→ Trafic) ou plus personne n'est connecté (→ Hub en attente) |
| Trafic | verte + jaune + rouge qui clignote selon le débit | le transfert est fini (→ Appareils connectés) |

Indépendamment de l'état : bip et LED bleue à chaque message reçu, mode muet au bouton.

## Matériel

| Composant | Qté | Rôle | Broche ESP32 principal |
|---|---|---|---|
| ESP32 | 1 | Hub : point d'accès WiFi, serveur web, chat | — |
| ESP32 relais | 1 | Second ESP32 autonome | — |
| Écran Grove LCD RGB Backlight V4.0 (I2C) | 1 | Affiche l'adresse du site | SDA GPIO 21, SCL GPIO 22 |
| LED verte + 220 Ω | 1 | Activité du hub | GPIO 25 |
| LED jaune + 220 Ω | 1 | Appareils connectés | GPIO 26 |
| LED rouge + 220 Ω | 1 | Trafic de données | GPIO 27 |
| LED bleue + 220 Ω | 1 | Message reçu | GPIO 32 |
| Buzzer passif | 1 | Bip à chaque message reçu | GPIO 18 |
| Bouton poussoir | 1 | Mode muet / son | GPIO 4 (pull-up interne) |
| Potentiomètre 10 kΩ | 1 | Volume du buzzer | GPIO 34 (alimenté en 3,3 V) |
| Breadboard + fils | 1 | Montage | — |

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

**Ce que nous avons essayé et abandonné :**

- un speed test du débit WiFi / Ethernet, remplacé par le chat de secours ;
- le potentiomètre pour régler la luminosité des LED : il règle maintenant le volume du buzzer ;
- l'écran LCD avait d'abord été écarté car jugé incompatible, puis réintégré quand nous avons trouvé comment le brancher.

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

## Limites et améliorations possibles

- **Portée** : une salle ou un couloir (quelques dizaines de mètres).
- **Nombre d'utilisateurs** : environ 10 appareils à la fois.
- **Mémoire** : messages perdus au redémarrage (amélioration : carte SD).
- **Sécurité** : réseau ouvert ; on pourrait ajouter un mot de passe WiFi et valider les messages reçus.
- **Talkie-walkie** : formats audio différents entre iPhone et Android, et quelques secondes de décalage.
