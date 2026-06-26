#ifndef MINI_ROBOTICS_PATH_PLANNING_H
#define MINI_ROBOTICS_PATH_PLANNING_H

#include <stddef.h>
#include <stdint.h>

#define MR_GRID_MAX_W 1024
#define MR_GRID_MAX_H 1024
#define MR_PATH_MAX_WP 4096
#define MR_RRT_MAX_NODES 10000
#define MR_RRT_STEP 0.1
#define MR_RRT_GOAL_BIAS 0.05
#define MR_RRT_STAR_RADIUS 1.5

typedef struct {
    double x, y;
} mr_point2d;

typedef struct {
    double x, y, theta;
} mr_config2d;

typedef struct {
    int width, height;
    uint8_t *cells; /* 0=free, 255=obstacle */
    double resolution; /* meters per cell */
    double origin_x, origin_y;
} mr_grid_map;

typedef struct {
    mr_point2d waypoints[MR_PATH_MAX_WP];
    int num_waypoints;
    double total_length;
} mr_path2d;

typedef struct {
    mr_point2d center;
    double radius;
} mr_obstacle_circle;

typedef struct {
    mr_point2d vertices[8];
    int num_vertices;
} mr_obstacle_polygon;

typedef enum {
    MR_OBSTACLE_CIRCLE = 0,
    MR_OBSTACLE_POLYGON = 1
} mr_obstacle_type;

typedef struct {
    mr_obstacle_type type;
    union {
        mr_obstacle_circle circle;
        mr_obstacle_polygon polygon;
    };
} mr_obstacle;

typedef struct {
    mr_obstacle obstacles[128];
    int num_obstacles;
    double width;  /* world boundaries */
    double height;
} mr_world2d;

struct mr_rrt_node {
    mr_config2d config;
    int parent_idx;
    double cost;
};

typedef struct {
    struct mr_rrt_node nodes[MR_RRT_MAX_NODES];
    int num_nodes;
    mr_world2d world;
    double step_size;
    double goal_bias;
} mr_rrt_tree;

typedef struct {
    mr_config2d config;
    double velocity;
    double curvature;
} mr_traj_point;

typedef struct {
    mr_traj_point points[MR_PATH_MAX_WP];
    int num_points;
    double duration;
} mr_trajectory;

typedef double (*mr_heuristic_fn)(const mr_point2d *a, const mr_point2d *b);
typedef double (*mr_cost_fn)(int x1, int y1, int x2, int y2, const mr_grid_map *map);
typedef int (*mr_collision_fn)(const mr_config2d *cfg, const mr_world2d *world);

int mr_a_star(const mr_grid_map *map,
              const mr_point2d *start, const mr_point2d *goal,
              mr_path2d *path_out);

int mr_a_star_custom(const mr_grid_map *map,
                     const mr_point2d *start, const mr_point2d *goal,
                     mr_heuristic_fn heuristic, mr_cost_fn cost,
                     mr_path2d *path_out);

int mr_dijkstra(const mr_grid_map *map,
                const mr_point2d *start, const mr_point2d *goal,
                mr_path2d *path_out);

void mr_grid_map_init(mr_grid_map *map, int w, int h, double resolution,
                      double ox, double oy);

void mr_grid_map_set_cell(mr_grid_map *map, int x, int y, uint8_t v);

uint8_t mr_grid_map_get_cell(const mr_grid_map *map, int x, int y);

int mr_grid_map_world_to_grid(const mr_grid_map *map,
                              double wx, double wy,
                              int *gx, int *gy);

void mr_grid_map_grid_to_world(const mr_grid_map *map,
                               int gx, int gy,
                               double *wx, double *wy);

double mr_heuristic_euclidean(const mr_point2d *a, const mr_point2d *b);

double mr_heuristic_manhattan(const mr_point2d *a, const mr_point2d *b);

void mr_rrt_init(mr_rrt_tree *tree, const mr_world2d *world,
                 double step_size, double goal_bias);

int mr_rrt_plan(mr_rrt_tree *tree,
                const mr_config2d *start, const mr_config2d *goal,
                int max_iter, mr_path2d *path_out);

int mr_rrt_star_plan(mr_rrt_tree *tree,
                     const mr_config2d *start, const mr_config2d *goal,
                     int max_iter, double radius, mr_path2d *path_out);

void mr_rrt_smooth_path(const mr_path2d *raw, mr_path2d *smooth);

void mr_trajectory_from_waypoints(const mr_path2d *path,
                                  double max_vel, double max_accel,
                                  mr_trajectory *traj);

void mr_trajectory_spline(const mr_path2d *path, int samples_per_seg,
                          mr_trajectory *traj);

int mr_collision_check_point(const mr_point2d *p, const mr_world2d *world);

int mr_collision_check_segment(const mr_point2d *a, const mr_point2d *b,
                               const mr_world2d *world);

double mr_repulsive_potential(const mr_point2d *robot,
                              const mr_world2d *world,
                              double eta, double d0,
                              mr_point2d *force_out);

#endif
