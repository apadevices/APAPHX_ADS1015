# APADevices PHX ADS1015 Library

Arduino library for pH and ORP/Redox measurements using ADS1015 ADC with optional temperature compensation. Originally designed for APADevices PHX hardware but compatible with any pH/ORP analog circuits using ADS1015.

## Features

- Non-blocking operation with state machine design
- Two-point calibration for both pH and ORP
- **NEW: Temperature compensation for pH measurements (Pasco 2001 formula)**
- Configurable sampling and filtering
- Built-in error detection
- Rolling average support
- No external dependencies (uses only Wire.h)
- pH range: 0-14 with validation
- ORP range: 0-1000mV with validation (configurable)
- Temperature range: 0-50°C (optimized for swimming pools)

## Installation

1. Download the library files
2. Create folder `APAPHX_ADS1015` in your Arduino libraries directory
3. Copy all files into the created folder
4. Restart Arduino IDE

## Hardware Setup

Connect your ADS1015 to Arduino:
- VDD to 3.3V or 5V
- GND to GND
- SCL to SCL (A5 on most Arduino boards)
- SDA to SDA (A4 on most Arduino boards)
- ADDR pin:
  - GND for address 0x48
  - VDD for address 0x49
  - SDA for address 0x4A
  - SCL for address 0x4B

## Basic Usage

```cpp
#include "APAPHX_ADS1015.h"

// Create instances
ADS1015 ads1015PH(ADDRESS_49);  // pH sensor
ADS1015 ads1015RX(ADDRESS_48);  // ORP sensor

void setup() {
    ads1015PH.begin();
    ads1015RX.begin();

    //Set preferred ADS gains aligned with your needs
    ads1015PH.setGain(ADS1015_REG_SET_GAIN0_6_144V);
    ads1015RX.setGain(ADS1015_REG_SET_GAIN1_4_096V);
}

void loop() {
    // Configure pH reading
    PHXConfig phConfig = {
        .type = "ph",        // pH measurement
        .samples = 100,      // samples per reading
        .delay_ms = 10,      // delay between samples
        .avg_buffer = 3      // rolling average size
    };
    
    // Configure ORP reading
    PHXConfig orpConfig = {
        .type = "rx",        // ORP measurement
        .samples = 100,      // samples per reading
        .delay_ms = 10,      // delay between samples
        .avg_buffer = 3      // rolling average size
    };
    
    // Take pH reading
    ads1015PH.startReading(phConfig);
    while(ads1015PH.getState() != PHXState::IDLE) {
        ads1015PH.updateReading();
    }
    
    // Take ORP reading
    ads1015RX.startReading(orpConfig);
    while(ads1015RX.getState() != PHXState::IDLE) {
        ads1015RX.updateReading();
    }
    
    float ph = ads1015PH.getLastReading();
    float orp = ads1015RX.getLastReading();
}
```

## Temperature Compensation (NEW!)

Temperature compensation improves pH accuracy using the scientifically proven Pasco 2001 formula, perfect for swimming pool applications:

### Basic Temperature Compensation
```cpp
// Enable temperature compensation
ads1015PH.enableTemperatureCompensation(true);

// Set current temperature (in Celsius)
ads1015PH.setTemperature(28.5);  // Pool temperature

// Take temperature-compensated reading
PHXConfig config = {"ph", 100, 10, 3};
ads1015PH.startReading(config);
while(ads1015PH.getState() != PHXState::IDLE) {
    ads1015PH.updateReading();
}

float compensatedPH = ads1015PH.getLastReading();
```

### Temperature Compensation Methods
```cpp
// Enable/disable temperature compensation
ads1015PH.enableTemperatureCompensation(true);

// Set current temperature (0-50°C range)
ads1015PH.setTemperature(25.4);

// Get current temperature setting
float temp = ads1015PH.getCurrentTemperature();

// Check if compensation is enabled
bool enabled = ads1015PH.isTemperatureCompensationEnabled();
```

### Benefits
- **Improved accuracy**: ±0.1 pH improvement for temperature variations
- **Swimming pool optimized**: Uses pool-specific Pasco 2001 formula
- **Automatic normalization**: All readings referenced to 25°C standard
- **Error handling**: Invalid temperatures (outside 0-50°C) are detected
- **Backward compatible**: Disabled by default, doesn't affect existing code

## Calibration

Two-point calibration is required for accurate readings:

```cpp
// pH Calibration (using pH 4 and 7 buffers by default, configurable)
PHX_Calibration phCal;

// First point (pH 4)
phCal.ref1_mV = ads1015PH.calibratePHXReading("ph");
phCal.ref1_value = 4.0;

// Second point (pH 7)
phCal.ref2_mV = ads1015PH.calibratePHXReading("ph");
phCal.ref2_value = 7.0;

ads1015PH.calibratePHX("ph", phCal);

// ORP Calibration (using 475mV and 650mV solution buffers by default)
PHX_Calibration orpCal;
orpCal.ref1_mV = ads1015RX.calibratePHXReading("rx");
orpCal.ref1_value = 475.0;
orpCal.ref2_mV = ads1015RX.calibratePHXReading("rx");
orpCal.ref2_value = 650.0;
ads1015RX.calibratePHX("rx", orpCal);
```

**Note**: Temperature compensation is automatically disabled during calibration (correct behavior).

## Error Handling

```cpp
PHXError error = sensor.getLastError();
switch(error) {
    case PHXError::NONE: break;
    case PHXError::PH_LOW: // pH below 0
    case PHXError::PH_HIGH: // pH above 14
    case PHXError::ORP_LOW: // ORP below 0mV
    case PHXError::ORP_HIGH: // ORP above 1000mV
    case PHXError::TEMP_INVALID: // Temperature outside 0-50°C range
}
```

## Examples

The library includes five example sketches with a logical learning progression:

1. **APAPHX_basic_example**: Simple continuous reading with calibration
2. **APAPHX_intermediate_example**: Separate pH/ORP calibration with rolling average
3. **APAPHX_advanced_example**: Full feature demonstration including error handling
4. **APAPHX_basic_temperature_example**: Temperature compensation demonstration
5. **APAPHX_eeprom_calsave_example**: Permanent calibration storage with automatic loading
6. **APAPHX_chlorine_levels_example**: Extract approx. CL levels (ppm) based on messurements 

### Learning Progression

The progression makes sense for users:

* Start with **Basic** to understand core functionality
* Move to **Intermediate** for better control and rolling averages
* Use **Advanced** for production applications with full error handling
* Add **Basic Temperature** to learn temperature compensation
* Implement **EEPROM CalSave** for permanent, professional installations

The EEPROM CalSave example represents the most complete solution - a professional pH/ORP monitoring system that remembers its calibration permanently and handles all edge cases. This would be perfect for swimming pool controllers, aquarium systems, or any application where the device needs to work immediately after power cycles without manual recalibration.

## Available Gain Settings

- `ADS1015_REG_SET_GAIN0_6_144V`: ±6.144V (recommended for APA Devices pH module and classic pH analog modules)
- `ADS1015_REG_SET_GAIN1_4_096V`: ±4.096V (recommended for APA Device ORP module) 
- `ADS1015_REG_SET_GAIN2_2_048V`: ±2.048V
- `ADS1015_REG_SET_GAIN4_1_024V`: ±1.024V
- `ADS1015_REG_SET_GAIN8_0_512V`: ±0.512V
- `ADS1015_REG_SET_GAIN16_0_256V`: ±0.256V

## Version History

### v1.1.0 (Current)
- Added temperature compensation using Pasco 2001 formula
- Added temperature error handling (TEMP_INVALID)
- Optimized for swimming pool applications
- Backward compatible with v1.0.0
- EEPROM example added

### v1.0.0
- Initial release
- Basic pH/ORP measurements
- Two-point calibration
- Non-blocking operation

## License

This library is released under the MIT License.

## Author

APADevices [@kecup]

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Support


No support included! Use as is or leave it. Sorry about that.
