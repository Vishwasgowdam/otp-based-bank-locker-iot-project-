/************************************************************
  Project: Biometric and OTP Based Security Locker
************************************************************/

// Libraries:
#include <SoftwareSerial.h>
/* Built-In Library */

#include <Keypad.h>
/* Install: Keypad by Mark Stanley, Alexander Brevig */

#include<Wire.h>
/* Built-In Library */

#include<LiquidCrystal_I2C.h>
/* Add Zip: https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library */

/* Install: "Adafruit Fingerprint Sensor Library by Adafruit" */
#include <Adafruit_Fingerprint.h>

// Pin Numbers: Fingerprint Sensor
#define FP_Rx_Pin 9     //Connect to Tx of Fingerprint Sensor Module
#define FP_Tx_Pin 10    //Connect to Rx of Fingerprint Sensor Module

// Pin Numbers: Sim Module
#define Sim_Rx_Pin 11   // Connect to TX of SIM Module
#define Sim_Tx_Pin 12   // Connect to RX of SIM Module

// Pin Numbers: Buzzer Module
#define Buzzer_Pin 13   // Connect to the I/O pin of Buzzer Module

// Pin Numbers: Relay Module
#define Relay_Pin A0    // Connect to the IN pin of Relay Module

// Configurations: Buzzer Module (Low Level)
#define BuzzerOn LOW
#define BuzzerOff HIGH

// Configurations: LCD Display
const uint8_t I2C_Addr = 0x27;  // I2C Address
const uint8_t lcdNumCols = 16;  // LCD's number of columns
const uint8_t lcdNumRows = 2;   // LCD's number of rows

// Configurations: Keypad
const byte kpNumRows = 4;  // Keypad's number of rows
const byte kpNumCols = 3;  // Keypad's number of columns
char hexaKeys[kpNumRows][kpNumCols] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

byte rowPins[kpNumRows] = {8, 7, 6, 5};   //R1, R2, R3, R4
byte colPins[kpNumCols] = {4, 3, 2};      //C1, C2, C3

// Configurations: Others
const String mobileNumber1 = "7259087343";   //Replace xxxxxxxxxx with 10 digit mobile number 
const unsigned long timeoutDuration = 30000;
const unsigned long relayOnDelay = 5000L;
const int otpLength = 5;

// Software Serial Port: Fingerprint Sensor
SoftwareSerial SerialFP(FP_Rx_Pin, FP_Tx_Pin);

// Software Serial Port: Sim Module
SoftwareSerial SerialSIM(Sim_Rx_Pin, Sim_Tx_Pin);

// Objects: Fingerprint Sensor
Adafruit_Fingerprint objFP = Adafruit_Fingerprint(&SerialFP);

// Objects: LCD Display
LiquidCrystal_I2C lcd(I2C_Addr, lcdNumCols, lcdNumRows);  //0x3F / 0x27

// Objects: Keypad
Keypad kp = Keypad(makeKeymap(hexaKeys), rowPins, colPins, kpNumRows, kpNumCols);

// Variables:
String OTP;
bool flagTimeout;
unsigned long millisB4Entry;
unsigned long elapsedTime;

void setup() {
  // Pin Configurations:
  pinMode(Relay_Pin, OUTPUT);
  digitalWrite(Relay_Pin, LOW);

  pinMode(Buzzer_Pin, OUTPUT);
  digitalWrite(Buzzer_Pin, BuzzerOff);

  randomSeed(analogRead(1));

  // Initialise the LCD display:
  lcd.begin(16,2);
  lcd.backlight();  // turn on backlight.

  /* Begin serial communication with Arduino and Fingerprint sensor */
  objFP.begin(57600);

  if (!objFP.verifyPassword()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Did not find FP ");
    lcd.setCursor(0, 1);
    lcd.print("sensor :(       ");
    while (true);
  }

  /* Begin serial communication with Arduino and SIM Module */
  SerialSIM.begin(9600);
  delay(500);
}

void loop() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Fingerprint     ");
  lcd.setCursor(0, 1);
  lcd.print("Authentication  ");

  if (isValidFingerprintDetected()) {
    Buzzer(1, 100, 0);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sending OTP...  ");
    OTP = String(random(11111, 99999));
    String msgContent = "OTP to unlock your security locker is " + OTP;
    SendMessage(mobileNumber1, msgContent);
    millisB4Entry = millis();
    OTP_Authentication();
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Invalid/Reading ");
    lcd.setCursor(0, 1);
    lcd.print("Failed!         ");
    Buzzer(3, 250, 250);
  }
}

void SendMessage(String mobileNumber, String msgContent) {
  SerialSIM.listen();
  while (!SerialSIM.isListening());

  /* Sets the GSM Module to Text Mode */
  SerialSIM.print("AT+CMGF=1\r");
  delay(1000);

  /* Sets how the modem will response when a SMS is received */
  SerialSIM.println("AT+CNMI=1,2,0,0,0");
  delay(1000);

  String cmdSetMobileNumber = "AT+CMGS=\"+91" + mobileNumber + "\"\r";
  SerialSIM.print(cmdSetMobileNumber);
  delay(500);
  SerialSIM.print(msgContent);
  SerialSIM.write(26);              // ASCII code of CTRL+Z
  delay(500);
}

void OTP_Authentication() {
  bool flag = true;
  while (flag) {
    lcd.clear();
    lcd.print("OTP:            ");
    lcd.setCursor(0, 1);
    lcd.print("*:Clear         ");
    String inputPIN = KeypadInput_Digits(otpLength, 5, 0);
    lcd.clear();
    if (inputPIN == OTP) {
      Buzzer(1, 100, 0);
      lcd.print("UNLOCKED!       ");
      digitalWrite(Relay_Pin, HIGH);
      delay(relayOnDelay);
      Buzzer(3, 100, 100);
      delay(1000);
      digitalWrite(Relay_Pin, LOW);
      flag = false;
    } else if (flagTimeout == false) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Timeout!");
      Buzzer(3, 250, 250);
      flag = false;
    } else {
      lcd.print("Invalid OTP!    ");
      Buzzer(1, 800, 0);
    }
  }
}

String KeypadInput_Digits(int numDigits, int colIdx, int rowIdx) {
  String keyStr = "";
  char keyChar = '\0';

  flagTimeout = true;
  bool isFirst = true;
  while (keyStr.length() < numDigits && flagTimeout) {
    elapsedTime = millis() - millisB4Entry;
    flagTimeout = elapsedTime <= timeoutDuration;

    keyChar = kp.getKey();
    if (isFirst || keyChar == '*') {
      keyStr = "";
      lcd.setCursor(colIdx, rowIdx);
      for (int i = 0; i < numDigits; i++) {
        lcd.print(" ");
      }
      lcd.setCursor(colIdx, rowIdx);
      lcd.blink();
      isFirst = false;
    } else if (keyChar) {
      lcd.print("*");
      keyStr += keyChar;
    }
  }
  lcd.noBlink();
  return keyStr;
}

bool isValidFingerprintDetected() {
  SerialFP.listen();
  while (!SerialFP.isListening());
  while (objFP.getImage() != FINGERPRINT_OK); /* Loop until the image is taken. */
  if (objFP.image2Tz() != FINGERPRINT_OK) return false;
  if (objFP.fingerFastSearch() != FINGERPRINT_OK)  return false;
  return true;
}

void Buzzer(int n, int onDelay, int offDelay) {
  for (int i = 1; i <= n; i++) {
    digitalWrite(Buzzer_Pin, BuzzerOn);
    delay(onDelay);
    digitalWrite(Buzzer_Pin, BuzzerOff);
    delay(offDelay);
  }
}
