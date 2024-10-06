// Blynk credentials
#define BLYNK_TEMPLATE_ID "TMPL6QLUJTlv6"
#define BLYNK_TEMPLATE_NAME "Air Quality Monitor"
#define BLYNK_AUTH_TOKEN "Tml5McEmZCi-SYYLAmVfFo7k07ullbJB"

// Set password to "" for open networks.
char ssid[] = "HONOR";
char pass[] = "12345678";

#include <Arduino.h>
#include <WiFi.h>  // ESP32 Wi-Fi library
#include <Firebase_ESP_Client.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <BlynkSimpleEsp32.h>

// Your network credentials
#define WIFI_SSID "HONOR"
#define WIFI_PASSWORD "12345678"

// Firebase project API Key and URL
#define API_KEY "************************"
#define DATABASE_URL "************************************"

// Pin assignments
#define DHTPIN 4      // DHT sensor pin
#define DHTTYPE DHT11
#define MQ9_PIN 32    // MQ9 sensor pin
#define MQ135_PIN 33  // MQ135 sensor pin

// Firebase and sensor objects
DHT dht(DHTPIN, DHTTYPE);
FirebaseData fbdo;
FirebaseAuth auth_firebase;
FirebaseConfig config;

// I2C LCD address
LiquidCrystal_I2C lcd(0x27, 16, 2);  // LCD I2C address

// NTP settings
WiFiUDP ntpudp;
NTPClient ntp(ntpudp, "pool.ntp.org", 19800);  // Timezone offset for India (UTC+5:30)

// Blynk and timer objects
BlynkTimer timer;
unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

void myTimerEvent() {
  // Send data every second to Blynk Virtual Pin V2
  Blynk.virtualWrite(V2, millis() / 1000);
}

void setup() {
  // Initialize Serial, Blynk, and Wi-Fi
  Serial.begin(115200);
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  // Initialize Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth_firebase, "", "")) {
    Serial.println("Firebase sign-up successful");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth_firebase);
  Firebase.reconnectWiFi(true);

  dht.begin();
  ntp.begin();

  lcd.begin(16, 2);  // Initialize LCD
  lcd.backlight();   // Turn on LCD backlight

  // Setup Blynk timer to send uptime data to Blynk every second
  timer.setInterval(1000L, myTimerEvent);
}

void loop() {
  Blynk.run();  // Run Blynk
  timer.run();  // Run Blynk timer

  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 30000 || sendDataPrevMillis == 0)) {
    ntp.update();
    sendDataPrevMillis = millis();

    // Get time for Firebase path
    String time = ntp.getFormattedTime();
    time.replace(":", "_");

    // Read sensor values
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    int mq9Value = analogRead(MQ9_PIN);
    int mq135Value = analogRead(MQ135_PIN);

    if (isnan(temp) || isnan(hum)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    String path = "TemperatureHumidity/data_" + time;

    // Send data to Firebase
    Firebase.RTDB.setFloat(&fbdo, path + "/temperature", temp);
    Firebase.RTDB.setFloat(&fbdo, path + "/humidity", hum);
    Firebase.RTDB.setInt(&fbdo, path + "/mq9_gas", mq9Value);
    Firebase.RTDB.setInt(&fbdo, path + "/mq135_gas", mq135Value);

    // Display on LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temp);
    lcd.print("C");
    lcd.setCursor(0, 1);
    lcd.print("Hum: ");
    lcd.print(hum);
    lcd.print("%");
    delay(2000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Carbon Monoxide: ");
    lcd.print(mq9Value);
    lcd.setCursor(0, 1);
    lcd.print("CO2 & NH3: ");
    lcd.print(mq135Value);
    delay(2000);

    // Send data to Blynk app
    Blynk.virtualWrite(V0, temp);       // Send temperature to Blynk
    Blynk.virtualWrite(V1, hum);        // Send humidity to Blynk
    Blynk.virtualWrite(V2, mq9Value);   // Send MQ9 gas value to Blynk
    Blynk.virtualWrite(V3, mq135Value); // Send MQ135 gas value to Blynk
  }
}
