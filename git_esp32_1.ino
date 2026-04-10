#include <esp_now.h>
#include <WiFi.h>
#include <driver/adc.h>

// Pins
#define SOIL_SENSOR_1_PIN 34  // Plant 1 AO
#define SOIL_SENSOR_2_PIN 35  // Plant 2 AO
#define RED_LED_PIN 2
#define GREEN_LED_PIN 4

// Data structure
typedef struct {
  uint8_t plant1_moisture;
  uint8_t plant2_moisture;
  uint32_t timestamp;
} soil_data_t;

soil_data_t sensorData;
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 360000; // 1 HOUR 

// ESP32(2) MAC ADDRESS - YOURS!
uint8_t receiverMacAddress[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6};

void setup() {
  Serial.begin(115200);
  
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  
  // ADC Setup for soil sensors
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11); // GPIO34
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11); // GPIO35
  
  WiFi.mode(WIFI_STA);
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    return;
  }
  
  // Add ESP32(2) as peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMacAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.printf("Failed to add peer: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  receiverMacAddress[0], receiverMacAddress[1], receiverMacAddress[2],
                  receiverMacAddress[3], receiverMacAddress[4], receiverMacAddress[5]);
  } else {
    Serial.println("ESP32(2) peer connected");
  }
  
  esp_now_register_send_cb(onDataSent);
  
  Serial.println("ESP32(1) Soil Monitor READY");
  Serial.print("MAC: "); Serial.println(WiFi.macAddress());
  Serial.printf("Next send in: %lu minutes\n", SEND_INTERVAL/60000);
}

void loop() {
  // Read soil sensors (AO pins)
  int raw1 = adc1_get_raw(ADC1_CHANNEL_6);
  int raw2 = adc1_get_raw(ADC1_CHANNEL_7);
  
  // *** CALIBRATE THESE VALUES FOR YOUR SENSORS ***
  const int DRY_AIR  = 0;  // ← MEASURE: Sensor in AIR
  const int WET_SOIL = 100;  // ← MEASURE: Sensor in WET SOIL
  
  sensorData.plant1_moisture = constrain(map(raw1, DRY_AIR, WET_SOIL, 0, 100), 0, 100);
  sensorData.plant2_moisture = constrain(map(raw2, DRY_AIR, WET_SOIL, 0, 100), 0, 100);
  sensorData.timestamp = millis();
  
  // Status LEDs (30% threshold)
  bool plant1Good = sensorData.plant1_moisture > 30;
  bool plant2Good = sensorData.plant2_moisture > 30;
  bool bothGood = plant1Good && plant2Good;
  
  digitalWrite(GREEN_LED_PIN, bothGood ? HIGH : LOW);
  digitalWrite(RED_LED_PIN, bothGood ? LOW : HIGH);
  
  // Send hourly
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    sendData();
    lastSendTime = millis();
  }
  
  // Status every 30s
  static unsigned long statusTime = 0;
  if (millis() - statusTime > 30000) {
    Serial.printf("P1:%3d%% %s | P2:%3d%% %s | Next:%lus\n",
                  sensorData.plant1_moisture, plant1Good?"OK":"DRY",
                  sensorData.plant2_moisture, plant2Good?"OK":"DRY",
                  (SEND_INTERVAL - (millis() - lastSendTime)) / 1000);
    statusTime = millis();
  }
  
  delay(1000);
}

void sendData() {
  Serial.println("\n HOURLY DATA TRANSMISSION");
  Serial.printf("P1: %d%% | P2: %d%%\n", sensorData.plant1_moisture, sensorData.plant2_moisture);
  
  esp_err_t result = esp_now_send(receiverMacAddress, (uint8_t*)&sensorData, sizeof(sensorData));
  if (result != ESP_OK) {
    Serial.printf("Send error: %d\n", result);
  }
}

void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.printf("%s\n", status == ESP_NOW_SEND_SUCCESS ? "DELIVERED" : "FAILED");
  Serial.println("================\n");
}