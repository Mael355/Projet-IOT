// Radio-Secours etape 1 reseau WiFi et ecran LCD
#include <Arduino.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>

const int LED_VERTE = 25;

LiquidCrystal_I2C lcd(0x27, 16, 2);

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

  digitalWrite(LED_VERTE, HIGH);
  Serial.print("Site : http://");
  Serial.println(WiFi.softAPIP());
}

void loop() {}