#include <EEPROM.h>

// EEPROM addresses for calibration data
#define EEPROM_PH_CAL_ADDR    0    // pH calibration starts at address 0 (16 bytes)
#define EEPROM_ORP_CAL_ADDR   16   // ORP calibration starts at address 16 (16 bytes)
#define EEPROM_MAGIC_ADDR     32   // Magic number at address 32 (2 bytes)

const uint16_t EEPROM_MAGIC = 0xA5C3;  // Magic number for validation

/**
 * Save calibration to EEPROM for both pH and ORP
 */
void saveCalibrationToEEPROM(const char* type, PHX_Calibration &cal) {
    int addr = (strcmp(type, "ph") == 0) ? EEPROM_PH_CAL_ADDR : EEPROM_ORP_CAL_ADDR;
    
    EEPROM.put(addr, cal);
    EEPROM.put(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);  // Mark as valid
    
    Serial.print("Calibration saved to EEPROM: ");
    Serial.println(type);
}

/**
 * Load calibration from EEPROM with validation for both pH and ORP
 */
bool loadCalibrationFromEEPROM(const char* type, PHX_Calibration &cal) {
    // Check if EEPROM has valid data
    uint16_t magic;
    EEPROM.get(EEPROM_MAGIC_ADDR, magic);
    if (magic != EEPROM_MAGIC) {
        Serial.print("No valid calibration in EEPROM for ");
        Serial.println(type);
        return false;
    }
    
    // Load calibration data
    int addr = (strcmp(type, "ph") == 0) ? EEPROM_PH_CAL_ADDR : EEPROM_ORP_CAL_ADDR;
    EEPROM.get(addr, cal);
    
    // Validate data makes sense for both pH and ORP
    float voltageDiff = abs(cal.ref2_mV - cal.ref1_mV);
    float valueDiff = abs(cal.ref2_value - cal.ref1_value);
    
    // Check voltage difference is reasonable
    if (voltageDiff < 50.0) {
        Serial.print("Invalid voltage range in EEPROM for ");
        Serial.println(type);
        return false;
    }
    
    // Type-specific validation
    if (strcmp(type, "ph") == 0) {
        // pH validation: expect 2-10 pH difference, voltage 100-1000mV difference
        if (valueDiff < 1.0 || valueDiff > 10.0 || voltageDiff > 1000.0) {
            Serial.println("Invalid pH calibration data in EEPROM");
            return false;
        }
    } else if (strcmp(type, "rx") == 0) {
        // ORP validation: expect 50-500mV difference, voltage 50-1000mV difference
        if (valueDiff < 50.0 || valueDiff > 500.0 || voltageDiff > 1000.0) {
            Serial.println("Invalid ORP calibration data in EEPROM");
            return false;
        }
    }
    
    Serial.print("Calibration loaded from EEPROM: ");
    Serial.println(type);
    return true;
}

/**
 * Load both pH and ORP calibrations on startup
 */
void loadBothCalibrations() {
    Serial.println("Loading saved calibrations...");
    
    PHX_Calibration cal;
    
    // Load pH calibration
    if (loadCalibrationFromEEPROM("ph", cal)) {
        ads1015PH.calibratePHX("ph", cal);
        Serial.println("pH calibration restored");
    } else {
        Serial.println("pH calibration required");
        // Call your pH calibration function here
        // calibratePH();
    }
    
    // Load ORP calibration
    if (loadCalibrationFromEEPROM("rx", cal)) {
        ads1015RX.calibratePHX("rx", cal);
        Serial.println("ORP calibration restored");
    } else {
        Serial.println("ORP calibration required");
        // Call your ORP calibration function here
        // calibrateORP();
    }
}

/**
 * Save both calibrations (call after each calibration process)
 */
void saveBothCalibrations(PHX_Calibration &phCal, PHX_Calibration &orpCal) {
    saveCalibrationToEEPROM("ph", phCal);
    saveCalibrationToEEPROM("rx", orpCal);
    Serial.println("Both calibrations saved to EEPROM");
}

/**
 * Clear EEPROM calibration data (for testing/reset)
 */
void clearCalibrationEEPROM() {
    // Write invalid magic number
    EEPROM.put(EEPROM_MAGIC_ADDR, (uint16_t)0x0000);
    Serial.println("EEPROM calibration data cleared");
}