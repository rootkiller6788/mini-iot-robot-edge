#include "mobile_robot.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double wrap_angle(double angle)
{
    while (angle > M_PI) angle -= 2.0 * M_PI;
    while (angle < -M_PI) angle += 2.0 * M_PI;
    return angle;
}

void mr_diff_drive_setup(mr_diff_drive_model *model,
                         double wheel_radius, double track_width,
                         double cpr)
{
    model->wheel_radius = wheel_radius;
    model->track_width = track_width;
    model->encoder_cpr = cpr;
    model->left_encoder = 0.0;
    model->right_encoder = 0.0;
    model->left_encoder_prev = 0.0;
    model->right_encoder_prev = 0.0;
}

void mr_diff_drive_wheel_to_body(const mr_diff_drive_model *model,
                                 const mr_wheel_velocities *wheels,
                                 mr_body_velocity *body)
{
    double r = model->wheel_radius;
    double L = model->track_width;

    body->linear = r * 0.5 * (wheels->right_velocity + wheels->left_velocity);
    body->angular = r * (wheels->right_velocity - wheels->left_velocity) / L;
}

void mr_diff_drive_body_to_wheel(const mr_diff_drive_model *model,
                                 const mr_body_velocity *body,
                                 mr_wheel_velocities *wheels)
{
    double r = model->wheel_radius;
    double L = model->track_width;

    wheels->left_velocity = (body->linear - 0.5 * L * body->angular) / r;
    wheels->right_velocity = (body->linear + 0.5 * L * body->angular) / r;
}

void mr_odometry_update(mr_diff_drive_model *model,
                        double left_enc, double right_enc,
                        mr_robot_state_2d *state)
{
    double d_left = left_enc - model->left_encoder_prev;
    double d_right = right_enc - model->right_encoder_prev;

    double dist_per_count = (2.0 * M_PI * model->wheel_radius)
                            / model->encoder_cpr;

    double d_left_m = d_left * dist_per_count;
    double d_right_m = d_right * dist_per_count;

    double d_center = (d_left_m + d_right_m) * 0.5;
    double d_theta = (d_right_m - d_left_m) / model->track_width;

    state->theta += d_theta;
    state->theta = wrap_angle(state->theta);

    state->x += d_center * cos(state->theta);
    state->y += d_center * sin(state->theta);

    model->left_encoder_prev = left_enc;
    model->right_encoder_prev = right_enc;
    model->left_encoder = left_enc;
    model->right_encoder = right_enc;
}

void mr_odometry_get_pose(const mr_robot_state_2d *state,
                          mr_robot_pose_2d *pose)
{
    pose->x = state->x;
    pose->y = state->y;
    pose->theta = state->theta;
}

void mr_odometry_get_velocity(const mr_diff_drive_model *model,
                              double dt,
                              const mr_robot_state_2d *state,
                              double *linear, double *angular)
{
    double d_left = model->left_encoder - model->left_encoder_prev;
    double d_right = model->right_encoder - model->right_encoder_prev;

    double dist_per_count = (2.0 * M_PI * model->wheel_radius)
                            / model->encoder_cpr;

    double d_left_m = d_left * dist_per_count;
    double d_right_m = d_right * dist_per_count;

    if (dt > 1e-9) {
        *linear = (d_left_m + d_right_m) * 0.5 / dt;
        *angular = (d_right_m - d_left_m) / model->track_width / dt;
    } else {
        *linear = 0.0;
        *angular = 0.0;
    }
    (void)state;
}

void mr_imu_init(mr_imu_data *imu)
{
    memset(imu, 0, sizeof(mr_imu_data));
    imu->qw = 1.0;
}

void mr_imu_update_accel(mr_imu_data *imu, double ax, double ay, double az)
{
    imu->ax = ax;
    imu->ay = ay;
    imu->az = az;
}

void mr_imu_update_gyro(mr_imu_data *imu, double gx, double gy, double gz)
{
    imu->gx = gx;
    imu->gy = gy;
    imu->gz = gz;
}

void mr_imu_update_mag(mr_imu_data *imu, double mx, double my, double mz)
{
    imu->mx = mx;
    imu->my = my;
    imu->mz = mz;
}

void mr_imu_compute_orientation(mr_imu_data *imu, double dt)
{
    double roll = imu->roll + imu->gx * dt;
    double pitch = imu->pitch + imu->gy * dt;
    double yaw = imu->yaw + imu->gz * dt;

    double norm = sqrt(imu->ax * imu->ax + imu->ay * imu->ay
                       + imu->az * imu->az);
    if (norm > 0.1) {
        double axn = imu->ax / norm;
        double ayn = imu->ay / norm;
        double azn = imu->az / norm;
        roll = atan2(ayn, azn);
        pitch = atan2(-axn, sqrt(ayn * ayn + azn * azn));
    }

    if (fabs(imu->mx) + fabs(imu->my) > 1e-6) {
        double yaw_mag = atan2(imu->my, imu->mx);
        yaw = yaw * 0.95 + yaw_mag * 0.05;
    }

    imu->roll = roll;
    imu->pitch = pitch;
    imu->yaw = yaw;

    double cy = cos(yaw * 0.5), sy = sin(yaw * 0.5);
    double cp = cos(pitch * 0.5), sp = sin(pitch * 0.5);
    double cr = cos(roll * 0.5), sr = sin(roll * 0.5);

    imu->qw = cr * cp * cy + sr * sp * sy;
    imu->qx = sr * cp * cy - cr * sp * sy;
    imu->qy = cr * sp * cy + sr * cp * sy;
    imu->qz = cr * cp * sy - sr * sp * cy;
}

void mr_imu_get_rpy(const mr_imu_data *imu,
                    double *roll, double *pitch, double *yaw)
{
    *roll = imu->roll;
    *pitch = imu->pitch;
    *yaw = imu->yaw;
}

void mr_laser_scan_init(mr_laser_scan *scan,
                        double angle_min, double angle_max,
                        double angle_inc,
                        double range_min, double range_max)
{
    scan->angle_min = angle_min;
    scan->angle_max = angle_max;
    scan->angle_increment = angle_inc;
    scan->range_min = range_min;
    scan->range_max = range_max;
    scan->num_beams = (int)((angle_max - angle_min) / angle_inc) + 1;
    if (scan->num_beams > MR_LASER_MAX_BEAMS)
        scan->num_beams = MR_LASER_MAX_BEAMS;
    memset(scan->lasers, 0, sizeof(double) * (size_t)scan->num_beams);
}

void mr_laser_scan_set_beam(mr_laser_scan *scan, int idx, double range)
{
    if (idx >= 0 && idx < scan->num_beams)
        scan->lasers[idx] = range;
}

double mr_laser_scan_min_range(const mr_laser_scan *scan)
{
    double min_r = scan->range_max;
    int i;
    for (i = 0; i < scan->num_beams; i++)
        if (scan->lasers[i] > scan->range_min
            && scan->lasers[i] < min_r)
            min_r = scan->lasers[i];
    return min_r;
}

int mr_laser_scan_detect_obstacle(const mr_laser_scan *scan,
                                  double threshold)
{
    int i;
    for (i = 0; i < scan->num_beams; i++)
        if (scan->lasers[i] > scan->range_min
            && scan->lasers[i] < threshold)
            return 1;
    return 0;
}

static double amcl_random_gauss(void)
{
    double u1 = (double)rand() / (double)RAND_MAX;
    double u2 = (double)rand() / (double)RAND_MAX;
    return sqrt(-2.0 * log(u1 + 1e-12)) * cos(2.0 * M_PI * u2);
}

void mr_amcl_init(mr_amcl *amcl, int num_particles,
                  double x_min, double x_max,
                  double y_min, double y_max)
{
    amcl->num_particles = num_particles;
    if (amcl->num_particles > MR_AMCL_MAX_PARTICLES)
        amcl->num_particles = MR_AMCL_MAX_PARTICLES;

    int i;
    for (i = 0; i < amcl->num_particles; i++) {
        amcl->particles[i].pose.x = x_min
            + (x_max - x_min) * (double)rand() / (double)RAND_MAX;
        amcl->particles[i].pose.y = y_min
            + (y_max - y_min) * (double)rand() / (double)RAND_MAX;
        amcl->particles[i].pose.theta
            = -M_PI + 2.0 * M_PI * (double)rand() / (double)RAND_MAX;
        amcl->particles[i].weight = 1.0 / amcl->num_particles;
    }

    amcl->pose_x = (x_min + x_max) * 0.5;
    amcl->pose_y = (y_min + y_max) * 0.5;
    amcl->pose_theta = 0.0;
}

void mr_amcl_predict(mr_amcl *amcl, double dx, double dy, double dtheta,
                     double alpha1, double alpha2,
                     double alpha3, double alpha4)
{
    int i;
    for (i = 0; i < amcl->num_particles; i++) {
        double ndx = dx + amcl_random_gauss()
                     * (alpha1 * fabs(dx) + alpha2 * fabs(dtheta));
        double ndy = dy + amcl_random_gauss()
                     * (alpha1 * fabs(dy) + alpha2 * fabs(dtheta));
        double ndt = dtheta + amcl_random_gauss()
                     * (alpha3 * sqrt(dx * dx + dy * dy)
                        + alpha4 * fabs(dtheta));

        double ct = cos(amcl->particles[i].pose.theta);
        double st = sin(amcl->particles[i].pose.theta);

        amcl->particles[i].pose.x += ndx * ct - ndy * st;
        amcl->particles[i].pose.y += ndx * st + ndy * ct;
        amcl->particles[i].pose.theta += ndt;
        amcl->particles[i].pose.theta
            = wrap_angle(amcl->particles[i].pose.theta);
    }
}

void mr_amcl_update_from_laser(mr_amcl *amcl,
                               const mr_laser_scan *scan,
                               const uint8_t *map, int map_w, int map_h,
                               double resolution,
                               double origin_x, double origin_y,
                               double likelihood_sigma)
{
    double total_weight = 0.0;
    int i;

    for (i = 0; i < amcl->num_particles; i++) {
        double px = amcl->particles[i].pose.x;
        double py = amcl->particles[i].pose.y;
        double ptheta = amcl->particles[i].pose.theta;

        double likelihood = 1.0;
        int beam_count = scan->num_beams > 16 ? 16 : scan->num_beams;
        int step = scan->num_beams / beam_count;
        if (step < 1) step = 1;

        int b;
        for (b = 0; b < scan->num_beams; b += step) {
            double angle = scan->angle_min
                           + (double)b * scan->angle_increment;
            double range = scan->lasers[b];
            if (range < scan->range_min || range > scan->range_max)
                continue;

            double wx = px + range * cos(ptheta + angle);
            double wy = py + range * sin(ptheta + angle);

            int mx = (int)((wx - origin_x) / resolution);
            int my = (int)((wy - origin_y) / resolution);

            if (mx >= 0 && mx < map_w && my >= 0 && my < map_h) {
                uint8_t cell = map[my * map_w + mx];
                double occ = (double)cell / 255.0;
                double dist = (occ - 0.5);
                likelihood *= exp(-0.5 * dist * dist
                                  / (likelihood_sigma * likelihood_sigma));
            } else {
                likelihood *= 0.1;
            }
        }

        amcl->particles[i].weight *= likelihood;
        total_weight += amcl->particles[i].weight;
    }

    if (total_weight > 1e-12)
        for (i = 0; i < amcl->num_particles; i++)
            amcl->particles[i].weight /= total_weight;
}

void mr_amcl_resample(mr_amcl *amcl)
{
    mr_amcl_particle new_parts[MR_AMCL_MAX_PARTICLES];
    double cdf[MR_AMCL_MAX_PARTICLES];
    int i;

    cdf[0] = amcl->particles[0].weight;
    for (i = 1; i < amcl->num_particles; i++)
        cdf[i] = cdf[i - 1] + amcl->particles[i].weight;

    for (i = 0; i < amcl->num_particles; i++) {
        double r = (double)rand() / (double)RAND_MAX;
        int j;
        for (j = 0; j < amcl->num_particles - 1 && cdf[j] < r; j++);
        new_parts[i] = amcl->particles[j];
        new_parts[i].weight = 1.0 / amcl->num_particles;
    }

    memcpy(amcl->particles, new_parts,
           sizeof(mr_amcl_particle) * (size_t)amcl->num_particles);

    double mx = 0.0, my = 0.0;
    double mcos = 0.0, msin = 0.0;
    for (i = 0; i < amcl->num_particles; i++) {
        mx += amcl->particles[i].pose.x * amcl->particles[i].weight;
        my += amcl->particles[i].pose.y * amcl->particles[i].weight;
        mcos += cos(amcl->particles[i].pose.theta)
                * amcl->particles[i].weight;
        msin += sin(amcl->particles[i].pose.theta)
                * amcl->particles[i].weight;
    }

    amcl->pose_x = mx;
    amcl->pose_y = my;
    amcl->pose_theta = atan2(msin, mcos);
}

void mr_amcl_get_estimate(const mr_amcl *amcl,
                          double *x, double *y, double *theta)
{
    *x = amcl->pose_x;
    *y = amcl->pose_y;
    *theta = amcl->pose_theta;
}

void mr_amcl_get_covariance(const mr_amcl *amcl,
                            double *cov_xx, double *cov_yy, double *cov_tt,
                            double *cov_xy, double *cov_xt, double *cov_yt)
{
    *cov_xx = amcl->cov_xx;
    *cov_yy = amcl->cov_yy;
    *cov_tt = amcl->cov_tt;
    *cov_xy = amcl->cov_xy;
    *cov_xt = amcl->cov_xt;
    *cov_yt = amcl->cov_yt;
}

void mr_pid_init(mr_pid_controller *pid,
                 double kp_l, double ki_l, double kd_l,
                 double kp_a, double ki_a, double kd_a,
                 double max_l, double max_a)
{
    pid->kp_linear = kp_l;
    pid->ki_linear = ki_l;
    pid->kd_linear = kd_l;
    pid->kp_angular = kp_a;
    pid->ki_angular = ki_a;
    pid->kd_angular = kd_a;
    pid->max_linear = max_l;
    pid->max_angular = max_a;
    pid->integral_linear = 0.0;
    pid->integral_angular = 0.0;
    pid->prev_error_linear = 0.0;
    pid->prev_error_angular = 0.0;
}

void mr_pid_compute(const mr_pid_controller *pid,
                    double target_linear, double target_angular,
                    double current_linear, double current_angular,
                    double dt,
                    double *cmd_linear, double *cmd_angular)
{
    double error_l = target_linear - current_linear;
    double error_a = target_angular - current_angular;

    double p_l = pid->kp_linear * error_l;
    double i_l = pid->ki_linear * (pid->integral_linear + error_l * dt);
    double d_l = 0.0;
    if (dt > 1e-9)
        d_l = pid->kd_linear * (error_l - pid->prev_error_linear) / dt;

    *cmd_linear = p_l + i_l + d_l;
    if (*cmd_linear > pid->max_linear) *cmd_linear = pid->max_linear;
    if (*cmd_linear < -pid->max_linear) *cmd_linear = -pid->max_linear;

    double p_a = pid->kp_angular * error_a;
    double i_a = pid->ki_angular * (pid->integral_angular + error_a * dt);
    double d_a = 0.0;
    if (dt > 1e-9)
        d_a = pid->kd_angular * (error_a - pid->prev_error_angular) / dt;

    *cmd_angular = p_a + i_a + d_a;
    if (*cmd_angular > pid->max_angular) *cmd_angular = pid->max_angular;
    if (*cmd_angular < -pid->max_angular) *cmd_angular = -pid->max_angular;
}

void mr_goto_goal(double current_x, double current_y, double current_theta,
                  double goal_x, double goal_y,
                  double *cmd_linear, double *cmd_angular)
{
    double dx = goal_x - current_x;
    double dy = goal_y - current_y;
    double dist = sqrt(dx * dx + dy * dy);
    double goal_angle = atan2(dy, dx);
    double angle_error = wrap_angle(goal_angle - current_theta);

    double k_linear = 0.5;
    double k_angular = 1.5;

    if (dist < 0.05) {
        *cmd_linear = 0.0;
        *cmd_angular = 0.0;
    } else {
        *cmd_linear = k_linear * dist;
        *cmd_angular = k_angular * angle_error;
    }

    if (*cmd_linear > 1.0) *cmd_linear = 1.0;
    if (*cmd_angular > 2.0) *cmd_angular = 2.0;
}

int mr_obstacle_avoidance_laser(const mr_laser_scan *scan,
                                double safe_distance,
                                double *turn_velocity)
{
    double left_sum = 0.0, right_sum = 0.0;
    int mid = scan->num_beams / 2;
    int i;

    for (i = 0; i < mid; i++)
        if (scan->lasers[i] < safe_distance
            && scan->lasers[i] > scan->range_min)
            left_sum += safe_distance - scan->lasers[i];

    for (i = mid; i < scan->num_beams; i++)
        if (scan->lasers[i] < safe_distance
            && scan->lasers[i] > scan->range_min)
            right_sum += safe_distance - scan->lasers[i];

    if (left_sum + right_sum < 0.01) return 0;

    *turn_velocity = (right_sum - left_sum) * 0.5;
    if (*turn_velocity > 2.0) *turn_velocity = 2.0;
    if (*turn_velocity < -2.0) *turn_velocity = -2.0;

    return 1;
}

void mr_wall_following(const mr_laser_scan *scan,
                       double desired_distance,
                       double forward_speed,
                       double *cmd_linear, double *cmd_angular)
{
    int right_idx = scan->num_beams / 4;
    if (right_idx < 0) right_idx = 0;
    if (right_idx >= scan->num_beams) right_idx = scan->num_beams - 1;

    double wall_dist = scan->lasers[right_idx];
    double error = wall_dist - desired_distance;

    double kp = 1.0;
    *cmd_linear = forward_speed;
    *cmd_angular = -kp * error;

    if (*cmd_angular > 1.5) *cmd_angular = 1.5;
    if (*cmd_angular < -1.5) *cmd_angular = -1.5;

    if (mr_laser_scan_min_range(scan) < 0.15) {
        *cmd_linear = 0.0;
        *cmd_angular = 0.5;
    }
}

void mr_navigation_stack(double current_x, double current_y,
                         double current_theta,
                         double goal_x, double goal_y,
                         const mr_laser_scan *scan,
                         double *cmd_linear, double *cmd_angular)
{
    double goto_linear, goto_angular;
    mr_goto_goal(current_x, current_y, current_theta,
                 goal_x, goal_y, &goto_linear, &goto_angular);

    double avoid_turn = 0.0;
    int obstacle = mr_obstacle_avoidance_laser(scan, 0.5, &avoid_turn);

    if (obstacle) {
        *cmd_linear = goto_linear * 0.3;
        *cmd_angular = avoid_turn;
    } else {
        *cmd_linear = goto_linear;
        *cmd_angular = goto_angular;
    }
}
