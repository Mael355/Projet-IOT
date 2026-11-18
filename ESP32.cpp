// Radio-Secours etapes 1 et 2 reseau WiFi LCD serveur web et chat
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LiquidCrystal_I2C.h>

// broches
const int LED_VERTE = 25;

const int MAX_MSG = 25;

WebServer serveur(80);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// historique en RAM
struct Message {
  int id;
  String pseudo;
  String texte;
};
Message historique[MAX_MSG];
int nbMsg = 0;
int dernierId = 0;

// page web mobile
const char PAGE[] = R"HTML(<!doctype html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Radio-Secours</title>
<style>
*{box-sizing:border-box}
body{margin:0;height:100dvh;display:flex;flex-direction:column;font:16px system-ui,sans-serif;background:#0f172a;color:#e2e8f0}
header{padding:12px;background:#1e293b}
h1{margin:0;font-size:18px}
#etat{font-size:13px;color:#94a3b8}
#liste{flex:1;overflow-y:auto;padding:12px;display:flex;flex-direction:column;gap:8px}
.msg{background:#1e293b;border-radius:12px;padding:8px 12px;max-width:85%;overflow-wrap:anywhere}
.moi{align-self:flex-end;background:#065f46}
.msg b{display:block;font-size:13px;color:#7dd3fc}
form{display:flex;flex-wrap:wrap;gap:8px;padding:8px;background:#1e293b}
input{font:inherit;padding:12px;border:0;border-radius:10px;background:#334155;color:#fff}
#pseudo{width:100%}
#texte{flex:1;min-width:0}
button{font:inherit;font-weight:bold;padding:0 18px;border:0;border-radius:10px;background:#22c55e;color:#052e16}
</style></head><body>
<header><h1>📻 Radio-Secours</h1><div id="etat">Connexion...</div></header>
<div id="liste"></div>
<form id="form">
<input id="pseudo" maxlength="24" placeholder="Ton pseudo" required>
<input id="texte" maxlength="200" placeholder="Message" autocomplete="off" enterkeyhint="send" required>
<button>Envoyer</button>
</form>
<script>
const $=id=>document.getElementById(id);
let dernier=0;
$('pseudo').value=localStorage.pseudo||'';
function ajouter(m){
 const d=document.createElement('div');
 d.className='msg'+(m.p==$('pseudo').value?' moi':'');
 const b=document.createElement('b');
 b.textContent=m.p;
 d.append(b,m.t);
 $('liste').append(d);
 if($('liste').children.length>25)$('liste').firstChild.remove();
 $('liste').scrollTop=1e9;
}
async function actualiser(){
 try{
  const d=await(await fetch('/msg?depuis='+dernier)).json();
  for(const m of d.msg)if(m.id>dernier){ajouter(m);dernier=m.id}
  $('etat').textContent='En ligne';
 }catch(e){$('etat').textContent='Connexion perdue'}
}
$('form').onsubmit=async e=>{
 e.preventDefault();
 localStorage.pseudo=$('pseudo').value;
 await fetch('/envoyer',{method:'POST',body:new URLSearchParams({p:$('pseudo').value,t:$('texte').value})});
 $('texte').value='';
 actualiser();
};
actualiser();
setInterval(actualiser,1500);
</script></body></html>)HTML";

// echappe les caracteres speciaux du JSON
String echapper(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", " ");
  s.replace("\r", " ");
  return s;
}

void ajouterMessage(String pseudo, String texte) {
  // historique plein on decale
  if (nbMsg == MAX_MSG) {
    for (int i = 1; i < MAX_MSG; i++) historique[i - 1] = historique[i];
    nbMsg--;
  }
  dernierId++;
  historique[nbMsg] = {dernierId, pseudo, texte};
  nbMsg++;
  Serial.println(pseudo + " : " + texte);
}

// GET /
void envoyerPage() {
  serveur.send(200, "text/html", PAGE);
}

// GET /msg?depuis=id
void envoyerMessages() {
  int depuis = serveur.arg("depuis").toInt();
  String json = "{\"msg\":[";
  for (int i = 0; i < nbMsg; i++) {
    if (historique[i].id <= depuis) continue;
    if (!json.endsWith("[")) json += ",";
    json += "{\"id\":" + String(historique[i].id);
    json += ",\"p\":\"" + echapper(historique[i].pseudo);
    json += "\",\"t\":\"" + echapper(historique[i].texte) + "\"}";
  }
  json += "]}";
  serveur.send(200, "application/json", json);
}

// POST /envoyer
void recevoirMessage() {
  String pseudo = serveur.arg("p");
  String texte = serveur.arg("t");
  pseudo.trim();
  texte.trim();
  if (pseudo == "" || texte == "") {
    serveur.send(400, "text/plain", "vide");
    return;
  }
  ajouterMessage(pseudo.substring(0, 24), texte.substring(0, 200));
  serveur.send(200, "text/plain", "ok");
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_VERTE, OUTPUT);

  // reseau WiFi ouvert sans Internet
  WiFi.softAP("Radio-Secours");

  // adresse du site sur le LCD
  lcd.init();
  lcd.backlight();
  lcd.print("Radio-Secours");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.softAPIP());

  // routes du site
  serveur.on("/", envoyerPage);
  serveur.on("/msg", envoyerMessages);
  serveur.on("/envoyer", HTTP_POST, recevoirMessage);
  serveur.begin();

  digitalWrite(LED_VERTE, HIGH);
  Serial.print("Site : http://");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  serveur.handleClient();
}