#ifndef MINI_ROBOTICS_MOBILE_ROBOT_H
#define MINI_ROBOTICS_MOBILE_ROBOT_H

#include <stddef.h>
#include <stdint.h>

#define MR_LASER_MAX_BEAMS 2048
#define MR_AMCL_MAX_PARTICLES 4096
#define MR_LOCAL_PLANNER_LOOKAHEAD 2.0

typedef struct {
    double left_velocity;   /* m/s */
    double right_velocity;  /* m/s */
} mr_wheel_velocities;

typedef struct {
    double linear;   /* v: m/s */
    double angular;  /* omega: rad/s */
} mr_body_velocity;

typedef struct {
    double wheel_radius;
    double track_width;      /* distance between wheels, m */
    double encoder_cpr;      /* counts per revolution */
    double left_encoder;
    double right_encoder;
    double left_encoder_prev;
    double right_encoder_prev;
} mr_diff_drive_model;

typedef struct {
    double x, y, theta;
} mr_robot_pose_2d;

typedef struct {
    double x, y, theta;
    double linear_vel, angular_vel;
} mr_robot_state_2d;

typedef struct {
    double ax, ay, az;       /* accelerometer (m/s^2) */
    double gx, gy, gz;       /* gyroscope (rad/s) */
    double mx, my, mz;       /* magnetometer (uT) */
    double roll, pitch, yaw; /* computed orientation */
    double qw, qx, qy, qz;   /* orientation quaternion */
} mr_imu_data;

typedef struct {
    double lasers[MR_LASER_MAX_BEAMS];
    int num_beams;
    double angle_min;
    double angle_max;
    double angle_increment;
    double range_min;
    double range_max;
    double timestamp;
} mr_laser_scan;

typedef struct {
    mr_robot_pose_2d pose;
    double weight;
} mr_amcl_particle;

typedef struct {
    mr_amcl_particle particles[MR_AMCL_MAX_PARTICLES];
    int num_particles;
    double pose_x, pose_y, pose_theta;
    double cov_xx, cov_yy, cov_tt;
    double cov_xy, cov_xt, cov_yt;
} mr_amcl;

typedef struct {
    double kp_linear;
    double ki_linear;
    double kd_linear;
    double kp_angular;
    double ki_angular;
    double kd_angular;
    double integral_linear;
    double integral_angular;
    double prev_error_linear;
    double prev_error_angular;
    double max_linear;
    double max_angular;
} mr_pid_controller;

typedef struct {
    mr_diff_drive_model model;
    mr_robot_state_2d state;
    mr_imu_data imu;
    mr_laser_scan laser;
    mr_amcl amcl;
    mr_pid_controller pid;
} mr_mobile_robot;

void mr_diff_drive_setup(mr_diff_drive_model *model,
                         double wheel_radius, double track_width,
                         double cpr);

void mr_diff_drive_wheel_to_body(const mr_diff_drive_model *model,
                                 const mr_wheel_velocities *wheels,
                                 mr_body_velocity *body);

void mr_diff_drive_body_to_wheel(const mr_diff_drive_model *model,
                                 const mr_body_velocity *body,
                                 mr_wheel_velocities *wheels);

void mr_odometry_update(mr_diff_drive_model *model,
                        double left_enc, double right_enc,
                        mr_robot_state_2d *state);

void mr_odometry_get_pose(const mr_robot_state_2d *state,
                          mr_robot_pose_2d *pose);

void mr_odometry_get_velocity(const mr_diff_drive_model *model,
                              double dt,
                              const mr_robot_state_2d *state,
                              double *linear, double *angular);

void mr_imu_init(mr_imu_data *imu);

void mr_imu_update_accel(mr_imu_data *imu, double ax, double ay, double az);

void mr_imu_update_gyro(mr_imu_data *imu, double gx, double gy, double gz);

void mr_imu_update_mag(mr_imu_data *imu, double mx, double my, double mz);

void mr_imu_compute_orientation(mr_imu_data *imu, double dt);

void mr_imu_get_rpy(const mr_imu_data *imu,
                    double *roll, double *pitch, double *yaw);

void mr_laser_scan_init(mr_laser_scan *scan,
                        double angle_min, double angle_max,
                        double angle_inc,
                        double range_min, double range_max);

void mr_laser_scan_set_beam(mr_laser_scan *scan, int idx, double range);

double mr_laser_scan_min_range(const mr_laser_scan *scan);

int mr_laser_scan_detect_obstacle(const mr_laser_scan *scan,
                                  double threshold);

void mr_amcl_init(mr_amcl *amcl, int num_particles,
                  double x_min, double x_max,
                  double y_min, double y_max);

void mr_amcl_predict(mr_amcl *amcl, double dx, double dy, double dtheta,
                     double alpha1, double alpha2,
                     double alpha3, double alpha4);

void mr_amcl_update_from_laser(mr_amcl *amcl,
                               const mr_laser_scan *scan,
                               const uint8_t *map, int map_w, int map_h,
                               double resolution,
                               double origin_x, double origin_y,
                               double likelihood_sigma);

void mr_amcl_resample(mr_amcl *amcl);

void mr_amcl_get_estimate(const mr_amcl *amcl,
                          double *x, double *y, double *theta);

void mr_amcl_get_covariance(const mr_amcl *amcl,
                            double *cov_xx, double *cov_yy, double *cov_tt,
                            double *cov_xy, double *cov_xt, double *cov_yt);

void mr_pid_init(mr_pid_controller *pid,
                 double kp_l, double ki_l, double kd_l,
                 double kp_a, double ki_a, double kd_a,
                 double max_l, double max_a);

void mr_pid_compute(const mr_pid_controller *pid,
                    double target_linear, double target_angular,
                    double current_linear, double current_angular,
                    double dt,
                    double *cmd_linear, double *cmd_angular);

void mr_goto_goal(double current_x, double current_y, double current_theta,
                  double goal_x, double goal_y,
                  double *cmd_linear, double *cmd_angular);

int mr_obstacle_avoidance_laser(const mr_laser_scan *scan,
                                double safe_distance,
                                double *turn_velocity);

void mr_wall_following(const mr_laser_scan *scan,
                       double desired_distance,
                       double forward_speed,
                       double *cmd_linear, double *cmd_angular);

void mr_navigation_stack(double current_x, double current_y,
                         double current_theta,
                         double goal_x, double goal_y,
                         const mr_laser_scan *scan,
                         double *cmd_linear, double *cmd_angular);

#endif
