// ------------------- Blynk Template -------------------
#define BLYNK_TEMPLATE_ID "TMPL3tjxnXP0Z"
#define BLYNK_TEMPLATE_NAME "Smart Blood Bank Management System"
#define BLYNK_AUTH_TOKEN "0oyQnOrknmiyT4I6ieOtC4mD3LHXuiYs"

// ------------------- Libraries -------------------
#include "HX711.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <HardwareSerial.h>  // For GSM

// ------------------- Pin Configuration -------------------
#define HX711_DT   32
#define HX711_SCK  33
#define DHTPIN     19
#define DHTTYPE    DHT22
#define LCD_ADDR   0x27
#define LCD_COLS   16
#define LCD_ROWS   2
#define GSM_TX     17   // ESP32 TX → GSM RX
#define GSM_RX     16   // ESP32 RX → GSM TX
#define BUZZER_PIN 18   // Buzzer pin


// ------------------- Objects -------------------
HX711 scale;
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
HardwareSerial gsm(1); // UART1 for GSM

// ------------------- Calibration -------------------
float calibration_factor = -384;  // Adjust after calibration
long zero_offset = 0;

// ------------------- Variables -------------------
int blood_units = 0;
int last_units = 0;
float weight = 0;
float temperature = 0, humidity = 0;
bool hx711_ok = false;
bool dht_ok = false;
bool gsm_ok = false;
bool buzzerActive = false;  // Track buzzer state

// ------------------- WiFi & Blynk -------------------
char ssid[] = "Blood Bank";          
char pass[] = "Blood Bank"; 

// ------------------- LCD Welcome -------------------
unsigned long welcomeStart = 0;
bool welcomeDone = false;

void showWelcomeMessage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Blood Bank Sys");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  welcomeStart = millis();
}

// ------------------- Functions -------------------
void sendSMS(String msg) {
  if (gsm_ok) {
    gsm.println("AT+CMGF=1");  
    delay(500);
    gsm.println("AT+CMGS=\"+916306292091\""); // Replace with your number
    delay(500);
    gsm.print(msg);
    gsm.write(26); // CTRL+Z to send
    delay(2000);
  } else {
    Serial.println("GSM not connected. SMS not sent.");
  }
}

void updateLCD() {
  if (!welcomeDone) {
    if (millis() - welcomeStart < 3000) return;
    welcomeDone = true;
    lcd.clear();
  }

  lcd.clear();
  if (dht_ok) {
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temperature, 0);
    lcd.print("C H:");
    lcd.print(humidity, 0);
    lcd.print("%");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("DHT FAIL");
  }

  if (hx711_ok) {
    lcd.setCursor(0, 1);
    lcd.print("A+: ");
    lcd.print(blood_units);
    lcd.print(" unit");
  } else {
    lcd.setCursor(0, 1);
    lcd.print("HX711 FAIL");
  }
}

void updateBlynk() {
  if (dht_ok) {
    Blynk.virtualWrite(V0, temperature);
    Blynk.virtualWrite(V1, humidity);
  }
  if (hx711_ok) {
    Blynk.virtualWrite(V2, weight);
    Blynk.virtualWrite(V3, blood_units);
  }
  // buzzer status
  Blynk.virtualWrite(V4, buzzerActive ? 1 : 0);
}

// ------------------- Setup -------------------
void setup() {
  Serial.begin(115200);
  Serial.println("Blood Bank System Starting...");

  // LCD
  lcd.init();
  lcd.backlight();
  showWelcomeMessage();

  // HX711
  scale.begin(HX711_DT, HX711_SCK);
  if (scale.is_ready()) {
    scale.set_scale(calibration_factor);
    scale.tare();
    zero_offset = scale.read_average(10);
    hx711_ok = true;
    Serial.println("HX711 connected.");
  } else {
    Serial.println("HX711 not connected!");
    hx711_ok = false;
  }

  // DHT
  dht.begin();
  delay(2000);
  float t = dht.readTemperature();
  if (!isnan(t)) {
    dht_ok = true;
    Serial.println("DHT sensor connected.");
  } else {
    Serial.println("DHT sensor not responding!");
    dht_ok = false;
  }

  // GSM
  gsm.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX);
  delay(1000);
  gsm.println("AT");
  delay(1000);
  if (gsm.available()) {
    String resp = gsm.readString();
    if (resp.indexOf("OK") != -1) {
      gsm_ok = true;
      Serial.println("GSM connected.");
    } else {
      gsm_ok = false;
      Serial.println("GSM not responding!");
    }
  }

  // WiFi + Blynk
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Buzzer setup
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH); // Buzzer OFF initially

  Serial.println("System Ready.");
}

// ------------------- Loop -------------------
void loop() {
  Blynk.run();

  // Read HX711
  if (hx711_ok && scale.is_ready()) {
    long raw = scale.read_average(10);
    weight = scale.get_units(5);
    if (weight < 0) weight = 0;
    blood_units = round(weight / 350.0);

    Serial.print("HX711 Raw: ");
    Serial.print(raw);
    Serial.print(" | Weight: ");
    Serial.print(weight, 2);
    Serial.print(" g | Units: ");
    Serial.println(blood_units);

    if (blood_units != last_units) {
      String msg = "Blood Bank Update: A+ Units = " + String(blood_units);
      sendSMS(msg);
      Serial.println(msg);
      last_units = blood_units;
    }
  } else {
    Serial.println("HX711 not connected or not ready.");
  }

  // Read DHT
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) {
    temperature = t;
    humidity = h;
    dht_ok = true;
  } else {
    Serial.println("DHT read failed.");
    dht_ok = false;
  }

  // 🔔 Buzzer + SMS + Blynk logic
  if (dht_ok) {
    if (temperature < 15.0 || temperature > 40.0) {
      digitalWrite(BUZZER_PIN, LOW);  // buzzer ON
      if (!buzzerActive) {
        sendSMS("ALERT: Temperature critical! " + String(temperature) + " C");
        buzzerActive = true;
      }
    } else {
      digitalWrite(BUZZER_PIN, HIGH); // buzzer OFF
      if (buzzerActive) {
        sendSMS("INFO: Temperature back to normal: " + String(temperature) + " C");
        buzzerActive = false;
      }
    }
  }

  // Update outputs
  updateLCD();
  updateBlynk();

  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.print(" C | Hum: ");
  Serial.print(humidity);
  Serial.println(" %");

  delay(100);
}
