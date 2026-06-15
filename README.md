# esp32-mmwave-radar-tripwire
A perimeter alarm built from scratch using Heltec ESP32-S3 boards, LoRa radio, and a 24GHz millimeter-wave radar sensor
How to Build a Military-Grade mmWave Radar Tripwire (ESP32 LoRa)
Level: Intermediate | Core Tech: ESP32-S3, LoRa Radio, mmWave Radar

Most DIY motion alarms use cheap PIR (Passive Infrared) sensors that trigger every time a dog walks by or the wind blows a warm bush. We are going to build a completely different beast.

Using millimeter-wave radar, we are going to build a system that actively tracks human skeletal mass, mathematically filters out the background noise, arms a digital tripwire, and beams your exact X/Y coordinates across the neighborhood to a live "Submarine Map" display.

Phase 1: The Exact Hardware List
Do not buy generic parts for this. The code in this tutorial is hard-coded to decode the exact serial byte-stream of one specific radar sensor and radio chip.

The Microcontrollers: 2x Heltec WiFi LoRa 32 (V3 or V4). These use the powerful ESP32-S3 chip and the SX1262 LoRa radio. (Warning: Do not buy the V2, it uses an older chip that will not work with this code).

The Radar Sensor: 1x Hi-Link HLK-LD2450. This is a 24GHz millimeter-wave radar. Do not buy the cheaper LD2410 (which only gives basic distance). The LD2450 calculates live X and Y coordinates and outputs the 30-byte hex stream our code requires.

Cables: Standard female-to-female jumper wires.

Power: 2x standard USB-C power banks or wall chargers.

Phase 2: Software & Library Setup
We are using the Arduino IDE (version 2.3 or newer). Before wiring anything, install the core board drivers and libraries.

1. Install the Heltec Board Drivers

Go to File > Preferences.

Paste this into the "Additional boards manager URLs" box: https://espressif.github.io/arduino-esp32/package_esp32_index.json

Go to Tools > Board > Boards Manager, search for esp32 by Espressif Systems, and install it.

Select Heltec WiFi LoRa 32(V4) (or V3) from the board dropdown.

2. Install the Required Libraries
Click the "Library Manager" icon on the left of the Arduino IDE. Search for and install exactly these three libraries:

RadioLib (by jgromes) - Runs the LoRa radio.

Adafruit SSD1306 (by Adafruit) - Runs the tiny OLED screen.

Adafruit GFX Library (by Adafruit) - Draws the map graphics.

Phase 3: Wiring the Transmitter
You only need to wire one board. The Receiver will sit inside your house and requires zero wiring, just a USB power cord. All physical assembly happens on the Transmitter board.

Connect the radar sensor to the Heltec board:

Radar VCC ➔ Heltec 5V (or 3.3V depending on your sensor's logic level)

Radar GND ➔ Heltec GND

Radar RX ➔ Heltec Pin 47

Radar TX ➔ Heltec Pin 48

CRITICAL: Look closely at your radar sensor. The "eyes" are the flat gold/silver squares printed on the circuit board. Make sure that side is facing your target area!

Phase 4: The "Gotchas" (Read Before Booting!)
Before you upload the code, you must know about the three massive traps this hardware will throw at you.

1. The "ESP-ROM" Crash (The Monitor Trap)
If your board freezes, the screen stays black, and the Arduino terminal spits out an ESP-ROM error, your board crashed into Safe Mode. Opening or closing the Serial Monitor sends a massive power jolt down the USB cable, accidentally pressing the board's internal "Pause" button.

The Fix (The Blind Boot): Completely close your Serial Monitor tab. Press the tiny physical RST button on the board. Wait 3 seconds. Do not open the Serial Monitor again while the board is running! Rely entirely on the physical OLED screen.

2. The Negative 'Y' Glitch
If the board says your target is "BLOCKED" even when you are right in front of it, your radar is reading a negative distance (e.g., Y: -500), thinking you are standing behind it.

The Fix: In our code, we use y = abs(y); to mathematically strip the negative sign off so the brain treats the front and back of the sensor identically.

3. The Screen Lag Crash
If you tell the OLED screen to redraw itself every time the radar blinks, the chip will overheat and crash. Our code limits screen updates to 5 times a second to keep the board stable.

Phase 5: The Transmitter Code (The Brain)
This code does the heavy lifting:

The Bounding Box: It creates an invisible rectangle, ignoring bugs (too close), the street (too far), or swaying trees (too wide).

The Memory Trap: It uses a digital lock (currentlyInZone) to ensure a person is only counted once per visit. They must completely leave the zone before the alarm resets.

Upload this to your Transmitter board:

C++
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RadioLib.h>

// --- THE BOUNDING BOX DIALS (In Millimeters) ---
const int MIN_Y_DISTANCE = 200;  // Ignore closer than 8 inches
const int MAX_Y_DISTANCE = 3000; // Ignore further than 10 feet
const int MAX_X_SPREAD   = 1000; // Ignore wider than 3 feet left/right

// Hardware Pins
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

// THE MEMORY TRAP
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

  // IMPORTANT: Set to your local LoRa frequency (915.0 for US, 868.0 for EU)
  radio.begin(915.0); 
  Serial1.begin(256000, SERIAL_8N1, RADAR_RX, RADAR_TX);
  delay(1000); 
}

void loop() {
  while (Serial1.available()) {
    buffer[bufIdx] = Serial1.read();
    bufIdx++;
    
    // Header Check
    if (bufIdx >= 4) {
      if (buffer[0] != 0xAA || buffer[1] != 0xFF || buffer[2] != 0x03 || buffer[3] != 0x00) {
        for (int i = 0; i < bufIdx - 1; i++) { buffer[i] = buffer[i + 1]; }
        bufIdx--;
      }
    }
    
    // Process Data
    if (bufIdx == 30) {
      int x = buffer[4] + (buffer[5] << 8);
      if (x & 0x8000) { x = (x & 0x7FFF) * -1; } 
      
      int y = buffer[6] + (buffer[7] << 8);
      if (y & 0x8000) { y = (y & 0x7FFF) * -1; } 
      
      // FIX: Strip negative signs
      y = abs(y);

      if (x != 0 || y != 0) {
        
        bool isGoodTarget = (y >= MIN_Y_DISTANCE && y <= MAX_Y_DISTANCE) && (abs(x) <= MAX_X_SPREAD);

        // THE MEMORY TRAP LOGIC
        if (isGoodTarget && !currentlyInZone) {
          targetsCaught++;
          currentlyInZone = true; // Lock the door
        } else if (!isGoodTarget) {
          currentlyInZone = false; // Unlock the door when they leave
        }

        // Screen Update (5 times a second)
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
          
          // Fire the radio ONLY if they are in the zone
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
Phase 6: The Receiver Code (The Submarine Map)
This board sits on your desk inside. It catches the LoRa radio burst and mathematically shrinks your real-world millimeter coordinates down to fit onto a 128x64 pixel grid.

Upload this to your Receiver board:

C++
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

  radio.begin(915.0); // Must match Transmitter frequency
}

void loop() {
  String incomingMessage;
  int state = radio.receive(incomingMessage);

  if (state == RADIOLIB_ERR_NONE) {
    int rawX = 0;
    int rawY = 0;
    
    // Slice numbers out of the text packet
    if (sscanf(incomingMessage.c_str(), "X: %d | Y: %d", &rawX, &rawY) == 2) {

      // Map millimeters to screen pixels!
      int screenX = map(rawX, -1000, 1000, 0, 128); 
      int screenY = map(rawY, 0, 3000, 63, 0);

      display.clearDisplay();
      
      // Draw radar rings
      display.drawCircle(64, 63, 20, SSD1306_WHITE);
      display.drawCircle(64, 63, 40, SSD1306_WHITE);
      display.drawCircle(64, 63, 60, SSD1306_WHITE);
      
      // Draw the Target Blip
      display.fillCircle(screenX, screenY, 4, SSD1306_WHITE);

      display.setCursor(0, 0);
      display.print("X:"); display.print(rawX); 
      display.print(" Y:"); display.print(rawY);
      
      display.display();
    }
  }
}
Phase 7: The Field Test
Put the Transmitter outside on a power bank facing an open area. Keep the Receiver inside.

Walk toward the radar. The moment you step into the 10-foot "Golden Zone," the Transmitter screen will flip to STATUS: IN ZONE, the counter will click to 1, and your Receiver inside will instantly plot your white blip on the Submarine Map.

Walk side to side and watch the blip track your exact center of gravity.

Walk away. The status will revert to BLOCKED, but the counter will remain at 1. The tripwire is re-armed and ready for the next target!

Note: This hardware uses bare circuit boards. If you deploy this permanently outdoors, you MUST put the Transmitter in a waterproof plastic junction box. Millimeter-wave radar can see perfectly through plastic and glass, but it cannot see through metal. Do not put it in a metal box!
Note: The code and troubleshooting guide for this project were developed with the assistance of an AI co-pilot (Gemini)
