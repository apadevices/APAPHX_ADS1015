/**
 * APAPHX Basic Temperature Compensation Example
 * 
 * Simple example showing how to add temperature compensation 
 * to your existing pH measurements. Based on the basic example
 * but with temperature compensation features added.
 * 
 * Features:
 * - Basic pH measurement
 * - Temperature compensation using Pasco 2001 formula
 * - Simple calibration
 * - Temperature setting via serial commands
 */

#include "APAPHX_ADS1015.h"

// Create pH sensor instance
ADS1015 ads1015PH(ADDRESS_49);

// Basic reading config
PHXConfig phConfig = {
    .type = "ph",
    .samples = 100,
    .delay_ms = 10,
    .avg_buffer = 1
};

void setup() {
    Serial.begin(9600);
    
    // Initialize pH sensor
    ads1015PH.begin();
    ads1015PH.setGain(ADS1015_REG_SET_GAIN0_6_144V);

    Serial.println(F("APAPHX Temperature Compensation Example"));
    Serial.println(F("Commands:"));
    Serial.println(F("c - Calibrate pH sensor"));
    Serial.println(F("t - Enable/disable temperature compensation"));
    Serial.println(F("25.5 - Set temperature (enter any number)"));
    Serial.println();
}

void loop() {
    // Handle serial commands
    if (Serial.available()) {
        handleCommands();
    }

    // Take pH reading and display results
    readPH();
    delay(2000);
}

/**
 * Handle serial commands for calibration and temperature
 */
void handleCommands() {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input == "c") {
        calibratePH();
    }
    else if (input == "t") {
        toggleTemperatureCompensation();
    }
    else {
        // Try to parse as temperature value
        float temp = input.toFloat();
        if (temp > 0) {
            setTemperature(temp);
        }
    }
}

/**
 * Read pH with optional temperature compensation
 */
void readPH() {
    // Take pH reading
    ads1015PH.startReading(phConfig);
    while(ads1015PH.getState() != PHXState::IDLE) {
        ads1015PH.updateReading();
    }
    
    // Display results
    Serial.print(F("pH: "));
    Serial.print(ads1015PH.getLastReading(), 2);
    
    // Show temperature compensation status
    if (ads1015PH.isTemperatureCompensationEnabled()) {
        Serial.print(F(" (temp compensated @ "));
        Serial.print(ads1015PH.getCurrentTemperature(), 1);
        Serial.print(F("°C)"));
    } else {
        Serial.print(F(" (no temp compensation)"));
    }
    
    // Check for errors
    PHXError error = ads1015PH.getLastError();
    if (error == PHXError::TEMP_INVALID) {
        Serial.print(F(" [TEMP ERROR]"));
    }
    
    Serial.println();
}

/**
 * Simple pH calibration routine
 */
void calibratePH() {
    Serial.println(F("\n=== pH Calibration ==="));
    
    // Temporarily disable temperature compensation during calibration
    bool wasEnabled = ads1015PH.isTemperatureCompensationEnabled();
    ads1015PH.enableTemperatureCompensation(false);
    
    PHX_Calibration phCal;
    
    // pH 4 calibration
    Serial.println(F("Place pH probe in pH 4.0 buffer"));
    Serial.println(F("Wait for stable reading, then press Enter"));
    while(!Serial.available()) {}
    Serial.readStringUntil('\n'); // Clear input
    
    phCal.ref1_mV = ads1015PH.calibratePHXReading("ph");
    phCal.ref1_value = 4.0;
    Serial.print(F("pH 4.0 reading: "));
    Serial.print(phCal.ref1_mV, 1);
    Serial.println(F(" mV"));
    
    // pH 7 calibration
    Serial.println(F("\nPlace pH probe in pH 7.0 buffer"));
    Serial.println(F("Wait for stable reading, then press Enter"));
    while(!Serial.available()) {}
    Serial.readStringUntil('\n'); // Clear input
    
    phCal.ref2_mV = ads1015PH.calibratePHXReading("ph");
    phCal.ref2_value = 7.0;
    Serial.print(F("pH 7.0 reading: "));
    Serial.print(phCal.ref2_mV, 1);
    Serial.println(F(" mV"));
    
    // Store calibration
    ads1015PH.calibratePHX("ph", phCal);
    
    // Restore temperature compensation setting
    ads1015PH.enableTemperatureCompensation(wasEnabled);
    
    Serial.println(F("Calibration complete!\n"));
}

/**
 * Toggle temperature compensation on/off
 */
void toggleTemperatureCompensation() {
    bool currentState = ads1015PH.isTemperatureCompensationEnabled();
    ads1015PH.enableTemperatureCompensation(!currentState);
    
    Serial.print(F("Temperature compensation: "));
    if (ads1015PH.isTemperatureCompensationEnabled()) {
        Serial.print(F("ENABLED ("));
        Serial.print(ads1015PH.getCurrentTemperature(), 1);
        Serial.println(F("°C)"));
    } else {
        Serial.println(F("DISABLED"));
    }
}

/**
 * Set current temperature for compensation
 */
void setTemperature(float temperature) {
    ads1015PH.setTemperature(temperature);
    
    // Check if temperature was accepted
    if (ads1015PH.getLastError() == PHXError::TEMP_INVALID) {
        Serial.print(F("ERROR: Invalid temperature "));
        Serial.print(temperature, 1);
        Serial.println(F("°C (valid range: 0-50°C)"));
    } else {
        Serial.print(F("Temperature set to: "));
        Serial.print(temperature, 1);
        Serial.println(F("°C"));
        
        // Auto-enable temperature compensation if not already enabled
        if (!ads1015PH.isTemperatureCompensationEnabled()) {
            ads1015PH.enableTemperatureCompensation(true);
            Serial.println(F("Temperature compensation auto-enabled"));
        }
    }
}