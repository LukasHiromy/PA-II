#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define CHANNEL 1

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int pulsePin = 36;
const int buttonPin = 4;
bool systemStarted = false;
bool messageDisplayed = false;
unsigned long startTime = 0;
unsigned long lastMinute = 0;
unsigned long lastPressTime = 0;
int beatCount = 0;
int currentMinute = 0;
int minuteCounts[480];
float averageBreathsPerMinute = 0;
int buttonPresses = 0;
int totalMinute = 0;

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

void configDeviceAP() {
  const char *SSID = "Slave_1";
  bool result = WiFi.softAP(SSID, "Slave_1_Password", CHANNEL, 0);
  if (!result) {
    Serial.println("AP Config failed.");
  } else {
    Serial.println("AP Config Success. Broadcasting with AP: " + String(SSID));
    Serial.print("AP CHANNEL "); Serial.println(WiFi.channel());
  }
}

// Ubah fungsi OnDataRecv agar sesuai dengan definisi callback yang baru
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *data, int data_len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x", info->src_addr[0], info->src_addr[1], info->src_addr[2], info->src_addr[3], info->src_addr[4], info->src_addr[5]);
  Serial.print("Last Packet Recv from: "); Serial.println(macStr);
  if (data_len == sizeof(float)) {
    memcpy(&averageBreathsPerMinute, data, sizeof(averageBreathsPerMinute));
  } else {
    Serial.println("Received data length mismatch.");
  }
}

void printDataMaster(){
  lcd.setCursor(0, 0);
  lcd.print("RPM : ");
  lcd.setCursor(5, 0);
  lcd.print(averageBreathsPerMinute);
}

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT);
  lcd.begin();
  lcd.backlight();
  Serial.println("ESPNow/Basic/Slave Example");
  WiFi.mode(WIFI_AP);
  configDeviceAP();
  Serial.print("AP MAC: "); Serial.println(WiFi.softAPmacAddress());
  InitESPNow();
  esp_now_register_recv_cb(OnDataRecv);
  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonPressed, FALLING); // Tambahkan interrupt
}

void loop() {
  if (buttonPresses > 0 && !systemStarted) {
    if (millis() - lastPressTime > 1000) { // Deteksi penekanan tombol selesai setelah 1 detik
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
      startTime = millis();
      beatCount = 0;
      currentMinute = 0;
      Serial.println("Sistem dimulai!");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Status: Periksa");
      lcd.setCursor(0, 1);
      lcd.print("Timer : ");
      lcd.print(totalMinute);
      lcd.print(" min");
      delay(500);
      messageDisplayed = false;
    }
  }

  if (systemStarted) {
    unsigned long currentTime = millis();
    int pulseVal = analogRead(pulsePin);

    // Serial.println(pulseVal);
    if (pulseVal > 2000) {
      Serial.println("ada denyut jantung");
      beatCount++;
      delay(300);
    }

    if (currentTime - lastMinute >= 60000) { // 60000 milliseconds = 1 minute
      minuteCounts[currentMinute] = beatCount;
      currentMinute++;
      beatCount = 0;
      lastMinute = currentTime;

      if (currentMinute >= totalMinute) {
        int totalBeats = 0;
        for (int i = 0; i < totalMinute; i++) {
          totalBeats += minuteCounts[i];
        }
        float averageBeatsPerMinute = totalBeats / (float)totalMinute;
        Serial.printf("Rata-rata denyut jantung per menit selama %d menit: ", totalMinute);
        Serial.println(averageBeatsPerMinute);

        currentMinute = 0;
        systemStarted = false;
        Serial.println("Selesai...");
        delay(10000);

        lcd.clear();
        printDataMaster();
        lcd.setCursor(0, 1);
        lcd.print("BPM: ");
        lcd.print(averageBeatsPerMinute);
        delay(30000);

        if (averageBreathsPerMinute >= 12 && averageBreathsPerMinute <= 16 && averageBeatsPerMinute >= 40 && averageBeatsPerMinute <= 50) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Status: Normal");
          Serial.println("Status: Normal");
        } else if ((averageBreathsPerMinute < 12 || averageBreathsPerMinute > 16) && (averageBeatsPerMinute >= 40 && averageBeatsPerMinute <= 50)) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Status: Terindikasi");
          lcd.setCursor(0, 1);
          lcd.print("Karena RPM");
          Serial.println("Status: Terindikasi RPM");
        } else if ((averageBreathsPerMinute >= 12 && averageBreathsPerMinute <= 16) && (averageBeatsPerMinute < 40 || averageBeatsPerMinute > 50)) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Status: Terindikasi");
          lcd.setCursor(0, 1);
          lcd.print("Karena BPM");
          Serial.println("Status: Terindikasi BPM");
        } else if ((averageBreathsPerMinute < 12 || averageBreathsPerMinute > 16) && (averageBeatsPerMinute < 40 || averageBeatsPerMinute > 50)) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Status: Terindikasi");
          lcd.setCursor(0, 1);
          lcd.print("Karena RPM & BPM");
          Serial.println("Status: Terindikasi RPM & BPM");
        }
        
        delay(60000);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Selesai, ulang?");
        lcd.setCursor(0, 1);
        lcd.print("Tekan Tombol");
        lcd.clear();
        messageDisplayed = false;
      }
    }
  }

  if (!systemStarted && !messageDisplayed) {
    Serial.println("Tekan tombol untuk memulai");
    lcd.setCursor(0, 0);
    lcd.print("Tekan tombol");
    lcd.setCursor(0, 1);
    lcd.print("untuk memulai");
    messageDisplayed = true;
  }

  delay(100);
}
