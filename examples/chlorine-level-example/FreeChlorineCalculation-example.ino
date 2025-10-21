/**
 * @file FreeChlorineCalculation.ino
 * @brief Single function to extract free chlorine from pH, ORP, and temperature
 * @author APADevices [@kecup]
 * 
 * One function, no dependencies, just the essential calculation.
 */

/**
 * @brief Calculate free chlorine from pH, ORP, and temperature getthered using APAPHX library
 * @param orp_mV ORP reading in millivolts
 * @param pH pH reading
 * @param temp_C Temperature in Celsius
 * @return Free chlorine in ppm
 */
float calculateFreeChlorine(float orp_mV, float pH, float temp_C) {
    // Temperature and pH compensation
    float correctedORP = orp_mV + (25.0f - temp_C) * 1.25f - (pH - 7.38f) * 30.0f;
    
    // Calculate power of 2 for the logarithmic relationship
    float exponent = (correctedORP - 716.0f) / 26.7f;
    
    // Simple 2^x calculation (integrated power function)
    float power_result = 1.0f;
    if (exponent > 0) {
        if (exponent >= 4.0f) {
            power_result = 16.0f;  // 2^4, safety limit
        } else {
            // Handle integer part
            int whole_part = (int)exponent;
            for (int i = 0; i < whole_part; i++) {
                power_result *= 2.0f;
            }
            
            // Handle fractional part (simple approximation)
            float fraction = exponent - whole_part;
            if (fraction > 0.5f) power_result *= 1.4f;       // ~2^0.5
            else if (fraction > 0.25f) power_result *= 1.2f; // ~2^0.25
        }
    }
    
    // Calculate final chlorine value
    float chlorine = 0.5f * power_result;
    
    // Apply practical limits
    if (chlorine < 0.0f) return 0.0f;
    if (chlorine > 10.0f) return 10.0f;
    
    return chlorine;
}

void setup() {
    Serial.begin(115200);
    Serial.println("Simple Free Chlorine Calculator");
    Serial.println("========================================");
}

void loop() {
    // Example usage - replace with your actual PHX readings
    float orp = 720.0f;    // Your calibrated ORP reading in mV
    float pH = 7.2f;       // Your calibrated pH reading
    float temp = 25.0f;    // Your temperature reading in °C
    
    // Single function call to get free chlorine
    float chlorine_ppm = calculateFreeChlorine(orp, pH, temp);
    
    // Display result
    Serial.print("Estimated Chlorine level: ");
    Serial.print(chlorine_ppm, 2);
    Serial.println(" ppm");
    
    delay(2000);
}