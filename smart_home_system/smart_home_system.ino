#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <Keypad.h>
#include <SPI.h>
#include <MFRC522.h>
#include <RTClib.h>

// 1. PIN CONFIGURATION (MATCHED EXACTLY TO YOUR HARDWARE)
const int tempPin = A0;     // Thermistor Module AO
const int flamePin = 4;     // Photodiode Voltage Divider Junction
const int pirPin = 2;       // PIR Motion Sensor OUT
const int buzzerPin = 8;    // Active Buzzer Positive (+)
const int servoPin = 9;     // Servo Motor Signal (Orange/Yellow)

// RFID RC522 Pins
const int rfidRST = 3;      
const int rfidSS = 10;      

// 4x4 Matrix Keypad Ribbon Pins (Left to Right: 1 to 8)
const byte ROWS = 4; 
const byte COLS = 4; 
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {5, 6, 7, A1};   // Keypad Pins 1, 2, 3, 4
byte colPins[COLS] = {A2, A3, 0, 1};  // Keypad Pins 5, 6, 7, 8

// 2. MODULE INSTANTIATIONS
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
MFRC522 rfid(rfidSS, rfidRST);
RTC_DS3231 rtc;
Servo doorServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// 3. SYSTEM VARIABLES
const String correctPIN = "1234";
String inputPIN = "";
bool doorUnlocked = false;
unsigned long doorUnlockTime = 0;
unsigned long lastDisplayUpdate = 0;

void setup() {
  // Safe pin initialization
  pinMode(pirPin, INPUT);
  pinMode(flamePin, INPUT); 
  pinMode(buzzerPin, OUTPUT);
  
  doorServo.attach(servoPin);
  doorServo.write(0); // Ensure door starts locked (0 degrees)
  
  // Initialize communication buses
  SPI.begin();     
  rfid.PCD_Init(); 
  rtc.begin();     
  
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("SMART HOME V3.0");
  delay(2000);
  lcd.clear();
}

void loop() {
  int motionState = digitalRead(pirPin);
  int flameState = digitalRead(flamePin); // Voltage divider drops to LOW when fire conducts
  DateTime now = rtc.now();

  // ==========================================
  // PRIORITY 1: SAFETY FIRE OVERRIDE
  // ==========================================
  if (flameState == LOW) { 
    doorServo.write(90); // Safety fail-safe: automatically unlock door for evacuation
    lcd.setCursor(0, 0);
    lcd.print("** FIRE ALERT **");
    lcd.setCursor(0, 1);
    lcd.print("EVACUATE HOUSE  ");
    digitalWrite(buzzerPin, HIGH); delay(100);
    digitalWrite(buzzerPin, LOW);  delay(100);
    return; // Halt all other code execution during fire emergency
  }

  // ==========================================
  // PRIORITY 2: TIME-BASED INTRUDER SECURITY
  // ==========================================
  if (motionState == HIGH && !doorUnlocked) {
    // Night hours arming condition: 10:00 PM (22) to 6:00 AM (6)
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

  // ==========================================
  // PRIORITY 3: AUTOMATIC DOOR CLOSURE TIMER
  // ==========================================
  if (doorUnlocked && (millis() - doorUnlockTime > 5000)) {
    doorServo.write(0); // Secure the lock after 5 seconds
    doorUnlocked = false;
    inputPIN = "";
    lcd.clear();
  }

  // ==========================================
  // PRIORITY 4: CONTACTLESS RFID AUTHENTICATION
  // ==========================================
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
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

  // ==========================================
  // PRIORITY 5: MANUAL KEYPAD AUTHENTICATION
  // ==========================================
  char key = keypad.getKey();
  if (key) {
    if (key == '#') { // '#' acts as Clear/Reset input
      inputPIN = "";
      lcd.clear();
    } else if (key == '*' || key == 'A' || key == 'B' || key == 'C' || key == 'D') {
      // Ignore functional/alpha keys to prevent incorrect password entry
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

  // ==========================================
  // PRIORITY 6: STATUS DASHBOARD REFRESH
  // ==========================================
  if (millis() - lastDisplayUpdate > 500 && !doorUnlocked) {
    lastDisplayUpdate = millis();
    
    // Read Thermistor values from A0 and parse Steinhart-Hart equation parameters
    int rawAnalog = analogRead(tempPin);
    float resistance = (1023.0 / (float)rawAnalog) - 1.0;
    resistance = 10000.0 / resistance;
    float tempC = resistance / 10000.0;
    tempC = log(tempC); tempC /= 3950.0; tempC += 1.0 / (25.0 + 273.15);
    tempC = 1.0 / tempC; tempC -= 273.15;

    // Line 1: Real-time clock output & parsed temperature values
    lcd.setCursor(0, 0);
    if(now.hour() < 10) lcd.print("0");
    lcd.print(now.hour());
    lcd.print(":");
    if(now.minute() < 10) lcd.print("0");
    lcd.print(now.minute());
    lcd.print("  T:");
    lcd.print(tempC, 1);
    lcd.print((char)223); // Degree symbol
    lcd.print("C   ");

    // Line 2: Visual feedback for keypad buffer string
    lcd.setCursor(0, 1);
    lcd.print("PIN: ");
    lcd.print(inputPIN);
    for(int i = inputPIN.length(); i < 4; i++) {
      lcd.print("_");
    }
    lcd.print("        ");
  }
}
