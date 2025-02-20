// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>> SISTEM SMARTDOOR RFID >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> //

#include <EEPROM.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include <SPI.h>
#include <MFRC522.h>

// memory
#define EEPROM_SIZE 512   // Ukuran EEPROM di ESP32 (512 byte)
#define MAX_CARDS 10      // Maksimal kartu yang dapat disimpan
#define CARD_LENGTH 22      // Panjang string ID kartu (misalnya "05 8A 24 ED")

// buzzer
#define BUZZER_PIN 15

// rfid
#define SS_PIN 5
#define RST_PIN 27

// servo
#define SERVO_PIN 13

// lcd i2c
#define SDA_PIN 21
#define SCL_PIN 22

String MASTER_UID = "05 8A 24 ED 27 02 00";  // Kartu Admin
String registeredCards[MAX_CARDS];  // Array untuk kartu yang terdaftar
int cardCount = 0;  // Jumlah kartu yang tersimpan

Servo servo;
LiquidCrystal_PCF8574 lcd(0x27);
MFRC522 rfid(SS_PIN, RST_PIN);
bool doorLocked = true;

void setup() {
    Serial.begin(115200);
    pinMode(BUZZER_PIN, OUTPUT);
    Wire.begin(SDA_PIN, SCL_PIN);
    lcd.begin(16, 2);
    lcd.setBacklight(255);
    SPI.begin();
    rfid.PCD_Init();
    servo.attach(SERVO_PIN, 500, 2500);
    EEPROM.begin(EEPROM_SIZE);
    checkEEPROM();
    // clearEEPROM();
    delay(10);

    lockDoor();
    Serial.println("Sistem Siap!");
}

void loop() {
    lcd.setCursor(2, 0);
    lcd.print("Hai Ganteng!");
    lcd.setCursor(0, 1);
    lcd.print("Tempelin Kartumu");

    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

    String cardID = getCardID();
    Serial.print("Kartu Terbaca: ");
    Serial.println(cardID);
    delay(300);
    lcd.clear();

    if (cardID.substring(1) == MASTER_UID) {
        adminMenu();
    } else if (isRegistered(cardID)) {
        toggleDoor();
    } else {
        lcd.print("Akses Ditolak");
        buzzer_sekali();
        delay(1000);
        lcd.clear();
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
}

// === FUNGSI EEPROM === //

// Menyimpan kartu ke EEPROM
bool registerCard(String cardID) {
    for (int i = 0; i < MAX_CARDS; i++) {
        String storedCard = loadCard(i);
        if (storedCard.length() == 0 || storedCard == " ") { // Cek slot kosong
            saveCard(i, cardID);
            Serial.println("Kartu berhasil disimpan di slot " + String(i));
            return true;
        }
    }
    Serial.println("Memori penuh! Tidak dapat menyimpan kartu.");
    return false;
}

// Menghapus kartu dari EEPROM
void removeCard(String cardID) {
    for (int i = 0; i < MAX_CARDS; i++) {
        if (loadCard(i) == cardID) {
            clearCard(i);
            break;
        }
    }
}

// Fungsi menyimpan kartu ke EEPROM
void saveCard(int index, String cardID) {
    int addr = index * CARD_LENGTH;
    for (int i = 0; i < cardID.length(); i++) {
        EEPROM.write(addr + i, cardID[i]);
    }
    EEPROM.write(addr + cardID.length(), '\0');  // Null terminator
    EEPROM.commit();
}

// Fungsi membaca kartu dari EEPROM
String loadCard(int index) {
    int addr = index * CARD_LENGTH;
    char buffer[CARD_LENGTH];
    for (int i = 0; i < CARD_LENGTH; i++) {
        buffer[i] = EEPROM.read(addr + i);
    }
    return String(buffer);
}

// Menghapus data kartu dari EEPROM
void clearCard(int index) {
    int addr = index * CARD_LENGTH;
    for (int i = 0; i < CARD_LENGTH; i++) {
        EEPROM.write(addr + i, 0);
    }
    EEPROM.commit();
}

// Mengecek apakah kartu sudah terdaftar
bool isRegistered(String cardID) {
    for (int i = 0; i < MAX_CARDS; i++) {
        if (loadCard(i) == cardID) return true;
    }
    return false;
}

void clearEEPROM() {
    for (int i = 0; i < EEPROM_SIZE; i++) {
        EEPROM.write(i, 0);
    }
    EEPROM.commit();
    Serial.println("EEPROM berhasil dikosongkan!");
}

void checkEEPROM() {
    Serial.println("Cek isi EEPROM:");
    for (int i = 0; i < MAX_CARDS; i++) {
        String storedCard = loadCard(i);
        Serial.print("Slot ");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(storedCard);
    }
}


// === FUNGSI RFID & PINTU === //
String getCardID() {
    String ID = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
        ID.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
        ID.concat(String(rfid.uid.uidByte[i], HEX));
    }
    ID.toUpperCase();
    return ID;
}

void toggleDoor() {
    if (doorLocked) unlockDoor();
    else lockDoor();
}

void unlockDoor() {
    lcd.clear();
    lcd.print("Akses Diterima");
    servo.write(160);
    buzzer_sekali();
    doorLocked = false;
    delay(500);
    lcd.clear();
}

void lockDoor() {
    lcd.clear();
    servo.write(70);
    doorLocked = true;
    lcd.print("Pintu Terkunci");
    buzzer_sekali();
    delay(500);
    lcd.clear();
}

// === MODE ADMIN === //
void adminMenu() {
    lcd.clear();
    lcd.print("Admin Mode");
    buzzer_duakali();
    delay(500);
    lcd.clear();
    lcd.print("1. Tambah Kartu");
    lcd.setCursor(0, 1);
    lcd.print("2. Hapus Kartu");
    delay(1500);

    while (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial());
    String newCardID = getCardID();
    lcd.clear();
    if (!isRegistered(newCardID)) {
        if (registerCard(newCardID)) {
            lcd.print("Kartu");
            lcd.setCursor(0, 1);
            lcd.print("Ditambahkan!");
            buzzer_tambahkartu();
        } else {
            lcd.print("Memori Penuh!");
        }
    } else {
        lcd.print("Kartu Dihapus!");
        removeCard(newCardID);
        buzzer_hapuskartu();
    }
    delay(1000);
    lcd.clear();
}

// === FUNGSI BUZZER === //
void buzzer_sekali() {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(150);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
}

void buzzer_duakali() {
    for (int i = 0; i < 2; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(150);
        digitalWrite(BUZZER_PIN, LOW);
        delay(100);
    }
}

void buzzer_tambahkartu() {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(350);
    digitalWrite(BUZZER_PIN, LOW);
}

void buzzer_hapuskartu() {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(250);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
}

// <<<<<<<<<<<<<<<<<<<<<<<<<<<<< AKHIR KODE <<<<<<<<<<<<<<<<<<<<<<<<<<<< //