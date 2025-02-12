#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>

// WiFi credentials
const char* ssid = "popo"; 
const char* password = "123456788"; 

// Google Apps Script ID
const String GAS_ID = "AKfycbwdLNUXYHGMBZxfB555Z8zkPke5XXTcmlex7xmr2CkRRQ5Ks6YJj1epjwWtaB6OlA4w"; 

// Voltage Measurement Parameters
const float V_max = 3.3;
const int ADC_resolution = 4095;
float voltage_scale_factor = 3.97 * (13.2 / 10.39);
float voltage_offset = 0.82;

// VAC Measurement
#define AC_SENSOR_PIN 33  
const float voltage_multiplier = 220.0 / 2.5; 

// Current Measurement (ACS758)
#define AAC_SENSOR_PIN 32  
#define ADCIN_SENSOR_PIN 25  
#define ADCOUT_SENSOR_PIN 26  
const float V_ref = 5.0;  
const float sensitivity_ACS758 = 0.01;  
const float power_factor = 0.85;  // Power Factor สำหรับ Wac

// Create instances for AHT20 and BMP280
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;

// **คำนวณเปอร์เซ็นต์แบตเตอรี่จากแรงดัน Vdcout**
float calculateBatteryPercentage(float Vdcout) {
    if (Vdcout >= 14.6) return 100;
    if (Vdcout <= 10.5) return 0;
    return (Vdcout - 10.5) * (100.0 / (14.6 - 10.5)); 
}

// **สะสมพลังงานที่ใช้ไป (Wh)**
float total_energy_used = 0;
float calculateEnergyUsage(float power, float time_interval) { 
    total_energy_used += (power * time_interval / 3600.0); 
    return total_energy_used;
}

// **คำนวณเวลาที่ใช้งานไปแล้ว**
unsigned long start_time = millis();
float getTimeUsed() {
    return (millis() - start_time) / 1000.0; 
}

// **Initialize sensors**
void initializeSensors() {
  Serial.begin(115200);
  Serial.println("Initializing Sensors...");
  
  if (!aht.begin()) {
    Serial.println("AHT20 not found!");
    while (1) delay(10);
  }

  if (!bmp.begin()) {
    Serial.println("BMP280 not found!");
    while (1) delay(10);
  }

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2, 
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);
}

// **Measure VAC using RMS**
float measureACVoltage() {
  int sampleCount = 1000;
  float sumSquared = 0.0;
  
  for (int i = 0; i < sampleCount; i++) {
    int adcValue = analogRead(AC_SENSOR_PIN);
    float voltage = (adcValue * V_max) / ADC_resolution;
    sumSquared += voltage * voltage;
    delayMicroseconds(500);
  }
  
  float rmsVoltage = sqrt(sumSquared / sampleCount);
  return rmsVoltage * voltage_multiplier;
}

// **Measure DC Voltage**
float measureVoltage(int pin) {
  int ADC_value = analogRead(pin);
  return ((ADC_value * V_max / ADC_resolution) * voltage_scale_factor) + voltage_offset;
}

// **Measure Current using ACS758**
float measureCurrent(int pin) {
  int ADC_value = analogRead(pin);
  float voltage = (ADC_value * V_ref) / ADC_resolution;
  return (voltage - (V_ref / 2)) / sensitivity_ACS758;
}

// **Calculate Power**
float calculateWdcin(float voltage_in, float current_in) {
    return voltage_in * current_in;
}

float calculateWdcout(float voltage_out, float current_out) {
    return voltage_out * current_out;
}

float calculateWac(float vac, float aac) {
    return vac * aac * power_factor;
}

// **Send Data to Google Sheets**
void sendData(float temp, float humidity, float pressure, float voltagein, float voltageout, float vac, 
              float aac, float adcin, float adcout, float wdcin, float wdcout, float wac,
              float batper, float energy, float timeuse) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://script.google.com/macros/s/" + GAS_ID + "/exec?"
                 "temp=" + String(temp, 2) + "&humi=" + String(humidity, 2) +
                 "&pressure=" + String(pressure, 2) + "&Vdcin=" + String(voltagein, 2) +
                 "&Vgpio35=" + String(voltageout, 2) + "&Vac=" + String(vac, 2) +
                 "&Aac=" + String(aac, 2) + "&Adcin=" + String(adcin, 2) +
                 "&Adcout=" + String(adcout, 2) + "&Wdcin=" + String(wdcin, 2) +
                 "&Wdcout=" + String(wdcout, 2) + "&Wac=" + String(wac, 2) +
                 "&Batper=" + String(batper, 2) + "&Energy=" + String(energy, 2) +
                 "&Timeuse=" + String(timeuse, 2);

    Serial.print("Sending data: ");
    Serial.println(url);
    
    http.begin(url);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.println("Data sent successfully!");
    } else {
      Serial.print("Error sending data: ");
      Serial.println(httpResponseCode);
    }
    
    http.end();
  } else {
    Serial.println("WiFi not connected!");
  }
}

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi ");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  // Initialize sensors
  initializeSensors();
}

void loop() {
  float temp, humidity, pressure, voltagein, voltageout, vac, aac, adcin, adcout, wdcin, wdcout, wac, batper, energy, timeuse;

  sensors_event_t hum, temp_event;
  aht.getEvent(&hum, &temp_event);
  temp = temp_event.temperature;
  humidity = hum.relative_humidity;
  pressure = bmp.readPressure() / 100.0F;

  voltagein = measureVoltage(34);
  voltageout = measureVoltage(35);
  vac = measureACVoltage();
  aac = measureCurrent(AAC_SENSOR_PIN);
  adcin = measureCurrent(ADCIN_SENSOR_PIN);
  adcout = measureCurrent(ADCOUT_SENSOR_PIN);

  wdcin = calculateWdcin(voltagein, adcin);
  wdcout = calculateWdcout(voltageout, adcout);
  wac = calculateWac(vac, aac);
  batper = calculateBatteryPercentage(voltageout);
  energy = calculateEnergyUsage(wdcin + wdcout + wac, 5);
  timeuse = getTimeUsed();

  sendData(temp, humidity, pressure, voltagein, voltageout, vac, aac, adcin, adcout, wdcin, wdcout, wac, batper, energy, timeuse);

  Serial.println();
  delay(300000);
}
