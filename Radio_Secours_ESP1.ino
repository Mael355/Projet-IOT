



#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <rgb_lcd.h>
#include <LittleFS.h>
#include <FS.h>
#include <esp_system.h>

// ---------- Broches d'origine ----------
const int LED_VERTE = 25;
const int LED_JAUNE = 26;
const int LED_ROUGE = 27;
const int LED_BLEUE = 32;
const int BUZZER = 18;
const int BOUTON = 4;
const int POT = 34;

// ---------- Reglages ----------
const int MAX_MSG = 25;
const int MAX_VOCAUX = 3;                  // 3 derniers vocaux conserves
const size_t MAX_TAILLE_VOCAL = 256UL * 1024UL; // 256 Kio/vocal maximum
const size_t MARGE_FLASH = 16UL * 1024UL;
// true = formate la zone LittleFS SEULEMENT si le montage echoue.
// ATTENTION : ce formatage peut effacer les fichiers de la partition LittleFS.
// Mettre false si la carte contient deja des fichiers a conserver.
const bool FORMATER_LITTLEFS_SI_ECHEC = false;

const char* NOM_WIFI = "Radio-Secours";
const char* MOT_DE_PASSE = "Kx7!Pm4q-Zr9w#";

// Parametres du deuxieme ESP32 (a garder identiques dans son sketch).
const char* NOM_WIFI_RELAIS = "Radio-Secours-Relais";
const char* MOT_DE_PASSE_RELAIS = MOT_DE_PASSE;
const char* ADRESSE_RELAIS = "192.168.5.1";
const char* CLE_PRESENCE_RELAIS = "radio-secours-relais-presence-v1";
const uint32_t DELAI_ABSENCE_RELAIS = 20000UL; // 20 secondes sans signal = inactif.
uint32_t dernierSignalRelais = 0;
bool relaisDejaVu = false;

bool relaisActif() {
  return relaisDejaVu && (uint32_t)(millis() - dernierSignalRelais) < DELAI_ABSENCE_RELAIS;
}


WebServer serveur(80);
DNSServer dns;
rgb_lcd lcd;

struct Message {
  uint32_t id;
  String pseudo;
  String texte;
  bool vocal;
  String mime;
};
Message historique[MAX_MSG];
int nbMsg = 0;
uint32_t dernierId = 0;
uint32_t sessionServeur = 0;

bool silencieux = false;
const int LUM = 180;
int volume = 50;
unsigned long octets = 0;
unsigned long debit = 0;
unsigned long debutBleue = 0;
unsigned long finBip = 0;

// Stockage des vocaux : aucune carte SD requise
bool stockagePret = false;
File fichierEnCours;
String cheminEnCours;
String mimeEnCours;
size_t tailleEnCours = 0;
int erreurEnvoi = 0; // 0 OK, 400 vide, 413 trop gros, 415 format, 507 stockage

// ---------- Interface Web mobile entierement hors ligne ----------
const char PAGE[] = R"HTML(<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>Radio-Secours</title>
<style>
*{box-sizing:border-box}
body{margin:0;height:100vh;height:100dvh;min-height:0;display:flex;flex-direction:column;font:16px system-ui,sans-serif;background:#0f172a;color:#e2e8f0}
header{padding:calc(12px + env(safe-area-inset-top)) 12px 12px;background:#1e293b}
h1{margin:0;font-size:18px}
#etat{font-size:13px;color:#94a3b8}
#etatRelais{font-size:12px;margin-top:5px;line-height:1.45;color:#94a3b8;overflow-wrap:anywhere}
#etatRelais.actif{color:#86efac}
#liste{flex:1;min-height:0;overflow-y:auto;-webkit-overflow-scrolling:touch;overscroll-behavior:contain;padding:12px;display:flex;flex-direction:column;gap:8px}
.msg{background:#1e293b;border-radius:12px;padding:9px 12px;max-width:90%;overflow-wrap:anywhere}
.moi{align-self:flex-end;background:#065f46}
.msg b{display:block;font-size:13px;color:#7dd3fc;margin-bottom:3px}
.msg audio{display:block;width:min(260px,70vw);height:42px;margin-top:6px}
.expire{font-size:13px;color:#fca5a5;margin-top:4px}
.ouvrir{display:block;font-size:12px;color:#93c5fd;margin-top:4px}
#barre{background:#1e293b;padding:9px 9px calc(9px + env(safe-area-inset-bottom));display:flex;flex-direction:column;gap:8px}
#ligne{display:flex;gap:8px;min-width:0}
input[type=text]{font:inherit;font-size:16px;padding:11px;border:0;border-radius:10px;background:#334155;color:#fff}
#pseudo{width:100%;min-width:0}
#pseudo:read-only{opacity:.8}
#identite{display:flex;gap:8px}
#changer{background:#475569;color:#fff;padding:8px 10px;white-space:nowrap}
#texte{flex:1;min-width:0}
button,.bouton{display:inline-flex;align-items:center;justify-content:center;font:inherit;font-weight:600;padding:10px 14px;border:0;border-radius:10px;background:#22c55e;color:#052e16;cursor:pointer}
button:disabled,.bouton:disabled{opacity:.5}
.bouton{background:#334155;color:#e2e8f0}
#outils{display:flex;gap:8px;flex-wrap:wrap}
#outils .bouton{flex:1;font-size:14px;text-align:center}
#info{font-size:12px;color:#94a3b8;min-height:17px}
#info.erreur{color:#fca5a5}
.aide{font-size:12px;line-height:1.5;color:#cbd5e1}
.cache{position:absolute;left:-10000px;width:1px;height:1px;opacity:0;overflow:hidden}
</style>
</head>
<body>
<header><h1>📻 Radio-Secours</h1><div id="etat">Connexion...</div><div id="etatRelais">📡 Recherche du relais...</div></header>
<div id="liste" aria-live="polite"></div>
<div id="barre">
  <div id="identite"><input id="pseudo" type="text" maxlength="24" placeholder="Ton pseudo" aria-label="Ton pseudo" required><button id="changer" type="button" title="Changer de pseudo">Modifier</button></div>
  <form id="form">
    <div id="ligne">
      <input id="texte" type="text" maxlength="200" placeholder="Message" autocomplete="off" enterkeyhint="send" aria-label="Ton message texte" required>
      <button id="envoyer" type="submit">Envoyer</button>
    </div>
  </form>
  <div id="outils">
    <label class="bouton" for="capture">🎙️ Enregistrer (si proposé)</label>
    <label class="bouton" for="fichier">📁 Choisir un audio</label>
    <input class="cache" id="capture" type="file" accept="audio/*" capture="user">
    <input class="cache" id="fichier" type="file" accept="audio/*,.m4a,.mp3,.wav,.aac,.mp4,.3gp,.opus,.ogg,.webm">
  </div>
  <div id="info" role="status" aria-live="polite">Messages texte et vocaux en Wi-Fi local · vocaux de 256 Kio max.</div>
  <div id="aide" class="aide">Pour les messages vocaux, choisis un fichier audio sur ton téléphone. Les fichiers M4A/AAC ou MP3 sont les plus compatibles.</div>
</div>
<script>
const $=id=>document.getElementById(id);
let dernier=0,init=false,audio=null,vol=50,occupe=false,lectureEnCours=false,sessionActive=null;
const MAX_AUDIO=256*1024;
let pseudoSauve='';
try {pseudoSauve=localStorage.getItem('rs_pseudo')||localStorage.getItem('pseudo')||'';} catch(e){}
$('pseudo').value=pseudoSauve;
$('pseudo').readOnly=!!pseudoSauve;
$('changer').onclick=()=>{$('pseudo').readOnly=false;$('pseudo').focus();$('pseudo').select();};
const estIOS=/iPhone|iPad|iPod/i.test(navigator.userAgent)||
  (navigator.platform==='MacIntel' && navigator.maxTouchPoints>1);
$('aide').textContent=estIOS ?
  'iPhone : ouvrir dans Safari. Pour un vocal, enregistrer dans Dictaphone, puis Partager → Enregistrer dans Fichiers → Choisir un audio ici. Si le téléphone propose un enregistrement direct, utilise le bouton micro.' :
  'Android : utiliser Chrome. Le bouton micro peut proposer un enregistreur natif. Sinon, enregistrer un son puis Choisir un audio. Le Wi-Fi Radio-Secours fonctionne sans Internet.';
document.addEventListener('pointerdown',()=>{
  try {audio=audio||(new (window.AudioContext||window.webkitAudioContext)());audio.resume()} catch(e){}
},{passive:true});
function son(){
  if(!audio||vol==0)return;
  const o=audio.createOscillator(),g=audio.createGain();
  g.gain.value=.2*vol/100;o.frequency.value=880;
  o.connect(g).connect(audio.destination);
  o.start();o.stop(audio.currentTime+.12);
}
function statut(texte,erreur=false){
  $('info').textContent=texte;
  $('info').className=erreur?'erreur':'';
}
function verifierPseudo(){
  const p=$('pseudo').value.trim();
  if(!p){statut('Indique ton pseudo avant d\'envoyer un message.',true);$('pseudo').focus();return null}
  $('pseudo').value=p;
  try{localStorage.setItem('rs_pseudo',p);}catch(e){}
  $('pseudo').readOnly=true;
  return p;
}
function ajouter(m){
  const d=document.createElement('div');
  d.className='msg'+(m.p===$('pseudo').value?' moi':'');
  const b=document.createElement('b');b.textContent=m.p;
  d.append(b);
  if(m.v){
    const icone=document.createElement('span');icone.textContent='🎤 Message vocal';d.append(icone);
    const lecteur=document.createElement('audio');
    lecteur.controls=true;lecteur.preload='none';
    lecteur.src='/vocal?id='+encodeURIComponent(m.id);
    lecteur.onerror=()=>{
      lecteur.remove();
      const erreur=document.createElement('div');erreur.className='expire';
      erreur.textContent='Vocal indisponible ou format non lisible par ce téléphone (essaye M4A/AAC ou MP3).';
      if(!d.querySelector('.expire'))d.append(erreur);
    };
    d.append(lecteur);
    const lien=document.createElement('a');
    lien.href=lecteur.src;lien.textContent='Ouvrir le vocal';
    lien.className='ouvrir';d.append(lien);
  }else{
    d.append(document.createTextNode(m.t));
  }
  $('liste').append(d);
  if($('liste').children.length>25)$('liste').firstElementChild.remove();
  $('liste').scrollTop=$('liste').scrollHeight;
}
async function actualiser(){
  if(lectureEnCours)return;
  lectureEnCours=true;
  try{
    const r=await fetch('/msg?depuis='+dernier,{cache:'no-store'});
    if(!r.ok)throw new Error('Indisponible');
    const d=await r.json();
    // Si l'ESP32 a redemarre, recommencer l'historique au debut.
    if(sessionActive!==null && d.session!==sessionActive){
      dernier=0;init=false;$('liste').replaceChildren();
      sessionActive=d.session;
      $('etat').textContent='Radio-Secours reconnecté';
      return;
    }
    sessionActive=d.session;
    let nouveau=false;
    for(const m of d.msg){
      if(m.id>dernier){ajouter(m);dernier=m.id;nouveau=true;}
    }
    vol=d.vol;
    $('etatRelais').className=d.relais?'actif':'';
    $('etatRelais').textContent=d.relais ?
      ('📡 Relais ACTIF · Wi-Fi : '+d.relaisSSID+' · MDP : '+d.relaisMDP+' · http://'+d.relaisIP+'/') :
      '📡 Relais INACTIF (ou hors de portee)';
    if(init&&nouveau&&!d.muet)son();
    init=true;
    $('etat').textContent=d.app+' connecté(s) · '+d.debit+' o/s · '+(d.muet?'🔇 silence':'🔊 '+d.vol+' %');
  }catch(e){$('etat').textContent='Connexion perdue'}
  finally{lectureEnCours=false;}
}
$('form').onsubmit=async e=>{
  e.preventDefault();
  if(occupe)return;
  const p=verifierPseudo();if(!p)return;
  const t=$('texte').value.trim();if(!t)return;
  occupe=true;$('envoyer').disabled=true;
  try{
    const r=await fetch('/envoyer',{method:'POST',body:new URLSearchParams({p,t})});
    if(!r.ok)throw new Error(await r.text());
    $('texte').value='';statut('Message texte envoye.');
    await actualiser();
  }catch(e){statut('Erreur : '+e.message,true)}
  finally{occupe=false;$('envoyer').disabled=false;}
};
async function envoyerAudio(file){
  if(!file)return;
  if(occupe){statut('Envoi precedent toujours en cours.',true);return}
  const p=verifierPseudo();if(!p)return;
  if(!file.size){statut('Le fichier audio est vide.',true);return}
  if(file.size>MAX_AUDIO){
    statut('Vocal trop volumineux ('+Math.ceil(file.size/1024)+' Kio). Maximum : 256 Kio. Enregistre un extrait plus court ou compresse-le en M4A/AAC.',true);
    return;
  }
  statut('Préparation : '+file.name+' ('+Math.ceil(file.size/1024)+' Kio)');
  const form=new FormData();
  form.append('p',p);
  form.append('audio',file,file.name||'vocal.m4a');
  occupe=true;$('envoyer').disabled=true;
  statut('Envoi du message vocal...');
  try{
    const r=await fetch('/envoyer-vocal',{method:'POST',body:form});
    const reponse=await r.text();
    if(!r.ok)throw new Error(reponse||('Code '+r.status));
    statut('Message vocal envoyé.');
    await actualiser();
  }catch(e){statut('Echec du vocal : '+e.message,true)}
  finally{occupe=false;$('envoyer').disabled=false;}
}
for(const id of ['capture','fichier']){
  $(id).onchange=async()=>{
    const file=$(id).files&&$(id).files[0];
    await envoyerAudio(file);
    $(id).value='';
  };
}
actualiser();
setInterval(actualiser,1500);
document.addEventListener('visibilitychange',()=>{if(!document.hidden)actualiser();});
</script>
</body></html>)HTML";

// ---------- Outils ----------
String echapper(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", "\\n");
  s.replace("\r", "\\r");
  s.replace("\t", "\\t");
  return s;
}

String cheminVocal(uint32_t id) {
  return "/rs_vocal_" + String(id) + ".bin";
}

// On ne conserve aucun ancien fichier sans message correspondant en RAM.
// Ne supprime QUE les fichiers dont le nom commence par /rs_vocal_.
void nettoyerFichiersVocaux() {
  File racine = LittleFS.open("/");
  if (!racine || !racine.isDirectory()) return;
  File entree = racine.openNextFile();
  while (entree) {
    String nom = String(entree.name());
    bool dossier = entree.isDirectory();
    entree.close();
    if (!nom.startsWith("/")) nom = "/" + nom;
    if (!dossier && nom.startsWith("/rs_vocal_")) {
      LittleFS.remove(nom);
    }
    entree = racine.openNextFile();
  }
  racine.close();
}

void bip() {
  if (silencieux || volume == 0) return;
  ledcWriteTone(BUZZER, 1000);
  ledcWrite(BUZZER, volume * 512 / 100);
  finBip = millis() + 120;
}

void supprimerFichierVocal(const Message &m) {
  if (stockagePret && m.vocal) LittleFS.remove(cheminVocal(m.id));
}

int compterVocaux() {
  int n = 0;
  for (int i = 0; i < nbMsg; i++) if (historique[i].vocal) n++;
  return n;
}

bool expirerPlusAncienVocal() {
  for (int i = 0; i < nbMsg; i++) {
    if (!historique[i].vocal) continue;
    supprimerFichierVocal(historique[i]);
    historique[i].vocal = false;
    historique[i].mime = "";
    historique[i].texte = "(Message vocal expire : limite de stockage)";
    return true;
  }
  return false;
}

void ajouterMessage(String pseudo, String texte, bool vocal = false, String mime = "") {
  if (nbMsg == MAX_MSG) {
    supprimerFichierVocal(historique[0]);
    for (int i = 1; i < MAX_MSG; i++) historique[i - 1] = historique[i];
    nbMsg--;
  }
  dernierId++;
  historique[nbMsg] = {dernierId, pseudo, texte, vocal, mime};
  nbMsg++;
  bip();
  debutBleue = millis();
  Serial.println(pseudo + (vocal ? " : [VOCAL]" : " : " + texte));
}

// Correspondance avec les formats generalement produits par Android.
// Le navigateur peut refuser la lecture de certains codecs (ex. AMR/3GP).
String determinerMime(String nom, String type) {
  nom.toLowerCase();
  type.toLowerCase();
  if (nom.endsWith(".m4a") || nom.endsWith(".mp4")) return "audio/mp4";
  if (nom.endsWith(".mp3")) return "audio/mpeg";
  if (nom.endsWith(".wav")) return "audio/wav";
  if (nom.endsWith(".ogg") || nom.endsWith(".opus")) return "audio/ogg";
  if (nom.endsWith(".webm")) return "audio/webm";
  if (nom.endsWith(".3gp")) return "audio/3gpp";
  if (nom.endsWith(".aac")) return "audio/aac";
  if (nom.endsWith(".amr")) return "audio/amr";
  if (type == "audio/mp4" || type == "audio/x-m4a" || type == "video/mp4" || type == "audio/m4a") return "audio/mp4";
  if (type == "audio/mpeg") return "audio/mpeg";
  if (type == "audio/wav" || type == "audio/x-wav") return "audio/wav";
  if (type == "audio/webm") return "audio/webm";
  if (type == "audio/ogg" || type == "audio/opus") return "audio/ogg";
  if (type == "audio/3gpp" || type == "video/3gpp") return "audio/3gpp";
  if (type == "audio/aac") return "audio/aac";
  if (type == "audio/amr") return "audio/amr";
  return "";
}

// ---------- API messages texte ----------
void envoyerPage() {
  serveur.sendHeader("Cache-Control", "no-store");
  serveur.send(200, "text/html; charset=utf-8", PAGE);
  octets += strlen(PAGE);
}

void envoyerMessages() {
  uint32_t depuis = (uint32_t)serveur.arg("depuis").toInt();
  String json = "{\"session\":\"" + String(sessionServeur, HEX) + "\",\"app\":" + String(WiFi.softAPgetStationNum());
  json += ",\"debit\":" + String(debit);
  json += ",\"muet\":" + String(silencieux ? "true" : "false");
  json += ",\"vol\":" + String(volume);
  json += ",\"relais\":" + String(relaisActif() ? "true" : "false");
  if (relaisActif()) {
    json += ",\"relaisSSID\":\"" + echapper(NOM_WIFI_RELAIS) + "\"";
    json += ",\"relaisMDP\":\"" + echapper(MOT_DE_PASSE_RELAIS) + "\"";
    json += ",\"relaisIP\":\"" + echapper(ADRESSE_RELAIS) + "\"";
  }
  json += ",\"msg\":[";
  for (int i = 0; i < nbMsg; i++) {
    if (historique[i].id <= depuis) continue;
    if (!json.endsWith("[")) json += ",";
    json += "{\"id\":" + String(historique[i].id);
    json += ",\"p\":\"" + echapper(historique[i].pseudo);
    json += "\",\"t\":\"" + echapper(historique[i].texte);
    json += "\",\"v\":" + String(historique[i].vocal ? "true" : "false") + "}";
  }
  json += "]}";
  serveur.sendHeader("Cache-Control", "no-store");
  serveur.send(200, "application/json; charset=utf-8", json);
  octets += json.length();
}

void recevoirMessage() {
  String pseudo = serveur.arg("p");
  String texte = serveur.arg("t");
  pseudo.trim();
  texte.trim();
  if (pseudo == "" || texte == "") {
    serveur.send(400, "text/plain", "Pseudo ou message vide");
    return;
  }
  octets += pseudo.length() + texte.length();
  ajouterMessage(pseudo.substring(0, 24), texte.substring(0, 200));
  serveur.send(200, "text/plain", "ok");
}

// ---------- Presence du relais ESP32 n°2 ----------
// Seul le relais connait cette cle. Reception toutes les 5 secondes environ.
// Aucun message du chat ne transite par ce point de controle.
void recevoirPresenceRelais() {
  if (!serveur.hasArg("cle") || serveur.arg("cle") != CLE_PRESENCE_RELAIS) {
    serveur.send(403, "text/plain", "Relais non autorise");
    return;
  }
  dernierSignalRelais = millis();
  relaisDejaVu = true;
  serveur.sendHeader("Cache-Control", "no-store");
  serveur.send(200, "text/plain", "ok");
}

void consulterPresenceRelais() {
  String json = "{\"actif\":" + String(relaisActif() ? "true" : "false");
  if (relaisActif()) {
    json += ",\"ssid\":\"" + echapper(NOM_WIFI_RELAIS) + "\"";
    json += ",\"mot_de_passe\":\"" + echapper(MOT_DE_PASSE_RELAIS) + "\"";
    json += ",\"adresse\":\"http://" + echapper(ADRESSE_RELAIS) + "/\"";
  }
  json += "}";
  serveur.sendHeader("Cache-Control", "no-store");
  serveur.send(200, "application/json; charset=utf-8", json);
}

// ---------- API vocaux ----------
void annulerUpload() {
  if (fichierEnCours) fichierEnCours.close();
  if (stockagePret && cheminEnCours.length()) LittleFS.remove(cheminEnCours);
  cheminEnCours = "";
}

void receptionFluxVocal() {
  HTTPUpload &u = serveur.upload();

  if (u.status == UPLOAD_FILE_START) {
    annulerUpload();
    erreurEnvoi = 0;
    tailleEnCours = 0;
    mimeEnCours = "";

    if (!stockagePret) { erreurEnvoi = 507; return; }
    if (u.name != "audio") { erreurEnvoi = 400; return; }
    mimeEnCours = determinerMime(u.filename, u.type);
    if (mimeEnCours == "") { erreurEnvoi = 415; return; }

    // Conserver au plus MAX_VOCAUX vocaux. Eviter aussi de remplir la flash.
    while (compterVocaux() >= MAX_VOCAUX) {
      if (!expirerPlusAncienVocal()) break;
    }
    while (LittleFS.totalBytes() < LittleFS.usedBytes() + MAX_TAILLE_VOCAL + MARGE_FLASH) {
      if (!expirerPlusAncienVocal()) break;
    }
    if (LittleFS.totalBytes() < LittleFS.usedBytes() + MARGE_FLASH) {
      erreurEnvoi = 507;
      return;
    }

    cheminEnCours = cheminVocal(dernierId + 1);
    fichierEnCours = LittleFS.open(cheminEnCours, "w");
    if (!fichierEnCours) { erreurEnvoi = 507; cheminEnCours = ""; }

  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (erreurEnvoi) return;
    if (!fichierEnCours) { erreurEnvoi = 507; return; }
    if (tailleEnCours + u.currentSize > MAX_TAILLE_VOCAL) {
      erreurEnvoi = 413;
      annulerUpload();
      return;
    }
    size_t ecrits = fichierEnCours.write(u.buf, u.currentSize);
    if (ecrits != u.currentSize) {
      erreurEnvoi = 507;
      annulerUpload();
      return;
    }
    tailleEnCours += u.currentSize;

  } else if (u.status == UPLOAD_FILE_END) {
    if (fichierEnCours) fichierEnCours.close();
    if (!erreurEnvoi && !tailleEnCours) erreurEnvoi = 400;
    if (erreurEnvoi) annulerUpload();

  } else if (u.status == UPLOAD_FILE_ABORTED) {
    annulerUpload();
    erreurEnvoi = 400;
  }
}

void confirmerVocal() {
  if (erreurEnvoi) {
    int erreur = erreurEnvoi;
    annulerUpload();
    if (erreur == 413) serveur.send(413, "text/plain; charset=utf-8", "Fichier trop gros (256 Kio maximum)");
    else if (erreur == 415) serveur.send(415, "text/plain; charset=utf-8", "Format audio non pris en charge");
    else if (erreur == 507) serveur.send(507, "text/plain; charset=utf-8", "Memoire flash indisponible ou pleine");
    else serveur.send(400, "text/plain; charset=utf-8", "Fichier vocal invalide");
    return;
  }
  String pseudo = serveur.arg("p");
  pseudo.trim();
  if (!cheminEnCours.length() || !tailleEnCours || !pseudo.length()) {
    annulerUpload();
    serveur.send(400, "text/plain; charset=utf-8", "Vocal ou pseudo manquant");
    return;
  }
  octets += tailleEnCours;
  ajouterMessage(pseudo.substring(0, 24), "", true, mimeEnCours);
  cheminEnCours = "";
  serveur.send(200, "text/plain", "ok");
}

// Safari iOS peut demander un bloc d'octets specifique (HTTP Range).
// La lecture M4A/MP3 est servie par morceaux, sans charger le fichier en RAM.
void lireVocal() {
  uint32_t id = (uint32_t)serveur.arg("id").toInt();
  String type;
  bool trouve = false;
  for (int i = 0; i < nbMsg; i++) {
    if (historique[i].id == id && historique[i].vocal) {
      type = historique[i].mime;
      trouve = true;
      break;
    }
  }
  if (!stockagePret || !trouve) {
    serveur.send(404, "text/plain; charset=utf-8", "Vocal expire ou introuvable");
    return;
  }
  File fichier = LittleFS.open(cheminVocal(id), "r");
  if (!fichier) {
    serveur.send(404, "text/plain", "Fichier introuvable");
    return;
  }

  size_t total = fichier.size();
  if (total == 0) {
    fichier.close();
    serveur.send(404, "text/plain", "Vocal vide");
    return;
  }
  size_t debut = 0;
  size_t fin = total - 1;
  bool partiel = false;
  if (serveur.hasHeader("Range")) {
    String plage = serveur.header("Range");
    plage.trim();
    bool valide = plage.startsWith("bytes=") && plage.indexOf(',') < 0;
    if (valide) {
      String spec = plage.substring(6);
      int tiret = spec.indexOf('-');
      valide = tiret >= 0;
      if (valide) {
        String a = spec.substring(0, tiret);
        String b = spec.substring(tiret + 1);
        if (a.length()) {
          if(a.length()>9 || b.length()>9) valide=false;
          for (size_t i=0; i<a.length(); i++) if (!isDigit(a[i])) valide=false;
          for (size_t i=0; i<b.length(); i++) if (!isDigit(b[i])) valide=false;
          if (valide) {
            debut = (size_t)a.toInt();
            if (b.length()) fin = (size_t)b.toInt();
            if (debut >= total || fin < debut) valide = false;
            if (fin >= total) fin = total - 1;
          }
        } else {
          // Suffixe Range: bytes=-128 (128 derniers octets)
          if (!b.length() || b.length()>9) valide = false;
          for (size_t i=0; i<b.length(); i++) if (!isDigit(b[i])) valide=false;
          if (valide) {
            size_t n = (size_t)b.toInt();
            if (!n) valide=false;
            else debut = n >= total ? 0 : total - n;
          }
        }
      }
    }
    if (!valide) {
      serveur.sendHeader("Content-Range", "bytes */" + String((unsigned long)total));
      fichier.close();
      serveur.send(416, "text/plain", "Plage invalide");
      return;
    }
    partiel = true;
  }

  size_t longueur = fin - debut + 1;
  serveur.sendHeader("Cache-Control", "no-store");
  serveur.sendHeader("Accept-Ranges", "bytes");
  serveur.sendHeader("Content-Disposition", "inline");
  if (partiel) {
    serveur.sendHeader("Content-Range", "bytes " + String((unsigned long)debut) + "-" +
      String((unsigned long)fin) + "/" + String((unsigned long)total));
  }
  serveur.setContentLength(longueur);
  serveur.send(partiel ? 206 : 200, type.c_str(), "");
  fichier.seek(debut);
  uint8_t bloc[1024];
  size_t restant = longueur;
  while (restant && serveur.client().connected()) {
    size_t attendu = restant < sizeof(bloc) ? restant : sizeof(bloc);
    size_t lu = fichier.read(bloc, attendu);
    if (!lu) break;
    size_t offset = 0;
    while (offset < lu && serveur.client().connected()) {
      size_t ecrit = serveur.client().write(bloc + offset, lu - offset);
      if (!ecrit) break;
      offset += ecrit;
      octets += ecrit;
      yield();
    }
    restant -= offset;
    if (offset < lu) break;
  }
  fichier.close();
}

// ---------- Portail captif d'origine ----------
void redirigerPortail() {
  serveur.sendHeader("Location", "http://192.168.4.1/", true);
  serveur.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  serveur.sendHeader("Pragma", "no-cache");
  serveur.send(302, "text/plain; charset=utf-8", "Ouverture de Radio-Secours");
}

void configurerPortailCaptif() {
  // Android
  serveur.on("/generate_204", HTTP_GET, redirigerPortail);
  serveur.on("/gen_204", HTTP_GET, redirigerPortail);
  serveur.on("/generate204", HTTP_GET, redirigerPortail);

  // iOS / macOS
  serveur.on("/hotspot-detect.html", HTTP_GET, redirigerPortail);
  serveur.on("/library/test/success.html", HTTP_GET, redirigerPortail);

  // Windows / autres
  serveur.on("/connecttest.txt", HTTP_GET, redirigerPortail);
  serveur.on("/ncsi.txt", HTTP_GET, redirigerPortail);
  serveur.on("/redirect", HTTP_GET, redirigerPortail);
  serveur.on("/fwlink", HTTP_GET, redirigerPortail);

  serveur.onNotFound(redirigerPortail);
}

void lireBouton() {
  static int avant = HIGH;
  static unsigned long dernierAppui = 0;
  int etat = digitalRead(BOUTON);
  if (etat == LOW && avant == HIGH && millis() - dernierAppui > 200) {
    silencieux = !silencieux;
    dernierAppui = millis();
    Serial.println(silencieux ? "Mode silencieux" : "Son actif");
  }
  avant = etat;
}

// LCD 16x2 : fait defiler les informations du principal et du relais.
// Ne presente les identifiants du relais que si son signal est recent.
void mettreAJourLCD() {
  static uint32_t dernierChangement = 0;
  static uint8_t page = 255;
  static bool dernierEtat = false;
  bool actif = relaisActif();
  uint32_t maintenant = millis();

  // Rafraichir immediatement lors de l'activation/desactivation du relais.
  if (page != 255 && actif == dernierEtat &&
      (uint32_t)(maintenant - dernierChangement) < 2800UL) return;
  if (page == 255) page = 0;
  else if (actif != dernierEtat) page = 2; // Montrer l'etat sans attendre.
  else page = (page + 1) % (actif ? 6 : 3);
  dernierEtat = actif;
  dernierChangement = maintenant;

  lcd.clear();
  if (page == 0) {
    lcd.print("Radio-Secours");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.softAPIP());
  } else if (page == 1) {
    lcd.print("MDP principal:");
    lcd.setCursor(0, 1);
    lcd.print(MOT_DE_PASSE);
  } else if (page == 2) {
    lcd.print(actif ? "RELAIS: ACTIF" : "RELAIS: INACTIF");
    lcd.setCursor(0, 1);
    lcd.print(actif ? "WiFi: voir suite" : "Attente liaison");
  } else if (page == 3) {
    // Nom Wi-Fi complet sur deux lignes (plus de 16 caracteres).
    String nom = String(NOM_WIFI_RELAIS);
    lcd.print(nom.substring(0, 16));
    lcd.setCursor(0, 1);
    lcd.print(nom.substring(16));
  } else if (page == 4) {
    lcd.print("Adresse relais:");
    lcd.setCursor(0, 1);
    lcd.print(ADRESSE_RELAIS);
  } else if (page == 5) {
    lcd.print("MDP relais:");
    lcd.setCursor(0, 1);
    lcd.print(MOT_DE_PASSE_RELAIS);
  }
}

void mettreAJourSorties() {
  unsigned long maintenant = millis();
  volume = analogRead(POT) * 100 / 4095;

  static unsigned long debutFenetre = 0;
  if (maintenant - debutFenetre >= 1000) {
    debit = octets;
    octets = 0;
    debutFenetre = maintenant;
  }

  unsigned long periode = constrain(500 - (long)debit / 5, 50L, 500L);
  bool rougeAllumee = debit > 0 && (maintenant / periode) % 2;

  ledcWrite(LED_VERTE, LUM);
  ledcWrite(LED_JAUNE, WiFi.softAPgetStationNum() > 0 ? LUM : 0);
  ledcWrite(LED_ROUGE, rougeAllumee ? LUM : 0);

  if (debutBleue) {
    unsigned long t = maintenant - debutBleue;
    if (t < 300) ledcWrite(LED_BLEUE, t * LUM / 300);
    else if (t < 1000) ledcWrite(LED_BLEUE, (1000 - t) * LUM / 700);
    else {ledcWrite(LED_BLEUE, 0);debutBleue = 0;}
  }

  if (finBip && (maintenant >= finBip || silencieux || volume == 0)) {
    ledcWriteTone(BUZZER, 0);
    finBip = 0;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BOUTON, INPUT_PULLUP);
  ledcAttach(LED_VERTE, 5000, 8);
  ledcAttach(LED_JAUNE, 5000, 8);
  ledcAttach(LED_ROUGE, 5000, 8);
  ledcAttach(LED_BLEUE, 5000, 8);
  ledcAttach(BUZZER, 1000, 10);

  // Monter LittleFS; si FORMATER_LITTLEFS_SI_ECHEC est true, un echec
  // de montage peut declencher un formatage de sa partition de donnees.
  // Sauvegarder tout fichier LittleFS deja present avant de flasher.
  stockagePret = LittleFS.begin(FORMATER_LITTLEFS_SI_ECHEC);
  if (stockagePret) {
    nettoyerFichiersVocaux();
    Serial.printf("Flash LittleFS : %u / %u octets occupes\n",
      (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
  } else {
    Serial.println("ERREUR LittleFS : messages texte OK, mais vocaux indisponibles. Verifiez le schema de partitions LittleFS et initialisez-le au besoin.");
  }

  sessionServeur = esp_random();
  WiFi.mode(WIFI_AP);
  IPAddress ip(192, 168, 4, 1);
  IPAddress masque(255, 255, 255, 0);
  WiFi.softAPConfig(ip, ip, masque);
  if (!WiFi.softAP(NOM_WIFI, MOT_DE_PASSE)) {
    Serial.println("ERREUR : impossible de demarrer le Wi-Fi Radio-Secours");
  }

  dns.start(53, "*", ip);
  lcd.begin(16, 2);
  lcd.setRGB(0, 120, 255);
  lcd.print("Radio-Secours");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.softAPIP());

  // Capturer Range pour lire les fichiers audio sur Safari iPhone.
  const char* entetes[] = {"Range"};
  serveur.collectHeaders(entetes, 1);

  // Chat texte
  serveur.on("/", HTTP_GET, envoyerPage);
  serveur.on("/msg", HTTP_GET, envoyerMessages);
  serveur.on("/envoyer", HTTP_POST, recevoirMessage);
  serveur.on("/relais-ping", HTTP_POST, recevoirPresenceRelais);
  serveur.on("/relais-etat", HTTP_GET, consulterPresenceRelais);

  // Envoi multipart en flux : aucun fichier audio complet charge en RAM.
  serveur.on("/envoyer-vocal", HTTP_POST, confirmerVocal, receptionFluxVocal);
  serveur.on("/vocal", HTTP_GET, lireVocal);

  configurerPortailCaptif();
  serveur.begin();

  Serial.println("Sur iPhone ouvrir Safari, sur Android ouvrir Chrome.");
  Serial.print("Site : http://");
  Serial.println(WiFi.softAPIP());
  Serial.print("Mot de passe WiFi : ");
  Serial.println(MOT_DE_PASSE);
}

void loop() {
  dns.processNextRequest();
  serveur.handleClient();
  lireBouton();
  mettreAJourSorties();
  mettreAJourLCD();
}
