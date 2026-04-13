#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);

#define PIN 32        //Spare_IO2
#define NUMPIXELS 4

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRBW + NEO_KHZ800);


uint8_t pins[] = {4, 0, 2, 15};
const char* farben[] = {"ROT", "BLAU", "GRUEN", "GELB"};
uint8_t koffer[100];
uint8_t kofferIndex = 0;

enum Status { WARTEN, ZEIGEN, WIEDERHOLEN, ERWEITERN, GAMEOVER };
Status aktuellerStatus = ERWEITERN; 
int wiederholSchritt = 0;

uint8_t zielAdresse[] = {0x80, 0xF3, 0xDA, 0x4A, 0x18, 0xB0};

void displayText(String t1, String t2 = "") {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.println(t1);
  display.setTextSize(1);
  display.println(t2);
  display.display();
}

void zeigeSequenz() {
  aktuellerStatus = ZEIGEN;
  delay(500);
  for(int i = 0; i < kofferIndex; i++) {
    displayText(farben[koffer[i]]);
    delay(800);
    display.clearDisplay();
    display.display();
    delay(200);
  }
  wiederholSchritt = 0;
  aktuellerStatus = WIEDERHOLEN;
  displayText("DEIN ZUG", "Wiederhole alles!");
}

// ALTE SIGNATUR FÜR PLATFORMIO
void onDataRecv(const uint8_t * mac, const uint8_t *data, int len) {
  kofferIndex = len;
  memcpy(koffer, data, len);
  zeigeSequenz();
}

void setup() {
  WiFi.mode(WIFI_STA);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  pixels.begin();
  pixels.clear();
  
  pixels.setPixelColor(3, pixels.Color(255, 0, 0));
  pixels.setPixelColor(2, pixels.Color(0, 0, 255));
  pixels.setPixelColor(1, pixels.Color(0, 255, 0));
  pixels.setPixelColor(0, pixels.Color(255,255,0));
  pixels.show();

  for(int i=0; i<4; i++) pinMode(pins[i], INPUT_PULLUP);
  
  if (esp_now_init() != 0) return;

  
  
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, zielAdresse, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
  esp_now_register_recv_cb(onDataRecv);
  
  displayText("START", "Eins dazu packen!");
}

void loop() {
  if (aktuellerStatus == WIEDERHOLEN) {
    for (int i = 0; i < 4; i++) {
      if (digitalRead(pins[i]) == LOW) {
        delay(50); 
        if (digitalRead(pins[i]) == LOW) {
          if (i == koffer[wiederholSchritt]) {
            wiederholSchritt++;
            displayText("OK!", String(wiederholSchritt) + " von " + String(kofferIndex));
            while(digitalRead(pins[i]) == LOW) delay(10);
            if (wiederholSchritt >= kofferIndex) {
              delay(500);
              aktuellerStatus = ERWEITERN;
              displayText("GUT!", "Eins dazu packen!");
            }
          } else {
            aktuellerStatus = GAMEOVER;
            displayText("FALSCH!", "VERLOREN!");
            while(digitalRead(pins[i]) == LOW) delay(10);
          }
        }
      }
    }
  } 
  else if (aktuellerStatus == ERWEITERN) {
    for (int i = 0; i < 4; i++) {
      if (digitalRead(pins[i]) == LOW) {
        delay(50);
        if (digitalRead(pins[i]) == LOW) {
          koffer[kofferIndex++] = i;
          esp_now_send(zielAdresse, koffer, kofferIndex);
          aktuellerStatus = WARTEN;
          displayText("SENDEN..", "Warte auf Gegner");
          while(digitalRead(pins[i]) == LOW) delay(10);
        }
      }
    }
  }
}
