#include "../include/temp_env.h"
#include <stdio.h>
#include <math.h>

int main(void)
{
    printf("=== mini-sensor-actuator: Environmental Sensor Demo ===\n\n");

    env_sensor_t env;
    env_init(&env);

    printf("--- Temperature & Humidity ---\n");
    env_temp_humi_init(&env.temp_humi);
    env.temp_humi.temperature_c = 25.3f;
    env.temp_humi.humidity_pct = 62.0f;
    env_temp_humi_update(&env.temp_humi, 25.3f, 62.0f, 1000);
    env.has_temp_humi = 1;

    float dew = env_dew_point(25.3f, 62.0f);
    float abs_hum = env_absolute_humidity(25.3f, 62.0f);
    float heat_idx = env_heat_index_c(25.3f, 62.0f);
    float vp = env_vapor_pressure_hpa(25.3f);

    printf("  Temperature:  %.1f C\n", env.temp_humi.temperature_c);
    printf("  Humidity:     %.1f %%\n", env.temp_humi.humidity_pct);
    printf("  Dew Point:    %.2f C\n", dew);
    printf("  Abs Humidity: %.3f g/m3\n", abs_hum);
    printf("  Heat Index:   %.2f C\n", heat_idx);
    printf("  Vapor Press:  %.2f hPa\n", vp);

    printf("\n--- Barometric Pressure ---\n");
    env_baro_init(&env.pressure);
    env_baro_update(&env.pressure, 1013.25f, 25.3f);
    env.has_pressure = 1;

    float altitude = env_altitude_from_pressure(1013.25f, PRESSURE_SEA_LEVEL_HPA);
    float slp = env_sea_level_pressure(980.0f, 300.0f, 22.0f);

    printf("  Pressure:      %.2f hPa\n", env.pressure.pressure_hpa);
    printf("  Altitude:      %.2f m\n", altitude);
    printf("  SLP at 300m:   %.2f hPa (from 980hPa @ 22C)\n", slp);

    printf("\n--- Gas Sensors ---\n");
    env_gas_init(&env.gas);
    env_gas_update_voc(&env.gas, 150.0f, 1000);
    env_gas_update_co2(&env.gas, 420.0f, 1000);
    env_gas_update_eco2(&env.gas, 450.0f, 1000);
    env_gas_update_resistance(&env.gas, 50000, 1000);
    env.has_gas = 1;

    float comp_voc = env_gas_compensate_humidity(150.0f, 62.0f);
    printf("  VOC:       %.0f ppb (compensated: %.0f ppb)\n",
           env.gas.voc_ppb, comp_voc);
    printf("  CO2:       %.0f ppm\n", env.gas.co2_ppm);
    printf("  eCO2:      %.0f ppm\n", env.gas.eco2_ppm);
    printf("  Resistance: %u ohm\n", env.gas.resistance_ohm);

    printf("\n--- PM2.5 / Dust ---\n");
    env_dust_init(&env.dust);
    env_dust_update(&env.dust, 5.0f, 12.0f, 18.0f, 250.0f, 1000);
    env.has_dust = 1;

    float aqi = env_dust_aqi_pm25(12.0f);
    printf("  PM1.0:   %.1f ug/m3\n", env.dust.pm1_0_ugm3);
    printf("  PM2.5:   %.1f ug/m3  AQI: %.0f\n",
           env.dust.pm2_5_ugm3, aqi);
    printf("  PM10:    %.1f ug/m3\n", env.dust.pm10_ugm3);

    printf("\n--- Wind ---\n");
    env_wind_init(&env.wind);
    env_wind_update(&env.wind, 4.2f, 225.0f, 7.1f, 1000);
    env.has_wind = 1;

    wind_direction_bin_t dir_bin = env_wind_to_bin(225.0f);
    printf("  Speed:     %.1f m/s\n", env.wind.speed_ms);
    printf("  Direction: %.0f deg (%s)\n",
           env.wind.direction_deg, env_wind_bin_name(dir_bin));
    printf("  Gust:      %.1f m/s\n", env.wind.gust_ms);

    printf("\n--- Rain Gauge ---\n");
    env_rain_init(&env.rain);
    env_rain_tip(&env.rain, 1000);
    env_rain_tip(&env.rain, 3500);
    env_rain_tip(&env.rain, 7200);
    env.has_rain = 1;

    float rate = env_rain_rate(&env.rain);
    printf("  Total:     %.2f mm\n", env.rain.total_mm);
    printf("  Rate:      %.2f mm/h\n", rate);
    printf("  Tips:      %u\n", env.rain.tip_count);

    printf("\n--- UV Index ---\n");
    env_uv_init(&env.uv);
    env_uv_update(&env.uv, 7.5f, 1000);
    env.has_uv = 1;

    uv_index_level_t uv_level = env_uv_classify(7.5f);
    printf("  UV Index:  %.1f\n", env.uv.uv_index);
    printf("  Level:     %s\n", env_uv_level_name(uv_level));

    printf("\n--- Full Summary ---\n");
    env_print_summary(&env);

    printf("\nDemo complete.\n");
    return 0;
}
