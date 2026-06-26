#include "../include/temp_env.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

float env_altitude_from_pressure(float pressure_hpa, float sea_level_hpa)
{
    float ratio = pressure_hpa / sea_level_hpa;
    float exponent = 1.0f / 5.255f;
    return ALTITUDE_SCALE_FACTOR * (1.0f - powf(ratio, exponent));
}

float env_sea_level_pressure(float pressure_hpa, float altitude_m, float temperature_c)
{
    float temp_k = temperature_c + 273.15f;
    float exponent = ((-altitude_m) / (29.271f * temp_k));
    return pressure_hpa * expf(exponent);
}

float env_dew_point(float temperature_c, float humidity_pct)
{
    float a = 17.27f, b = 237.7f;
    float alpha = ((a * temperature_c) / (b + temperature_c)) + logf(humidity_pct / 100.0f);
    return (b * alpha) / (a - alpha);
}

float env_absolute_humidity(float temperature_c, float humidity_pct)
{
    float svp = 6.112f * expf((17.67f * temperature_c) / (temperature_c + 243.5f));
    return (2.1674f * humidity_pct * svp) / (temperature_c + 273.15f);
}

float env_heat_index_c(float temperature_c, float humidity_pct)
{
    float t = temperature_c, h = humidity_pct;
    float hi = 0.5f * (t + 61.0f + ((t - 68.0f) * 1.2f) + (h * 0.094f));
    if ((hi + t) * 0.5f < 26.7f) return hi;
    hi = -42.379f + 2.04901523f*t + 10.14333127f*h
       - 0.22475541f*t*h - 0.00683783f*t*t - 0.05481717f*h*h
       + 0.00122874f*t*t*h + 0.00085282f*t*h*h - 0.00000199f*t*t*h*h;
    return hi;
}

float env_vapor_pressure_hpa(float temperature_c)
{
    return 6.112f * expf((17.67f * temperature_c) / (temperature_c + 243.5f));
}

void env_temp_humi_init(temp_humi_t *th)
{
    memset(th, 0, sizeof(*th));
}

void env_temp_humi_update(temp_humi_t *th, float temp_c, float hum_pct, uint32_t ts)
{
    th->temperature_c = temp_c;
    th->humidity_pct = hum_pct;
    th->timestamp_ms = ts;
}

void env_baro_init(baro_pressure_t *bp)
{
    memset(bp, 0, sizeof(*bp));
}

void env_baro_update(baro_pressure_t *bp, float pressure_hpa, float temp_c)
{
    bp->pressure_hpa = pressure_hpa;
    bp->temperature_c = temp_c;
}

void env_gas_init(gas_sensor_t *gas)
{
    memset(gas, 0, sizeof(*gas));
}

void env_gas_update_voc(gas_sensor_t *gas, float voc_ppb, uint32_t ts)
{
    gas->voc_ppb = voc_ppb;
    gas->timestamp_ms = ts;
}

void env_gas_update_co2(gas_sensor_t *gas, float co2_ppm, uint32_t ts)
{
    gas->co2_ppm = co2_ppm;
    gas->timestamp_ms = ts;
}

void env_gas_update_eco2(gas_sensor_t *gas, float eco2_ppm, uint32_t ts)
{
    gas->eco2_ppm = eco2_ppm;
    gas->timestamp_ms = ts;
}

void env_gas_update_resistance(gas_sensor_t *gas, uint32_t resistance_ohm, uint32_t ts)
{
    gas->resistance_ohm = resistance_ohm;
    gas->timestamp_ms = ts;
}

float env_gas_compensate_humidity(float reading, float humidity_pct)
{
    float comp = 1.0f + 0.0033f * (humidity_pct - 33.0f);
    return reading * comp;
}

void env_dust_init(pm25_dust_t *dust)
{
    memset(dust, 0, sizeof(*dust));
}

void env_dust_update(pm25_dust_t *dust, float pm1, float pm25, float pm10, float count, uint32_t ts)
{
    dust->pm1_0_ugm3 = pm1;
    dust->pm2_5_ugm3 = pm25;
    dust->pm10_ugm3 = pm10;
    dust->particle_count_100ml = count;
    dust->timestamp_ms = ts;
}

float env_dust_aqi_pm25(float pm25_ugm3)
{
    if (pm25_ugm3 <= 12.0f)       return (50.0f / 12.0f) * pm25_ugm3;
    if (pm25_ugm3 <= 35.4f)       return ((100.0f - 51.0f) / (35.4f - 12.1f)) * (pm25_ugm3 - 12.1f) + 51.0f;
    if (pm25_ugm3 <= 55.4f)       return ((150.0f - 101.0f) / (55.4f - 35.5f)) * (pm25_ugm3 - 35.5f) + 101.0f;
    if (pm25_ugm3 <= 150.4f)      return ((200.0f - 151.0f) / (150.4f - 55.5f)) * (pm25_ugm3 - 55.5f) + 151.0f;
    if (pm25_ugm3 <= 250.4f)      return ((300.0f - 201.0f) / (250.4f - 150.5f)) * (pm25_ugm3 - 150.5f) + 201.0f;
    if (pm25_ugm3 <= 350.4f)      return ((400.0f - 301.0f) / (350.4f - 250.5f)) * (pm25_ugm3 - 250.5f) + 301.0f;
    return ((500.0f - 401.0f) / (500.4f - 350.5f)) * (pm25_ugm3 - 350.5f) + 401.0f;
}

void env_wind_init(wind_data_t *wind)
{
    memset(wind, 0, sizeof(*wind));
}

void env_wind_update(wind_data_t *wind, float speed_ms, float dir_deg, float gust_ms, uint32_t ts)
{
    wind->speed_ms = speed_ms;
    wind->direction_deg = dir_deg;
    wind->gust_ms = gust_ms;
    wind->timestamp_ms = ts;
}

wind_direction_bin_t env_wind_to_bin(float direction_deg)
{
    while (direction_deg < 0.0f) direction_deg += 360.0f;
    while (direction_deg >= 360.0f) direction_deg -= 360.0f;
    float bin_size = 360.0f / WIND_DIRECTION_BINS;
    float offset = bin_size * 0.5f;
    return (wind_direction_bin_t)((uint16_t)((direction_deg + offset) / bin_size) % WIND_DIRECTION_BINS);
}

const char* env_wind_bin_name(wind_direction_bin_t bin)
{
    static const char* names[WIND_DIRECTION_BINS] = {
        "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
        "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
    };
    return names[bin % WIND_DIRECTION_BINS];
}

void env_rain_init(rain_gauge_t *rain)
{
    memset(rain, 0, sizeof(*rain));
}

void env_rain_tip(rain_gauge_t *rain, uint32_t timestamp_ms)
{
    rain->tip_count++;
    rain->total_mm += RAIN_BUCKET_TIP_MM;
    if (rain->last_tip_ms > 0 && timestamp_ms > rain->last_tip_ms) {
        float dt_h = (timestamp_ms - rain->last_tip_ms) / 3600000.0f;
        if (dt_h > 0.0f) rain->intensity_mmh = RAIN_BUCKET_TIP_MM / dt_h;
    }
    rain->last_tip_ms = timestamp_ms;
    rain->timestamp_ms = timestamp_ms;
}

float env_rain_rate(const rain_gauge_t *rain)
{
    if (rain->last_tip_ms == 0) return 0.0f;
    return rain->intensity_mmh;
}

void env_rain_reset(rain_gauge_t *rain)
{
    memset(rain, 0, sizeof(*rain));
}

void env_uv_init(uv_sensor_t *uv)
{
    memset(uv, 0, sizeof(*uv));
}

void env_uv_update(uv_sensor_t *uv, float index, uint32_t ts)
{
    uv->uv_index = index;
    uv->level = env_uv_classify(index);
    uv->timestamp_ms = ts;
}

uv_index_level_t env_uv_classify(float index)
{
    if (index < 3.0f)  return UV_LOW;
    if (index < 6.0f)  return UV_MODERATE;
    if (index < 8.0f)  return UV_HIGH;
    if (index < 11.0f) return UV_VERY_HIGH;
    return UV_EXTREME;
}

const char* env_uv_level_name(uv_index_level_t level)
{
    static const char* names[] = {"Low", "Moderate", "High", "Very High", "Extreme"};
    return names[level];
}

void env_init(env_sensor_t *env)
{
    memset(env, 0, sizeof(*env));
}

void env_print_summary(const env_sensor_t *env)
{
    (void)env;
    printf("=== Environmental Sensor Summary ===\n");
    if (env->has_temp_humi) {
        printf("  Temperature: %.1f C  Humidity: %.1f %%\n",
               env->temp_humi.temperature_c, env->temp_humi.humidity_pct);
    }
    if (env->has_pressure) {
        printf("  Pressure: %.1f hPa  Altitude: %.1f m\n",
               env->pressure.pressure_hpa,
               env_altitude_from_pressure(env->pressure.pressure_hpa, PRESSURE_SEA_LEVEL_HPA));
    }
    if (env->has_gas) {
        printf("  VOC: %.0f ppb  CO2: %.0f ppm  eCO2: %.0f ppm\n",
               env->gas.voc_ppb, env->gas.co2_ppm, env->gas.eco2_ppm);
    }
    if (env->has_dust) {
        printf("  PM1.0: %.1f  PM2.5: %.1f  PM10: %.1f ug/m3  AQI: %.0f\n",
               env->dust.pm1_0_ugm3, env->dust.pm2_5_ugm3, env->dust.pm10_ugm3,
               env_dust_aqi_pm25(env->dust.pm2_5_ugm3));
    }
    if (env->has_wind) {
        printf("  Wind: %.1f m/s @ %.0f deg (%s)  Gust: %.1f m/s\n",
               env->wind.speed_ms, env->wind.direction_deg,
               env_wind_bin_name(env_wind_to_bin(env->wind.direction_deg)),
               env->wind.gust_ms);
    }
    if (env->has_rain) {
        printf("  Rain: %.1f mm  Rate: %.1f mm/h  Tips: %u\n",
               env->rain.total_mm, env_rain_rate(&env->rain), env->rain.tip_count);
    }
    if (env->has_uv) {
        printf("  UV Index: %.1f (%s)\n",
               env->uv.uv_index, env_uv_level_name(env->uv.level));
    }
    printf("====================================\n");
}
