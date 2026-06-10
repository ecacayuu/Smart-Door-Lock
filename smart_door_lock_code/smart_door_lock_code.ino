#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 5
#define RST_PIN 22

MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(115200);

  SPI.begin();

  rfid.PCD_Init();

  Serial.println("Tempelkan kartu RFID...");
}

void loop() {

  // cek kartu baru
  if (!rfid.PICC_IsNewCardPresent())
    return;

  // baca kartu
  if (!rfid.PICC_ReadCardSerial())
    return;

  Serial.print("UID Kartu: ");

  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i], HEX);
    Serial.print(" ");
  }

  Serial.println();

  delay(1000);
}