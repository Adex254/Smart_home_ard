#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <Keypad.h>
#include <SPI.h>
#include <MFRC522.h>
#include <RTClib.h>

// Pin Configurations
const int pirPin = 2;       
const int flamePin = 4;     
const int buzzerPin = 8;    
const int servoPin = 9;     
const int tempPin = A0;     

// RFID Pins (SPI Bus requires specific pins on Uno)
const int rfidSS = 10;
const int rfidRST = 3;

// Keypad Configuration
const byte ROWS = 4; 
const byte COLS = 4; 
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
// Repinned to make room for RFID SPI pins (11, 12, 13)
byte rowPins[ROWS] = {5, 6, 7, A1}; 
byte colPins[COLS] = {A2, A3, 0, 1}; // Using pins 0 and 1 safely for scanning  

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
MFRC522 rfid(rfidSS, rfidRST);
RTC_DS3231 rtc;
Servo doorServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// System Variables
const String correctPIN = "1234";
String inputPIN = "";
bool doorUnlocked = false;
unsigned long doorUnlockTime = 0;
unsigned long lastDisplayUpdate = 0;

void setup() {
  pinMode(pirPin, INPUT);
  pinMode(flamePin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  
  doorServo.attach(servoPin);
  doorServo.write(0); 
  
  SPI.begin();     // Init SPI bus for RFID
  rfid.PCD_Init(); // Init RFID reader
  rtc.begin();     // Init Real-Time Clock
  
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("ULTIMATE SMART");
  lcd.setCursor(0,1);
  lcd.print("HOME SYSTEM");
  delay(2000);
  lcd.clear();
  
  // Optional: Set RTC time if it lost battery power (Year, Month, Day, Hour, Min, Sec)
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

void loop() {
  int flameState = digitalRead(flamePin);
  int motionState = digitalRead(pirPin);
  DateTime now = rtc.now();

  // 1. CRITICAL SAFETY OVERRIDE (FIRE)
  if (flameState == LOW) { 
    lcd.setCursor(0, 0);
    lcd.print("** FIRE ALERT **");
    lcd.setCursor(0, 1);
    lcd.print("EVACUATE HOUSE  ");
    digitalWrite(buzzerPin, HIGH); delay(100);
    digitalWrite(buzzerPin, LOW);  delay(100);
    return; 
  }

  // 2. INTRUDER ALARM LOGIC (Armed between 10PM and 6AM)
  if (motionState == HIGH && !doorUnlocked) {
    if (now.hour() >= 22 || now.hour() < 6) {
      lcd.setCursor(0, 0);
      lcd.print("INTRUDER ALERT! ");
      lcd.setCursor(0, 1);
      lcd.print("SYSTEM BREACHED ");
      digitalWrite(buzzerPin, HIGH);
      return;
    }
  } else if (motionState == LOW && !doorUnlocked) {
    digitalWrite(buzzerPin, LOW);
  }

  // 3. DOOR LOCK TIMEOUT
  if (doorUnlocked && (millis() - doorUnlockTime > 5000)) {
    doorServo.write(0); 
    doorUnlocked = false;
    inputPIN = "";
    lcd.clear();
  }

  // 4. RFID SWIPE DETECTION
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    // Found a card! Open the door
    lcd.clear();
    lcd.print("CARD VERIFIED");
    lcd.setCursor(0,1);
    lcd.print("WELCOME ADEOYE!");
    doorServo.write(90);
    doorUnlocked = true;
    doorUnlockTime = millis();
    digitalWrite(buzzerPin, HIGH); delay(150); digitalWrite(buzzerPin, LOW);
    rfid.PICC_HaltA();
  }

  // 5. KEYPAD PIN DETECTION
  char key = keypad.getKey();
  if (key) {
    if (key == '#') {
      inputPIN = "";
      lcd.clear();
    } else {
      inputPIN += key;
      if (inputPIN.length() == 4) {
        if (inputPIN == correctPIN) {
          lcd.clear();
          lcd.print("ACCESS GRANTED");
          doorServo.write(90);
          doorUnlocked = true;
          doorUnlockTime = millis();
          digitalWrite(buzzerPin, HIGH); delay(150); digitalWrite(buzzerPin, LOW);
        } else {
          lcd.clear();
          lcd.print("WRONG PIN!");
          digitalWrite(buzzerPin, HIGH); delay(500); digitalWrite(buzzerPin, LOW);
          inputPIN = "";
          lcd.clear();
        }
      }
    }
  }

  // 6. DASHBOARD METRICS DISPLAY
  if (millis() - lastDisplayUpdate > 500 && !doorUnlocked) {
    lastDisplayUpdate = millis();
    
    // Read Thermistor Temperature
    int rawAnalog = analogRead(tempPin);
    float resistance = (1023.0 / (float)rawAnalog) - 1.0;
    resistance = 10000.0 / resistance;
    float tempC = resistance / 10000.0;
    tempC = log(tempC); tempC /= 3950.0; tempC += 1.0 / (25.0 + 273.15);
    tempC = 1.0 / tempC; tempC -= 273.15;

    // Line 1: Clock & Temperature
    lcd.setCursor(0, 0);
    if(now.hour() < 10) lcd.print("0");
    lcd.print(now.hour());
    lcd.print(":");
    if(now.minute() < 10) lcd.print("0");
    lcd.print(now.minute());
    lcd.print("  T:");
    lcd.print(tempC, 1);
    lcd.print((char)223);
    lcd.print("C   ");

    // Line 2: PIN Entry Mask
    lcd.setCursor(0, 1);
    lcd.print("PIN: ");
    lcd.print(inputPIN);
    for(int i = inputPIN.length(); i < 4; i++) {
      lcd.print("_");
    }
    lcd.print("        ");
  }
}
