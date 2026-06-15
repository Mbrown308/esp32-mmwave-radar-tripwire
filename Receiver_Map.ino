#include <RadioLib.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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
  display.println("RADAR MAP ONLINE...");
  display.display();

  radio.begin(915.0); 
}

void loop() {
  String incomingMessage;
  int state = radio.receive(incomingMessage);

  if (state == RADIOLIB_ERR_NONE) {
    int rawX = 0;
    int rawY = 0;
    
    if (sscanf(incomingMessage.c_str(), "X: %d | Y: %d", &rawX, &rawY) == 2) {

      int screenX = map(rawX, -1000, 1000, 0, 128); 
      int screenY = map(rawY, 0, 3000, 63, 0);

      display.clearDisplay();
      
      display.drawCircle(64, 63, 20, SSD1306_WHITE);
      display.drawCircle(64, 63, 40, SSD1306_WHITE);
      display.drawCircle(64, 63, 60, SSD1306_WHITE);
      
      display.fillCircle(screenX, screenY, 4, SSD1306_WHITE);

      display.setCursor(0, 0);
      display.print("X:"); display.print(rawX); 
      display.print(" Y:"); display.print(rawY);
      
      display.display();
    }
  }
}