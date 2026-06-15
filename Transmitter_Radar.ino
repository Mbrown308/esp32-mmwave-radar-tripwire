#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RadioLib.h>

const int MIN_Y_DISTANCE = 200;  
const int MAX_Y_DISTANCE = 3000; 
const int MAX_X_SPREAD   = 1000; 

#define RADAR_RX 47
#define RADAR_TX 48
#define LORA_CS 8
#define LORA_DIO1 14
#define LORA_RST 12
#define LORA_BUSY 13
#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21
#define VEXT 36 
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

uint8_t buffer[30];
int bufIdx = 0;
unsigned long lastDisplayTime = 0;

int targetsCaught = 0;
bool currentlyInZone = false;

void setup() {
  Serial.begin(115200);
  pinMode(VEXT, OUTPUT);
  digitalWrite(VEXT, LOW); 
  delay(50);

  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("TRIPWIRE ONLINE...");
  display.display();

  radio.begin(915.0); 
  Serial1.begin(256000, SERIAL_8N1, RADAR_RX, RADAR_TX);
  delay(1000); 
}

void loop() {
  while (Serial1.available()) {
    buffer[bufIdx] = Serial1.read();
    bufIdx++;
    
    if (bufIdx >= 4) {
      if (buffer[0] != 0xAA || buffer[1] != 0xFF || buffer[2] != 0x03 || buffer[3] != 0x00) {
        for (int i = 0; i < bufIdx - 1; i++) { buffer[i] = buffer[i + 1]; }
        bufIdx--;
      }
    }
    
    if (bufIdx == 30) {
      int x = buffer[4] + (buffer[5] << 8);
      if (x & 0x8000) { x = (x & 0x7FFF) * -1; } 
      
      int y = buffer[6] + (buffer[7] << 8);
      if (y & 0x8000) { y = (y & 0x7FFF) * -1; } 
      
      y = abs(y);

      if (x != 0 || y != 0) {
        
        bool isGoodTarget = (y >= MIN_Y_DISTANCE && y <= MAX_Y_DISTANCE) && (abs(x) <= MAX_X_SPREAD);

        if (isGoodTarget && !currentlyInZone) {
          targetsCaught++;
          currentlyInZone = true; 
        } else if (!isGoodTarget) {
          currentlyInZone = false; 
        }

        if (millis() - lastDisplayTime > 200) {
          display.clearDisplay();
          display.setCursor(0, 0);
          
          display.setTextSize(1);
          if (isGoodTarget) { display.println("STATUS: IN ZONE!"); } 
          else { display.println("STATUS: BLOCKED"); }
          
          display.setTextSize(2);
          display.print("CAUGHT:"); display.println(targetsCaught);
          
          display.setTextSize(1);
          display.print("X:"); display.print(x);
          display.print(" Y:"); display.println(y);
          display.display();
          
          if (isGoodTarget) {
            String payload = "X: " + String(x) + " | Y: " + String(y);
            radio.transmit(payload);
          }
          
          lastDisplayTime = millis();
        }
      }
      bufIdx = 0;
    }
  }
}