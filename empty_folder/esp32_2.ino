#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Pin definitions for ESP32(2)
#define BUTTON_1_PIN 14
#define BUTTON_2_PIN 27
#define RED_LED_PIN 2
#define GREEN_LED_PIN 15
#define TOGGLE_SWITCH_PIN 13

// LCD setup (I2C) - ESP32 Compatible
LiquidCrystal_I2C lcd(0x27, 16, 2); // Address 0x27, 16x2 display

// Data structure to receive
typedef struct {
  uint8_t plant1_moisture;
  uint8_t plant2_moisture;
  uint32_t timestamp;
} soil_data_t;

soil_data_t receivedData;
bool dataReceived = false;
unsigned long wakeTime = 0;
const unsigned long DISPLAY_TIME = 30000; // 30 seconds

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_2_PIN, INPUT_PULLUP);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(TOGGLE_SWITCH_PIN, INPUT_PULLUP);
  
  // Initialize LCD
  Wire.begin(21, 22); // SDA=GPIO21, SCL=GPIO22
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ESP32(2) Ready");
  
  // Set WiFi station mode
  WiFi.mode(WIFI_STA);
  
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    return;
  }
  
  // FIXED: ESP32 Core 3.0+ Receive Callback
  esp_now_register_recv_cb(onDataReceived);
  
  Serial.println("ESP32(2) Display Node Ready");
  Serial.print("My MAC: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  // Button wake-up
  if (digitalRead(BUTTON_1_PIN) == LOW || digitalRead(BUTTON_2_PIN) == LOW) {
    wakeUpDisplay();
    delay(200);
  }
  
  // Toggle switch - continuous mode
  if (digitalRead(TOGGLE_SWITCH_PIN) == LOW) {
    continuousDisplayMode();
  }
  
  // Show data if received recently
  if (dataReceived && (millis() - wakeTime < DISPLAY_TIME)) {
    displayData();
  } else if (dataReceived && (millis() - wakeTime >= DISPLAY_TIME)) {
    goToSleep();
  }
  
  delay(100);
}

void wakeUpDisplay() {
  Serial.println("Button pressed - Wake up");
  digitalWrite(GREEN_LED_PIN, HIGH);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Waking up...");
  delay(500);
  wakeTime = millis();
}

void continuousDisplayMode() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CONTINUOUS MODE");
  lcd.setCursor(0, 1);
  lcd.print("Switch: OFF");
  digitalWrite(GREEN_LED_PIN, HIGH);
  delay(1500);
  
  while (digitalRead(TOGGLE_SWITCH_PIN) == LOW) {
    displayData();
    delay(2500);
  }
  
  goToSleep();
}

void displayData() {
  lcd.clear();
  
  // LED status (30% threshold)
  bool plant1Good = receivedData.plant1_moisture > 30;
  bool plant2Good = receivedData.plant2_moisture > 30;
  
  // Update LEDs
  digitalWrite(RED_LED_PIN, (plant1Good && plant2Good) ? LOW : HIGH);
  digitalWrite(GREEN_LED_PIN, (plant1Good && plant2Good) ? HIGH : LOW);
  
  // Plant 1
  lcd.setCursor(0, 0);
  lcd.print("P1: ");
  lcd.print(receivedData.plant1_moisture);
  lcd.print("% ");
  lcd.print(plant1Good ? "OK" : "DRY");
  
  // Plant 2
  lcd.setCursor(0, 1);
  lcd.print("P2: ");
  lcd.print(receivedData.plant2_moisture);
  lcd.print("% ");
  lcd.print(plant2Good ? "OK" : "DRY");
}

void goToSleep() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sleep Mode");
  lcd.setCursor(0, 1);
  lcd.print("Press Button");
  dataReceived = false;
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
}

// FIXED CALLBACK - ESP32 Arduino Core 3.0+ Compatible
void onDataReceived(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  memcpy(&receivedData, data, sizeof(receivedData));
  dataReceived = true;
  wakeTime = millis();
  
  const uint8_t* mac = recv_info->src_addr;
  Serial.printf("Received from %02X:%02X:%02X:%02X:%02X:%02X\n", 
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("P1: %d%% | P2: %d%%\n\n", 
                receivedData.plant1_moisture, receivedData.plant2_moisture);
}