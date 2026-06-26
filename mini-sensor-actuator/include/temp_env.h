#ifndef TEMP_ENV_H
#define TEMP_ENV_H

#include <stdint.h>

#define TEMP_ACCURACY_C  0.5f
#define HUMIDITY_ACCURACY_PCT 3.0f
#define PRESSURE_SEA_LEVEL_HPA 1013.25f
#define ALTITUDE_SCALE_FACTOR 44330.0f
#define PM25_SAMPLE_INTERVAL_MS 1000
#define WIND_DIRECTION_BINS 16
#define RAIN_BUCKET_TIP_MM 0.2794f

typedef struct {
    float temperature_c;
    float humidity_pct;
    uint32_t timestamp_ms;
} temp_humi_t;

typedef struct {
    float pressure_hpa;
    float temperature_c;
} baro_pressure_t;

typedef enum {
    GAS_VOC  = 0,
    GAS_CO2  = 1,
    GAS_eCO2 = 2
} gas_type_t;

typedef struct {
    float voc_ppb;
    float co2_ppm;
    float eco2_ppm;
    uint32_t resistance_ohm;
    uint32_t timestamp_ms;
} gas_sensor_t;

typedef struct {
    float pm1_0_ugm3;
    float pm2_5_ugm3;
    float pm10_ugm3;
    float particle_count_100ml;
    uint32_t timestamp_ms;
} pm25_dust_t;

typedef struct {
    float speed_ms;
    float direction_deg;
    float gust_ms;
    uint32_t timestamp_ms;
} wind_data_t;

typedef enum {
    WIND_N   = 0,  WIND_NNE = 1,  WIND_NE  = 2,  WIND_ENE = 3,
    WIND_E   = 4,  WIND_ESE = 5,  WIND_SE  = 6,  WIND_SSE = 7,
    WIND_S   = 8,  WIND_SSW = 9,  WIND_SW  = 10, WIND_WSW = 11,
    WIND_W   = 12, WIND_WNW = 13, WIND_NW  = 14, WIND_NNW = 15
} wind_direction_bin_t;

typedef struct {
    float total_mm;
    float intensity_mmh;
    uint32_t tip_count;
    uint32_t last_tip_ms;
    uint32_t timestamp_ms;
} rain_gauge_t;

typedef enum {
    UV_LOW      = 0,
    UV_MODERATE = 1,
    UV_HIGH     = 2,
    UV_VERY_HIGH = 3,
    UV_EXTREME  = 4
} uv_index_level_t;

typedef struct {
    float uv_index;
    uv_index_level_t level;
    uint32_t timestamp_ms;
} uv_sensor_t;

typedef struct {
    temp_humi_t temp_humi;
    baro_pressure_t pressure;
    gas_sensor_t gas;
    pm25_dust_t dust;
    wind_data_t wind;
    rain_gauge_t rain;
    uv_sensor_t uv;
    uint8_t has_temp_humi;
    uint8_t has_pressure;
    uint8_t has_gas;
    uint8_t has_dust;
    uint8_t has_wind;
    uint8_t has_rain;
    uint8_t has_uv;
} env_sensor_t;

float env_altitude_from_pressure(float pressure_hpa, float sea_level_hpa);
float env_sea_level_pressure(float pressure_hpa, float altitude_m, float temperature_c);
float env_dew_point(float temperature_c, float humidity_pct);
float env_absolute_humidity(float temperature_c, float humidity_pct);
float env_heat_index_c(float temperature_c, float humidity_pct);
float env_vapor_pressure_hpa(float temperature_c);

void env_temp_humi_init(temp_humi_t *th);
void env_temp_humi_update(temp_humi_t *th, float temp_c, float hum_pct, uint32_t ts);
void env_baro_init(baro_pressure_t *bp);
void env_baro_update(baro_pressure_t *bp, float pressure_hpa, float temp_c);

void env_gas_init(gas_sensor_t *gas);
void env_gas_update_voc(gas_sensor_t *gas, float voc_ppb, uint32_t ts);
void env_gas_update_co2(gas_sensor_t *gas, float co2_ppm, uint32_t ts);
void env_gas_update_eco2(gas_sensor_t *gas, float eco2_ppm, uint32_t ts);
void env_gas_update_resistance(gas_sensor_t *gas, uint32_t resistance_ohm, uint32_t ts);
float env_gas_compensate_humidity(float reading, float humidity_pct);

void env_dust_init(pm25_dust_t *dust);
void env_dust_update(pm25_dust_t *dust, float pm1, float pm25, float pm10, float count, uint32_t ts);
float env_dust_aqi_pm25(float pm25_ugm3);

void env_wind_init(wind_data_t *wind);
void env_wind_update(wind_data_t *wind, float speed_ms, float dir_deg, float gust_ms, uint32_t ts);
wind_direction_bin_t env_wind_to_bin(float direction_deg);
const char* env_wind_bin_name(wind_direction_bin_t bin);

void env_rain_init(rain_gauge_t *rain);
void env_rain_tip(rain_gauge_t *rain, uint32_t timestamp_ms);
float env_rain_rate(const rain_gauge_t *rain);
void env_rain_reset(rain_gauge_t *rain);

void env_uv_init(uv_sensor_t *uv);
void env_uv_update(uv_sensor_t *uv, float index, uint32_t ts);
uv_index_level_t env_uv_classify(float index);
const char* env_uv_level_name(uv_index_level_t level);

void env_init(env_sensor_t *env);
void env_print_summary(const env_sensor_t *env);

#endif
