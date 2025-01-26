/**
 * APAPHX Advanced Example
 * Demonstrates all library features:
 * - Non-blocking operation
 * - Rolling average
 * - Error handling
 * - Separate pH/ORP calibration
 * - Status monitoring
 */

#include "APAPHX_ADS1015.h"

// Create sensor instances
ADS1015 ads1015PH(ADDRESS_49);
ADS1015 ads1015RX(ADDRESS_48);

// Advanced reading config with rolling average
PHXConfig phConfig = {
    .type = "ph",
    .samples = 100,
    .delay_ms = 10,
    .avg_buffer = 3  // Average last 3 readings
};

PHXConfig rxConfig = {
    .type = "rx",
    .samples = 100,
    .delay_ms = 10,
    .avg_buffer = 3
};

bool needNewReading = false;
unsigned long lastReadingTime = 0;
const unsigned long READ_INTERVAL = 1000;

void setup() {
    Serial.begin(9600);
    
    ads1015PH.begin();
    ads1015RX.begin();
    
    ads1015PH.setGain(ADS1015_REG_SET_GAIN1_4_096V);
    ads1015RX.setGain(ADS1015_REG_SET_GAIN1_4_096V);

    Serial.println(F("APAPHX Advanced Test"));
    Serial.println(F("Commands:"));
    Serial.println(F("r - Toggle continuous reading"));
    Serial.println(F("p - Calibrate pH"));
    Serial.println(F("o - Calibrate ORP"));
    Serial.println(F("s - Show status"));
}

void loop() {
    if (Serial.available()) {
        handleCommand();
    }
    
    if (needNewReading && millis() - lastReadingTime >= READ_INTERVAL) {
        updateReadings();
        lastReadingTime = millis();
    }
}

void handleCommand() {
    char cmd = Serial.read();
    switch(cmd) {
        case 'r':
            needNewReading = !needNewReading;
            Serial.print(F("Continuous reading: "));
            Serial.println(needNewReading ? F("ON") : F("OFF"));
            break;
        case 'p':
            calibratePH();
            break;
        case 'o':
            calibrateORP();
            break;
        case 's':
            showStatus();
            break;
    }
}

void updateReadings() {
    static bool phStarted = false;
    static bool orpStarted = false;
    
    // Start pH reading if not in progress
    if (!phStarted && ads1015PH.getState() == PHXState::IDLE) {
        ads1015PH.startReading(phConfig);
        phStarted = true;
    }
    
    // Start ORP reading if not in progress
    if (!orpStarted && ads1015RX.getState() == PHXState::IDLE) {
        ads1015RX.startReading(rxConfig);
        orpStarted = true;
    }
    
    // Update both sensors
    if (phStarted) ads1015PH.updateReading();
    if (orpStarted) ads1015RX.updateReading();
    
    // Check if both complete
    if (phStarted && orpStarted && 
        ads1015PH.isReadingComplete() && ads1015RX.isReadingComplete()) {
        printReadings();
        phStarted = false;
        orpStarted = false;
    }
}

void printReadings() {
    Serial.println(F("\n=== Readings ==="));
    
    // Print pH with error checking
    Serial.print(F("pH: "));
    Serial.print(ads1015PH.getLastReading(), 2);
    if (ads1015PH.getLastError() != PHXError::NONE) {
        Serial.print(F(" [ERROR: "));
        switch(ads1015PH.getLastError()) {
            case PHXError::PH_LOW: Serial.print(F("Below range")); break;
            case PHXError::PH_HIGH: Serial.print(F("Above range")); break;
            default: break;
        }
        Serial.print(F("]"));
    }
    Serial.println();
    
    // Print ORP with error checking
    Serial.print(F("ORP: "));
    Serial.print(ads1015RX.getLastReading(), 0);
    Serial.print(F(" mV"));
    if (ads1015RX.getLastError() != PHXError::NONE) {
        Serial.print(F(" [ERROR: "));
        switch(ads1015RX.getLastError()) {
            case PHXError::ORP_LOW: Serial.print(F("Below range")); break;
            case PHXError::ORP_HIGH: Serial.print(F("Above range")); break;
            default: break;
        }
        Serial.print(F("]"));
    }
    Serial.println();
}

void showStatus() {
    Serial.println(F("\n=== Status ==="));
    Serial.print(F("pH State: "));
    Serial.println((int)ads1015PH.getState());
    Serial.print(F("ORP State: "));
    Serial.println((int)ads1015RX.getState());
    Serial.print(F("Continuous: "));
    Serial.println(needNewReading ? F("ON") : F("OFF"));
}

void calibratePH() {
    Serial.println(F("\n=== pH Calibration ==="));
    PHX_Calibration phCal;
    
    Serial.println(F("Place pH probe in pH 4 buffer"));
    Serial.println(F("Wait 3-5 minutes, then press 'c'"));
    while(!Serial.available() || Serial.read() != 'c') {}
    
    phCal.ref1_mV = ads1015PH.calibratePHXReading("ph");
    phCal.ref1_value = 4.0;
    Serial.print(F("pH 4 reading (mV): "));
    Serial.println(phCal.ref1_mV);
    delay(5000);
    
    Serial.println(F("\nPlace pH probe in pH 7 buffer"));
    Serial.println(F("Wait 3-5 minutes, then press 'c'"));
    while(!Serial.available() || Serial.read() != 'c') {}
    
    phCal.ref2_mV = ads1015PH.calibratePHXReading("ph");
    phCal.ref2_value = 7.0;
    Serial.print(F("pH 7 reading (mV): "));
    Serial.println(phCal.ref2_mV);
    
    ads1015PH.calibratePHX("ph", phCal);
    Serial.println(F("pH Calibration complete!"));
}

void calibrateORP() {
    Serial.println(F("\n=== ORP Calibration ==="));
    PHX_Calibration orpCal;
    
    Serial.println(F("Place ORP probe in 475mV solution"));
    Serial.println(F("Wait 3-5 minutes, then press 'c'"));
    while(!Serial.available() || Serial.read() != 'c') {}
    
    orpCal.ref1_mV = ads1015RX.calibratePHXReading("rx");
    orpCal.ref1_value = 475.0;
    Serial.print(F("475mV reading (mV): "));
    Serial.println(orpCal.ref1_mV);
    delay(5000);
    
    Serial.println(F("\nPlace ORP probe in 650mV solution"));
    Serial.println(F("Wait 3-5 minutes, then press 'c'"));
    while(!Serial.available() || Serial.read() != 'c') {}
    
    orpCal.ref2_mV = ads1015RX.calibratePHXReading("rx");
    orpCal.ref2_value = 650.0;
    Serial.print(F("650mV reading (mV): "));
    Serial.println(orpCal.ref2_mV);
    
    ads1015RX.calibratePHX("rx", orpCal);
    Serial.println(F("ORP Calibration complete!"));
}