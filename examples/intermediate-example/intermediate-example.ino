/**
 * APAPHX Intermediate Example
 * Features:
 * - Continuous reading with separate calibration triggers
 * - Non-blocking operation
 * - Rolling average support
 * - Simple command interface
 */

#include "APAPHX_ADS1015.h"

// Create sensor instances
ADS1015 ads1015PH(ADDRESS_49);  // pH sensor
ADS1015 ads1015RX(ADDRESS_48);  // ORP sensor

// Configure reading parameters with rolling average
PHXConfig phConfig = {
    .type = "ph",
    .samples = 100,    // Number of samples per reading
    .delay_ms = 10,    // Delay between samples
    .avg_buffer = 3    // Rolling average of last 3 readings
};

PHXConfig rxConfig = {
    .type = "rx",
    .samples = 100,
    .delay_ms = 10,
    .avg_buffer = 3
};

// Calibration control flags
bool calibratePH = false;
bool calibrateORP = false;

void setup() {
    Serial.begin(9600);
    
    // Initialize ADCs
    ads1015PH.begin();
    ads1015RX.begin();
    
    // Configure ADC gain
    ads1015PH.setGain(ADS1015_REG_SET_GAIN1_4_096V);
    ads1015RX.setGain(ADS1015_REG_SET_GAIN1_4_096V);

    Serial.println(F("APAPHX Simple Test"));
    Serial.println(F("Commands:"));
    Serial.println(F("p - Calibrate pH"));
    Serial.println(F("o - Calibrate ORP"));
}

void loop() {
    // Handle calibration commands
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'p') calibratePH = true;
        if (cmd == 'o') calibrateORP = true;
    }

    // Perform calibration if requested
    if (calibratePH) {
        calibratePHSensor();
        calibratePH = false;
    }
    if (calibrateORP) {
        calibrateORPSensor();
        calibrateORP = false;
    }

    // Regular measurement cycle
    readSensors();
    delay(1000);
}

/**
 * Reads both sensors and displays results
 * Uses non-blocking state machine approach
 */
void readSensors() {
    // Get pH reading
    ads1015PH.startReading(phConfig);
    while(ads1015PH.getState() != PHXState::IDLE) {
        ads1015PH.updateReading();
    }
    
    // Get ORP reading
    ads1015RX.startReading(rxConfig);
    while(ads1015RX.getState() != PHXState::IDLE) {
        ads1015RX.updateReading();
    }
    
    // Display results
    Serial.print(F("pH: "));
    Serial.print(ads1015PH.getLastReading(), 2);
    Serial.print(F(" | ORP: "));
    Serial.print(ads1015RX.getLastReading(), 0);
    Serial.println(F(" mV"));
}

/**
 * Performs two-point pH calibration
 * Uses pH 4 and pH 7 buffer solutions
 */
void calibratePHSensor() {
    Serial.println(F("\n=== pH Calibration ==="));
    PHX_Calibration phCal;
    
    // First calibration point (pH 4)
    Serial.println(F("Place pH probe in pH 4 buffer"));
    Serial.println(F("Wait 3-5 minutes, then press 'c'"));
    while(true) {
        if (Serial.available() && Serial.read() == 'c') break;
    }
    
    float reading = ads1015PH.calibratePHXReading("ph");
    phCal.ref1_mV = reading;
    phCal.ref1_value = 4.0;
    delay(5000);
    
    // Second calibration point (pH 7)
    Serial.println(F("\nPlace pH probe in pH 7 buffer"));
    Serial.println(F("Wait 3-5 minutes, then press 'c'"));
    while(true) {
        if (Serial.available() && Serial.read() == 'c') break;
    }
    
    reading = ads1015PH.calibratePHXReading("ph");
    phCal.ref2_mV = reading;
    phCal.ref2_value = 7.0;
    
    // Store calibration
    ads1015PH.calibratePHX("ph", phCal);
    Serial.println(F("pH Calibration complete!"));
}

/**
 * Performs two-point ORP calibration
 * Uses 475mV and 650mV standard solutions
 */
void calibrateORPSensor() {
    Serial.println(F("\n=== ORP Calibration ==="));
    PHX_Calibration orpCal;
    
    // First calibration point (475mV)
    Serial.println(F("Place ORP probe in 475mV solution"));
    Serial.println(F("Wait 3-5 minutes, then press 'c'"));
    while(true) {
        if (Serial.available() && Serial.read() == 'c') break;
    }
    
    float reading = ads1015RX.calibratePHXReading("rx");
    orpCal.ref1_mV = reading;
    orpCal.ref1_value = 475.0;
    delay(5000);
    
    // Second calibration point (650mV)
    Serial.println(F("Place ORP probe in 650mV solution"));
    Serial.println(F("Wait 3-5 minutes, then press 'c'"));
    while(true) {
        if (Serial.available() && Serial.read() == 'c') break;
    }
    
    reading = ads1015RX.calibratePHXReading("rx");
    orpCal.ref2_mV = reading;
    orpCal.ref2_value = 650.0;
    
    // Store calibration
    ads1015RX.calibratePHX("rx", orpCal);
    Serial.println(F("ORP Calibration complete!"));
}