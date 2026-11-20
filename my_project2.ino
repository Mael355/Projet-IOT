#include <WiFi.h>
#include <DNSServer.h>

// reglages identiques dans le sketch du principal
const char* WIFI_PRINCIPAL = "Radio-Secours";
const char* MOT_DE_PASSE = "Kx7!Pm4q-Zr9w#";
const char* WIFI_RELAIS = "Radio-Secours-Relais";
const char* CLE_RELAIS = "radio-secours-relais-presence-v1";

IPAddress IP_PRINCIPAL(192, 168, 4, 1);
IPAddress IP_RELAIS(192, 168, 5, 1);

WiFiServer serveur(80);
DNSServer dns;
unsigned long dernierPing = 0;
unsigned long dernierEssai = 0;

bool principalConnecte() {
  return WiFi.status() == WL_CONNECTED;
}

// adresses du chat a transmettre au principal
bool estPageDuChat(String chemin) {
  return chemin == "/" || chemin == "/envoyer" || chemin == "/envoyer-vocal" ||
         chemin.startsWith("/msg") || chemin.startsWith("/vocal?");
}

// copie nb octets de source vers dest ou tout jusqu'a la fermeture si nb vaut -1
void copier(WiFiClient& source, WiFiClient& dest, int nb) {
  uint8_t bloc[1024];
  unsigned long dernier = millis();
  while (nb != 0 && millis() - dernier < 10000) {
    int dispo = source.available();
    if (dispo > 0) {
      int voulu = min(dispo, 1024);
      if (nb > 0 && nb < voulu) voulu = nb;
      int lu = source.read(bloc, voulu);
      dest.write(bloc, lu);
      if (nb > 0) nb -= lu;
      dernier = millis();
    } else if (!source.connected()) {
      break;
    } else {
      delay(1);
    }
  }
}

// signale au principal que le relais est allume toutes les 5 secondes
void envoyerPing() {
  if (!principalConnecte() || millis() - dernierPing < 5000) return;
  dernierPing = millis();

  WiFiClient client;
  if (!client.connect(IP_PRINCIPAL, 80, 1000)) return;
  String corps = "cle=" + String(CLE_RELAIS);
  client.print("POST /relais-ping HTTP/1.1\r\n");
  client.print("Host: 192.168.4.1\r\n");
  client.print("Content-Type: application/x-www-form-urlencoded\r\n");
  client.print("Content-Length: " + String(corps.length()) + "\r\n");
  client.print("Connection: close\r\n\r\n");
  client.print(corps);

  // attend la reponse du principal
  client.setTimeout(1000);
  client.readStringUntil('\n');
  client.stop();
}

// recoit la requete d'un telephone et la transmet au principal
void traiter(WiFiClient& telephone) {
  telephone.setTimeout(5000);

  // premiere ligne par exemple GET /msg?depuis=3 HTTP/1.1
  String premiereLigne = telephone.readStringUntil('\n');
  int espace1 = premiereLigne.indexOf(' ');
  int espace2 = premiereLigne.indexOf(' ', espace1 + 1);
  String chemin = premiereLigne.substring(espace1 + 1, espace2);

  // lecture des en-tetes jusqu'a la ligne vide
  String entetes = "";
  int taille = 0;
  while (true) {
    String ligne = telephone.readStringUntil('\n');
    ligne.trim();
    if (ligne == "") break;
    int deuxPoints = ligne.indexOf(':');
    String nom = ligne.substring(0, deuxPoints);
    nom.toLowerCase();
    if (nom == "content-length") taille = ligne.substring(deuxPoints + 1).toInt();
    // host et connection sont reecrits plus bas
    if (nom != "host" && nom != "connection" && nom != "expect") entetes += ligne + "\r\n";
  }

  // portail captif toute autre adresse renvoie vers le chat du relais
  if (!estPageDuChat(chemin)) {
    telephone.print("HTTP/1.1 302 Found\r\nLocation: http://192.168.5.1/\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    return;
  }

  WiFiClient principal;
  if (!principalConnecte() || !principal.connect(IP_PRINCIPAL, 80, 3000)) {
    telephone.print("HTTP/1.1 503 Service Unavailable\r\nContent-Type: text/plain; charset=utf-8\r\nConnection: close\r\n\r\nESP32 principal hors de portee");
    return;
  }

  // envoi de la requete au principal
  principal.print(premiereLigne + "\n");
  principal.print(entetes);
  principal.print("Host: 192.168.4.1\r\nConnection: close\r\n\r\n");
  copier(telephone, principal, taille);

  // renvoi de la reponse au telephone
  copier(principal, telephone, -1);
  principal.stop();
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);

  // connexion au principal d'abord pour prendre le meme canal wifi
  WiFi.begin(WIFI_PRINCIPAL, MOT_DE_PASSE);
  unsigned long debut = millis();
  while (!principalConnecte() && millis() - debut < 10000) delay(250);
  Serial.println(principalConnecte() ? "Principal connecte" : "Principal absent");

  // point d'acces du relais
  int canal = principalConnecte() ? WiFi.channel() : 1;
  WiFi.softAPConfig(IP_RELAIS, IP_RELAIS, IPAddress(255, 255, 255, 0));
  WiFi.softAP(WIFI_RELAIS, MOT_DE_PASSE, canal);
  dns.start(53, "*", IP_RELAIS);
  serveur.begin();

  Serial.println("Relais pret sur http://192.168.5.1");
}

void loop() {
  dns.processNextRequest();
  envoyerPing();

  WiFiClient telephone = serveur.accept();
  if (telephone) {
    traiter(telephone);
    telephone.stop();
  }

  // reconnexion au principal toutes les 10 secondes si perdu
  if (!principalConnecte() && millis() - dernierEssai > 10000) {
    dernierEssai = millis();
    WiFi.reconnect();
  }
}
