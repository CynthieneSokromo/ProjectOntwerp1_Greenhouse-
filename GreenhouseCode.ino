#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal.h>
#include <DHT.h>
#include <ESP32Servo.h>     // === SERVO ===

// === Pin-definities ===
#define soil_moisture_pin 35     // Bodemvocht sensor (ADC)
#define waterSensorPin 34        // Waterniveau sensor (ADC)
#define relayPin 12              // Relay output
#define DHTPIN 19               // DHT11 pin
#define DHTTYPE DHT11
#define adcPin 36               // Potentiometer voor motor snelheid (VERPLAATST naar 36)
#define in1Pin 18               // L293D IN1 (FET 2)
#define in2Pin 23                // L293D IN2 (FET 3)
#define enable1Pin 2          // L293D EN1 (PWM op FET 1)

// === SERVO & BUTTON ===
#define servoPin  15     // servo op pin 15 (zoals jij hebt bedraad)
 // === Function prototypes ===
void driveMotor(bool dir, int speed);
void resetLCD();

Servo myservo;

// === LCD pin-configuratie ===
const int rs = 13, en = 14, d4 = 25, d5 = 27, d6 = 32, d7 = 33;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// === Sensor & Modules ===
DHT dht(DHTPIN, DHTTYPE);
RTC_DS1307 rtc;

// === PWM instellingen ===
const int PWM_CHANNEL = 0;
const int PWM_FREQ = 1000;
const int PWM_RESOLUTION = 11; // 0-2047

// === Overige instellingen ===
const int dryThresholdPercent = 20;       // Pomp aan bij <= 20% vocht
const int waterSensorMin = 0;              // Calibratie water sensor min
const int waterSensorMax = 944;            // Calibratie water sensor max
const float tempThreshold = 24.0;          // Temperatuur grens voor motor aan/uit

unsigned long lastSwitchTime = 0;
const unsigned long switchInterval = 3000; // Wissel display om de 3 seconden
bool showDHT = true;
bool windowOpen = false;


void setup() {
  Serial.begin(115200);
  
  // Init pins
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);  // Relay uit (HIGH = uit)

  pinMode(in1Pin, OUTPUT);
  pinMode(in2Pin, OUTPUT);
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(enable1Pin, PWM_CHANNEL);

  // === BUTTON & SERVO INIT ===

  myservo.attach(servoPin, 500, 2400);
  myservo.write(0); // begin gesloten

  dht.begin();
  Wire.begin(21, 22);  // SDA = 21, SCL = 22

  if (!rtc.begin()) {
    lcd.begin(16, 2);
    lcd.print("RTC niet gevonden");
    Serial.println("RTC niet gevonden");
    while (1);
  }

  if (!rtc.isrunning()) {
    Serial.println("RTC loopt niet, stel tijd in...");
  }

  lcd.begin(16, 2);
  lcd.print("Systeem start...");
  delay(2000);
  lcd.clear();

  Serial.println("=== Systeem gestart ===");
  Serial.println("Button op pin 5 (naar GND). Servo signaal op pin 15. Potentiometer naar pin 36.");
}

void loop() {
  DateTime now = rtc.now();

  // Lees sensoren
  float temp = dht.readTemperature();   // °C
  float hum = dht.readHumidity();
  int soilRaw = analogRead(soil_moisture_pin);
  int soilPercent = map(soilRaw, 0, 4095, 100, 0);
  soilPercent = constrain(soilPercent, 0, 100);

  int waterRaw = analogRead(waterSensorPin);
  int waterPercent = map(waterRaw, waterSensorMin, waterSensorMax, 0, 100);
  waterPercent = constrain(waterPercent, 0, 100);

  // Relay (pomp) aan/uit op basis bodemvochtigheid
  bool pumpOn = (soilPercent <= dryThresholdPercent);
  digitalWrite(relayPin, pumpOn ? LOW : HIGH);

  // Motor aan/uit en snelheid op basis temperatuur en potentiometer
  if (!isnan(temp) && temp > tempThreshold) {
    int potVal = analogRead(adcPin);
    int deviation = abs(potVal - 2048);
    bool rotationDir = potVal > 2048;
    int rotationSpeed = map(deviation, 0, 2047, 0, 2047);
    rotationSpeed = constrain(rotationSpeed, 0, 2047);
    driveMotor(rotationDir, rotationSpeed);

    // === RAAM OPEN ===
   //myservo.write(90);   // open raam
   // SERVO alleen openen als hij nog dicht is
   if (!windowOpen) {
    myservo.write(90);
    windowOpen = true;
   }
  
  } 
  else {
    // Temperatuur laag, motor uitzetten
    driveMotor(true, 0);  // snelheid 0 = motor uit
    // === FAN UIT ===
    // SERVO alleen sluiten als hij open is
   if (windowOpen) {
    myservo.write(0);
    windowOpen = false;

   // === RAAM DICHT ===
    //myservo.write(0);    // sluit raam
   }
  }


  // Seriële debug overige info (elke lus)
  Serial.print(now.timestamp());
  Serial.print(" | Temp: "); Serial.print(temp, 1);
  Serial.print("C | Hum: "); Serial.print(hum, 1);
  Serial.print("% | Soil: "); Serial.print(soilPercent); 
  Serial.print("% | Water: "); Serial.print(waterPercent);
  Serial.print("% | Pump: "); Serial.print(pumpOn ? "ON" : "OFF");
  Serial.print(" | Motor Speed: ");
  if (!isnan(temp) && temp <= tempThreshold) {
    Serial.println(map(abs(analogRead(adcPin) - 2048), 0, 2047, 0, 2047));
  } else {
    Serial.println("OFF");
  }

  // Wissel display elke 3 seconden
  if (millis() - lastSwitchTime >= switchInterval) {
    showDHT = !showDHT;
    lastSwitchTime = millis();
    lcd.clear();
  }

  // Display update
  lcd.setCursor(0, 0);
  //lcd.print("                ");  // Clear line

  // Toon tijd en datum
  lcd.setCursor(0, 0);
  if (now.hour() < 10) lcd.print('0');
  lcd.print(now.hour());
  lcd.print(':');
  if (now.minute() < 10) lcd.print('0');
  lcd.print(now.minute());
  lcd.print(' ');
  if (now.day() < 10) lcd.print('0');
  lcd.print(now.day());
  lcd.print('-');
  if (now.month() < 10) lcd.print('0');
  lcd.print(now.month());
  lcd.print('-');
  lcd.print(now.year());

  lcd.setCursor(0, 1);
  //lcd.print("                ");  // Clear line
  lcd.setCursor(0, 1);

  if (showDHT) {
    // Toon temp & vocht
    lcd.print("T:");
    if (isnan(temp)) lcd.print("--"); else lcd.print(temp, 1);
    lcd.print("C H:");
    if (isnan(hum)) lcd.print("--"); else lcd.print(hum, 1);
    lcd.print("%");
  } else {
    // Toon bodemvocht, water en pompstatus
    lcd.print("S:");
    lcd.print(soilPercent);
    lcd.print("% W:");
    lcd.print(waterPercent);
    lcd.print("% P:");
    lcd.print(pumpOn ? "ON " : "OFF");

    // Motor snelheid (indien ruimte en temp laag)
    if (!isnan(temp) && temp <= tempThreshold) {
      int potVal = analogRead(adcPin);
      int deviation = abs(potVal - 2048);
      bool rotationDir = potVal > 2048;
      int rotationSpeed = map(deviation, 0, 2047, 0, 99);
      if (rotationSpeed > 0) {
        lcd.setCursor(13, 1);
        lcd.print(rotationDir ? ">" : "<");
        lcd.print(rotationSpeed);
      }
    }
  }

  delay(500);
  
}

void driveMotor(bool dir, int speed) {
  if (speed == 0) {
    // Motor uit
    digitalWrite(in1Pin, LOW);
    digitalWrite(in2Pin, LOW);
    ledcWrite(PWM_CHANNEL, 0);
    return;
  }

  if (dir) {
    digitalWrite(in1Pin, HIGH);
    digitalWrite(in2Pin, LOW);
  } else {
    digitalWrite(in1Pin, LOW);
    digitalWrite(in2Pin, HIGH);
  }
  ledcWrite(PWM_CHANNEL, speed);

}

void resetLCD() {
  lcd.begin(16, 2);
  lcd.clear();
}




















/////////////////////////////////////////////////////
//




























