#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Ping.h>

// === WiFi CONFIG ===
const char* ssid = "ESP32_WIFI";
const char* password = "esp32project";
const char* serverName = "http://192.168.10.99:8080/data";

// === OLED CONFIG ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// === ULTRASONIC CONFIG ===
#define TRIG_PIN 40
#define ECHO_PIN 37
#define SDA_PIN 17
#define SCL_PIN 18

// === Variabel sistem ===
float distanceCm = 0;
float cpuUsage = 0;
float packetLoss = 0;
float latency = 0;
String statusText = "Normal";

// === TIMER UNTUK SERVER ===
unsigned long lastSend = 0;
int sendInterval = 1000;

// === TIMER UNTUK OLED ===
unsigned long lastSensorRead = 0;
int sensorInterval = 100; // 1 detik 

// === Fungsi membaca jarak (DITAMBAH TIMEOUT) ===
float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // timeout 30ms supaya tidak menunggu lama
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  return duration * 0.034 / 2;
}

// === sistem ===
IPAddress gateway(192,168,10,1); //

void SystemStats() {

  int totalPing = 10;          
  int successPing = 0;
  float totalLatency = 0;

  for(int i=0;i<totalPing;i++){

    if(Ping.ping(gateway,1)){
      successPing++;
      totalLatency += Ping.averageTime();
    }

    delay(100); 
  }

  // =========================
  // LATENCY
  // =========================
  if(successPing > 0){
    latency = totalLatency / successPing;
  }else{
    latency = 999; // jaringan down
  }

  // =========================
  // PACKET LOSS (%)
  // =========================
  packetLoss = ((totalPing - successPing) * 100.0) / totalPing;

  // =========================
  // SIMULASI DAMPAK DDOS (PENTING)
  // =========================
  if(packetLoss > 0){
    latency += random(20,50); // simulasi delay karena congestion
  }

  // =========================
  // CPU ESTIMASI (opsional)
  // =========================
  uint32_t heap = ESP.getFreeHeap();

  cpuUsage = 100 - (heap / 3000);
  cpuUsage += random(-3,3);

  if(cpuUsage < 5) cpuUsage = 5;
  if(cpuUsage > 95) cpuUsage = 95;

  
}
// === Koneksi WiFi ===
void connectWiFi() {

  Serial.print("Menghubungkan ke ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\nWiFi Terhubung!");
  Serial.println(WiFi.localIP());
}

// === Setup ===
void setup() {

  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED gagal start");
    while(true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  connectWiFi();
}

// === LOOP UTAMA ===
void loop() {

  // baca sensor tiap 1 detik
  if (millis() - lastSensorRead > sensorInterval) {

    distanceCm = getDistance();
    if(distanceCm > 40) distanceCm = 40;

    SystemStats();

    lastSensorRead = millis();
  }

  // tampilkan ke OLED langsung
  display.clearDisplay();
  display.setCursor(0,0);

  display.print("IP: ");
  display.println(WiFi.localIP());

  display.print("Jarak: ");
  display.print(distanceCm,1);
  display.println(" cm");

  display.print("CPU: ");
  display.print(cpuUsage);
  display.println(" %");

  display.print("Status: ");
  display.println(statusText);

  display.display();

  // kirim ke server tiap 1 detik
  if (millis() - lastSend > sendInterval) {

    if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type","application/json");

    String postData = "{\"distance\":" + String(distanceCm,1) +
                      ",\"cpu\":" + String(cpuUsage) +
                      ",\"packetLoss\":" + String(packetLoss) +
                      ",\"latency\":" + String(latency) +
                      ",\"status\":\"" + statusText + "\"}";

    // =========================
    // 🔥 DETEKSI BERDASARKAN HTTP DELAY
    // =========================
    unsigned long start = millis();

    int httpResponseCode = http.POST(postData);

    unsigned long duration = millis() - start;

    latency = duration; // ini jadi indikator utama

    // =========================
    // RULE DDOS
    // =========================
    if(latency > 200 || packetLoss > 5){
      statusText = "DDOS";
    }else{
      statusText = "Normal";
    }

    http.end();
  }

    lastSend = millis();
  }

  delay(100);
}