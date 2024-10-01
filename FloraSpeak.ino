#include <Wire.h>
#include <BH1750.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Arduino.h"
#include "Audio.h"

#define SENSOR_PIN 2    // Soil moisture sensor (D2)
#define BH1750_SDA 21   // I2C SDA (D21)
#define BH1750_SCL 22   // I2C SCL (D22)
#define ONE_WIRE_BUS 5  // Temperature sensor data wire (D5)
#define I2S_DOUT 25
#define I2S_LRC  26

Audio audio;
BH1750 lightMeter;
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);

unsigned long previousMillis = 0;  // Store last time sensors were read
unsigned long happyMillis = 0;     // Store last time happy.mp3 was played
const long sensorInterval = 5000;  // Sensor read interval in milliseconds (5 seconds)
const long happyInterval = 50000;  // Interval for happy.mp3 in milliseconds (30 seconds)
bool moistureAlert = false;        // To track whether we are in the "water.mp3" state
bool heatAlert = false;            // To track whether we are in the "garmi.mp3" state
bool lightAlert = false;           // To track whether we are in the "chhao.mp3" state

void setup() {
    Serial.begin(115200);

    // Initialize audio
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(21);  // Set volume 0...21
    
    if (!SPIFFS.begin()) {
        Serial.println("SPIFFS initialization failed!");
        return;
    }

    if (!audio.connecttoFS(SPIFFS, "/intro.mp3")) {
        Serial.println("Initial audio file not found!");
        return;
    }

    // Initialize sensors
    Wire.begin(BH1750_SDA, BH1750_SCL);  // Initialize I2C for BH1750
    lightMeter.begin();                  // Start light sensor
    sensors.begin();                     // Start temperature sensor

    Serial.println(F("Initialization complete"));
}

void playAudio(const char* file) {
    // Play the specified audio file
    if (!audio.isRunning()) {
        audio.connecttoFS(SPIFFS, file);
    }
    audio.loop();  // Keep looping to ensure audio plays continuously
}

void readMoistureSensor() {
    int sensor_analog = analogRead(SENSOR_PIN);
    float moisture_percentage = (100 - ((sensor_analog / 4095.00) * 100));  // 12-bit ADC

    Serial.print("Moisture Percentage: ");
    Serial.print(moisture_percentage);
    Serial.println("%");

    // Check if moisture is below 60%
    if (moisture_percentage < 60) {
        if (!moistureAlert) {
            Serial.println("Moisture is less than 60%, switching to water.mp3...");
            audio.stopSong();             // Stop current audio (new.mp3 or garmi.mp3)
            playAudio("/water.mp3");      // Play water.mp3 continuously
            moistureAlert = true;         // Set flag that water.mp3 is playing
            heatAlert = false;            // Reset heat alert if moisture alert is triggered
            lightAlert = false;           // Reset light alert if moisture alert is triggered
        }
    } else {
        // If moisture is above 60%, reset the moisture alert and stop water.mp3
        if (moistureAlert) {
            Serial.println("Moisture level is adequate, stopping water.mp3...");
            audio.stopSong();
            moistureAlert = false;  // Reset alert
        }
    }
}

void readTemperatureSensor() {
    sensors.requestTemperatures();
    float temperature = sensors.getTempCByIndex(0);
    Serial.print("Celsius temperature: ");
    Serial.println(temperature);

    // Check if temperature is above 40°C
    if (temperature > 40) {
        if (!heatAlert) {
            Serial.println("Temperature is above 40°C, switching to garmi.mp3...");
            audio.stopSong();             // Stop current audio (new.mp3 or water.mp3)
            playAudio("/garmi.mp3");      // Play garmi.mp3 continuously
            heatAlert = true;             // Set flag that garmi.mp3 is playing
            moistureAlert = false;        // Reset moisture alert if heat alert is triggered
            lightAlert = false;           // Reset light alert if heat alert is triggered
        }
    } else {
        // If temperature is 40°C or below, reset the heat alert and stop garmi.mp3
        if (heatAlert) {
            Serial.println("Temperature is adequate, stopping garmi.mp3...");
            audio.stopSong();
            heatAlert = false;  // Reset alert
        }
    }
}

void readLightSensor() {
    float lux = lightMeter.readLightLevel();
    Serial.print("Light: ");
    Serial.print(lux);
    Serial.println(" lx");

    // Check if light level is above 300 lux
    if (lux > 300) {
        if (!lightAlert) {
            Serial.println("Light level is above 300 lx, switching to chhao.mp3...");
            audio.stopSong();             // Stop current audio (new.mp3, water.mp3, or garmi.mp3)
            playAudio("/chhao.mp3");      // Play chhao.mp3 continuously
            lightAlert = true;            // Set flag that chhao.mp3 is playing
            moistureAlert = false;        // Reset moisture alert if light alert is triggered
            heatAlert = false;            // Reset heat alert if light alert is triggered
        }
    } else {
        // If light level is 300 lx or below, reset the light alert and stop chhao.mp3
        if (lightAlert) {
            Serial.println("Light level is adequate, stopping chhao.mp3...");
            audio.stopSong();
            lightAlert = false;  // Reset alert
        }
    }
}

void readSensors() {
    // Read moisture sensor
    readMoistureSensor();

    // If moisture is okay, read other sensors
    if (!moistureAlert) {
        readTemperatureSensor();
        readLightSensor();
    }
}

void loop() {
    // Step 1: Read sensors periodically without blocking
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= sensorInterval) {
        readSensors();  // Read all sensors
    }

    // Step 2: Play audio based on alerts
    if (moistureAlert) {
        playAudio("/water.mp3");
    } else if (heatAlert) {
        playAudio("/garmi.mp3");
    } else if (lightAlert) {
        playAudio("/chhao.mp3");
    } else {
        // If conditions are normal, check for happy.mp3 playback
        if (currentMillis - happyMillis >= happyInterval) {
            playAudio("/Happy.mp3");      // Play happy.mp3 every 30 seconds
        } else {
            playAudio("/new.mp3");        // Play new.mp3 if conditions are normal and happy.mp3 isn't due
        }
    }

    audio.loop();  // Keep current audio playing
}
