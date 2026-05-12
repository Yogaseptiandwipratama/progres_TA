#include <WiFi.h>
#include <ThingSpeak.h>
#include <BH1750.h>
#include <Wire.h>
#include <DHT.h>

// ================= WIFI =================
const char* ssid = "12345678";
const char* password = "00000000";

// ================= THINGSPEAK =================
unsigned long channelID = 3345267;
const char* writeAPIKey = "GDFD6ZN0V45T6WQR";

// ================= PIN =================
#define SOIL_PIN   34
#define RAIN_PIN   35
#define RELAY_PIN  26
#define DHT_PIN    4

// ================= SENSOR =================
#define DHTTYPE DHT22

DHT dht(DHT_PIN, DHTTYPE);
BH1750 lightMeter;

WiFiClient client;

// ================= VAR =================
int soilValue;
int rainValue;
int soilPercent;

float temp;
float hum;
float lux;

// ================= RELAY =================
bool relayState = false;

// ================= TIMER =================
unsigned long lastRead = 0;
unsigned long lastSend = 0;

// ================= BACA SOIL =================
int readSoil() {

  long total = 0;

  for (int i = 0; i < 10; i++) {

    total += analogRead(SOIL_PIN);

    delay(2);
  }

  return total / 10;
}

// ================= SIGMOID =================
float sigmoid(float x) {

  return 1.0 / (1.0 + exp(-x));
}

// ================= ANN =================
float ANN(float in0, float in1, float in2) {

  float l0[8];

  l0[0] = sigmoid(in0*1.2930576 + in1*0.60113907 + in2*(-0.31289467) -0.5857859);
  l0[1] = sigmoid(in0*1.0824976 + in1*0.5569962 + in2*(-0.11672764) -0.6708619);
  l0[2] = sigmoid(in0*(-0.20869803) + in1*(-0.6109007) + in2*(0.02103621));
  l0[3] = sigmoid(in0*1.8751119 + in1*0.306551 + in2*(-0.0800264) -0.50028604);
  l0[4] = sigmoid(in0*(-1.1193444) + in1*0.82331926 + in2*(-0.2550045) +1.1003883);
  l0[5] = sigmoid(in0*1.3116686 + in1*0.8306797 + in2*0.84859383 +0.7931869);
  l0[6] = sigmoid(in0*1.9486679 + in1*(-0.06054327) + in2*0.5364226 -0.6815062);
  l0[7] = sigmoid(in0*(-0.97272426) + in1*0.3535269 + in2*0.150887 +1.2556818);

  float l1[4];

  l1[0] = sigmoid(
    l0[0]*(-0.29957956) +
    l0[1]*(-0.46343505) +
    l0[2]*(-0.60556746) +
    l0[3]*(-0.56952596) +
    l0[4]*(0.97323793) +
    l0[5]*(0.73386395) +
    l0[6]*(0.11728249) +
    l0[7]*(1.2747829) +
    0.69239825
  );

  l1[1] = sigmoid(
    l0[0]*(1.4655309) +
    l0[1]*(0.8257919) +
    l0[2]*(-0.2691803) +
    l0[3]*(1.6280375) +
    l0[4]*(-1.5663654) +
    l0[5]*(0.2896724) +
    l0[6]*(1.2779499) +
    l0[7]*(-2.054662) -
    1.0743663
  );

  l1[2] = sigmoid(
    l0[0]*(1.948991) +
    l0[1]*(1.8846134) +
    l0[2]*(-0.39060232) +
    l0[3]*(1.5472399) +
    l0[4]*(-2.130653) +
    l0[5]*(0.1725639) +
    l0[6]*(1.0373484) +
    l0[7]*(-2.1715062) -
    0.594299
  );

  l1[3] = sigmoid(
    l0[0]*(-0.81385165) +
    l0[1]*(-0.92961806) +
    l0[2]*(0.02462828) +
    l0[3]*(-6.1678476) +
    l0[4]*(0.8979901) +
    l0[5]*(0.70569474) +
    l0[6]*(-4.7379694) +
    l0[7]*(0.621388) +
    0.24245876
  );

  float out = sigmoid(
    l1[0]*1.3790678 +
    l1[1]*(-3.3667502) +
    l1[2]*(-4.8156853) +
    l1[3]*(-2.120818) +
    0.61708444
  );

  return out;
}

void setup() {

  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);

  // relay OFF awal
  digitalWrite(RELAY_PIN, HIGH);

  dht.begin();

  Wire.begin();

  lightMeter.begin();

  // ================= WIFI =================
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

    // ================= SOIL =================
    soilValue = readSoil();

    // ================= HUJAN =================
    rainValue = analogRead(RAIN_PIN);

    // ================= DHT =================
    temp = dht.readTemperature();

    hum = dht.readHumidity();

    // ================= CAHAYA =================
    lux = lightMeter.readLightLevel();

    // jika DHT error
    if (isnan(temp) || isnan(hum)) {

      Serial.println("DHT ERROR");

      return;
    }

    // ================= SOIL KE PERSEN =================
    soilPercent = map(soilValue, 2100, 900, 0, 100);

    soilPercent = constrain(soilPercent, 0, 100);

    // ================= NORMALISASI ANN =================
    float soilN = soilPercent / 100.0;
    float tempN = temp / 50.0;
    float humN  = hum / 100.0;

    // ================= ANN =================
    float annOutput = ANN(soilN, tempN, humN);

    // ================= LOGIKA POMPA =================

    // jika hujan -> pompa mati
    if (rainValue < 2000) {

      relayState = false;

      digitalWrite(RELAY_PIN, HIGH);
    }

    // tanah kering -> pompa nyala
    else if (soilPercent <= 20) {

      relayState = true;

      digitalWrite(RELAY_PIN, LOW);
    }

    // tanah basah -> pompa mati
    else if (soilPercent >= 60) {

      relayState = false;

      digitalWrite(RELAY_PIN, HIGH);
    }

    // ================= SERIAL MONITOR =================
    Serial.println("================================");

    Serial.print("SOIL RAW  : ");
    Serial.println(soilValue);

    Serial.print("SOIL %    : ");
    Serial.print(soilPercent);
    Serial.println("%");

    Serial.print("RAIN RAW  : ");
    Serial.println(rainValue);

    // status hujan
    if (rainValue < 2000) {

      Serial.println("CUACA     : HUJAN");
    }
    else {

      Serial.println("CUACA     : TIDAK HUJAN");
    }

    Serial.print("TEMP      : ");
    Serial.print(temp);
    Serial.println(" C");

    Serial.print("HUM       : ");
    Serial.print(hum);
    Serial.println("%");

    Serial.print("LUX       : ");
    Serial.println(lux);

    Serial.print("ANN OUT   : ");
    Serial.println(annOutput);

    // status pompa
    if (relayState) {

      Serial.println("POMPA     : ON");
      Serial.println("KONDISI   : TANAH KERING");
    }
    else {

      Serial.println("POMPA     : OFF");
    }
  }

  // ================= THINGSPEAK =================
  if (now - lastSend >= 15000) {

    lastSend = now;

    ThingSpeak.setField(1, soilPercent);
    ThingSpeak.setField(2, temp);
    ThingSpeak.setField(3, hum);
    ThingSpeak.setField(4, lux);
    ThingSpeak.setField(5, rainValue);

    int status = ThingSpeak.writeFields(channelID, writeAPIKey);

    if (status == 200) {

      Serial.println("ThingSpeak Update Success");
    }
    else {

      Serial.print("ThingSpeak Error : ");
      Serial.println(status);
    }
  }
}