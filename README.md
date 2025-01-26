# APAPHX ADS1015 Library

Arduino library for pH and ORP/Redox measurements using ADS1015 ADC. Originally designed for APADevices PHX hardware but compatible with any pH/ORP analog circuits using ADS1015.

## Features

- Non-blocking operation with state machine design
- Two-point calibration for both pH and ORP
- Configurable sampling and filtering
- Built-in error detection
- Rolling average support
- No external dependencies (uses only Wire.h)
- pH range: 0-14 with validation
- ORP range: 0-1000mV with validation (configurable)

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
    ads1015PH.setGain(ADS1015_REG_SET_GAIN1_4_096V);
    ads1015RX.setGain(ADS1015_REG_SET_GAIN1_4_096V);
}

void loop() {
    // Configure reading
    PHXConfig config = {
        .type = "ph",        // "ph" or "rx"
        .samples = 100,      // samples per reading
        .delay_ms = 10,      // delay between samples
        .avg_buffer = 3      // rolling average size
    };
    
    // Take reading
    ads1015PH.startReading(config);
    while(ads1015PH.getState() != PHXState::IDLE) {
        ads1015PH.updateReading();
    }
    
    float ph = ads1015PH.getLastReading();
}
```

## Calibration

Two-point calibration is required for accurate readings:

```cpp
// pH Calibration (using pH 4 and 7 buffers by default, configurable - see bellow)
PHX_Calibration phCal;

// First point (pH 4)
phCal.ref1_mV = ads1015PH.calibratePHXReading("ph");
phCal.ref1_value = 4.0;

// Second point (pH 7)
phCal.ref2_mV = ads1015PH.calibratePHXReading("ph");
phCal.ref2_value = 7.0;

ads1015PH.calibratePHX("ph", phCal);

// ORP Calibration (using 475mV and 650mV solution buffers by default, edit if wanted)
PHX_Calibration orpCal;
orpCal.ref1_mV = ads1015RX.calibratePHXReading("rx");
orpCal.ref1_value = 475.0;
orpCal.ref2_mV = ads1015RX.calibratePHXReading("rx");
orpCal.ref2_value = 650.0;
ads1015RX.calibratePHX("rx", orpCal);
```

## Error Handling

```cpp
PHXError error = sensor.getLastError();
switch(error) {
    case PHXError::NONE: break;
    case PHXError::PH_LOW: // pH below 0
    case PHXError::PH_HIGH: // pH above 14
    case PHXError::ORP_LOW: // ORP below 0mV
    case PHXError::ORP_HIGH: // ORP above 1000mV
}
```

## Examples

The library includes three example sketches:

1. **APAPHX_basic_example**: Simple continuous reading with calibration
2. **APAPHX_intermediate_example**: Separate pH/ORP calibration with rolling average
3. **APAPHX_advanced_example**: Full feature demonstration including error handling

## Available Gain Settings

- `ADS1015_REG_SET_GAIN0_6_144V`: ±6.144V (recommended for APA Devicess pH module and for classic ph analog module as well)
- `ADS1015_REG_SET_GAIN1_4_096V`: ±4.096V (recommanded for APA Device ORP module) 
- `ADS1015_REG_SET_GAIN2_2_048V`: ±2.048V
- `ADS1015_REG_SET_GAIN4_1_024V`: ±1.024V
- `ADS1015_REG_SET_GAIN8_0_512V`: ±0.512V
- `ADS1015_REG_SET_GAIN16_0_256V`: ±0.256V

## License

This library is released under the MIT License.

## Author

APADevices [@kecup]

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Support

No support included! Use as is or leave it. Sorry about that.
