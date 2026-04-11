#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "esp_sleep.h"

#define BUTTON_PIN 14
#define RED_LED_PIN 18
#define GREEN_LED_PIN 19

LiquidCrystal_I2C lcd(0x27, 16, 2);

typedef struct {
  uint8_t plant1_moisture;
  uint8_t plant2_moisture;
  uint32_t timestamp;
} soil_data_t;

soil_data_t receivedData;
bool dataReceived = false;
bool hasValidData = false;
unsigned long displayStartTime = 0;
const unsigned long HUMIDITY_TIME = 30000;
const unsigned long COUNTDOWN_TIME = 10000;

void onDataReceived(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== LIGHT SLEEP DISPLAY ===");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  WiFi.mode(WIFI_STA);
  esp_now_init();
  esp_now_register_recv_cb(onDataReceived);

  displayStartTime = millis();
  Serial.println("Ready");
}

void loop() {
  unsigned long elapsed = millis() - displayStartTime;

  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("Button pressed!");
    buttonPressed();
    delay(500);
    return;
  }

  if (dataReceived || hasValidData) {
    displayHumidity();
    dataReceived = false;
  } else if (elapsed < HUMIDITY_TIME) {
    showNoDataScreen();
  } else if (elapsed < (HUMIDITY_TIME + COUNTDOWN_TIME)) {
    showCountdown();
  } else {
    Serial.println("Entering light sleep...");
    lightSleepWake();
  }

  delay(1000);
}

void onDataReceived(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  if (len == sizeof(soil_data_t)) {
    memcpy(&receivedData, data, sizeof(soil_data_t));
    dataReceived = true;
    Serial.printf("Data received: P1=%d%% P2=%d%%\n",
                  receivedData.plant1_moisture,
                  receivedData.plant2_moisture);
  } else {
    Serial.printf("Unexpected data length: %d\n", len);
  }
}

void buttonPressed() {
  displayStartTime = millis();
  lcd.backlight();
  lcd.clear();
  lcd.print("Button OK!");
  delay(1000);
}

void lightSleepWake() {
  lcd.noBacklight();
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);

  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0);
  esp_light_sleep_start();

  // Resumes here after wake
  Serial.println("Woke from light sleep!");
  displayStartTime = millis();  // prevents immediate re-sleep
  lcd.backlight();              // turn screen back on
  dataReceived = false;
  delay(300);                   // debounce button
}

void displayHumidity() {
  bool p1Good = receivedData.plant1_moisture > 30;
  bool p2Good = receivedData.plant2_moisture > 30;

  digitalWrite(RED_LED_PIN,   (p1Good && p2Good) ? LOW  : HIGH);
  digitalWrite(GREEN_LED_PIN, (p1Good && p2Good) ? HIGH : LOW);

  lcd.clear();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("P1:");
  lcd.print(receivedData.plant1_moisture);
  lcd.print("% ");
  lcd.print(p1Good ? "OK" : "DRY");

  lcd.setCursor(0, 1);
  lcd.print("P2:");
  lcd.print(receivedData.plant2_moisture);
  lcd.print("% ");
  lcd.print(p2Good ? "OK" : "DRY");

  hasValidData = true;
  displayStartTime = millis();
}

void showNoDataScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("No Data");
  lcd.setCursor(0, 1);
  lcd.print("Wait ESP32-1");
}

void showCountdown() {
  int countdown = (HUMIDITY_TIME + COUNTDOWN_TIME - (millis() - displayStartTime)) / 1000;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sleep in:");
  lcd.setCursor(0, 1);
  lcd.print(countdown);
  lcd.print("s  GPIO14");
}
