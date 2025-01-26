/**
 * @file APAPHX_ADS1015.h
 * @brief ADS1015 ADC Library for pH and ORP/Redox Measurements
 * @version 1.0
 * 
 * This library provides a standalone implementation for pH and ORP/Redox measurements
 * using the ADS1015 12-bit ADC. While originally designed for APADevices PHX hardware,
 * it's compatible with any pH/ORP analog circuits using ADS1015 for digitization.
 * 
 * Key Features:
 * - Standalone operation (no dependencies except Wire.h)
 * - Non-blocking operation with state machine design
 * - Two-point calibration for both pH and ORP measurements
 * - Configurable sampling and filtering
 * - Built-in error detection and range validation
 * - Rolling average support for stable readings
 * - Voltage range: ±6.144V to ±0.256V (programmable gain)
 * 
 * Measurement Ranges:
 * - pH: 0-14 with out-of-range detection
 * - ORP: 0-1000mV with out-of-range detection
 * 
 * Example Usage:
 * @code
 * ADS1015 pHSensor(0x49);
 * pHSensor.begin();
 * pHSensor.setGain(ADS1015_REG_SET_GAIN1_4_096V);
 * // Configure readings and start measuring
 * @endcode
 */

#ifndef APAPHX_ADS1015_H
#define APAPHX_ADS1015_H

#include <Arduino.h>
#include <Wire.h>

// ADS1015 I2C addresses
#define ADDRESS_48     0x48  // ADDR pin connected to GND
#define ADDRESS_49     0x49  // ADDR pin connected to VDD
#define ADDRESS_4A     0x4A  // ADDR pin connected to SDA
#define ADDRESS_4B     0x4B  // ADDR pin connected to SCL

// Pointer Register
#define ADS1015_REG_POINTER_CONVERT 0x00  // Conversion register
#define ADS1015_REG_POINTER_CONFIG  0x01  // Configuration register

// Config Register settings
#define ADS1015_REG_CONFIG_OS_SINGLE    0x8000  // Start single conversion
#define ADS1015_REG_CONFIG_MUX_SINGLE_0 0x4000  // Single-ended AIN0
#define ADS1015_REG_CONFIG_MUX_SINGLE_1 0x5000  // Single-ended AIN1
#define ADS1015_REG_CONFIG_MUX_SINGLE_2 0x6000  // Single-ended AIN2
#define ADS1015_REG_CONFIG_MUX_SINGLE_3 0x7000  // Single-ended AIN3

// Programmable gain settings
#define ADS1015_REG_SET_GAIN0_6_144V    0x0000  // +/-6.144V range = Gain 2/3
#define ADS1015_REG_SET_GAIN1_4_096V    0x0200  // +/-4.096V range = Gain 1
#define ADS1015_REG_SET_GAIN2_2_048V    0x0400  // +/-2.048V range = Gain 2
#define ADS1015_REG_SET_GAIN4_1_024V    0x0600  // +/-1.024V range = Gain 4
#define ADS1015_REG_SET_GAIN8_0_512V    0x0800  // +/-0.512V range = Gain 8
#define ADS1015_REG_SET_GAIN16_0_256V   0x0A00  // +/-0.256V range = Gain 16

#define ADS1015_REG_CONFIG_MODE_CONTIN  0x0000  // Continuous conversion mode
#define ADS1015_REG_CONFIG_DR_1600SPS   0x0080  // 1600 samples per second

/**
 * @brief Measurement state machine states
 */
enum class PHXState {
    IDLE,       ///< Ready for new measurement
    COLLECTING, ///< Collecting samples
    PROCESSING  ///< Processing collected data
};

/**
 * @brief Error conditions for measurements
 */
enum class PHXError {
    NONE,     ///< No error
    PH_LOW,   ///< pH below 0
    PH_HIGH,  ///< pH above 14
    ORP_LOW,  ///< ORP below 0mV
    ORP_HIGH  ///< ORP above 1000mV
};

/**
 * @brief Two-point calibration data structure
 */
struct PHX_Calibration {
    float ref1_mV;     ///< First calibration point mV reading
    float ref2_mV;     ///< Second calibration point mV reading
    float ref1_value;  ///< First reference value (pH 4 or 475mV)
    float ref2_value;  ///< Second reference value (pH 7 or 650mV)
};

/**
 * @brief Reading configuration structure
 */
struct PHXConfig {
    const char* type;  ///< Measurement type ("ph" or "rx")
    int samples;       ///< Number of samples to collect
    int delay_ms;      ///< Delay between samples
    uint8_t avg_buffer;///< Size of rolling average buffer (1-10)
};

/**
 * @brief ADS1015 ADC controller for pH/ORP measurements
 */
class ADS1015 {
public:
    static const uint8_t MAX_AVG_BUFFER = 10;
    static constexpr float STABILITY_THRESHOLD = 0.5f;

    /**
     * @brief Construct a new ADS1015 instance
     * @param i2cAddress I2C address of the ADS1015
     */
    ADS1015(uint8_t i2cAddress);

    /**
     * @brief Initialize the ADS1015
     */
    void begin();

    /**
     * @brief Set the ADC gain
     * @param gain Gain setting (use ADS1015_REG_SET_GAINx defines)
     */
    void setGain(uint16_t gain);

    /**
     * @brief Read single-ended ADC value
     * @param channel ADC channel (0-3)
     * @return int16_t Raw ADC value
     */
    int16_t readADC_SingleEnded(uint8_t channel);

    /**
     * @brief Store calibration data
     * @param type Measurement type ("ph" or "rx")
     * @param cal Calibration data
     */
    void calibratePHX(const char* type, PHX_Calibration &cal);

    /**
     * @brief Get stable calibration reading
     * @param type Measurement type ("ph" or "rx")
     * @return float Stable reading value
     */
    float calibratePHXReading(const char* type);

    /**
     * @brief Start a new reading sequence
     * @param config Reading configuration
     */
    void startReading(const PHXConfig& config);

    /**
     * @brief Update ongoing reading process
     */
    void updateReading();

    /**
     * @brief Cancel current reading
     */
    void cancelReading();
    
    // Status getters
    PHXState getState() const { return _state; }
    bool isReadingComplete() const { return _readingComplete; }
    float getLastReading() const { return _lastReading; }
    PHXError getLastError() const { return _lastError; }

private:
    uint8_t _i2cAddress;
    uint16_t _gain = ADS1015_REG_SET_GAIN0_6_144V;
    PHXState _state = PHXState::IDLE;
    PHXError _lastError = PHXError::NONE;
    bool _readingComplete = false;
    float _lastReading = 0;
    
    PHX_Calibration ph_cal = {0, 0, 4, 7};
    PHX_Calibration orp_cal = {0, 0, 475, 650};
    
    PHXConfig _config;
    float* _readings = nullptr;
    float* _lastReadings = nullptr;
    uint8_t _avgBufferSize = 1;
    int _currentSample = 0;
    uint8_t _readingIndex = 0;
    bool _rollingAverageReady = false;
    unsigned long _lastSampleTime = 0;
    
    void writeRegister(uint8_t i2cAddress, uint8_t reg, uint16_t value);
    uint16_t readRegister(uint8_t i2cAddress, uint8_t reg);
};

#endif // APAPHX_ADS1015_H