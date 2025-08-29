#include <Arduino.h>
// #include "pinDefs.h"
// #include <TFT_eSPI.h> // Graphics and font library for ILI9341 driver chip

void setup() {
  // UART setup
  Serial.begin(115200);          // initialize USB serial
  while (!Serial) { }            // wait for USB connection
  Serial.println("Hello from FireBeetle C6!");
  // Builtin LED setup
  pinMode(15, OUTPUT);

  vTaskDelay(1000); // Delay to allow time to open Serial Monitor
  Serial.println("Turning on the LED for 1 second...");
  digitalWrite(15, HIGH); // Turn the LED on (Note that LOW is the voltage level)
  vTaskDelay(500);       // Wait for a second
  Serial.println("Turning off the LED for 1 second...");
  digitalWrite(15, LOW);  // Turn the LED off by making the voltage HIGH
  vTaskDelay(500);       // Wait for a second
  // TFT setup
  // TFT_eSPI tft = TFT_eSPI(); // Invoke library, pins defined in User_Setup.h
  // tft.init();
  // tft.setRotation(1);
  // tft.fillScreen(TFT_BLACK);
  // tft.setTextColor(TFT_WHITE, TFT_BLACK); // Note: Adding the background
  // tft.setTextSize(2);
  // tft.setCursor(0, 0);
  // vTaskDelay(1000); // Delay to allow time to see the display update
  // tft.println("Hello, world!");
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("Hello, world!");
  // FLash led every 2 seconds
  digitalWrite(15, HIGH); // Turn the LED on (Note that LOW is the voltage level)
  vTaskDelay(200);       // Wait for a second
  digitalWrite(15, LOW);  // Turn the LED off by making the voltage HIGH
  vTaskDelay(1800);       // Wait for a second
}
