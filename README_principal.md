# Radio-Secours — ESP32 principal

Chat de secours hors ligne : l'ESP32 crée son propre Wi-Fi et sert une page web de discussion (messages texte et vocaux). Aucun Internet n'est nécessaire.

## Matériel et branchements

| Élément | Broche ESP32 |
|---|---|
| LED verte (allumée = ESP32 en marche) | 25 |
| LED jaune (allumée = au moins un téléphone connecté) | 26 |
| LED rouge (clignote = des données passent) | 27 |
| LED bleue (s'allume 0,5 s = nouveau message) | 32 |
| Buzzer (bip à chaque message) | 18 |
| Bouton (active / coupe le son) | 4 → GND |
| Potentiomètre (volume du bip) | 34 |
| Écran Grove LCD RGB Backlight | I2C : SDA 21, SCL 22 |

## Installation

1. Carte : ESP32 avec le paquet **esp32 by Espressif 3.x** (le code utilise `ledcAttach`).
2. Bibliothèque : **Grove - LCD RGB Backlight** (`rgb_lcd.h`).
3. Schéma de partitions : garder celui **par défaut** (il contient la zone de stockage utilisée pour les vocaux).
4. Téléverser `radio_secours_principal.ino`.

## Utilisation

1. Se connecter au Wi-Fi **Radio-Secours** (mot de passe affiché sur l'écran LCD).
2. La page du chat s'ouvre toute seule, sinon aller sur **http://192.168.4.1**.
3. Entrer un pseudo (il est retenu, le bouton « Modifier » permet de le changer), puis écrire.

### Messages vocaux

- 256 Kio maximum, formats conseillés : **M4A** ou **MP3**.
- Seuls les **3 derniers vocaux** sont gardés, les plus anciens deviennent « (vocal expiré) ».
- **Android (Chrome)** : bouton vocal → choisir un enregistrement ou l'enregistreur du téléphone.
- **iPhone (Safari)** : enregistrer dans **Dictaphone** → Partager → Enregistrer dans Fichiers → bouton vocal → choisir le fichier.
  Ouvrir la page dans Safari, pas dans la petite fenêtre de connexion Wi-Fi.

## Écran LCD

Les pages défilent toutes les 3 secondes :

1. Nom et adresse (192.168.4.1)
2. Mot de passe du Wi-Fi
3. État du relais (ACTIF / INACTIF) et son adresse
4. Nom du Wi-Fi du relais (seulement si le relais est actif)

## Adresses du serveur

| Adresse | Rôle |
|---|---|
| `GET /` | Page du chat |
| `GET /msg?depuis=N` | Messages après le numéro N (JSON) |
| `POST /envoyer` | Envoi d'un message texte |
| `POST /envoyer-vocal` | Envoi d'un vocal |
| `GET /vocal?id=N` | Lecture d'un vocal |
| `POST /relais-ping` | Signal de présence envoyé par le relais |
| `GET /relais-etat` | État du relais (JSON) |
| autre adresse | Redirige vers le chat (portail captif) |

Le relais est considéré **actif** s'il a envoyé un signal dans les 20 dernières secondes.

## Limites

- 25 messages gardés en mémoire, tout est effacé au redémarrage.
- Une seule requête traitée à la fois : pendant l'envoi d'un vocal, le chat se met à jour un peu plus lentement.
