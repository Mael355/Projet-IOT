#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <rgb_lcd.h>

// broches
const int LED_VERTE = 25;
const int LED_JAUNE = 26;
const int LED_ROUGE = 27;
const int LED_BLEUE = 32;
const int BUZZER = 18;
const int BOUTON = 4;
const int POT = 34;

// reglages wifi
const char* NOM_WIFI = "Radio-Secours";
const char* MOT_DE_PASSE = "Kx7!Pm4q-Zr9w#";
IPAddress IP(192, 168, 4, 1);

// reglages relais identiques dans le sketch du relais
const char* NOM_WIFI_RELAIS = "Radio-Secours-Relais";
const char* ADRESSE_RELAIS = "192.168.5.1";
const char* CLE_RELAIS = "radio-secours-relais-presence-v1";

// limites
const int MAX_MSG = 25;
const int MAX_VOCAUX = 3;
const int TAILLE_MAX_VOCAL = 256 * 1024;

WebServer serveur(80);
DNSServer dns;
rgb_lcd lcd;

// historique des messages
struct Message {
  int id;
  String pseudo;
  String texte;
  bool vocal;
  String type;
};
Message messages[MAX_MSG];
int nbMessages = 0;
int dernierId = 0;

// etat general
bool silencieux = false;
int volume = 50;
unsigned long dernierPingRelais = 0;
unsigned long finBip = 0;
unsigned long finLedBleue = 0;
unsigned long octets = 0;
unsigned long debit = 0;

// vocal en cours de reception
File fichierRecu;
int tailleRecue = 0;
String typeRecu;

// page web
const char PAGE[] = R"HTML(<!doctype html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Radio-Secours</title>
<style>
body{margin:0;height:100dvh;display:flex;flex-direction:column;font-family:sans-serif;background:#0f172a;color:#e2e8f0}
header,#bas{background:#1e293b;padding:10px}
h1{margin:0;font-size:18px}
small{display:block;color:#94a3b8}
#liste{flex:1;overflow-y:auto;padding:10px}
.msg{background:#1e293b;border-radius:10px;padding:8px;margin-bottom:8px}
.msg b{display:block;color:#7dd3fc}
input,button{font-size:16px;padding:10px;border:0;border-radius:8px;margin:3px 0}
input[type=text]{width:100%;box-sizing:border-box}
button{background:#22c55e}
</style></head><body>
<header><h1>📻 Radio-Secours</h1><small id="etat">Connexion...</small><small id="relais"></small></header>
<div id="liste"></div>
<div id="bas">
  <div style="display:flex;gap:8px"><input id="pseudo" type="text" maxlength="24" placeholder="Ton pseudo"><button onclick="modifierPseudo()" style="background:#475569;color:#fff">Modifier</button></div>
  <input id="texte" type="text" maxlength="200" placeholder="Message" onkeydown="if(event.key==='Enter')envoyerTexte()">
  <button onclick="envoyerTexte()">Envoyer</button>
  <label>🎙️ Vocal (256 Kio max, M4A ou MP3) : <input id="fichier" type="file" accept="audio/*,.m4a,.mp3,.wav,.aac" onchange="envoyerVocal()"></label>
  <small>iPhone : ouvre la page dans Safari. Enregistre d'abord dans Dictaphone, puis Partager → Enregistrer dans Fichiers, puis choisis le fichier ici.</small>
  <small id="info"></small>
</div>
<script>
const $ = id => document.getElementById(id);
let dernier = 0;
let premiereLecture = true;
let audio = null;
let lectureEnCours = false;
let envoiEnCours = false;
$('pseudo').value = localStorage.getItem('pseudo') || '';
$('pseudo').readOnly = $('pseudo').value != '';

function modifierPseudo() {
  $('pseudo').readOnly = false;
  $('pseudo').focus();
}

// le navigateur autorise le son seulement apres un appui
document.addEventListener('pointerdown', () => {
  try {
    if (!audio) audio = new (window.AudioContext || window.webkitAudioContext)();
  } catch (e) {}
});

// libere le son quand on quitte la page pour l'appli dictaphone
document.addEventListener('visibilitychange', () => {
  if (document.hidden && audio) audio.suspend();
});

// bip court dans le telephone
function son(vol) {
  if (!audio || vol == 0 || document.hidden) return;
  audio.resume();
  const o = audio.createOscillator();
  const g = audio.createGain();
  g.gain.value = 0.2 * vol / 100;
  o.frequency.value = 880;
  o.connect(g).connect(audio.destination);
  o.start();
  o.stop(audio.currentTime + 0.12);
}

// affiche un message dans la liste
function afficher(m) {
  const div = document.createElement('div');
  div.className = 'msg';
  const nom = document.createElement('b');
  nom.textContent = m.p;
  div.append(nom);
  if (m.v) {
    const lecteur = document.createElement('audio');
    lecteur.controls = true;
    lecteur.preload = 'none';
    lecteur.src = '/vocal?id=' + m.id;
    div.append(lecteur);
  } else {
    div.append(m.t);
  }
  $('liste').append(div);
  $('liste').scrollTop = $('liste').scrollHeight;
}

// recupere les nouveaux messages
async function actualiser() {
  // une seule lecture a la fois sinon les messages s'affichent en double
  if (lectureEnCours) return;
  lectureEnCours = true;
  try {
    const r = await fetch('/msg?depuis=' + dernier);
    const d = await r.json();
    // l'ESP32 a redemarre
    if (d.dernier < dernier) { dernier = 0; $('liste').innerHTML = ''; return; }
    for (const m of d.msg) {
      if (m.id <= dernier) continue;
      afficher(m);
      dernier = m.id;
    }
    // pas de bip pour l'historique au chargement
    if (!premiereLecture && d.msg.length > 0 && !d.muet) son(d.vol);
    premiereLecture = false;
    $('etat').textContent = d.connectes + ' connecté(s) · ' + d.debit + ' o/s · ' + (d.muet ? '🔇 silence' : '🔊 ' + d.vol + ' %');
    $('relais').textContent = d.relais
      ? '📡 Relais ACTIF · Wi-Fi : ' + d.ssid + ' (même mot de passe) · http://' + d.ip + '/'
      : '📡 Relais INACTIF';
  } catch (e) {
    $('etat').textContent = 'Connexion perdue';
  } finally {
    lectureEnCours = false;
  }
}

function lirePseudo() {
  const p = $('pseudo').value.trim();
  if (!p) { $('info').textContent = 'Indique ton pseudo'; return null; }
  localStorage.setItem('pseudo', p);
  $('pseudo').readOnly = true;
  return p;
}

async function envoyerTexte() {
  const p = lirePseudo();
  const t = $('texte').value.trim();
  if (!p || !t) return;
  await fetch('/envoyer', {method: 'POST', body: new URLSearchParams({p, t})});
  $('texte').value = '';
  actualiser();
}

async function envoyerVocal() {
  const f = $('fichier').files && $('fichier').files[0];
  const p = lirePseudo();
  if (!p || !f || envoiEnCours) return;
  if (f.size > 256 * 1024) { $('info').textContent = 'Fichier trop gros (256 Kio max)'; return; }

  envoiEnCours = true;
  $('info').textContent = 'Envoi du vocal...';
  try {
    const form = new FormData();
    form.append('p', p);
    form.append('audio', f, f.name || 'vocal.m4a');
    const r = await fetch('/envoyer-vocal', {method: 'POST', body: form});
    $('info').textContent = r.ok ? 'Vocal envoyé' : 'Échec : ' + await r.text();
  } catch (e) {
    $('info').textContent = 'Échec de l\'envoi du vocal';
  }
  // vide le champ apres l'envoi pour pouvoir reprendre le meme fichier
  $('fichier').value = '';
  envoiEnCours = false;
  actualiser();
}

actualiser();
setInterval(actualiser, 1500);
</script>
</body></html>)HTML";

// ---------- outils ----------

bool relaisActif() {
  return dernierPingRelais != 0 && millis() - dernierPingRelais < 20000;
}

// protege le texte pour le json
String echapper(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", " ");
  s.replace("\r", " ");
  return s;
}

String fichierVocal(int id) {
  return "/vocal" + String(id);
}

// type audio selon l'extension
String typeAudio(String nom) {
  nom.toLowerCase();
  if (nom.endsWith(".mp3")) return "audio/mpeg";
  if (nom.endsWith(".wav")) return "audio/wav";
  if (nom.endsWith(".ogg") || nom.endsWith(".opus")) return "audio/ogg";
  if (nom.endsWith(".webm")) return "audio/webm";
  if (nom.endsWith(".aac")) return "audio/aac";
  return "audio/mp4";
}

// efface les vocaux restes d'un ancien demarrage
void supprimerAnciensVocaux() {
  File dossier = LittleFS.open("/");
  File f = dossier.openNextFile();
  while (f) {
    String nom = f.name();
    f.close();
    if (nom.startsWith("vocal")) LittleFS.remove("/" + nom);
    f = dossier.openNextFile();
  }
}

void bip() {
  if (silencieux || volume == 0) return;
  ledcWriteTone(BUZZER, 1000);
  ledcWrite(BUZZER, volume * 512 / 100);
  finBip = millis() + 120;
}

void ajouterMessage(String pseudo, String texte, bool vocal, String type) {
  // historique plein on retire le plus ancien
  if (nbMessages == MAX_MSG) {
    if (messages[0].vocal) LittleFS.remove(fichierVocal(messages[0].id));
    for (int i = 1; i < MAX_MSG; i++) messages[i - 1] = messages[i];
    nbMessages--;
  }
  dernierId++;
  messages[nbMessages] = {dernierId, pseudo, texte, vocal, type};
  nbMessages++;

  bip();
  finLedBleue = millis() + 500;
  Serial.println(pseudo + " : " + (vocal ? "[VOCAL]" : texte));
}

// garde seulement les derniers vocaux
void limiterVocaux() {
  int nb = 0;
  for (int i = nbMessages - 1; i >= 0; i--) {
    if (!messages[i].vocal) continue;
    nb++;
    if (nb > MAX_VOCAUX) {
      LittleFS.remove(fichierVocal(messages[i].id));
      messages[i].vocal = false;
      messages[i].texte = "(vocal expiré)";
    }
  }
}

// ---------- pages du serveur ----------

void envoyerPage() {
  serveur.send(200, "text/html; charset=utf-8", PAGE);
}

// liste des messages en json
void envoyerMessages() {
  int depuis = serveur.arg("depuis").toInt();

  String json = "{\"dernier\":" + String(dernierId);
  json += ",\"connectes\":" + String(WiFi.softAPgetStationNum());
  json += ",\"debit\":" + String(debit);
  json += ",\"muet\":" + String(silencieux ? "true" : "false");
  json += ",\"vol\":" + String(volume);
  json += ",\"relais\":" + String(relaisActif() ? "true" : "false");
  json += ",\"ssid\":\"" + String(NOM_WIFI_RELAIS) + "\"";
  json += ",\"ip\":\"" + String(ADRESSE_RELAIS) + "\"";
  json += ",\"msg\":[";

  bool premier = true;
  for (int i = 0; i < nbMessages; i++) {
    if (messages[i].id <= depuis) continue;
    if (!premier) json += ",";
    premier = false;
    json += "{\"id\":" + String(messages[i].id);
    json += ",\"p\":\"" + echapper(messages[i].pseudo) + "\"";
    json += ",\"t\":\"" + echapper(messages[i].texte) + "\"";
    json += ",\"v\":" + String(messages[i].vocal ? "true" : "false") + "}";
  }
  json += "]}";

  serveur.send(200, "application/json", json);
  octets += json.length();
}

void recevoirTexte() {
  String pseudo = serveur.arg("p");
  String texte = serveur.arg("t");
  pseudo.trim();
  texte.trim();
  if (pseudo == "" || texte == "") {
    serveur.send(400, "text/plain", "Pseudo ou message vide");
    return;
  }
  ajouterMessage(pseudo.substring(0, 24), texte.substring(0, 200), false, "");
  octets += texte.length();
  serveur.send(200, "text/plain", "ok");
}

// le relais signale sa presence toutes les 5 secondes
void pingRelais() {
  if (serveur.arg("cle") != CLE_RELAIS) {
    serveur.send(403, "text/plain", "Relais non autorise");
    return;
  }
  dernierPingRelais = millis();
  serveur.send(200, "text/plain", "ok");
}

// etat du relais en json
void etatRelais() {
  String json = "{\"actif\":" + String(relaisActif() ? "true" : "false");
  if (relaisActif()) {
    json += ",\"ssid\":\"" + String(NOM_WIFI_RELAIS) + "\"";
    json += ",\"mot_de_passe\":\"" + echapper(MOT_DE_PASSE) + "\"";
    json += ",\"adresse\":\"http://" + String(ADRESSE_RELAIS) + "/\"";
  }
  json += "}";
  serveur.send(200, "application/json", json);
}

// reception du vocal morceau par morceau
void recevoirMorceauVocal() {
  HTTPUpload& u = serveur.upload();
  if (u.status == UPLOAD_FILE_START) {
    fichierRecu = LittleFS.open("/recu.tmp", "w");
    tailleRecue = 0;
    typeRecu = typeAudio(u.filename);
  } else if (u.status == UPLOAD_FILE_WRITE) {
    tailleRecue += u.currentSize;
    if (fichierRecu && tailleRecue <= TAILLE_MAX_VOCAL) fichierRecu.write(u.buf, u.currentSize);
  } else {
    if (fichierRecu) fichierRecu.close();
  }
}

// appele une fois le vocal entierement recu
void finVocal() {
  String pseudo = serveur.arg("p");
  pseudo.trim();
  if (pseudo == "" || tailleRecue == 0 || tailleRecue > TAILLE_MAX_VOCAL) {
    LittleFS.remove("/recu.tmp");
    serveur.send(400, "text/plain", "Vocal vide ou trop gros (256 Kio max)");
    return;
  }
  String destination = fichierVocal(dernierId + 1);
  LittleFS.remove(destination);
  if (!LittleFS.rename("/recu.tmp", destination)) {
    serveur.send(500, "text/plain", "Stockage indisponible");
    return;
  }
  ajouterMessage(pseudo.substring(0, 24), "", true, typeRecu);
  limiterVocaux();
  octets += tailleRecue;
  serveur.send(200, "text/plain", "ok");
}

// envoie un vocal
void lireVocal() {
  int id = serveur.arg("id").toInt();

  String type = "";
  for (int i = 0; i < nbMessages; i++) {
    if (messages[i].id == id && messages[i].vocal) type = messages[i].type;
  }
  File f = LittleFS.open(fichierVocal(id), "r");
  if (type == "" || !f) {
    serveur.send(404, "text/plain", "Vocal introuvable");
    return;
  }

  int total = f.size();
  int debut = 0;
  int fin = total - 1;

  // safari iphone demande le fichier par morceaux avec Range bytes=debut-fin
  String range = serveur.header("Range");
  bool partiel = range.startsWith("bytes=");
  if (partiel) {
    int tiret = range.indexOf('-');
    debut = range.substring(6, tiret).toInt();
    String finTexte = range.substring(tiret + 1);
    if (finTexte != "" && finTexte.toInt() < fin) fin = finTexte.toInt();
    if (debut > fin) {
      f.close();
      serveur.send(416, "text/plain", "Plage invalide");
      return;
    }
    serveur.sendHeader("Content-Range", "bytes " + String(debut) + "-" + String(fin) + "/" + String(total));
  }

  int reste = fin - debut + 1;
  serveur.sendHeader("Accept-Ranges", "bytes");
  serveur.setContentLength(reste);
  serveur.send(partiel ? 206 : 200, type, "");

  // envoi par blocs de 1 Kio
  f.seek(debut);
  uint8_t bloc[1024];
  while (reste > 0) {
    int lu = f.read(bloc, min(reste, 1024));
    if (lu <= 0) break;
    serveur.client().write(bloc, lu);
    reste -= lu;
    octets += lu;
  }
  f.close();
}

// portail captif toute adresse inconnue renvoie vers le chat
void redirigerPortail() {
  serveur.sendHeader("Location", "http://192.168.4.1/");
  serveur.send(302, "text/plain", "");
}

// ---------- materiel ----------

void lireBouton() {
  static int avant = HIGH;
  int etat = digitalRead(BOUTON);
  if (etat == LOW && avant == HIGH) {
    silencieux = !silencieux;
    Serial.println(silencieux ? "Mode silencieux" : "Son actif");
    delay(50);
  }
  avant = etat;
}

// change de page toutes les 3 secondes
void afficherLCD() {
  static unsigned long dernierChangement = 0;
  static int page = 0;
  if (millis() - dernierChangement < 3000) return;
  dernierChangement = millis();

  int nbPages = relaisActif() ? 4 : 3;
  page = (page + 1) % nbPages;

  lcd.clear();
  if (page == 0) {
    lcd.print("Radio-Secours");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.softAPIP());
  } else if (page == 1) {
    lcd.print("Mot de passe:");
    lcd.setCursor(0, 1);
    lcd.print(MOT_DE_PASSE);
  } else if (page == 2) {
    lcd.print(relaisActif() ? "Relais: ACTIF" : "Relais: INACTIF");
    lcd.setCursor(0, 1);
    lcd.print(relaisActif() ? ADRESSE_RELAIS : "En attente");
  } else {
    String nom = NOM_WIFI_RELAIS;
    lcd.print(nom.substring(0, 16));
    lcd.setCursor(0, 1);
    lcd.print(nom.substring(16));
  }
}

void mettreAJourSorties() {
  volume = analogRead(POT) * 100 / 4095;

  // debit en octets par seconde
  static unsigned long debutSeconde = 0;
  if (millis() - debutSeconde >= 1000) {
    debit = octets;
    octets = 0;
    debutSeconde = millis();
  }

  digitalWrite(LED_VERTE, HIGH);
  digitalWrite(LED_JAUNE, WiFi.softAPgetStationNum() > 0);
  digitalWrite(LED_ROUGE, debit > 0 && (millis() / 200) % 2);
  digitalWrite(LED_BLEUE, millis() < finLedBleue);

  // arret du bip
  if (finBip != 0 && millis() > finBip) {
    ledcWriteTone(BUZZER, 0);
    finBip = 0;
  }
}

// ---------- programme principal ----------

void setup() {
  Serial.begin(115200);
  pinMode(LED_VERTE, OUTPUT);
  pinMode(LED_JAUNE, OUTPUT);
  pinMode(LED_ROUGE, OUTPUT);
  pinMode(LED_BLEUE, OUTPUT);
  pinMode(BOUTON, INPUT_PULLUP);
  ledcAttach(BUZZER, 1000, 10);

  // memoire flash pour les vocaux
  if (LittleFS.begin()) supprimerAnciensVocaux();
  else Serial.println("Erreur LittleFS : vocaux indisponibles");

  // point d'acces wifi
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IP, IP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(NOM_WIFI, MOT_DE_PASSE);
  dns.start(53, "*", IP);

  lcd.begin(16, 2);
  lcd.setRGB(0, 120, 255);
  lcd.print("Radio-Secours");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.softAPIP());

  // routes du serveur
  const char* entetes[] = {"Range"};
  serveur.collectHeaders(entetes, 1);
  serveur.on("/", HTTP_GET, envoyerPage);
  serveur.on("/msg", HTTP_GET, envoyerMessages);
  serveur.on("/envoyer", HTTP_POST, recevoirTexte);
  serveur.on("/envoyer-vocal", HTTP_POST, finVocal, recevoirMorceauVocal);
  serveur.on("/vocal", HTTP_GET, lireVocal);
  serveur.on("/relais-ping", HTTP_POST, pingRelais);
  serveur.on("/relais-etat", HTTP_GET, etatRelais);
  serveur.onNotFound(redirigerPortail);
  serveur.begin();

  Serial.println("Radio-Secours pret sur http://192.168.4.1");
}

void loop() {
  dns.processNextRequest();
  serveur.handleClient();
  lireBouton();
  mettreAJourSorties();
  afficherLCD();
}
