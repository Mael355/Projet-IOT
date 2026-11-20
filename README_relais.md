# Radio-Secours — ESP32 relais

Deuxième ESP32 qui agrandit la portée du chat. Il se connecte au Wi-Fi de l'ESP32 principal et crée son propre Wi-Fi. Les téléphones trop loin du principal se connectent au relais, qui transmet tout au principal.

Le relais **n'enregistre rien** : tous les messages et vocaux restent sur le principal. Il n'a ni écran ni LED.

## Installation

1. Carte : ESP32 avec le paquet **esp32 by Espressif 3.x**.
2. Aucune bibliothèque en plus.
3. Vérifier que les réglages en haut du fichier sont identiques à ceux du principal (nom du Wi-Fi, mot de passe, clé du relais).
4. Téléverser `radio_secours_relais.ino`.
5. Allumer le relais **à portée du principal**.

## Utilisation

1. Se connecter au Wi-Fi **Radio-Secours-Relais** (même mot de passe que le principal).
2. La page du chat s'ouvre toute seule, sinon aller sur **http://192.168.5.1**.
3. Le chat est le même que sur le principal.

Quand le relais est bien relié, l'écran du principal et la page du chat affichent **Relais : ACTIF**.

## Fonctionnement

- Au démarrage, le relais se connecte au principal puis crée son Wi-Fi sur le même canal.
- Toutes les 5 secondes, il envoie un signal de présence au principal (`/relais-ping` avec la clé du relais).
- Il transmet au principal seulement les adresses du chat : `/`, `/msg`, `/envoyer`, `/envoyer-vocal`, `/vocal`.
  Toute autre adresse renvoie vers la page du chat du relais (portail captif).
- S'il perd le principal, il réessaie de se connecter toutes les 10 secondes. Pendant ce temps, la page affiche « ESP32 principal hors de portée ».

## À savoir

- Si une nouvelle adresse est ajoutée au principal, il faut aussi l'ajouter dans la fonction `estPageDuChat` du relais.
- Un seul téléphone est traité à la fois : avec plusieurs téléphones sur le relais, le chat peut être un peu plus lent.
