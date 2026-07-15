#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <time.h>
#include <Preferences.h>

// ======================
// WIFI
// ======================

const char* ssid = "nurhayatul";
const char* password = "12345678";

// ======================
// NTP
// ======================

const char* ntpServer = "id.pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;   
const int daylightOffset_sec = 0;

// ======================
// MQTT
// ======================

const char* mqtt_server = "192.168.15.13"; 
const int mqtt_port = 1883;

//======================
// ThingsBoard
//======================

const char* tb_server = "thingsboard.cloud";
const int tb_port = 1883;

const char* tb_token =
"Qwg6W1nnFtvlgGtYZX1M";

WiFiClient wifiMosquitto;
WiFiClient wifiTB;

PubSubClient mqttLocal(wifiMosquitto);
PubSubClient mqttTB(wifiTB);

// ======================
// RFID
// ======================

#define SS_PIN 5
#define RST_PIN 22

MFRC522 rfid(SS_PIN, RST_PIN);

// UID yang terdaftar
String authorizedUID = "166E4887";

// ======================
// SERVO
// ======================

#define SERVO_PIN 13

Servo doorServo;

// ======================
// MQTT TOPIC
// ======================

const char* topicAccess = "smartdoorlock/rfid/akses";
const char* topicControl = "smartdoorlock/control";

const char* tbTelemetry =
"v1/devices/me/telemetry";

Preferences prefs;

// ======================
// WIFI CONNECT
// ======================

void setupWiFi() {

  delay(10);

  Serial.println();
  Serial.print("Menghubungkan WiFi ");

  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi Connected");
  Serial.println(WiFi.localIP());

  configTime(
    gmtOffset_sec,
    daylightOffset_sec,
    ntpServer
  );
}

// ======================
// CALLBACK MQTT
// ======================

void callback(char* topic, byte* payload, unsigned int length) {

  String message = "";

  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Pesan MQTT: ");
  Serial.println(message);

  if (message == "BUKA") {

    Serial.println("Pintu Dibuka Manual");

    doorServo.write(0);
  }

  else if (message == "TUTUP") {

    Serial.println("Pintu Ditutup Manual");

    doorServo.write(90);
  }
}

// ======================
// RECONNECT MQTT
// ======================

void reconnectLocal()
{
    if (mqttLocal.connected())
        return;

    Serial.print("Reconnect MQTT Local... ");

    if (mqttLocal.connect("ESP32_SmartDoorLock"))
    {
        mqttLocal.subscribe(topicControl);
        Serial.println("Berhasil");
    }
    else
    {
        Serial.print("Gagal, kode = ");
        Serial.println(mqttLocal.state());
    }
}

void reconnectTB()
{
    if (mqttTB.connected())
        return;

    Serial.print("Reconnect ThingsBoard... ");

    if (mqttTB.connect("ESP32_TB", tb_token, NULL))
    {
        Serial.println("Berhasil");
    }
    else
    {
        Serial.print("Gagal, kode = ");
        Serial.println(mqttTB.state());
    }
} 

// ======================
// SETUP
// ======================

void setup() {

  Serial.begin(115200);

  mqttLocal.setServer(mqtt_server,mqtt_port);
  mqttTB.setServer(tb_server,tb_port);
  mqttLocal.setCallback(callback);

  prefs.begin("doorlog", false);

  setupWiFi();

  SPI.begin();
  rfid.PCD_Init();

  doorServo.attach(SERVO_PIN);

  // posisi awal terkunci
  doorServo.write(90);

  Serial.println("Smart Door Lock Ready!");
  Serial.println("Tempelkan kartu RFID...");
}

String getTimestamp() {

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return "TIME_ERROR";
  }

  char buffer[25];

  strftime(
    buffer,
    sizeof(buffer),
    "%Y-%m-%d %H:%M:%S",
    &timeinfo
  );

  return String(buffer);
}

void saveOfflineLog(String data)
{
    int total = prefs.getInt("count",0);

    String key = "log" + String(total);

    prefs.putString(key.c_str(), data);

    prefs.putInt("count", total + 1);

    Serial.println("Log disimpan ke memori.");
}

void sendOfflineLogs()
{
    int total = prefs.getInt("count",0);

    if(total==0)
        return;

    Serial.println("Mengirim log offline...");

    for(int i=0;i<total;i++)
    {
        String key="log"+String(i);

        String data=prefs.getString(key.c_str(),"");

        if(data!="")
        {
            mqttLocal.publish(topicAccess,data.c_str());

            mqttTB.publish(tbTelemetry,data.c_str());

            prefs.remove(key.c_str());

            delay(200);
        }
    }

    prefs.putInt("count",0);

    Serial.println("Semua log berhasil dikirim.");
}

// ======================
// LOOP
// ======================

void loop() {

  // ======================
  // KONEKSI MQTT
  // ======================

  if (WiFi.status() == WL_CONNECTED)
  {
    if (!mqttLocal.connected())
      reconnectLocal();

    if (!mqttTB.connected())
      reconnectTB();

    mqttLocal.loop();
    mqttTB.loop();

    // Kirim log offline jika koneksi sudah kembali
    if (mqttLocal.connected() && mqttTB.connected())
    {
      sendOfflineLogs();
    }
  }

  // ======================
  // MENUNGGU KARTU RFID
  // ======================

  if (!rfid.PICC_IsNewCardPresent())
    return;

  if (!rfid.PICC_ReadCardSerial())
    return;

  String cardUID = "";

  for (byte i = 0; i < rfid.uid.size; i++)
  {
    if (rfid.uid.uidByte[i] < 0x10)
      cardUID += "0";

    cardUID += String(rfid.uid.uidByte[i], HEX);
  }

  cardUID.toUpperCase();

  Serial.print("UID Kartu: ");
  Serial.println(cardUID);

  // ======================
  // AKSES DITERIMA
  // ======================

  if (cardUID == authorizedUID)
  {
    Serial.println("AKSES DITERIMA");

    String payload =
      "{"
      "\"security\":\"AMAN\","
      "\"status\":\"AKSES_DITERIMA\","
      "\"pintu\":\"BUKA\","
      "\"uid\":\"" + cardUID + "\","
      "\"timestamp\":\"" + getTimestamp() + "\""
      "}";

    if (WiFi.status() == WL_CONNECTED &&
        mqttLocal.connected() &&
        mqttTB.connected())
    {
      bool localOK = mqttLocal.publish(topicAccess, payload.c_str());
      bool tbOK = mqttTB.publish(tbTelemetry, payload.c_str());

      if (localOK && tbOK)
      {
        Serial.println("Data berhasil dikirim.");
      }
      else
      {
        saveOfflineLog(payload);
        Serial.println("Publish gagal, log disimpan.");
      }
    }
    else
    {
      saveOfflineLog(payload);
      Serial.println("Offline -> log disimpan.");
    }

    // Buka pintu
    doorServo.write(0);
    Serial.println("Pintu Terbuka");

    delay(3000);

    // Tutup pintu
    doorServo.write(90);
    Serial.println("Pintu Tertutup");
  }

  // ======================
  // AKSES DITOLAK
  // ======================

  else
  {
    Serial.println("AKSES DITOLAK");

    String alertPayload =
      "{"
      "\"security\":\"TIDAK_AMAN!\","
      "\"status\":\"AKSES_DITOLAK!\","
      "\"pintu\":\"TETAP_TERKUNCI\","
      "\"uid\":\"" + cardUID + "\","
      "\"timestamp\":\"" + getTimestamp() + "\""
      "}";

    if (WiFi.status() == WL_CONNECTED &&
        mqttLocal.connected() &&
        mqttTB.connected())
    {
      bool localOK = mqttLocal.publish(topicAccess, alertPayload.c_str());
      bool tbOK = mqttTB.publish(tbTelemetry, alertPayload.c_str());

      if (localOK && tbOK)
      {
        Serial.println("Data berhasil dikirim.");
      }
      else
      {
        saveOfflineLog(alertPayload);
        Serial.println("Publish gagal, log disimpan.");
      }
    }
    else
    {
      saveOfflineLog(alertPayload);
      Serial.println("Offline -> log disimpan.");
    }
  }

  Serial.println("----------------------");

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  delay(1000);
}