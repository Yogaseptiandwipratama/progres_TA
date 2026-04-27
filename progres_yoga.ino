#include <WiFi.h>
#include <ThingSpeak.h>
#include <BH1750.h>
#include <Wire.h>
#include <DHT.h>

// ================= WIFI =================
const char* ssid = "Garam";
const char* password = "yoga1234";

// ================= THINGSPEAK =================
unsigned long channelID = 3345267;
const char* writeAPIKey = "GDFD6ZN0V45T6WQR";

// ================= PIN =================
#define SOIL_PIN 34
#define RAIN_PIN 35
#define RELAY_PIN 26
#define DHT_PIN 4

// ================= SENSOR =================
#define DHTTYPE DHT22
DHT dht(DHT_PIN, DHTTYPE);
BH1750 lightMeter;

WiFiClient client;

// ================= VAR =================
int soilValue, rainValue;
float temp, hum, lux;

// ================= FILTER =================
int readSoil() {
  long total = 0;
  for (int i = 0; i < 20; i++) {
    total += analogRead(SOIL_PIN);
    delay(2);
  }
  return total / 20;
}

// ================= THRESHOLD STABIL =================
// 🔥 SET SESUAI DATA REAL KAMU
int soilDryThreshold  = 1300; // di atas ini = kering
int soilWetThreshold  = 1100; // di bawah ini = basah

// ================= RELAY =================
bool relayState = false;
unsigned long pumpStartTime = 0;
unsigned long pumpDuration = 10000; // 10 detik

// ================= TIMER =================
unsigned long lastRead = 0;
unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== SYSTEM STABLE MODE ===");

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  dht.begin();
  Wire.begin();

  if (lightMeter.begin()) {
    Serial.println("BH1750 OK");
  } else {
    Serial.println("BH1750 ERROR");
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  ThingSpeak.begin(client);
}

void loop() {

  unsigned long now = millis();

  // ================= BACA SENSOR =================
  if (now - lastRead >= 2000) {
    lastRead = now;

    soilValue = readSoil();
    rainValue = analogRead(RAIN_PIN);

    temp = dht.readTemperature();
    hum = dht.readHumidity();
    lux = lightMeter.readLightLevel();

    if (isnan(temp) || isnan(hum)) return;

    // ================= DEBUG =================
    Serial.print("RAW: "); Serial.print(soilValue);
    Serial.print(" | Temp: "); Serial.print(temp);
    Serial.print(" | Hum: "); Serial.println(hum);

    // ================= LOGIKA STABIL (HYSTERESIS) =================

    // kondisi kering → nyalakan pompa
    if (!relayState && soilValue > soilDryThreshold) {
      relayState = true;
      pumpStartTime = millis();
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("POMPA ON (KERING)");
    }

    // kondisi basah → matikan pompa
    if (relayState && soilValue < soilWetThreshold) {
      relayState = false;
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("POMPA OFF (BASAH)");
    }
  }

  // ================= TIMER POMPA =================
  if (relayState && (millis() - pumpStartTime >= pumpDuration)) {
    relayState = false;
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("POMPA OFF (TIMEOUT)");
  }

  // ================= KIRIM DATA =================
  if (now - lastSend >= 15000) {
    lastSend = now;

    ThingSpeak.setField(1, soilValue); // 🔥 kirim RAW (lebih jujur)
    ThingSpeak.setField(2, temp);
    ThingSpeak.setField(3, hum);
    ThingSpeak.setField(4, lux);
    ThingSpeak.setField(5, rainValue);

    ThingSpeak.writeFields(channelID, writeAPIKey);
  }
}