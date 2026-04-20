#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
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
Status aktuellerStatus = WARTEN; 
int wiederholSchritt = 0;

uint8_t zielAdresse[] = {0x48, 0xE7, 0x29, 0x95, 0x9A, 0x7C};

// special messages
const uint8_t MSG_WIN = 0xFE;
const uint8_t MSG_RESTART = 0xFD;

int8_t currentColor = -1; // -1 = none
unsigned long lastBlink = 0;
const unsigned long BLINK_INTERVAL = 400;
bool blinkOn = false;
bool amWinner = false;

// forward declaration
void displayText(String t1, String t2 = "");

void setStaticColors() {
  pixels.setPixelColor(3, pixels.Color(255, 0, 0));
  pixels.setPixelColor(2, pixels.Color(0, 0, 255));
  pixels.setPixelColor(1, pixels.Color(0, 255, 0));
  pixels.setPixelColor(0, pixels.Color(255,255,0));
}

void showColorPixel(int colorIdx, bool on) {
  int pix = 3 - colorIdx;
  uint32_t c = 0;
  if (on) {
    if (colorIdx == 0) c = pixels.Color(255,0,0);
    else if (colorIdx == 1) c = pixels.Color(0,0,255);
    else if (colorIdx == 2) c = pixels.Color(0,255,0);
    else if (colorIdx == 3) c = pixels.Color(255,255,0);
  }
  pixels.setPixelColor(pix, c);
}

void displayColor(uint8_t idx) {
  displayText(farben[idx]);
  currentColor = idx;
  lastBlink = millis();
  blinkOn = false;
}

void doReset() {
  kofferIndex = 0;
  aktuellerStatus = ERWEITERN;
  amWinner = false;
  currentColor = -1;
  setStaticColors();
  pixels.show();
  displayText("NEUSTART", "Eins dazu packen!");
}

void displayText(String t1, String t2) {
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
    displayColor(koffer[i]);
    delay(800);
    display.clearDisplay();
    display.display();
    delay(200);
  }
  // stop blinking after showing sequence
  currentColor = -1;
  setStaticColors();
  pixels.show();
  wiederholSchritt = 0;
  aktuellerStatus = WIEDERHOLEN;
  displayText("DEIN ZUG", "Wiederhole alles!");
}

// NEUE SIGNATUR FÜR ARDUINO IDE (V3.0+)
void onDataRecv(const uint8_t * mac, const uint8_t *data, int len) {
  if (len == 1) {
    if (data[0] == MSG_WIN) {
      amWinner = true;
      aktuellerStatus = GAMEOVER;
      currentColor = -1;
      setStaticColors();
      pixels.show();
      displayText("SIEGER!", "Warte auf Master Reset");
      return;
    }
    if (data[0] == MSG_RESTART) {
      doReset();
      return;
    }
  }
  kofferIndex = len;
  memcpy(koffer, data, len);
  zeigeSequenz();
}

void setup() {
  WiFi.mode(WIFI_STA);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  for(int i=0; i<4; i++) pinMode(pins[i], INPUT_PULLUP);

  if (esp_now_init() != ESP_OK) return;

  pixels.begin();
  pixels.clear();
  setStaticColors();
  pixels.show();


  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, zielAdresse, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  esp_now_register_recv_cb(onDataRecv);
  displayText("BEREIT", "Warte auf Master");
}

void loop() {
  // handle LED blinking for current displayed color
  if (millis() - lastBlink >= BLINK_INTERVAL) {
    lastBlink = millis();
    blinkOn = !blinkOn;
    if (currentColor >= 0) {
      for (int i = 0; i < NUMPIXELS; i++) pixels.setPixelColor(i, 0);
      showColorPixel(currentColor, blinkOn);
      pixels.show();
    } else {
      setStaticColors();
      pixels.show();
    }
  }

  // ensure static colors are shown while it's a player's turn
  if (aktuellerStatus == WIEDERHOLEN || aktuellerStatus == ERWEITERN) {
    if (currentColor != -1) {
      currentColor = -1;
      setStaticColors();
      pixels.show();
    }
  }

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
            // inform peer that they won
            esp_now_send(zielAdresse, &MSG_WIN, 1);
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
  } else if (aktuellerStatus == GAMEOVER) {
    // allow winner to restart by pressing any button
    if (amWinner) {
      for (int i = 0; i < 4; i++) {
        if (digitalRead(pins[i]) == LOW) {
          delay(50);
          if (digitalRead(pins[i]) == LOW) {
            esp_now_send(zielAdresse, &MSG_RESTART, 1);
            doReset();
            while(digitalRead(pins[i]) == LOW) delay(10);
          }
        }
      }
    }
  }
}

