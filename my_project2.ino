/*
  RADIO-SECOURS — ESP32 n°2 : RELAIS TRANSPARENT VERS LE PREMIER ESP32
*/

#include <WiFi.h>
#include <DNSServer.h>

// ---------------- A CONFIGURER ----------------
const char* WIFI_PRINCIPAL = "Radio-Secours";
const char* MDP_PRINCIPAL  = "Kx7!Pm4q-Zr9w#";
const char* WIFI_RELAIS   = "Radio-Secours-Relais";
const char* MDP_RELAIS    = "Kx7!Pm4q-Zr9w#"; // identique au Wi-Fi principal
const char* CLE_PRESENCE_RELAIS = "radio-secours-relais-presence-v1";
const uint32_t PERIODE_PRESENCE = 5000UL;

IPAddress IP_PRINCIPAL(192, 168, 4, 1);
IPAddress IP_RELAIS(192, 168, 5, 1);
IPAddress MASQUE(255, 255, 255, 0);

// Pour eviter les requetes abusives et limiter la RAM utilisee :
const size_t MAX_ENTETES    = 8192;                // 8 Kio
const size_t MAX_REQUETE    = 512UL * 1024UL;      // texte + multipart audio
const uint32_t TIMEOUT_HTTP = 10000;               // 10 s sans progression
const uint32_t TIMEOUT_CONN = 3000;                // connexion n°1
const uint32_t RECONNEXION  = 8000;

WiFiServer serveur(80);
DNSServer dns;
uint32_t prochainEssai = 0;
uint32_t requetesRelais = 0;
uint32_t erreursRelais = 0;
uint32_t dernierEnvoiPresence = 0;
bool presenceDejaEnvoyee = false;

bool principalConnecte() {
  return WiFi.status() == WL_CONNECTED;
}

// Ecrit la totalite d'un bloc (les ecritures TCP peuvent etre partielles).
bool ecrireTout(WiFiClient &dest, const uint8_t *donnees, size_t longueur) {
  size_t fait = 0;
  uint32_t dernier = millis();
  while (fait < longueur) {
    if (!dest.connected()) return false;
    size_t n = dest.write(donnees + fait, longueur - fait);
    if (n > 0) { fait += n; dernier = millis(); }
    else if (millis() - dernier >= TIMEOUT_HTTP) return false;
    delay(1);
  }
  return true;
}

bool ecrireTexte(WiFiClient &dest, const String &texte) {
  return ecrireTout(dest, (const uint8_t*)texte.c_str(), texte.length());
}

void repondre(WiFiClient &client, int code, const char *raison,
             const String &corps, const char *type = "text/plain; charset=utf-8",
             const String &extra = "") {
  String h = "HTTP/1.1 " + String(code) + " " + String(raison) + "\r\n";
  h += "Content-Type: " + String(type) + "\r\n";
  h += "Content-Length: " + String(corps.length()) + "\r\n";
  h += "Cache-Control: no-store\r\nConnection: close\r\n";
  h += extra + "\r\n";
  ecrireTexte(client, h);
  ecrireTexte(client, corps);
}

// Lit seulement l'en-tete ; n'aspire pas le corps d'une requete POST.
bool lireEntetes(WiFiClient &client, String &entetes) {
  entetes = "";
  entetes.reserve(1600);
  uint32_t dernier = millis();
  while (entetes.length() < MAX_ENTETES) {
    if (client.available()) {
      int b = client.read();
      if (b < 0) continue;
      entetes += (char)b;
      dernier = millis();
      if (entetes.endsWith("\r\n\r\n")) return true;
    } else {
      if (!client.connected() || millis() - dernier >= TIMEOUT_HTTP) return false;
      delay(1);
    }
  }
  return false;
}

String valeurEntete(const String &texte, const String &nomCherche) {
  int debut = texte.indexOf("\r\n") + 2;
  while (debut >= 2 && debut < (int)texte.length()) {
    int fin = texte.indexOf("\r\n", debut);
    if (fin < 0 || fin == debut) break;
    int deuxPoints = texte.indexOf(':', debut);
    if (deuxPoints > debut && deuxPoints < fin) {
      String nom = texte.substring(debut, deuxPoints);
      if (nom.equalsIgnoreCase(nomCherche)) {
        String v = texte.substring(deuxPoints + 1, fin);
        v.trim();
        return v;
      }
    }
    debut = fin + 2;
  }
  return "";
}

// Assure l'envoi du corps POST sans copier le fichier audio dans la flash.
bool transmettreCorps(WiFiClient &telephone, WiFiClient &principal, size_t taille) {
  uint8_t bloc[1024];
  uint32_t dernier = millis();
  while (taille > 0) {
    int dispo = telephone.available();
    if (dispo <= 0) {
      if (!telephone.connected() || !principal.connected() ||
          millis() - dernier >= TIMEOUT_HTTP) return false;
      dns.processNextRequest();
      delay(1);
      continue;
    }
    size_t voulu = min((size_t)dispo, min(taille, sizeof(bloc)));
    int lu = telephone.read(bloc, voulu);
    if (lu <= 0 || !ecrireTout(principal, bloc, (size_t)lu)) return false;
    taille -= (size_t)lu;
    dernier = millis();
    yield();
  }
  return true;
}

// Copie une reponse 200/206/etc en conservant MIME, Content-Range et corps.
// S'arrete des que Content-Length octets ont ete transferes, s'il existe.
bool transmettreReponse(WiFiClient &principal, WiFiClient &telephone,
                        const String &methode) {
  String h;
  if (!lireEntetes(principal, h)) return false;
  if (!h.startsWith("HTTP/1.")) return false;

  String longueurStr = valeurEntete(h, "Content-Length");
  bool longueurConnue = longueurStr.length() > 0;
  size_t restant = 0;
  if (longueurConnue) {
    for (size_t i = 0; i < longueurStr.length(); i++)
      if (!isDigit(longueurStr[i])) return false;
    restant = (size_t)strtoul(longueurStr.c_str(), nullptr, 10);
  }

  // Ne recopie pas le Connection du principal : on ferme apres la requete.
  String hFinal;
  int pos = h.indexOf("\r\n");
  if (pos < 0) return false;
  hFinal = h.substring(0, pos) + "\r\n";
  int debut = pos + 2;
  while (debut < (int)h.length()) {
    int fin = h.indexOf("\r\n", debut);
    if (fin < 0 || fin == debut) break;
    String ligne = h.substring(debut, fin);
    if (!ligne.startsWith("Connection:") && !ligne.startsWith("connection:"))
      hFinal += ligne + "\r\n";
    debut = fin + 2;
  }
  hFinal += "Connection: close\r\n\r\n";
  if (!ecrireTexte(telephone, hFinal)) return false;

  // HEAD, 204, 304 : aucune donnee de corps, meme si en-tetes presents.
  int code = h.substring(9, 12).toInt();
  if (methode == "HEAD" || code == 204 || code == 304 ||
      (code >= 100 && code < 200)) return true;

  uint8_t bloc[1024];
  uint32_t dernier = millis();
  while (true) {
    if (longueurConnue && restant == 0) return true;
    int dispo = principal.available();
    if (dispo > 0) {
      size_t voulu = min((size_t)dispo, sizeof(bloc));
      if (longueurConnue) voulu = min(voulu, restant);
      int lu = principal.read(bloc, voulu);
      if (lu <= 0 || !ecrireTout(telephone, bloc, (size_t)lu)) return false;
      if (longueurConnue) restant -= (size_t)lu;
      dernier = millis();
    } else {
      if (!principal.connected()) return !longueurConnue || restant == 0;
      if (!telephone.connected() || millis() - dernier > TIMEOUT_HTTP) return false;
      dns.processNextRequest();
      delay(1);
    }
  }
}

// Envoie un signe de vie au principal pour que son LCD affiche
// "RELAIS: ACTIF", l'adresse du relais et son mot de passe.
// Les messages utilisateurs ne sont pas enregistres ici.
void signalerPresence() {
  if (!principalConnecte()) {
    presenceDejaEnvoyee = false;
    return;
  }
  if (presenceDejaEnvoyee &&
      (uint32_t)(millis() - dernierEnvoiPresence) < PERIODE_PRESENCE) return;
  dernierEnvoiPresence = millis();

  WiFiClient client;
  client.setTimeout(1200);
  if (!client.connect(IP_PRINCIPAL, 80, 1200)) return;
  String corps = "cle=" + String(CLE_PRESENCE_RELAIS);
  String requete = "POST /relais-ping HTTP/1.1\r\n";
  requete += "Host: 192.168.4.1\r\n";
  requete += "Content-Type: application/x-www-form-urlencoded\r\n";
  requete += "Content-Length: " + String(corps.length()) + "\r\n";
  requete += "Connection: close\r\n\r\n";
  bool ok = ecrireTexte(client, requete) && ecrireTexte(client, corps);
  if (ok) {
    uint32_t debutAttente = millis();
    while (!client.available() && client.connected() &&
           (uint32_t)(millis() - debutAttente) < 750UL) delay(1);
    if (client.available()) {
      String statut = client.readStringUntil('\n');
      if (statut.indexOf("200") >= 0) {
        presenceDejaEnvoyee = true;
      } else {
        Serial.println("Principal : route /relais-ping absente ou refusee");
      }
    }
  }
  client.stop();
}

bool sondePortail(const String &uri) {
  return uri == "/generate_204" || uri == "/gen_204" ||
         uri == "/generate204" || uri == "/hotspot-detect.html" ||
         uri == "/library/test/success.html" ||
         uri == "/connecttest.txt" || uri == "/ncsi.txt" ||
         uri == "/redirect" || uri == "/fwlink";
}

void traiter(WiFiClient &telephone) {
  String requete;
  if (!lireEntetes(telephone, requete)) {
    repondre(telephone, 408, "Request Timeout", "En-tetes HTTP incomplets ou trop longs");
    return;
  }
  int finLigne = requete.indexOf("\r\n");
  int premierEspace = requete.indexOf(' ');
  int secondEspace = requete.indexOf(' ', premierEspace + 1);
  if (finLigne < 0 || premierEspace <= 0 || secondEspace <= premierEspace) {
    repondre(telephone, 400, "Bad Request", "Requete HTTP incorrecte");
    return;
  }
  String methode = requete.substring(0, premierEspace);
  String uri = requete.substring(premierEspace + 1, secondEspace);
  if (!(methode == "GET" || methode == "POST" || methode == "HEAD")) {
    repondre(telephone, 405, "Method Not Allowed", "Utiliser GET, POST ou HEAD");
    return;
  }
  // Chemins absolus et domaines externes ne sont pas des requetes de chat.
  if (!uri.startsWith("/") || uri.length() > 1200) {
    repondre(telephone, 400, "Bad Request", "Chemin HTTP invalide");
    return;
  }

  if (sondePortail(uri)) {
    repondre(telephone, 302, "Found", "Ouvrir Radio-Secours-Relais",
            "text/plain; charset=utf-8", "Location: http://192.168.5.1/\r\n");
    return;
  }
  if (uri == "/etat" && methode == "GET") {
    String corps = "<!doctype html><html lang='fr'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    corps += "<body style='font:17px system-ui;background:#0f172a;color:#e2e8f0;padding:20px'>";
    corps += "<h2>Radio-Secours : relais transparent</h2><p>Reseau du relais : <b>";
    corps += WIFI_RELAIS;
    corps += "</b></p><p>Mot de passe Wi-Fi : <b>";
    corps += MDP_RELAIS;
    corps += "</b></p><p>Adresse du relais : <b>http://192.168.5.1/</b></p>";
    corps += "<p>Premier ESP32 : <b>";
    corps += principalConnecte() ? "connecte" : "deconnecte";
    corps += "</b></p><p>Adresse premier : 192.168.4.1</p>";
    corps += "<p>Telephones sur le relais : " + String(WiFi.softAPgetStationNum()) + "</p>";
    if (principalConnecte()) corps += "<p>Signal : " + String(WiFi.RSSI()) + " dBm</p>";
    corps += "<p>Requetes transmises : " + String(requetesRelais) + "</p>";
    corps += "<p>Erreurs : " + String(erreursRelais) + "</p>";
    corps += "<p>Aucun stockage sur le relais. Tout part vers le premier ESP32.</p>";
    corps += "<p><a style='color:#7dd3fc' href='/'>Ouvrir la conversation</a></p></body></html>";
    repondre(telephone, 200, "OK", corps, "text/html; charset=utf-8");
    return;
  }

  String transfert = valeurEntete(requete, "Transfer-Encoding");
  if (transfert.length()) {
    repondre(telephone, 501, "Not Implemented",
            "Envoi HTTP chunked non pris en charge par ce relais");
    return;
  }
  String tailleTexte = valeurEntete(requete, "Content-Length");
  size_t taille = 0;
  if (tailleTexte.length()) {
    for (size_t i = 0; i < tailleTexte.length(); i++) {
      if (!isDigit(tailleTexte[i])) {
        repondre(telephone, 400, "Bad Request", "Content-Length incorrect");
        return;
      }
    }
    taille = (size_t)strtoul(tailleTexte.c_str(), nullptr, 10);
    if (taille > MAX_REQUETE) {
      repondre(telephone, 413, "Payload Too Large", "Envoi depassant 512 Kio");
      return;
    }
  } else if (methode == "POST") {
    repondre(telephone, 411, "Length Required", "Content-Length requis");
    return;
  }

  if (!principalConnecte()) {
    erreursRelais++;
    repondre(telephone, 503, "Service Unavailable",
            "ESP32 principal hors de portee : aucune donnee n'a ete enregistree");
    return;
  }
  WiFiClient distant;
  distant.setTimeout(TIMEOUT_HTTP);
  if (!distant.connect(IP_PRINCIPAL, 80, TIMEOUT_CONN)) {
    erreursRelais++;
    repondre(telephone, 503, "Service Unavailable",
            "Impossible de joindre le premier ESP32 sur 192.168.4.1");
    return;
  }

  // Le premier ESP32 reste l'unique serveur et stockage.
  String enTeteVersPrincipal = methode + " " + uri + " HTTP/1.1\r\n";
  int debut = finLigne + 2;
  while (debut < (int)requete.length()) {
    int fin = requete.indexOf("\r\n", debut);
    if (fin < 0 || fin == debut) break;
    String ligne = requete.substring(debut, fin);
    int sep = ligne.indexOf(':');
    if (sep > 0) {
      String nom = ligne.substring(0, sep);
      nom.toLowerCase();
      if (nom != "host" && nom != "connection" && nom != "proxy-connection" &&
          nom != "expect") enTeteVersPrincipal += ligne + "\r\n";
    }
    debut = fin + 2;
  }
  enTeteVersPrincipal += "Host: 192.168.4.1\r\nConnection: close\r\n\r\n";
  bool ok = ecrireTexte(distant, enTeteVersPrincipal);
  if (ok && taille > 0) ok = transmettreCorps(telephone, distant, taille);
  if (ok) ok = transmettreReponse(distant, telephone, methode);
  if (ok) requetesRelais++;
  else erreursRelais++;
  // Si la transmission echoue apres envoi du POST, le principal a peut-etre
  // tout de meme recu le message. Ne pas renvoyer automatiquement.
  distant.stop();
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println("\n--- Radio-Secours : Relais transparent ESP32 n°2 ---");
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

  // Connecter d'abord la station afin de caler le canal Wi-Fi du relais.
  WiFi.begin(WIFI_PRINCIPAL, MDP_PRINCIPAL);
  uint32_t debut = millis();
  while (!principalConnecte() && millis() - debut < 10000) delay(250);
  Serial.println(principalConnecte() ? "Principal connecte !" : "Principal absent, reconnexion automatique.");

  WiFi.softAPConfig(IP_RELAIS, IP_RELAIS, MASQUE);
  int canal = principalConnecte() ? WiFi.channel() : 1;
  if (!WiFi.softAP(WIFI_RELAIS, MDP_RELAIS, canal, 0, 4))
    Serial.println("ERREUR : creation du Wi-Fi relais impossible");
  dns.start(53, "*", IP_RELAIS);
  serveur.begin();
  Serial.println("Relais : http://192.168.5.1/");
  Serial.println("Diagnostic : http://192.168.5.1/etat");
  Serial.println("Toutes les requetes du chat sont envoyees au premier ESP32.");
  Serial.print("Mot de passe WiFi relais : ");
  Serial.println(MDP_RELAIS);
  Serial.println("Le principal affichera ACTIF apres le premier signal /relais-ping.");
}

void loop() {
  dns.processNextRequest();
  signalerPresence();
  WiFiClient telephone = serveur.accept();
  if (telephone) {
    traiter(telephone);
    telephone.stop();
  }
  if (!principalConnecte() && millis() - prochainEssai >= RECONNEXION) {
    prochainEssai = millis();
    WiFi.reconnect();
  }
  delay(1);
}
