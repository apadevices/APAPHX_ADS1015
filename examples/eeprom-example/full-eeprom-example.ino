/**
 * APAPHX EEPROM Calibration Management Example
 * 
 * This example demonstrates how to permanently save and load calibration
 * data to/from Arduino EEPROM. Calibration data survives power cycles
 * and automatically loads on startup.
 * 
 * Features:
 * - Automatic calibration loading on startup
 * - Permanent EEPROM storage for both pH and ORP
 * - Data validation and error checking
 * - Automatic calibration process when no valid data found
 * - Manual recalibration commands
 */

#include "APAPHX_ADS1015.h"
#include <EEPROM.h>

// Create sensor instances
ADS1015 ads1015PH(ADDRESS_49);  // pH sensor
ADS1015 ads1015RX(ADDRESS_48);  // ORP sensor

// EEPROM memory layout
#define EEPROM_PH_CAL_ADDR    0    // pH calibration starts at address 0 (16 bytes)
#define EEPROM_ORP_CAL_ADDR   16   // ORP calibration starts at address 16 (16 bytes)
#define EEPROM_MAGIC_ADDR     32   // Magic number at address 32 (2 bytes)

const uint16_t EEPROM_MAGIC = 0xA5C3;  // Magic number to verify valid EEPROM data

// Reading configurations
PHXConfig phConfig = {
    .type = "ph",
    .samples = 100,
    .delay_ms = 10,
    .avg_buffer = 3
};

PHXConfig orpConfig = {
    .type = "rx",
    .samples = 100,
    .delay_ms = 10,
    .avg_buffer = 3
};

// System state
bool phCalibrated = false;
bool orpCalibrated = false;

void setup() {
    Serial.begin(9600);
    while (!Serial) delay(10);
    
    Serial.println(F("=== APAPHX EEPROM Calibration Example ==="));
    Serial.println(F("Automatic calibration loading/saving"));
    Serial.println();
    
    // Initialize sensors
    ads1015PH.begin();
    ads1015RX.begin();
    
    ads1015PH.setGain(ADS1015_REG_SET_GAIN0_6_144V);
    ads1015RX.setGain(ADS1015_REG_SET_GAIN1_4_096V);
    
    // Try to load saved calibrations
    loadAllCalibrations();
    
    Serial.println(F("Commands:"));
    Serial.println(F("p - Recalibrate pH"));
    Serial.println(F("o - Recalibrate ORP"));
    Serial.println(F("r - Read sensors"));
    Serial.println(F("s - Show calibration status"));
    Serial.println();
}

void loop() {
    if (Serial.available()) {
        char cmd = Serial.read();
        switch(cmd) {
            case 'p':
                calibratePHWithSave();
                break;
            case 'o':
                calibrateORPWithSave();
                break;
            case 'r':
                readSensors();
                break;
            case 's':
                showCalibrationStatus();
                break;
        }
    }
    
    delay(100);
}

// ========================================
// EEPROM MANAGEMENT FUNCTIONS
// ========================================

/**
 * Save calibration data to EEPROM
 */
void saveCalibrationToEEPROM(const char* type, PHX_Calibration &cal) {
    int addr = (strcmp(type, "ph") == 0) ? EEPROM_PH_CAL_ADDR : EEPROM_ORP_CAL_ADDR;
    
    // Save calibration structure
    EEPROM.put(addr, cal);
    
    // Mark EEPROM as containing valid data
    EEPROM.put(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);
    
    Serial.print(F("✓ "));
    Serial.print(type);
    Serial.println(F(" calibration saved to EEPROM"));
    
    // Update status flags
    if (strcmp(type, "ph") == 0) phCalibrated = true;
    if (strcmp(type, "rx") == 0) orpCalibrated = true;
}

/**
 * Load calibration from EEPROM with validation
 */
bool loadCalibrationFromEEPROM(const char* type, PHX_Calibration &cal) {
    // Check if EEPROM has valid data
    uint16_t magic;
    EEPROM.get(EEPROM_MAGIC_ADDR, magic);
    if (magic != EEPROM_MAGIC) {
        Serial.print(F("No valid "));
        Serial.print(type);
        Serial.println(F(" calibration in EEPROM"));
        return false;
    }
    
    // Load calibration data
    int addr = (strcmp(type, "ph") == 0) ? EEPROM_PH_CAL_ADDR : EEPROM_ORP_CAL_ADDR;
    EEPROM.get(addr, cal);
    
    // Validate calibration data makes sense
    float voltageDiff = abs(cal.ref2_mV - cal.ref1_mV);
    float valueDiff = abs(cal.ref2_value - cal.ref1_value);
    
    if (voltageDiff < 50.0 || voltageDiff > 2000.0) {
        Serial.print(F("Invalid "));
        Serial.print(type);
        Serial.println(F(" voltage range in EEPROM"));
        return false;
    }
    
    if (strcmp(type, "ph") == 0 && (valueDiff < 1.0 || valueDiff > 10.0)) {
        Serial.println(F("Invalid pH value range in EEPROM"));
        return false;
    }
    
    if (strcmp(type, "rx") == 0 && (valueDiff < 50.0 || valueDiff > 500.0)) {
        Serial.println(F("Invalid ORP value range in EEPROM"));
        return false;
    }
    
    Serial.print(F("✓ "));
    Serial.print(type);
    Serial.println(F(" calibration loaded from EEPROM"));
    return true;
}

/**
 * Load all calibrations on startup
 */
void loadAllCalibrations() {
    Serial.println(F("=== Loading Saved Calibrations ==="));
    
    PHX_Calibration cal;
    
    // Try to load pH calibration
    if (loadCalibrationFromEEPROM("ph", cal)) {
        ads1015PH.calibratePHX("ph", cal);
        phCalibrated = true;
        displayCalibrationDetails("pH", cal);
    } else {
        Serial.println(F("Starting pH calibration process..."));
        calibratePHWithSave();
    }
    
    // Try to load ORP calibration
    if (loadCalibrationFromEEPROM("rx", cal)) {
        ads1015RX.calibratePHX("rx", cal);
        orpCalibrated = true;
        displayCalibrationDetails("ORP", cal);
    } else {
        Serial.println(F("Starting ORP calibration process..."));
        calibrateORPWithSave();
    }
    
    Serial.println(F("=== Calibration Loading Complete ==="));
    Serial.println();
}

// ========================================
// CALIBRATION FUNCTIONS
// ========================================

/**
 * Perform pH calibration and save to EEPROM
 */
void calibratePHWithSave() {
    Serial.println(F("\n=== pH Calibration Process ==="));
    
    PHX_Calibration phCal;
    
    // First calibration point (pH 4.0)
    Serial.println(F("STEP 1: pH 4.0 Buffer"));
    Serial.println(F("1. Rinse probe with distilled water"));
    Serial.println(F("2. Place probe in pH 4.0 buffer solution"));
    Serial.println(F("3. Wait for stable reading"));
    Serial.println(F("4. Press Enter to continue..."));
    waitForEnter();
    
    Serial.println(F("Taking pH 4.0 reading..."));
    phCal.ref1_mV = ads1015PH.calibratePHXReading("ph");
    phCal.ref1_value = 4.0;
    
    Serial.print(F("pH 4.0 reading: "));
    Serial.print(phCal.ref1_mV, 1);
    Serial.println(F(" mV"));
    
    // Second calibration point (pH 7.0)
    Serial.println(F("\nSTEP 2: pH 7.0 Buffer"));
    Serial.println(F("1. Rinse probe with distilled water"));
    Serial.println(F("2. Place probe in pH 7.0 buffer solution"));
    Serial.println(F("3. Wait for stable reading"));
    Serial.println(F("4. Press Enter to continue..."));
    waitForEnter();
    
    Serial.println(F("Taking pH 7.0 reading..."));
    phCal.ref2_mV = ads1015PH.calibratePHXReading("ph");
    phCal.ref2_value = 7.0;
    
    Serial.print(F("pH 7.0 reading: "));
    Serial.print(phCal.ref2_mV, 1);
    Serial.println(F(" mV"));
    
    // Validate calibration
    float slope = (phCal.ref2_value - phCal.ref1_value) / (phCal.ref2_mV - phCal.ref1_mV) * 1000.0;
    Serial.print(F("Calibration slope: "));
    Serial.print(slope, 2);
    Serial.println(F(" pH/V"));
    
    if (abs(slope) < 1.0 || abs(slope) > 8.0) {
        Serial.println(F("⚠ Warning: Unusual calibration slope"));
        Serial.println(F("Check probe condition and buffer solutions"));
    }
    
    // Store in library and EEPROM
    ads1015PH.calibratePHX("ph", phCal);
    saveCalibrationToEEPROM("ph", phCal);
    
    Serial.println(F("pH calibration complete and saved!"));
    Serial.println();
}

/**
 * Perform ORP calibration and save to EEPROM
 */
void calibrateORPWithSave() {
    Serial.println(F("\n=== ORP Calibration Process ==="));
    
    PHX_Calibration orpCal;
    
    // First calibration point (475mV)
    Serial.println(F("STEP 1: 475mV Standard"));
    Serial.println(F("1. Rinse probe with distilled water"));
    Serial.println(F("2. Place probe in 475mV standard solution"));
    Serial.println(F("3. Wait for stable reading"));
    Serial.println(F("4. Press Enter to continue..."));
    waitForEnter();
    
    Serial.println(F("Taking 475mV reading..."));
    orpCal.ref1_mV = ads1015RX.calibratePHXReading("rx");
    orpCal.ref1_value = 475.0;
    
    Serial.print(F("475mV reading: "));
    Serial.print(orpCal.ref1_mV, 1);
    Serial.println(F(" mV"));
    
    // Second calibration point (650mV)
    Serial.println(F("\nSTEP 2: 650mV Standard"));
    Serial.println(F("1. Rinse probe with distilled water"));
    Serial.println(F("2. Place probe in 650mV standard solution"));
    Serial.println(F("3. Wait for stable reading"));
    Serial.println(F("4. Press Enter to continue..."));
    waitForEnter();
    
    Serial.println(F("Taking 650mV reading..."));
    orpCal.ref2_mV = ads1015RX.calibratePHXReading("rx");
    orpCal.ref2_value = 650.0;
    
    Serial.print(F("650mV reading: "));
    Serial.print(orpCal.ref2_mV, 1);
    Serial.println(F(" mV"));
    
    // Validate calibration
    float slope = (orpCal.ref2_value - orpCal.ref1_value) / (orpCal.ref2_mV - orpCal.ref1_mV);
    Serial.print(F("Calibration slope: "));
    Serial.print(slope, 3);
    Serial.println(F(" mV/mV"));
    
    if (abs(slope) < 0.5 || abs(slope) > 2.0) {
        Serial.println(F("⚠ Warning: Unusual calibration slope"));
        Serial.println(F("Check probe condition and standard solutions"));
    }
    
    // Store in library and EEPROM
    ads1015RX.calibratePHX("rx", orpCal);
    saveCalibrationToEEPROM("rx", orpCal);
    
    Serial.println(F("ORP calibration complete and saved!"));
    Serial.println();
}

// ========================================
// UTILITY FUNCTIONS
// ========================================

/**
 * Wait for user to press Enter
 */
void waitForEnter() {
    while (Serial.available()) Serial.read(); // Clear buffer
    while (!Serial.available()) delay(10);
    while (Serial.available()) Serial.read(); // Clear buffer
}

/**
 * Read both sensors and display results
 */
void readSensors() {
    if (!phCalibrated || !orpCalibrated) {
        Serial.println(F("⚠ Sensors not fully calibrated"));
        return;
    }
    
    Serial.println(F("=== Sensor Readings ==="));
    
    // Read pH
    ads1015PH.startReading(phConfig);
    while(ads1015PH.getState() != PHXState::IDLE) {
        ads1015PH.updateReading();
    }
    
    // Read ORP
    ads1015RX.startReading(orpConfig);
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
 * Show calibration status and details
 */
void showCalibrationStatus() {
    Serial.println(F("=== Calibration Status ==="));
    Serial.print(F("pH: "));
    Serial.println(phCalibrated ? F("✓ Calibrated") : F("❌ Not calibrated"));
    Serial.print(F("ORP: "));
    Serial.println(orpCalibrated ? F("✓ Calibrated") : F("❌ Not calibrated"));
    
    // Show EEPROM magic number status
    uint16_t magic;
    EEPROM.get(EEPROM_MAGIC_ADDR, magic);
    Serial.print(F("EEPROM: "));
    Serial.println((magic == EEPROM_MAGIC) ? F("✓ Valid data") : F("❌ No valid data"));
    Serial.println();
}

/**
 * Display detailed calibration information
 */
void displayCalibrationDetails(const char* type, PHX_Calibration &cal) {
    Serial.print(F("  "));
    Serial.print(type);
    Serial.println(F(" calibration details:"));
    Serial.print(F("    Point 1: "));
    Serial.print(cal.ref1_value, 1);
    Serial.print(F(" = "));
    Serial.print(cal.ref1_mV, 1);
    Serial.println(F(" mV"));
    Serial.print(F("    Point 2: "));
    Serial.print(cal.ref2_value, 1);
    Serial.print(F(" = "));
    Serial.print(cal.ref2_mV, 1);
    Serial.println(F(" mV"));
}