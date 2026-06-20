#include <WiFi.h>
#include <esp_now.h>
#include <esp_sleep.h>

#define PIR_PIN 13    // PIR motion sensor pin
#define LDR_PIN 34    // LDR analog input pin

// Person code: 10802468
// Deep sleep time X[s] = [(68 mod 50) + 5] / 10 = 2.3s = 2300000 us
const uint64_t DEEP_SLEEP_US = 2300000ULL;

// Broadcast MAC address used as sink address
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

esp_now_peer_info_t peerInfo;

// Variables used to measure transmission time and status
unsigned long txEnd = 0;
bool sendDone = false;
esp_now_send_status_t sendStatus;

void OnDataSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status) {
  txEnd = micros();          // store transmission end time
  sendDone = true;           // mark transmission as completed
  sendStatus = status;       // save transmission result
}

// Initialize ESP-NOW and add broadcast peer
bool initEspNow() {
  WiFi.mode(WIFI_STA);

  esp_now_init();
  // Send callback
  esp_now_register_send_cb(OnDataSent);

  // Peer registration
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  // Add peer
  esp_now_add_peer(&peerInfo);

  return true;
}

void printDebugInfo(const String& message,
  unsigned long wakeUp,
  unsigned long sensorReadStart, unsigned long sensorReadEnd,
  unsigned long msgBuildStart, unsigned long msgBuildEnd, 
  unsigned long wifiOnStart, unsigned long wifiOnEnd,
  unsigned long txStart, unsigned long beforeSleep){

  unsigned long T_SensorRead = sensorReadEnd - sensorReadStart;
  unsigned long T_MsgBuild = msgBuildEnd - msgBuildStart;
  unsigned long T_WiFiOn = wifiOnEnd - wifiOnStart;
  unsigned long T_Tx = txEnd - txStart;
  unsigned long T_TotalAwake = beforeSleep - wakeUp;

  unsigned long T_Idle = T_TotalAwake - (
    T_SensorRead + 
    T_MsgBuild +
    T_WiFiOn + 
    T_Tx
  );

  Serial.println("----- WAKE UP -----");

  Serial.print("Message: ");
  Serial.println(message);

  Serial.print("Idle_1 = ");
  Serial.println(sensorReadStart - wakeUp);

  Serial.println("Sensor reading");
  Serial.print("T_SensorRead [us] = ");
  Serial.println(T_SensorRead);

  Serial.println("Message building");
  Serial.print("T_MsgBuild [us] = ");
  Serial.println(T_MsgBuild);

  Serial.print("Idel_2 = ");
  Serial.println(wifiOnStart - msgBuildEnd);

  Serial.print("T_WiFiOn [us] = ");
  Serial.println(T_WiFiOn);

  Serial.println("Message transmitting");
  Serial.print("T_Tx [us] = ");
  Serial.println(T_Tx);

  Serial.print("Idle_3 = ");
  Serial.println(beforeSleep - txEnd);

  Serial.print("T_Idle [us] = ");
  Serial.println(T_Idle);

  Serial.print("Send Status: ");
  Serial.println(sendStatus == ESP_NOW_SEND_SUCCESS ? "Ok" : "Error");

  Serial.println("----- GOING TO DEEP SLEEP -----");
}

// Enter deep sleep and wake up again after 2.3 seconds
void goToDeepSleep(){
  Serial.flush();
  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_US);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);

  unsigned long wakeUp = micros();

  // Measure sensor reading time
  unsigned long sensorReadStart = micros();
  int motion = digitalRead(PIR_PIN);   // read motion state from PIR
  int lightValue = analogRead(LDR_PIN); // read ambient light from LDR
  unsigned long sensorReadEnd = micros();

  String message;

  // Build message in required format
  unsigned long msgBuildStart = micros();
  if (motion == HIGH) {
    message = "MOTION_DETECTED-LUMINOSITY:" + String(lightValue);
  } else {
    message = "MOTION_NOT_DETECTED-LUMINOSITY:" + String(lightValue);
  }
  unsigned long msgBuildEnd = micros();

  // Initialize ESP-NOW
  unsigned long wifiOnStart = micros();
  if (!initEspNow()) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  unsigned long wifiOnEnd = micros();

  // Send message via ESP-NOW
  unsigned long txStart = micros();
  esp_now_send(broadcastAddress, (uint8_t*)message.c_str(), message.length() + 1);

  // Wait until sending is complete or timeout after 500 ms
  unsigned long waitStart = millis();
  while (!sendDone && (millis() - waitStart < 500)) {
    delay(1);
  }

  unsigned long beforeSleep = micros();

  // Print measured values
  printDebugInfo(message, wakeUp, sensorReadStart, sensorReadEnd,
  msgBuildStart, msgBuildEnd, wifiOnStart, wifiOnEnd, 
  txStart, beforeSleep);

  // Enter deep sleep
  goToDeepSleep();
}

void loop() {
  // Empty because all operations are done in setup() after each wake-up
}