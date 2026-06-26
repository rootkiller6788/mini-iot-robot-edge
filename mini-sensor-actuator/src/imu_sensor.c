#include "../include/imu_sensor.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static const float imu_deg_to_rad = M_PI / 180.0f;
static const float imu_rad_to_deg = 180.0f / M_PI;

void imu_init(imu_sensor_t *imu, imu_ahrs_method_t method, float sample_freq)
{
    memset(imu, 0, sizeof(imu_sensor_t));
    imu->ahrs_method = method;
    imu->beta = (method == IMU_AHRS_MADGWICK) ? 0.1f : 0.5f;
    imu->sample_period = 1.0f / sample_freq;
    imu->orientation.w = 1.0f;
    imu->initialized = 1;
}

void imu_set_calibration(imu_sensor_t *imu, const imu_calib_t *calib)
{
    imu->calib = *calib;
}

void imu_apply_calibration(imu_sensor_t *imu, imu_sample_t *sample)
{
    sample->accel.ax = (sample->accel.ax - imu->calib.ax_bias) * (1.0f + imu->calib.ax_scale);
    sample->accel.ay = (sample->accel.ay - imu->calib.ay_bias) * (1.0f + imu->calib.ay_scale);
    sample->accel.az = (sample->accel.az - imu->calib.az_bias) * (1.0f + imu->calib.az_scale);
    sample->gyro.gx = (sample->gyro.gx - imu->calib.gx_bias) * (1.0f + imu->calib.gx_scale);
    sample->gyro.gy = (sample->gyro.gy - imu->calib.gy_bias) * (1.0f + imu->calib.gy_scale);
    sample->gyro.gz = (sample->gyro.gz - imu->calib.gz_bias) * (1.0f + imu->calib.gz_scale);
    sample->mag.mx = (sample->mag.mx - imu->calib.mx_bias) * (1.0f + imu->calib.mx_scale);
    sample->mag.my = (sample->mag.my - imu->calib.my_bias) * (1.0f + imu->calib.my_scale);
    sample->mag.mz = (sample->mag.mz - imu->calib.mz_bias) * (1.0f + imu->calib.mz_scale);
}

void imu_update_accel(imu_sensor_t *imu, float ax, float ay, float az, uint32_t ts)
{
    imu->accel.ax = ax; imu->accel.ay = ay; imu->accel.az = az;
    imu->last_update_ms = ts;
}

void imu_update_gyro(imu_sensor_t *imu, float gx, float gy, float gz, uint32_t ts)
{
    imu->gyro.gx = gx; imu->gyro.gy = gy; imu->gyro.gz = gz;
    imu->last_update_ms = ts;
}

void imu_update_mag(imu_sensor_t *imu, float mx, float my, float mz, uint32_t ts)
{
    imu->mag.mx = mx; imu->mag.my = my; imu->mag.mz = mz;
    imu->last_update_ms = ts;
}

void imu_update_all(imu_sensor_t *imu, const imu_sample_t *sample)
{
    imu_sample_t calibrated = *sample;
    imu_apply_calibration(imu, &calibrated);
    imu->accel = calibrated.accel;
    imu->gyro = calibrated.gyro;
    imu->mag = calibrated.mag;
    imu->last_update_ms = calibrated.timestamp_ms;
    imu_fifo_push(&imu->fifo, &calibrated);

    if (imu->ahrs_method == IMU_AHRS_MADGWICK) {
        imu_ahrs_update_madgwick(imu, &calibrated);
    } else {
        imu_ahrs_update_mahony(imu, &calibrated);
    }

    imu_quat_to_euler(&imu->orientation, &imu->angles);
}

void imu_ahrs_update_madgwick(imu_sensor_t *imu, const imu_sample_t *sample)
{
    float q1 = imu->orientation.w, q2 = imu->orientation.x;
    float q3 = imu->orientation.y, q4 = imu->orientation.z;
    float ax = sample->accel.ax, ay = sample->accel.ay, az = sample->accel.az;
    float gx = sample->gyro.gx * imu_deg_to_rad;
    float gy = sample->gyro.gy * imu_deg_to_rad;
    float gz = sample->gyro.gz * imu_deg_to_rad;
    float mx = sample->mag.mx, my = sample->mag.my, mz = sample->mag.mz;

    float recip_norm, s1, s2, s3, s4;
    float q_dot1, q_dot2, q_dot3, q_dot4;
    float _2q1, _2q2, _2q3, _2q4;
    float _2q1q3, _2q3q4;

    recip_norm = 1.0f / sqrtf(ax*ax + ay*ay + az*az);
    ax *= recip_norm; ay *= recip_norm; az *= recip_norm;
    recip_norm = 1.0f / sqrtf(mx*mx + my*my + mz*mz);
    mx *= recip_norm; my *= recip_norm; mz *= recip_norm;

    _2q1 = 2.0f * q1; _2q2 = 2.0f * q2; _2q3 = 2.0f * q3; _2q4 = 2.0f * q4;
    _2q1q3 = 2.0f * q1 * q3;
    _2q3q4 = 2.0f * q3 * q4;

    float hx = mx * (0.5f - q3*q3 - q4*q4) + my * (q2*q3 - q1*q4) + mz * (q2*q4 + q1*q3);
    float hy = mx * (q2*q3 + q1*q4) + my * (0.5f - q2*q2 - q4*q4) + mz * (q3*q4 - q1*q2);
    float _2bx = sqrtf(hx*hx + hy*hy);
    float _2bz = mx * (q2*q4 - q1*q3) + my * (q3*q4 + q1*q2) + mz * (0.5f - q2*q2 - q3*q3);
    float _4bx = 2.0f * _2bx, _4bz = 2.0f * _2bz;

    s1 = -_2q3 * (2.0f*q2*q4 - _2q1q3 - ax) + _2q2 * (2.0f*q1*q2 + _2q3q4 - ay)
       - _2bz * q3 * (_2bx*(0.5f - q3*q3 - q4*q4) + _2bz*(q2*q4 - q1*q3) - mx)
       + (-_2bx*q4 + _2bz*q2) * (_2bx*(q2*q3 - q1*q4) + _2bz*(q1*q2 + q3*q4) - my)
       + _2bx * q3 * (_2bx*(q1*q3 + q2*q4) + _2bz*(0.5f - q2*q2 - q3*q3) - mz);
    s2 = _2q4 * (2.0f*q2*q4 - _2q1q3 - ax) + _2q1 * (2.0f*q1*q2 + _2q3q4 - ay)
       - 4.0f*q2 * (1.0f - 2.0f*q2*q2 - 2.0f*q3*q3 - az)
       + _2bz*q4 * (_2bx*(0.5f - q3*q3 - q4*q4) + _2bz*(q2*q4 - q1*q3) - mx)
       + (_2bx*q3 + _2bz*q1) * (_2bx*(q2*q3 - q1*q4) + _2bz*(q1*q2 + q3*q4) - my)
       + (_2bx*q4 - _4bz*q2) * (_2bx*(q1*q3 + q2*q4) + _2bz*(0.5f - q2*q2 - q3*q3) - mz);
    s3 = -_2q1 * (2.0f*q2*q4 - _2q1q3 - ax) + _2q4 * (2.0f*q1*q2 + _2q3q4 - ay)
       - 4.0f*q3 * (1.0f - 2.0f*q2*q2 - 2.0f*q3*q3 - az)
       + (-_4bx*q3 - _2bz*q1) * (_2bx*(0.5f - q3*q3 - q4*q4) + _2bz*(q2*q4 - q1*q3) - mx)
       + (_2bx*q2 + _2bz*q4) * (_2bx*(q2*q3 - q1*q4) + _2bz*(q1*q2 + q3*q4) - my)
       + (_2bx*q1 - _4bz*q3) * (_2bx*(q1*q3 + q2*q4) + _2bz*(0.5f - q2*q2 - q3*q3) - mz);
    s4 = _2q2 * (2.0f*q2*q4 - _2q1q3 - ax) + _2q3 * (2.0f*q1*q2 + _2q3q4 - ay)
       + (-_4bx*q4 + _2bz*q2) * (_2bx*(0.5f - q3*q3 - q4*q4) + _2bz*(q2*q4 - q1*q3) - mx)
       + (-_2bx*q1 + _2bz*q3) * (_2bx*(q2*q3 - q1*q4) + _2bz*(q1*q2 + q3*q4) - my)
       + _2bx*q2 * (_2bx*(q1*q3 + q2*q4) + _2bz*(0.5f - q2*q2 - q3*q3) - mz);

    recip_norm = 1.0f / sqrtf(s1*s1 + s2*s2 + s3*s3 + s4*s4);
    s1 *= recip_norm; s2 *= recip_norm; s3 *= recip_norm; s4 *= recip_norm;

    q_dot1 = 0.5f*(-q2*gx - q3*gy - q4*gz) - imu->beta*s1;
    q_dot2 = 0.5f*( q1*gx + q3*gz - q4*gy) - imu->beta*s2;
    q_dot3 = 0.5f*( q1*gy - q2*gz + q4*gx) - imu->beta*s3;
    q_dot4 = 0.5f*( q1*gz + q2*gy - q3*gx) - imu->beta*s4;

    imu->orientation.w += q_dot1 * imu->sample_period;
    imu->orientation.x += q_dot2 * imu->sample_period;
    imu->orientation.y += q_dot3 * imu->sample_period;
    imu->orientation.z += q_dot4 * imu->sample_period;
    imu_quat_normalize(&imu->orientation);
}

void imu_ahrs_update_mahony(imu_sensor_t *imu, const imu_sample_t *sample)
{
    float q1 = imu->orientation.w, q2 = imu->orientation.x;
    float q3 = imu->orientation.y, q4 = imu->orientation.z;
    float ax = sample->accel.ax, ay = sample->accel.ay, az = sample->accel.az;
    float gx = sample->gyro.gx * imu_deg_to_rad;
    float gy = sample->gyro.gy * imu_deg_to_rad;
    float gz = sample->gyro.gz * imu_deg_to_rad;
    float mx = sample->mag.mx, my = sample->mag.my, mz = sample->mag.mz;
    float recip_norm, halfvx, halfvy, halfvz, halfex, halfey, halfez;
    float q_dot1, q_dot2, q_dot3, q_dot4;

    if (ax == 0.0f && ay == 0.0f && az == 0.0f) {
        q_dot1 = 0.5f*(-q2*gx - q3*gy - q4*gz);
        q_dot2 = 0.5f*( q1*gx + q3*gz - q4*gy);
        q_dot3 = 0.5f*( q1*gy - q2*gz + q4*gx);
        q_dot4 = 0.5f*( q1*gz + q2*gy - q3*gx);
        imu->orientation.w += q_dot1 * imu->sample_period;
        imu->orientation.x += q_dot2 * imu->sample_period;
        imu->orientation.y += q_dot3 * imu->sample_period;
        imu->orientation.z += q_dot4 * imu->sample_period;
        imu_quat_normalize(&imu->orientation);
        return;
    }

    recip_norm = 1.0f / sqrtf(ax*ax + ay*ay + az*az);
    ax *= recip_norm; ay *= recip_norm; az *= recip_norm;

    halfvx = q2*q4 - q1*q3;
    halfvy = q1*q2 + q3*q4;
    halfvz = q1*q1 - 0.5f + q4*q4;

    halfex = ay*halfvz - az*halfvy;
    halfey = az*halfvx - ax*halfvz;
    halfez = ax*halfvy - ay*halfvx;

    if (mx != 0.0f || my != 0.0f || mz != 0.0f) {
        float halfwx, halfwy, halfwz;
        recip_norm = 1.0f / sqrtf(mx*mx + my*my + mz*mz);
        mx *= recip_norm; my *= recip_norm; mz *= recip_norm;
        halfwx = 2.0f*mx*(0.5f - q3*q3 - q4*q4) + 2.0f*my*(q2*q3 - q1*q4) + 2.0f*mz*(q2*q4 + q1*q3);
        halfwy = 2.0f*mx*(q2*q3 + q1*q4) + 2.0f*my*(0.5f - q2*q2 - q4*q4) + 2.0f*mz*(q3*q4 - q1*q2);
        halfwz = 2.0f*mx*(q2*q4 - q1*q3) + 2.0f*my*(q3*q4 + q1*q2) + 2.0f*mz*(0.5f - q2*q2 - q3*q3);
        halfex += my*halfwz - mz*halfwy;
        halfey += mz*halfwx - mx*halfwz;
        halfez += mx*halfwy - my*halfwx;
    }

    gx += 2.0f * imu->beta * halfex;
    gy += 2.0f * imu->beta * halfey;
    gz += 2.0f * imu->beta * halfez;

    q_dot1 = 0.5f*(-q2*gx - q3*gy - q4*gz);
    q_dot2 = 0.5f*( q1*gx + q3*gz - q4*gy);
    q_dot3 = 0.5f*( q1*gy - q2*gz + q4*gx);
    q_dot4 = 0.5f*( q1*gz + q2*gy - q3*gx);

    imu->orientation.w += q_dot1 * imu->sample_period;
    imu->orientation.x += q_dot2 * imu->sample_period;
    imu->orientation.y += q_dot3 * imu->sample_period;
    imu->orientation.z += q_dot4 * imu->sample_period;
    imu_quat_normalize(&imu->orientation);
}

void imu_quat_to_euler(const imu_quat_t *q, imu_euler_t *e)
{
    float w = q->w, x = q->x, y = q->y, z = q->z;
    float sin_pitch = 2.0f * (w*y - z*x);
    if (sin_pitch > 1.0f) sin_pitch = 1.0f;
    if (sin_pitch < -1.0f) sin_pitch = -1.0f;
    e->roll  = atan2f(2.0f*(w*x + y*z), 1.0f - 2.0f*(x*x + y*y)) * imu_rad_to_deg;
    e->pitch = asinf(sin_pitch) * imu_rad_to_deg;
    e->yaw   = atan2f(2.0f*(w*z + x*y), 1.0f - 2.0f*(y*y + z*z)) * imu_rad_to_deg;
}

void imu_euler_to_quat(const imu_euler_t *e, imu_quat_t *q)
{
    float cr = cosf(e->roll  * imu_deg_to_rad * 0.5f);
    float sr = sinf(e->roll  * imu_deg_to_rad * 0.5f);
    float cp = cosf(e->pitch * imu_deg_to_rad * 0.5f);
    float sp = sinf(e->pitch * imu_deg_to_rad * 0.5f);
    float cy = cosf(e->yaw   * imu_deg_to_rad * 0.5f);
    float sy = sinf(e->yaw   * imu_deg_to_rad * 0.5f);
    q->w = cr*cp*cy + sr*sp*sy;
    q->x = sr*cp*cy - cr*sp*sy;
    q->y = cr*sp*cy + sr*cp*sy;
    q->z = cr*cp*sy - sr*sp*cy;
}

void imu_quat_normalize(imu_quat_t *q)
{
    float norm = sqrtf(q->w*q->w + q->x*q->x + q->y*q->y + q->z*q->z);
    if (norm > 0.0f) {
        q->w /= norm; q->x /= norm; q->y /= norm; q->z /= norm;
    }
}

void imu_quat_multiply(imu_quat_t *out, const imu_quat_t *a, const imu_quat_t *b)
{
    out->w = a->w*b->w - a->x*b->x - a->y*b->y - a->z*b->z;
    out->x = a->w*b->x + a->x*b->w + a->y*b->z - a->z*b->y;
    out->y = a->w*b->y - a->x*b->z + a->y*b->w + a->z*b->x;
    out->z = a->w*b->z + a->x*b->y - a->y*b->x + a->z*b->w;
}

void imu_fifo_push(imu_fifo_t *fifo, const imu_sample_t *sample)
{
    fifo->buffer[fifo->head] = *sample;
    fifo->head = (fifo->head + 1) % IMU_FIFO_DEPTH;
    if (fifo->count < IMU_FIFO_DEPTH) {
        fifo->count++;
    } else {
        fifo->tail = (fifo->tail + 1) % IMU_FIFO_DEPTH;
    }
}

uint8_t imu_fifo_pop(imu_fifo_t *fifo, imu_sample_t *sample)
{
    if (fifo->count == 0) return 0;
    *sample = fifo->buffer[fifo->tail];
    fifo->tail = (fifo->tail + 1) % IMU_FIFO_DEPTH;
    fifo->count--;
    return 1;
}

uint8_t imu_fifo_peek(const imu_fifo_t *fifo, imu_sample_t *sample)
{
    if (fifo->count == 0) return 0;
    *sample = fifo->buffer[fifo->tail];
    return 1;
}

uint8_t imu_fifo_count(const imu_fifo_t *fifo)
{
    return fifo->count;
}

void imu_fifo_flush(imu_fifo_t *fifo)
{
    fifo->head = 0; fifo->tail = 0; fifo->count = 0;
}

imu_motion_evt_t imu_detect_freefall(const imu_accel_t *accel, float threshold)
{
    float magnitude = sqrtf(accel->ax*accel->ax + accel->ay*accel->ay + accel->az*accel->az);
    return (magnitude < threshold) ? IMU_MOTION_FREEFALL : IMU_MOTION_NONE;
}

imu_motion_evt_t imu_detect_tap(const imu_sensor_t *imu)
{
    float mag = sqrtf(imu->accel.ax*imu->accel.ax + imu->accel.ay*imu->accel.ay + imu->accel.az*imu->accel.az);
    float delta = mag - IMU_GRAVITY;
    if (delta > 2.0f) return IMU_MOTION_SINGLE_TAP;
    if (delta > 1.5f) return IMU_MOTION_DOUBLE_TAP;
    return IMU_MOTION_NONE;
}

void imu_estimate_bias(imu_sensor_t *imu, const imu_sample_t *samples, uint16_t num_samples)
{
    float sum_ax = 0, sum_ay = 0, sum_az = 0;
    float sum_gx = 0, sum_gy = 0, sum_gz = 0;
    uint16_t i;
    for (i = 0; i < num_samples; i++) {
        sum_ax += samples[i].accel.ax; sum_ay += samples[i].accel.ay; sum_az += samples[i].accel.az;
        sum_gx += samples[i].gyro.gx; sum_gy += samples[i].gyro.gy; sum_gz += samples[i].gyro.gz;
    }
    float n = (float)num_samples;
    imu->calib.gx_bias = sum_gx / n;
    imu->calib.gy_bias = sum_gy / n;
    imu->calib.gz_bias = sum_gz / n;
    imu->calib.ax_bias = sum_ax / n;
    imu->calib.ay_bias = sum_ay / n;
    imu->calib.az_bias = sum_az / n - IMU_GRAVITY;
}
