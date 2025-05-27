#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "lionfish_image.h"

// Display Pin Definitions (adjust if needed)
#define TFT_CS    D1
#define TFT_DC    D2
#define TFT_RST   D3

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Adafruit_ADS1115 ads;

float reference_voltage = 0.0;
const float air_o2_percent = 20.9;
const float ppO2_max = 1.4;
const float ppO2_max_tech = 1.6;

float batteryVoltage = 0.0;
int batteryPercentFromVoltage(float voltage);

void drawInterface() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(30, 10);
  tft.println("OXYGEN ANALYZER");

  tft.drawFastHLine(0, 35, 240, ST77XX_WHITE);
}

void drawReadings(float o2, float mod, float mod_tech) {
  tft.fillRect(0, 40, 240, 160, ST77XX_BLACK);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(25, 50);
  tft.println("O2 Concentration");

  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(4);
  tft.setCursor(75, 80);
  tft.print(o2, 1);
  tft.println("%");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 135);
  tft.println("MOD1.4:");

  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(3);
  tft.setCursor(105, 130);
  tft.print(mod, 1);
  tft.println("m");

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 175);
  tft.println("MOD1.6:");

  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(3);
  tft.setCursor(105, 170);
  tft.print(mod_tech, 1);
  tft.println("m");
}

void drawStatus(String msg) {
  tft.fillRect(0, 210, 240, 30, ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.setCursor(20, 210);
  tft.print(msg);
}

void drawImage() {
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRGBBitmap(0, 0, lionfish_image, lionfish_image_width, lionfish_image_height);
  delay(3000);
}

void drawBatteryIcon(float voltage) {
  int x = 10;
  int y = 200;
  int w = 40;
  int h = 20;
  int tipW = 4;

  tft.drawRect(x, y, w, h, ST77XX_WHITE);
  tft.fillRect(x + w, y + h / 4, tipW, h / 2, ST77XX_WHITE);

  int percent = batteryPercentFromVoltage(voltage);

  uint16_t fillColor = ST77XX_GREEN;
  if (percent < 40) fillColor = ST77XX_YELLOW;
  if (percent < 20) fillColor = ST77XX_RED;

  int fillW = (w - 4) * percent / 100;
  tft.fillRect(x + 2, y + 2, w - 4, h - 4, ST77XX_BLACK);
  if (percent > 0) {
    tft.fillRect(x + 2, y + 2, fillW, h - 4, fillColor);
  }

  tft.fillRect(x + 50, y + 5, w, h, ST77XX_BLACK);
  tft.setCursor(x + 50, y+5);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.print(percent);
  tft.print("%");
}

void calibrateSensor() {
  drawStatus("Calibrating...  \n   Hold in Air");
  delay(7000);

  float voltageSum = 0.0;
  for (int i = 0; i < 10; i++) {
    int16_t adc = ads.readADC_SingleEnded(0);
    float voltage = adc * 0.0078125 / 1000.0;
    voltageSum += voltage;
    delay(100);
  }

  reference_voltage = (voltageSum / 10.0) * (21.0 / air_o2_percent);
  drawStatus("Calibrated: \n   " + String(reference_voltage * 1000.0, 2) + " mV");
  delay(2000);
  drawStatus("");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(6, 7);

  // UART to LilyGo: D1 (TX), D0 (RX)
  Serial1.begin(9600, SERIAL_8N1, D7, D6);

  tft.init(240, 240);
  tft.setRotation(0);

  drawImage();
  drawInterface();

  if (!ads.begin(0x48)) {
    drawStatus("ADS1115 FAIL");
    while (1);
  }
  ads.setGain(GAIN_SIXTEEN);

  calibrateSensor();
}

int batteryPercentFromVoltage(float voltage) {
  struct VPoint { float v; int p; };
  const VPoint table[] = {
    {4.10, 100},
    {4.05, 95},
    {4.00, 90},
    {3.95, 85},
    {3.90, 75},
    {3.85, 65},
    {3.80, 55},
    {3.75, 45},
    {3.70, 35},
    {3.65, 25},
    {3.60, 15},
    {3.50, 8},
    {3.30, 0}
  };

  for (int i = 0; i < sizeof(table) / sizeof(table[0]) - 1; i++) {
    if (voltage >= table[i + 1].v) {
      float v1 = table[i].v, v2 = table[i + 1].v;
      int   p1 = table[i].p, p2 = table[i + 1].p;

      float percent = p1 + (voltage - v1) * (p2 - p1) / (v2 - v1);
      return constrain((int)percent, 0, 100);
    }
  }

  return 0; // voltage < 3.30V
}

void loop() {
  static unsigned long lastBatteryUpdate = 0;
  unsigned long now = millis();

  if (now - lastBatteryUpdate > 3000) {  // every 30 sec
    Serial1.println("get");

    unsigned long start = millis();
    String response = "";
    while (millis() - start < 1000) {
      if (Serial1.available()) {
        response = Serial1.readStringUntil('\n');
        Serial.print("Received: ");
        Serial.println(response);
        break;
      }
    }

    if (response.startsWith("BAT:")) {
      batteryVoltage = response.substring(4).toFloat();
    } else {
      batteryVoltage = 0.0;
    }

    drawBatteryIcon(batteryVoltage);
    lastBatteryUpdate = now;
  }


  int16_t adc0 = ads.readADC_SingleEnded(0);
  float voltage = adc0 * 0.0078125 / 1000.0;
  float O2_percent = (voltage / reference_voltage) * 21.0;
  O2_percent = constrain(O2_percent, 1.0, 100.0);
  float MOD = ((ppO2_max / (O2_percent / 100.0)) - 1.0) * 10.0;
  float MOD_tech = ((ppO2_max_tech / (O2_percent / 100.0)) - 1.0) * 10.0;

  Serial.print("O2: ");
  Serial.print(O2_percent, 1);
  Serial.print("% | MOD: ");
  Serial.print(MOD, 1);
  Serial.println(" m");

  drawReadings(O2_percent, MOD, MOD_tech);
  // drawStatus("Oran Hakeves");

  delay(1000);
}
