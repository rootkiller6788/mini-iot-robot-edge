#ifndef MINI_ROBOTICS_SLAM_SYSTEM_H
#define MINI_ROBOTICS_SLAM_SYSTEM_H

#include <stddef.h>
#include <stdint.h>

#define MR_SLAM_MAX_LANDMARKS 1024
#define MR_SLAM_MAX_PARTICLES 256
#define MR_SLAM_STATE_DIM 3
#define MR_SLAM_LM_DIM 2

typedef struct {
    double m[4][4]; /* covariance for (x,y,theta, lm1_x, lm1_y, ...) */
    int size;
} mr_cov_matrix;

typedef struct {
    double x, y, theta; /* robot pose */
} mr_robot_pose;

typedef struct {
    double range;    /* distance measurement */
    double bearing;  /* angular measurement */
    int id;          /* landmark ID (-1 if unknown) */
    double x, y;     /* estimated landmark position */
} mr_landmark_obs;

typedef struct {
    mr_robot_pose pose;
    double lm_x[MR_SLAM_MAX_LANDMARKS];
    double lm_y[MR_SLAM_MAX_LANDMARKS];
    int lm_ids[MR_SLAM_MAX_LANDMARKS];
    int num_landmarks;
    mr_cov_matrix cov;
} mr_ekf_slam;

typedef struct {
    mr_robot_pose pose;
    double weight;
} mr_particle;

typedef struct {
    double x, y;
} mr_lm_estimate;

typedef struct {
    mr_particle particles[MR_SLAM_MAX_PARTICLES];
    mr_lm_estimate landmarks[MR_SLAM_MAX_PARTICLES][MR_SLAM_MAX_LANDMARKS];
    int lm_counts[MR_SLAM_MAX_PARTICLES];
    int num_particles;
} mr_fast_slam;

typedef struct {
    int from, to;
    double dx, dy, dtheta;
    double info[3][3]; /* information matrix for 2D constraint */
} mr_pose_edge;

typedef struct {
    mr_robot_pose nodes[MR_SLAM_MAX_LANDMARKS + MR_SLAM_MAX_PARTICLES];
    int num_nodes;
    mr_pose_edge edges[MR_SLAM_MAX_LANDMARKS * 4];
    int num_edges;
} mr_graph_slam;

typedef struct {
    uint8_t cells[MR_SLAM_MAX_LANDMARKS][MR_SLAM_MAX_LANDMARKS]; /* unused large alloc; real impl uses dynamic */
    int width, height;
    double resolution;
    double origin_x, origin_y;
} mr_occ_grid;

typedef struct {
    uint8_t *data;
    int length;
} mr_descriptor;

void mr_ekf_slam_init(mr_ekf_slam *slam);

void mr_ekf_slam_predict(mr_ekf_slam *slam, double dx, double dy, double dt);

int mr_ekf_slam_update(mr_ekf_slam *slam,
                       const mr_landmark_obs *observations,
                       int num_obs);

void mr_ekf_slam_get_pose(const mr_ekf_slam *slam, mr_robot_pose *pose);

int mr_ekf_slam_get_landmark(const mr_ekf_slam *slam, int id,
                             double *x, double *y);

int mr_ekf_slam_add_landmark(mr_ekf_slam *slam, int id,
                             double range, double bearing);

void mr_fast_slam_init(mr_fast_slam *fs, int num_particles);

void mr_fast_slam_predict(mr_fast_slam *fs,
                          double dx, double dy, double dt,
                          double noise_std);

void mr_fast_slam_update(mr_fast_slam *fs,
                         const mr_landmark_obs *observations,
                         int num_obs);

void mr_fast_slam_resample(mr_fast_slam *fs);

void mr_fast_slam_best_pose(const mr_fast_slam *fs, mr_robot_pose *pose);

void mr_graph_slam_init(mr_graph_slam *gs, const mr_robot_pose *initial);

int mr_graph_slam_add_node(mr_graph_slam *gs, const mr_robot_pose *pose);

int mr_graph_slam_add_edge(mr_graph_slam *gs, int from, int to,
                           double dx, double dy, double dtheta);

int mr_graph_slam_optimize(mr_graph_slam *gs, int max_iters);

int mr_graph_slam_loop_closure(mr_graph_slam *gs, int node_a, int node_b,
                               double dx, double dy, double dtheta);

void mr_occ_grid_init(mr_occ_grid *grid, int w, int h,
                      double resolution, double ox, double oy);

void mr_occ_grid_update_from_scan(mr_occ_grid *grid,
                                  const mr_robot_pose *pose,
                                  const double *ranges, int num_ranges,
                                  double angle_min, double angle_inc,
                                  double max_range);

uint8_t mr_occ_grid_get(const mr_occ_grid *grid, int x, int y, int *valid);

double mr_occ_grid_occupied(const mr_occ_grid *grid, double wx, double wy);

mr_descriptor mr_compute_scan_descriptor(const double *ranges, int num_ranges);

double mr_descriptor_similarity(const mr_descriptor *a, const mr_descriptor *b);

void mr_descriptor_free(mr_descriptor *d);

#endif
