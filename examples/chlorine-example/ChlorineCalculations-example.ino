/**
 * @file TwoFunctionChlorineCalculator.ino
 * @brief Two functions: calculateFreeChlorine and calculateBoundChlorine
 * @author APADevices [@kecup]
 * 
 * Function 1: Calculate free chlorine from ORP, pH, temperature
 * Function 2: Estimate bound chlorine from ORP, pH, temperature (rough estimation)
 */

/**
 * @brief Calculate free chlorine from pH, ORP, and temperature
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
    
    // Simple 2^x calculation
    float power_result = 1.0f;
    if (exponent > 0) {
        if (exponent >= 4.0f) {
            power_result = 16.0f;  // Safety limit
        } else {
            // Handle integer part
            int whole_part = (int)exponent;
            for (int i = 0; i < whole_part; i++) {
                power_result *= 2.0f;
            }
            
            // Handle fractional part
            float fraction = exponent - whole_part;
            if (fraction > 0.5f) power_result *= 1.4f;
            else if (fraction > 0.25f) power_result *= 1.2f;
        }
    }
    
    // Calculate final chlorine value
    float chlorine = 0.5f * power_result;
    
    // Apply practical limits
    if (chlorine < 0.0f) return 0.0f;
    if (chlorine > 10.0f) return 10.0f;
    
    return chlorine;
}

/**
 * @brief Estimate bound chlorine from pH, ORP, and temperature
 * @param orp_mV ORP reading in millivolts
 * @param pH pH reading
 * @param temp_C Temperature in Celsius
 * @return Estimated bound chlorine in ppm (rough approximation)
 * 
 * WARNING: This is a rough estimation based on typical pool conditions.
 * Actual bound chlorine depends on many factors not measured here.
 * For accurate results, use DPD test kit for total chlorine.
 */
float calculateBoundChlorine(float orp_mV, float pH, float temp_C) {
    // First get free chlorine
    float free_chlorine = calculateFreeChlorine(orp_mV, pH, temp_C);
    
    // Estimate total chlorine based on conditions that promote chloramine formation
    float estimated_total = free_chlorine;
    
    // pH factor: Higher pH (>7.5) promotes chloramine formation
    // For every 0.1 pH unit above 7.5, add ~5% more chloramines
    if (pH > 7.5f) {
        float pH_factor = (pH - 7.5f) * 0.5f;  // 0.5 = 50% increase per pH unit
        estimated_total += free_chlorine * pH_factor;
    }
    
    // Temperature factor: Higher temperature (>25°C) promotes chloramine formation
    // For every degree above 25°C, add ~2% more chloramines
    if (temp_C > 25.0f) {
        float temp_factor = (temp_C - 25.0f) * 0.02f;  // 2% per degree
        estimated_total += free_chlorine * temp_factor;
    }
    
    // Low ORP for given conditions suggests chloramine presence
    // Expected ORP for clean water with this free chlorine level
    float expected_orp = 716.0f + (free_chlorine / 0.5f) * 26.7f * 0.693f;  // log(2) ≈ 0.693
    expected_orp += (25.0f - temp_C) * 1.25f - (pH - 7.38f) * 30.0f;
    
    // If actual ORP is lower than expected, estimate chloramine depression
    float orp_depression = expected_orp - orp_mV;
    if (orp_depression > 20.0f) {  // Significant depression suggests chloramines
        float chloramine_from_orp = orp_depression / 60.0f;  // ~60mV per ppm chloramine
        estimated_total += chloramine_from_orp;
    }
    
    // Base chloramine level for typical pool conditions (empirical)
    // Most pools have some level of chloramines
    float base_chloramines = free_chlorine * 0.1f;  // 10% of free chlorine as baseline
    estimated_total += base_chloramines;
    
    // Calculate bound chlorine
    float bound_chlorine = estimated_total - free_chlorine;
    
    // Apply practical limits
    if (bound_chlorine < 0.0f) return 0.0f;
    if (bound_chlorine > free_chlorine * 2.0f) return free_chlorine * 2.0f;  // Limit to 2x free chlorine
    if (bound_chlorine > 3.0f) return 3.0f;  // Absolute upper limit
    
    return bound_chlorine;
}

void setup() {
    Serial.begin(115200);
    Serial.println("Two Function Chlorine Calculator");
    Serial.println("================================");
    Serial.println("Function 1: Free Chlorine (accurate)");
    Serial.println("Function 2: Bound Chlorine (rough estimate)");
    Serial.println();
}

void loop() {
    // Example measurements - replace with your actual PHX sensor readings
    float orp = 700.0f;     // Your ORP reading in mV
    float pH = 7.6f;        // Your pH reading
    float temp = 28.0f;     // Your temperature in °C
    
    // Calculate both values
    float free_cl = calculateFreeChlorine(orp, pH, temp);
    float bound_cl = calculateBoundChlorine(orp, pH, temp);
    float total_cl = free_cl + bound_cl;
    
    // Display results
    Serial.println("=== Pool Chemistry Analysis ===");
    Serial.print("ORP: "); Serial.print(orp, 0); Serial.println(" mV");
    Serial.print("pH: "); Serial.println(pH, 1);
    Serial.print("Temperature: "); Serial.print(temp, 1); Serial.println("°C");
    Serial.println();
    
    Serial.println("Chlorine Analysis:");
    Serial.print("Free Chlorine: "); Serial.print(free_cl, 2); Serial.println(" ppm (calculated)");
    Serial.print("Bound Chlorine: "); Serial.print(bound_cl, 2); Serial.println(" ppm (estimated)");
    Serial.print("Total Chlorine: "); Serial.print(total_cl, 2); Serial.println(" ppm (estimated)");
    Serial.println();
    
    // Water quality assessment
    Serial.println("Water Quality Assessment:");
    
    // Free chlorine status
    if (free_cl < 1.0f) {
        Serial.println("⚠️  Free chlorine LOW - Add sanitizer");
    } else if (free_cl > 3.0f) {
        Serial.println("⚠️  Free chlorine HIGH - Reduce dosing");
    } else {
        Serial.println("✅ Free chlorine level good");
    }
    
    // Bound chlorine status
    if (bound_cl > 0.5f) {
        Serial.println("⚠️  HIGH chloramines estimated!");
        Serial.println("   Consider shock treatment");
        Serial.println("   Verify with DPD test kit");
    } else if (bound_cl > 0.2f) {
        Serial.println("⚠️  Moderate chloramines estimated");
        Serial.println("   Monitor water quality");
    } else {
        Serial.println("✅ Low chloramines estimated");
    }
    
    // Conditions promoting chloramines
    Serial.println();
    Serial.println("Condition Analysis:");
    if (pH > 7.6f) {
        Serial.println("• High pH promotes chloramine formation");
    }
    if (temp > 27.0f) {
        Serial.println("• High temperature promotes chloramine formation");
    }
    if (orp < 650.0f) {
        Serial.println("• Low ORP may indicate chloramine presence");
    }
    
    Serial.println();
    Serial.println("NOTE: Bound chlorine is estimated only.");
    Serial.println("For accurate total chlorine, use DPD test kit.");
    Serial.println("===============================");
    Serial.println();
    
    delay(5000);
}

/**
 * @brief Example of using both functions together
 */
void exampleUsage() {
    // Your PHX sensor readings
    float orp = 720.0f;
    float pH = 7.4f;
    float temp = 25.0f;
    
    // Get both values
    float free_ppm = calculateFreeChlorine(orp, pH, temp);
    float bound_ppm = calculateBoundChlorine(orp, pH, temp);
    
    // Display
    Serial.print("Free: "); Serial.print(free_ppm, 2);
    Serial.print(" ppm, Bound: "); Serial.print(bound_ppm, 2);
    Serial.println(" ppm");
    
    // Quick status
    if (bound_ppm > 0.3f) {
        Serial.println("Action: Consider shock treatment");
    }
}