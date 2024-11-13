#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

const int micPin = 34;
const int buttonPin = 4;
unsigned long lastMinute = 0;
unsigned long lastPressTime = 0;
int breathCount = 0;
int minuteCounts[10] = {0};
int currentMinute = 0;
bool systemStarted = false;
bool messageDisplayed = false;
int buttonPresses = 0;
int totalMinute = 0; 

// Global copy of slave
esp_now_peer_info_t slave;

#define CHANNEL 1
#define PRINTSCANRESULTS 0
#define DELETEBEFOREPAIR 0

void IRAM_ATTR buttonPressed() {
  unsigned long currentTime = millis();
  if (currentTime - lastPressTime > 50) { // 50 ms debounce
    buttonPresses++;
    lastPressTime = currentTime;
  }
}

void InitESPNow() {
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  } else {
    Serial.println("ESPNow Init Failed");
    ESP.restart();
  }
}

void ScanForSlave() {
  int16_t scanResults = WiFi.scanNetworks(false, false, false, 300, CHANNEL);
  bool slaveFound = false;
  memset(&slave, 0, sizeof(slave));

  if (scanResults == 0) {
    Serial.println("No WiFi devices in AP Mode found");
  } else {
    for (int i = 0; i < scanResults; ++i) {
      String SSID = WiFi.SSID(i);
      if (SSID.indexOf("Slave") == 0) {
        Serial.println("Found a Slave.");
        int mac[6];
        if (6 == sscanf(WiFi.BSSIDstr(i).c_str(), "%x:%x:%x:%x:%x:%x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5])) {
          for (int ii = 0; ii < 6; ++ii) {
            slave.peer_addr[ii] = (uint8_t)mac[ii];
          }
        }

        slave.channel = CHANNEL;
        slave.encrypt = 0;
        slaveFound = true;
        break;
      }
    }
  }

  if (slaveFound) {
    Serial.println("Slave Found, processing..");
  } else {
    Serial.println("Slave Not Found, trying again.");
  }

  WiFi.scanDelete();
}

bool manageSlave() {
  if (slave.channel == CHANNEL) {
    if (DELETEBEFOREPAIR) {
      esp_now_del_peer(slave.peer_addr);
    }

    bool exists = esp_now_is_peer_exist(slave.peer_addr);
    if (exists) {
      Serial.println("Already Paired");
      return true;
    } else {
      esp_err_t addStatus = esp_now_add_peer(&slave);
      if (addStatus == ESP_OK) {
        Serial.println("Pair success");
        return true;
      } else {
        Serial.println("Pair failed");
        return false;
      }
    }
  } else {
    Serial.println("No Slave found to process");
    return false;
  }
}

void sendData(float averageBreathsPerMinute) {
  esp_err_t result = esp_now_send(slave.peer_addr, (uint8_t*)&averageBreathsPerMinute, sizeof(averageBreathsPerMinute));
  Serial.print("Send Status: ");
  Serial.println(result == ESP_OK ? "Success" : "Failed");
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Sent to: "); Serial.println(macStr);
  Serial.print("Last Packet Send Status: "); Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {
  pinMode(buttonPin, INPUT);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  Serial.println("ESPNow/Basic/Master Example");
  InitESPNow();
  esp_now_register_send_cb(OnDataSent);
  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonPressed, FALLING); // Tambahkan interrupt
}

void loop() {
  if (buttonPresses > 0 && !systemStarted) {
    
    if (millis() - lastPressTime > 1000) {
      if (buttonPresses == 1) {
        totalMinute = 60;
      } else if (buttonPresses == 2) {
        totalMinute = 40;
      }
      Serial.print("Total menit: ");
      Serial.println(totalMinute);
      buttonPresses = 0; // Reset jumlah penekanan tombol setelah mengatur totalMinute

      systemStarted = true;
      lastMinute = millis();
      breathCount = 0;
      Serial.println("Sistem dimulai!");
      messageDisplayed = false;
    }
  }

  if (systemStarted) {
    int soundLevel = analogRead(micPin);

    // Serial.println(soundLevel);

    if (soundLevel > 2000) {
      Serial.println("Deteksi pernapasan manusia!");
      delay(500);
      breathCount++;
    }

    if (millis() - lastMinute >= 60000) {
      minuteCounts[currentMinute] = breathCount;
      currentMinute++;
      breathCount = 0;
      lastMinute = millis();

      if (currentMinute >= totalMinute) {
        int totalBreaths = 0;
        for (int i = 0; i < totalMinute; i++) {
          totalBreaths += minuteCounts[i];
        }
        float averageBreathsPerMinute = totalBreaths / totalMinute;
        Serial.printf("Rata-rata pernapasan per menit selama %d menit: ", totalMinute);
        Serial.println(averageBreathsPerMinute);

        ScanForSlave();
        if (slave.channel == CHANNEL) {
          bool isPaired = manageSlave();
          if (isPaired) {
            sendData(averageBreathsPerMinute);
          } else {
            Serial.println("Slave pair failed!");
          }
        }

        currentMinute = 0;
        lastMinute = millis();
        systemStarted = false;
        Serial.println("Selesai...");
        messageDisplayed = false;
      }
    }
  }

  if (!systemStarted && !messageDisplayed) {
    Serial.println("Tekan tombol untuk memulai");
    messageDisplayed = true;
  }
}
