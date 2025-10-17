#include "APAPHX_ADS1015.h"

ADS1015 pHSensor(ADDRESS_49);

void setup() {
    pHSensor.begin();
    pHSensor.enableTemperatureCompensation(true);
}

void loop() {
    pHSensor.setTemperature(28.5);  // Current pool temp
    
    PHXConfig config = {"ph", 100, 10, 3};
    pHSensor.startReading(config);
    while(pHSensor.getState() != PHXState::IDLE) {
        pHSensor.updateReading();
    }
    
    float compensatedPH = pHSensor.getLastReading();
    // pH is now temperature-compensated to 25°C standard!
}