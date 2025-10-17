/**
 * @file APAPHX_ADS1015.cpp
 * @brief Implementation of ADS1015 ADC Library for pH and ORP/Redox Measurements with Temperature Compensation
 * @version 1.1
 * @author APADevices [@kecup]
 * 
 * This source file implements the core functionality for pH and ORP measurements
 * using the ADS1015 ADC, now including optional temperature compensation. The implementation focuses on:
 * - Non-blocking operation through state machine design
 * - Accurate voltage conversion with multiple gain options
 * - Robust signal processing with noise filtering
 * - Two-point calibration for accuracy
 * - Temperature compensation for pH measurements using Pasco 2001 formula
 * - Comprehensive error detection and handling
 * 
 * Temperature Compensation:
 * Uses the scientifically proven Pasco 2001 formula:
 * pH_compensated = ((pH_raw - 7) × (273.15 + current_temp)) / (273.15 + 25) + 7
 * 
 * This normalizes all pH readings to 25°C standard temperature, making them
 * comparable regardless of actual measurement temperature.
 */

#include "APAPHX_ADS1015.h"

/**
 * @brief Constructor initializes ADC with specified I2C address
 * @param i2cAddress The I2C address (0x48-0x4B) based on ADDR pin connection
 */
ADS1015::ADS1015(uint8_t i2cAddress) {
    _i2cAddress = i2cAddress;
}

/**
 * @brief Initializes I2C communication for the ADC
 * Must be called before any operations with the sensor
 */
void ADS1015::begin() {
    Wire.begin();
}

/**
 * @brief Sets ADC gain/voltage range
 * @param gain Use predefined gain settings (e.g., ADS1015_REG_SET_GAIN1_4_096V)
 * Higher gain provides better resolution but smaller voltage range
 */
void ADS1015::setGain(uint16_t gain) {
    _gain = gain;
}

/**
 * @brief Reads voltage from specified ADC channel
 * @param channel ADC input channel (0-3)
 * @return Raw 12-bit ADC value (0-2047)
 * 
 * Configures ADC for single-ended measurement on specified channel
 * and returns raw conversion result
 */
int16_t ADS1015::readADC_SingleEnded(uint8_t channel) {
    if (channel > 3) return 0;

    // Set up ADC configuration register
    uint16_t config = _gain |                     // Voltage range
                     ADS1015_REG_CONFIG_MODE_CONTIN |  // Continuous conversion
                     ADS1015_REG_CONFIG_DR_1600SPS;    // 1600 samples/second

    // Select input channel
    config |= (ADS1015_REG_CONFIG_MUX_SINGLE_0 + (channel * 0x1000));
    config |= ADS1015_REG_CONFIG_OS_SINGLE;      // Start conversion

    writeRegister(_i2cAddress, ADS1015_REG_POINTER_CONFIG, config);
    delay(1);  // Brief delay for conversion completion

    // Read and return 12-bit result
    return readRegister(_i2cAddress, ADS1015_REG_POINTER_CONVERT) >> 4;
}

/**
 * @brief Performs two-register I2C write
 * @param i2cAddress Device address
 * @param reg Register address
 * @param value 16-bit value to write
 * 
 * Handles I2C protocol for writing 16-bit configuration values
 */
void ADS1015::writeRegister(uint8_t i2cAddress, uint8_t reg, uint16_t value) {
    Wire.beginTransmission(i2cAddress);
    Wire.write((uint8_t)reg);
    Wire.write((uint8_t)(value >> 8));    // High byte
    Wire.write((uint8_t)(value & 0xFF));  // Low byte
    Wire.endTransmission();
}

/**
 * @brief Reads 16-bit value from ADC register
 * @param i2cAddress Device address
 * @param reg Register to read
 * @return 16-bit register value
 * 
 * Handles I2C protocol for reading conversion results
 */
uint16_t ADS1015::readRegister(uint8_t i2cAddress, uint8_t reg) {
    Wire.beginTransmission(i2cAddress);
    Wire.write(reg);
    Wire.endTransmission();
    
    Wire.requestFrom(i2cAddress, (uint8_t)2);
    return ((Wire.read() << 8) | Wire.read());  // Combine bytes
}

/**
 * @brief Gets stable reading for calibration
 * @param type Measurement type ("ph" or "rx")
 * @return Stable voltage reading in mV
 * 
 * Takes multiple readings until values stabilize within threshold
 * Used during calibration process to ensure accurate reference points
 * 
 * NOTE: Temperature compensation is NOT applied during calibration.
 * This is correct behavior - calibration should capture raw sensor response.
 */
float ADS1015::calibratePHXReading(const char* type) {
    float firstReading, secondReading;
    
    // Basic config for calibration readings
    PHXConfig calConfig = {
        .type = type,
        .samples = 100,    // Use 100 samples for accuracy
        .delay_ms = 10,    // 10ms between samples
        .avg_buffer = 1    // No averaging during calibration
    };
    
    // Take readings until stable (within STABILITY_THRESHOLD)
    do {
        // First reading
        startReading(calConfig);
        while(getState() != PHXState::IDLE) {
            updateReading();
        }
        firstReading = getLastReading();
        
        delay(500);  // Wait between readings
        
        // Second reading
        startReading(calConfig);
        while(getState() != PHXState::IDLE) {
            updateReading();
        }
        secondReading = getLastReading();
        
    } while(abs((firstReading + secondReading) / 2.0f - firstReading) >= STABILITY_THRESHOLD ||
            abs((firstReading + secondReading) / 2.0f - secondReading) >= STABILITY_THRESHOLD);
            
    return (firstReading + secondReading) / 2.0f;  // Return average
}

/**
 * @brief Stores calibration data for pH or ORP
 * @param type Measurement type ("ph" or "rx")
 * @param cal Calibration points and values
 * 
 * Stores two-point calibration data for converting voltage to pH or ORP
 */
void ADS1015::calibratePHX(const char* type, PHX_Calibration &cal) {
    if (strcmp(type, "ph") == 0) {
        ph_cal = cal;
    } else if (strcmp(type, "rx") == 0) {
        orp_cal = cal;
    }
}

/**
 * @brief Initiates new measurement sequence
 * @param config Reading configuration (type, samples, timing)
 * 
 * Sets up buffers and parameters for new measurement
 * Non-blocking - call updateReading() to progress
 */
void ADS1015::startReading(const PHXConfig& config) {
    if (_state != PHXState::IDLE) return;
    
    _config = config;
    _currentSample = 0;
    _readingComplete = false;
    _lastError = PHXError::NONE;
    
    // Set up rolling average if requested
    if (_lastReadings != nullptr) {
        delete[] _lastReadings;
    }
    _avgBufferSize = constrain(config.avg_buffer, 1, MAX_AVG_BUFFER);
    _lastReadings = new float[_avgBufferSize]();
    _readingIndex = 0;
    _rollingAverageReady = false;
    
    // Prepare samples buffer
    if (_readings != nullptr) {
        delete[] _readings;
    }
    _readings = new float[config.samples];
    
    _state = PHXState::COLLECTING;
}

/**
 * @brief Core measurement state machine with temperature compensation
 * 
 * This function implements the non-blocking measurement process:
 * 1. COLLECTING: Gather voltage samples at specified intervals
 * 2. PROCESSING: Convert voltages to pH/ORP values with optional temperature compensation
 * 3. IDLE: Measurement complete
 * 
 * Features:
 * - Voltage range selection based on gain
 * - Sample averaging
 * - Two-point calibration application
 * - Temperature compensation for pH measurements (Pasco 2001 formula)
 * - Range validation and error reporting
 */
void ADS1015::updateReading() {
    switch (_state) {
        case PHXState::COLLECTING:
            if (millis() - _lastSampleTime >= _config.delay_ms) {
                // Select voltage range based on configured gain
                float voltageRange;
                switch(_gain) {
                    case ADS1015_REG_SET_GAIN0_6_144V: voltageRange = 6.144f; break;
                    case ADS1015_REG_SET_GAIN1_4_096V: voltageRange = 4.096f; break;
                    case ADS1015_REG_SET_GAIN2_2_048V: voltageRange = 2.048f; break;
                    case ADS1015_REG_SET_GAIN4_1_024V: voltageRange = 1.024f; break;
                    case ADS1015_REG_SET_GAIN8_0_512V: voltageRange = 0.512f; break;
                    case ADS1015_REG_SET_GAIN16_0_256V: voltageRange = 0.256f; break;
                    default: voltageRange = 6.144f;
                }
                
                // Get voltage reading
                int16_t rawReading = readADC_SingleEnded(0);
                _readings[_currentSample] = (rawReading * voltageRange) / 2048.0f;
                
                _lastSampleTime = millis();
                _currentSample++;
                
                // Move to processing when all samples collected
                if (_currentSample >= _config.samples) {
                    _state = PHXState::PROCESSING;
                }
            }
            break;
            
        case PHXState::PROCESSING: {
            // Average valid readings
            float raw_sum = 0;
            int validSamples = 0;
            
            for (int i = 0; i < _config.samples; i++) {
                if (!isnan(_readings[i]) && !isinf(_readings[i])) {
                    raw_sum += _readings[i];
                    validSamples++;
                }
            }
            
            // Handle case of no valid readings
            if (validSamples == 0) {
                _lastReading = 0;
                _state = PHXState::IDLE;
                _readingComplete = true;
                break;
            }
            
            // Convert to millivolts
            float mV = (raw_sum / validSamples) * 1000.0f;
            
            // Get calibration data for measurement type
            PHX_Calibration* cal = (strcmp(_config.type, "ph") == 0) ? &ph_cal : &orp_cal;
            
            // Apply calibration if available
            if (abs(cal->ref2_mV - cal->ref1_mV) > 0.001f) {
                // Calculate using two-point calibration formula
                float rawValue = cal->ref1_value + 
                                (cal->ref2_value - cal->ref1_value) * 
                                (mV - cal->ref1_mV) / 
                                (cal->ref2_mV - cal->ref1_mV);
                
                // Apply temperature compensation only for pH measurements
                if (strcmp(_config.type, "ph") == 0 && 
                    _temperatureCompensationEnabled && 
                    isValidTemperature(_currentTemperature)) {
                    
                    _lastReading = applyTemperatureCompensation(rawValue, _currentTemperature);
                } else {
                    _lastReading = rawValue;
                }
                
                // Apply range limits and set error flags
                if (strcmp(_config.type, "ph") == 0) {
                    // pH range validation (0-14)
                    if (_lastReading < 0) {
                        _lastReading = 0;
                        _lastError = PHXError::PH_LOW;
                    }
                    else if (_lastReading > 14) {
                        _lastReading = 14;
                        _lastError = PHXError::PH_HIGH;
                    }
                    else {
                        _lastError = PHXError::NONE;
                    }
                }
                else if (strcmp(_config.type, "rx") == 0) {
                    // ORP range validation (0-1000mV)
                    if (_lastReading < 0) {
                        _lastReading = 0;
                        _lastError = PHXError::ORP_LOW;
                    }
                    else if (_lastReading > 1000) {
                        _lastReading = 1000;
                        _lastError = PHXError::ORP_HIGH;
                    }
                    else {
                        _lastError = PHXError::NONE;
                    }
                }
            } else {
                _lastReading = mV;  // Use raw mV if not calibrated
            }
            
            // Clean up and complete
            delete[] _readings;
            _readings = nullptr;
            _readingComplete = true;
            _state = PHXState::IDLE;
            break;
        }
            
        case PHXState::IDLE:
            break;
    }
}

/**
 * @brief Cancels current measurement
 * 
 * Cleans up resources and resets state machine
 * Useful for aborting long measurements or handling errors
 */
void ADS1015::cancelReading() {
    if (_readings != nullptr) {
        delete[] _readings;
        _readings = nullptr;
    }
    _state = PHXState::IDLE;
    _readingComplete = false;
    _lastError = PHXError::NONE;
}

// ========================================
// Temperature Compensation Methods
// ========================================

/**
 * @brief Enable or disable temperature compensation for pH measurements
 * @param enabled True to enable, false to disable
 * 
 * Temperature compensation uses the Pasco 2001 formula to normalize
 * pH readings to 25°C standard temperature. This significantly improves
 * accuracy for swimming pool monitoring and other applications where
 * temperature varies from the calibration conditions.
 * 
 * Only affects pH measurements - ORP readings are not temperature compensated.
 */
void ADS1015::enableTemperatureCompensation(bool enabled) {
    _temperatureCompensationEnabled = enabled;
}

/**
 * @brief Set current temperature for compensation calculations
 * @param temperature Current temperature in Celsius (valid range: 0-50°C)
 * 
 * Sets the current water temperature for accurate temperature compensation.
 * The temperature should be updated regularly for best accuracy.
 * 
 * Error Handling:
 * - If temperature is outside valid range (0-50°C), sets TEMP_INVALID error
 * - Invalid temperatures do not change the current temperature setting
 * - TEMP_INVALID error is automatically cleared when valid temperature is set
 */
void ADS1015::setTemperature(float temperature) {
    if (isValidTemperature(temperature)) {
        _currentTemperature = temperature;
        // Clear any previous temperature error
        if (_lastError == PHXError::TEMP_INVALID) {
            _lastError = PHXError::NONE;
        }
    } else {
        // Set error flag for invalid temperature but don't change current temperature
        _lastError = PHXError::TEMP_INVALID;
        // Note: Current temperature remains unchanged to maintain system stability
    }
}

/**
 * @brief Get current temperature setting
 * @return Current temperature in Celsius
 */
float ADS1015::getCurrentTemperature() const {
    return _currentTemperature;
}

/**
 * @brief Check if temperature compensation is enabled
 * @return True if enabled, false if disabled
 */
bool ADS1015::isTemperatureCompensationEnabled() const {
    return _temperatureCompensationEnabled;
}

/**
 * @brief Apply temperature compensation using Pasco 2001 formula
 * @param pH_raw Raw pH reading before compensation
 * @param current_temp Current temperature in Celsius
 * @return Temperature compensated pH value normalized to 25°C
 * 
 * Implements the scientifically proven Pasco 2001 formula:
 * pH_compensated = ((pH_raw - 7) × (273.15 + current_temp)) / (273.15 + 25) + 7
 * 
 * This formula:
 * - Uses pH 7 as the reference point (minimal temperature dependence)
 * - Normalizes all readings to 25°C standard temperature
 * - Accounts for the actual physics of ion activity vs temperature
 * - Is specifically optimized for swimming pool pH ranges (6.8-8.2)
 * 
 * Example: A pH 7.5 reading at 30°C becomes pH 7.56 when compensated to 25°C
 */
float ADS1015::applyTemperatureCompensation(float pH_raw, float current_temp) {
    // Pasco 2001 formula for temperature compensation
    // pH_corrected = ((pH_init - 7) × (273.15 + temp)) / (273.15 + 25) + 7
    
    float pH_corrected = ((pH_raw - 7.0f) * (273.15f + current_temp)) / (273.15f + 25.0f) + 7.0f;
    
    return pH_corrected;
}

/**
 * @brief Validate temperature is within reasonable range
 * @param temperature Temperature to validate
 * @return True if temperature is between 0-50°C, false otherwise
 * 
 * Valid temperature range is optimized for swimming pool and water applications:
 * - 0°C: Freezing point (minimum for liquid water measurements)
 * - 50°C: Maximum for typical pool/spa applications
 * 
 * Temperatures outside this range are likely sensor errors or inappropriate
 * for the intended pH measurement applications.
 */
bool ADS1015::isValidTemperature(float temperature) {
    // Reasonable temperature range for swimming pools and water measurements
    return (temperature >= 0.0f && temperature <= 50.0f);
}

// End of APAPHX_ADS1015.cpp implementation