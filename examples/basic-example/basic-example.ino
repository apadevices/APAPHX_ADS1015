/**
 * APAPHX Basic Example
 * Simple pH and ORP measurement with calibration
 */

#include "APAPHX_ADS1015.h"

// Create sensor instances
ADS1015 ads1015PH(ADDRESS_49);  // pH sensor
ADS1015 ads1015RX(ADDRESS_48);  // ORP sensor

// Basic reading config
PHXConfig phConfig = {
    .type = "ph",
    .samples = 100,
    .delay_ms = 10,
    .avg_buffer = 1  // No rolling average
};

PHXConfig rxConfig = {
    .type = "rx",
    .samples = 100,
    .delay_ms = 10,
    .avg_buffer = 1
};

void setup() {
    Serial.begin(9600);
    
    // Initialize sensors
    ads1015PH.begin();
    ads1015RX.begin();
    
    ads1015PH.setGain(ADS1015_REG_SET_GAIN1_4_096V);
    ads1015RX.setGain(ADS1015_REG_SET_GAIN1_4_096V);

    Serial.println(F("APAPHX Basic Test"));
    Serial.println(F("Send 'c' to calibrate"));
}

void loop() {
    if (Serial.available()) {
        if (Serial.read() == 'c') {
            calibrateSensors();
        }
    }

    // Read both sensors
    readSensors();
    delay(1000);
}

void readSensors() {
    // pH reading
    ads1015PH.startReading(phConfig);
    while(ads1015PH.getState() != PHXState::IDLE) {
        ads1015PH.updateReading();
    }
    
    // ORP reading
    ads1015RX.startReading(rxConfig);
    while(ads1015RX.getState() != PHXState::IDLE) {
        ads1015RX.updateReading();
    }
    
    // Print results
    Serial.print(F("pH: "));
    Serial.print(ads1015PH.getLastReading(), 2);
    Serial.print(F(" | ORP: "));
    Serial.print(ads1015RX.getLastReading(), 0);
    Serial.println(F(" mV"));
}

void calibrateSensors() {
    // pH Calibration
    Serial.println(F("\n=== pH Calibration ==="));
    PHX_Calibration phCal;
    
    Serial.println(F("Place pH probe in pH 4 buffer"));
    Serial.println(F("Wait 3-5 minutes, then press 'c'"));
    while(!Serial.available() || Serial.read() != 'c') {}
    
    phCal.ref1_mV = ads1015PH.calibratePHXReading("ph");
    phCal.ref1_value = 4.0;
    delay(5000);
    
    Serial.println(F("\nPlace pH probe in pH 7 buffer"));
    Serial.println(F("Wait 3-5 minutes, then press 'c'"));
    while(!Serial.available() || Serial.read() != 'c') {}
    
    phCal.ref2_mV = ads1015PH.calibratePHXReading("ph");
    phCal.ref2_value = 7.0;
    ads1015PH.calibratePHX("ph", phCal);
    
    // ORP Calibration
    Serial.println(F("\n=== ORP Calibration ==="));
    PHX_Calibration orpCal;
    
    Serial.println(F("Place ORP probe in 475mV solution"));
    Serial.println(F("Wait 3-5 minutes, then press 'c'"));
    while(!Serial.available() || Serial.read() != 'c') {}
    
    orpCal.ref1_mV = ads1015RX.calibratePHXReading("rx");
    orpCal.ref1_value = 475.0;
    delay(5000);
    
    Serial.println(F("Place ORP probe in 650mV solution"));
    Serial.println(F("Wait 3-5 minutes, then press 'c'"));
    while(!Serial.available() || Serial.read() != 'c') {}
    
    orpCal.ref2_mV = ads1015RX.calibratePHXReading("rx");
    orpCal.ref2_value = 650.0;
    ads1015RX.calibratePHX("rx", orpCal);
    
    Serial.println(F("\nCalibration complete!"));
}