#include "path_planning.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    int x, y;
    double g, f;
    int open, closed;
    int parent_x, parent_y;
} mr_grid_node;

static double euclidean_cost(int x1, int y1, int x2, int y2,
                             const mr_grid_map *map)
{
    (void)map;
    double dx = (double)(x2 - x1);
    double dy = (double)(y2 - y1);
    return sqrt(dx * dx + dy * dy);
}

void mr_grid_map_init(mr_grid_map *map, int w, int h, double resolution,
                      double ox, double oy)
{
    map->width = w;
    map->height = h;
    map->resolution = resolution;
    map->origin_x = ox;
    map->origin_y = oy;
}

void mr_grid_map_set_cell(mr_grid_map *map, int x, int y, uint8_t v)
{
    if (x >= 0 && x < map->width && y >= 0 && y < map->height)
        map->cells[y * map->width + x] = v;
}

uint8_t mr_grid_map_get_cell(const mr_grid_map *map, int x, int y)
{
    if (x >= 0 && x < map->width && y >= 0 && y < map->height)
        return map->cells[y * map->width + x];
    return 255;
}

int mr_grid_map_world_to_grid(const mr_grid_map *map,
                              double wx, double wy,
                              int *gx, int *gy)
{
    *gx = (int)((wx - map->origin_x) / map->resolution);
    *gy = (int)((wy - map->origin_y) / map->resolution);
    if (*gx >= 0 && *gx < map->width && *gy >= 0 && *gy < map->height)
        return 1;
    return 0;
}

void mr_grid_map_grid_to_world(const mr_grid_map *map,
                               int gx, int gy,
                               double *wx, double *wy)
{
    *wx = map->origin_x + (gx + 0.5) * map->resolution;
    *wy = map->origin_y + (gy + 0.5) * map->resolution;
}

double mr_heuristic_euclidean(const mr_point2d *a, const mr_point2d *b)
{
    double dx = a->x - b->x;
    double dy = a->y - b->y;
    return sqrt(dx * dx + dy * dy);
}

double mr_heuristic_manhattan(const mr_point2d *a, const mr_point2d *b)
{
    return fabs(a->x - b->x) + fabs(a->y - b->y);
}

static int astar_core(const mr_grid_map *map,
                      int sx, int sy, int gx, int gy,
                      mr_heuristic_fn heuristic, mr_cost_fn cost_fn,
                      int *path_x, int *path_y, int *path_len)
{
    int w = map->width, h = map->height;
    mr_grid_node *nodes = (mr_grid_node *)calloc((size_t)(w * h),
                                                  sizeof(mr_grid_node));
    if (!nodes) return 0;

    int i, j;
    for (i = 0; i < h; i++)
        for (j = 0; j < w; j++) {
            nodes[i * w + j].x = j;
            nodes[i * w + j].y = i;
            nodes[i * w + j].g = DBL_MAX;
        }

    nodes[sy * w + sx].g = 0.0;
    nodes[sy * w + sx].f = heuristic(
        &(mr_point2d){(double)sx, (double)sy},
        &(mr_point2d){(double)gx, (double)gy});

    int found = 0;
    int max_iter = w * h;

    for (int iter = 0; iter < max_iter && !found; iter++) {
        double min_f = DBL_MAX;
        int ci = -1;

        for (i = 0; i < h; i++)
            for (j = 0; j < w; j++) {
                int idx = i * w + j;
                if (!nodes[idx].open && !nodes[idx].closed) {
                    nodes[idx].open = 1;
                }
                if (nodes[idx].open && !nodes[idx].closed
                    && nodes[idx].f < min_f) {
                    min_f = nodes[idx].f;
                    ci = idx;
                }
            }

        if (ci < 0) break;

        nodes[ci].closed = 1;
        int cx = nodes[ci].x, cy = nodes[ci].y;

        if (cx == gx && cy == gy) {
            found = 1;
            break;
        }

        int dirs[8][2] = {{1,0},{0,1},{-1,0},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1}};

        for (int d = 0; d < 8; d++) {
            int nx = cx + dirs[d][0];
            int ny = cy + dirs[d][1];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;

            int nidx = ny * w + nx;
            if (nodes[nidx].closed) continue;
            if (mr_grid_map_get_cell(map, nx, ny) > 200) continue;

            double step_cost = (dirs[d][0] != 0 && dirs[d][1] != 0) ? 1.414 : 1.0;
            step_cost = cost_fn(cx, cy, nx, ny, map);

            double tent_g = nodes[ci].g + step_cost;
            if (tent_g < nodes[nidx].g) {
                nodes[nidx].g = tent_g;
                nodes[nidx].f = tent_g + heuristic(
                    &(mr_point2d){(double)nx, (double)ny},
                    &(mr_point2d){(double)gx, (double)gy});
                nodes[nidx].parent_x = cx;
                nodes[nidx].parent_y = cy;
            }
        }
    }

    if (found) {
        int px = gx, py = gy;
        *path_len = 0;
        while (px != sx || py != sy) {
            if (*path_len < MR_PATH_MAX_WP) {
                path_x[*path_len] = px;
                path_y[*path_len] = py;
                (*path_len)++;
            }
            int nidx = py * w + px;
            int tpx = nodes[nidx].parent_x;
            py = nodes[nidx].parent_y;
            px = tpx;
        }
        if (*path_len < MR_PATH_MAX_WP) {
            path_x[*path_len] = sx;
            path_y[*path_len] = sy;
            (*path_len)++;
        }
    }

    free(nodes);
    return found;
}

int mr_a_star(const mr_grid_map *map,
              const mr_point2d *start, const mr_point2d *goal,
              mr_path2d *path_out)
{
    return mr_a_star_custom(map, start, goal,
                            mr_heuristic_euclidean, euclidean_cost,
                            path_out);
}

int mr_a_star_custom(const mr_grid_map *map,
                     const mr_point2d *start, const mr_point2d *goal,
                     mr_heuristic_fn heuristic, mr_cost_fn cost,
                     mr_path2d *path_out)
{
    int sx, sy, gx, gy;
    if (!mr_grid_map_world_to_grid(map, start->x, start->y, &sx, &sy))
        return 0;
    if (!mr_grid_map_world_to_grid(map, goal->x, goal->y, &gx, &gy))
        return 0;

    int px[MR_PATH_MAX_WP], py[MR_PATH_MAX_WP];
    int path_len = 0;

    int found = astar_core(map, sx, sy, gx, gy, heuristic, cost,
                           px, py, &path_len);
    if (!found) return 0;

    path_out->num_waypoints = 0;
    path_out->total_length = 0.0;
    double prev_x = 0.0, prev_y = 0.0;
    int first = 1;

    for (int i = path_len - 1; i >= 0; i--) {
        double wx, wy;
        mr_grid_map_grid_to_world(map, px[i], py[i], &wx, &wy);
        path_out->waypoints[path_out->num_waypoints].x = wx;
        path_out->waypoints[path_out->num_waypoints].y = wy;
        if (!first) {
            path_out->total_length += sqrt(
                (wx - prev_x) * (wx - prev_x)
                + (wy - prev_y) * (wy - prev_y));
        }
        prev_x = wx;
        prev_y = wy;
        first = 0;
        path_out->num_waypoints++;
        if (path_out->num_waypoints >= MR_PATH_MAX_WP) break;
    }

    return 1;
}

int mr_dijkstra(const mr_grid_map *map,
                const mr_point2d *start, const mr_point2d *goal,
                mr_path2d *path_out)
{
    return mr_a_star_custom(map, start, goal,
                            (mr_heuristic_fn)NULL, euclidean_cost, path_out);
}

static double random_double(double min, double max)
{
    return min + (max - min) * (double)rand() / (double)RAND_MAX;
}

static double config_distance(const mr_config2d *a, const mr_config2d *b)
{
    double dx = a->x - b->x;
    double dy = a->y - b->y;
    double dt = a->theta - b->theta;
    return sqrt(dx * dx + dy * dy + dt * dt);
}

static mr_config2d extend_toward(const mr_config2d *from,
                                 const mr_config2d *to,
                                 double step)
{
    double d = config_distance(from, to);
    mr_config2d result;
    if (d < step) {
        result = *to;
    } else {
        double ratio = step / d;
        result.x = from->x + (to->x - from->x) * ratio;
        result.y = from->y + (to->y - from->y) * ratio;
        result.theta = from->theta + (to->theta - from->theta) * ratio;
    }
    return result;
}

void mr_rrt_init(mr_rrt_tree *tree, const mr_world2d *world,
                 double step_size, double goal_bias)
{
    tree->num_nodes = 0;
    tree->world = *world;
    tree->step_size = step_size;
    tree->goal_bias = goal_bias;
}

static int nearest_node(const mr_rrt_tree *tree, const mr_config2d *cfg)
{
    double min_d = DBL_MAX;
    int best = -1;
    for (int i = 0; i < tree->num_nodes; i++) {
        double d = config_distance(&tree->nodes[i].config, cfg);
        if (d < min_d) {
            min_d = d;
            best = i;
        }
    }
    return best;
}

int mr_rrt_plan(mr_rrt_tree *tree,
                const mr_config2d *start, const mr_config2d *goal,
                int max_iter, mr_path2d *path_out)
{
    tree->nodes[0].config = *start;
    tree->nodes[0].parent_idx = -1;
    tree->nodes[0].cost = 0.0;
    tree->num_nodes = 1;

    for (int iter = 0; iter < max_iter && tree->num_nodes < MR_RRT_MAX_NODES - 1; iter++) {
        mr_config2d sample;
        if (random_double(0.0, 1.0) < tree->goal_bias) {
            sample = *goal;
        } else {
            sample.x = random_double(0.0, tree->world.width);
            sample.y = random_double(0.0, tree->world.height);
            sample.theta = random_double(-M_PI, M_PI);
        }

        int nearest = nearest_node(tree, &sample);
        if (nearest < 0) continue;

        mr_config2d new_cfg = extend_toward(&tree->nodes[nearest].config,
                                            &sample, tree->step_size);

        if (mr_collision_check_point(
                &(mr_point2d){new_cfg.x, new_cfg.y}, &tree->world))
            continue;

        int nidx = tree->num_nodes;
        tree->nodes[nidx].config = new_cfg;
        tree->nodes[nidx].parent_idx = nearest;
        tree->nodes[nidx].cost = tree->nodes[nearest].cost
                                 + config_distance(&tree->nodes[nearest].config, &new_cfg);
        tree->num_nodes++;

        if (config_distance(&new_cfg, goal) < tree->step_size) {
            tree->nodes[tree->num_nodes].config = *goal;
            tree->nodes[tree->num_nodes].parent_idx = nidx;
            tree->nodes[tree->num_nodes].cost = tree->nodes[nidx].cost
                                                + config_distance(&new_cfg, goal);
            tree->num_nodes++;

            path_out->num_waypoints = 0;
            int ci = tree->num_nodes - 1;
            while (ci >= 0 && path_out->num_waypoints < MR_PATH_MAX_WP) {
                path_out->waypoints[path_out->num_waypoints].x
                    = tree->nodes[ci].config.x;
                path_out->waypoints[path_out->num_waypoints].y
                    = tree->nodes[ci].config.y;
                path_out->num_waypoints++;
                ci = tree->nodes[ci].parent_idx;
            }
            return 1;
        }
    }
    return 0;
}

int mr_rrt_star_plan(mr_rrt_tree *tree,
                     const mr_config2d *start, const mr_config2d *goal,
                     int max_iter, double radius, mr_path2d *path_out)
{
    tree->nodes[0].config = *start;
    tree->nodes[0].parent_idx = -1;
    tree->nodes[0].cost = 0.0;
    tree->num_nodes = 1;

    int best_goal_idx = -1;
    double best_goal_cost = DBL_MAX;

    for (int iter = 0; iter < max_iter && tree->num_nodes < MR_RRT_MAX_NODES - 1; iter++) {
        mr_config2d sample;
        if (random_double(0.0, 1.0) < tree->goal_bias) {
            sample = *goal;
        } else {
            sample.x = random_double(0.0, tree->world.width);
            sample.y = random_double(0.0, tree->world.height);
            sample.theta = random_double(-M_PI, M_PI);
        }

        int nearest = nearest_node(tree, &sample);
        if (nearest < 0) continue;

        mr_config2d new_cfg = extend_toward(&tree->nodes[nearest].config,
                                            &sample, tree->step_size);

        if (mr_collision_check_point(
                &(mr_point2d){new_cfg.x, new_cfg.y}, &tree->world))
            continue;

        double min_cost = tree->nodes[nearest].cost
                          + config_distance(&tree->nodes[nearest].config, &new_cfg);
        int best_parent = nearest;

        for (int j = 0; j < tree->num_nodes; j++) {
            if (config_distance(&tree->nodes[j].config, &new_cfg) <= radius) {
                double cost_via_j = tree->nodes[j].cost
                                    + config_distance(&tree->nodes[j].config, &new_cfg);
                if (cost_via_j < min_cost) {
                    min_cost = cost_via_j;
                    best_parent = j;
                }
            }
        }

        int nidx = tree->num_nodes;
        tree->nodes[nidx].config = new_cfg;
        tree->nodes[nidx].parent_idx = best_parent;
        tree->nodes[nidx].cost = min_cost;
        tree->num_nodes++;

        for (int j = 0; j < tree->num_nodes - 1; j++) {
            double d = config_distance(&tree->nodes[j].config, &new_cfg);
            if (d <= radius) {
                double cost_via_new = min_cost + d;
                if (cost_via_new < tree->nodes[j].cost) {
                    tree->nodes[j].parent_idx = nidx;
                    tree->nodes[j].cost = cost_via_new;
                }
            }
        }

        double dg = config_distance(&new_cfg, goal);
        if (dg < tree->step_size && min_cost + dg < best_goal_cost) {
            best_goal_cost = min_cost + dg;
            best_goal_idx = nidx;
        }
    }

    if (best_goal_idx >= 0) {
        tree->nodes[tree->num_nodes].config = *goal;
        tree->nodes[tree->num_nodes].parent_idx = best_goal_idx;
        tree->nodes[tree->num_nodes].cost = best_goal_cost;
        tree->num_nodes++;

        path_out->num_waypoints = 0;
        int ci = tree->num_nodes - 1;
        while (ci >= 0 && path_out->num_waypoints < MR_PATH_MAX_WP) {
            path_out->waypoints[path_out->num_waypoints].x
                = tree->nodes[ci].config.x;
            path_out->waypoints[path_out->num_waypoints].y
                = tree->nodes[ci].config.y;
            path_out->num_waypoints++;
            ci = tree->nodes[ci].parent_idx;
        }
        return 1;
    }

    return 0;
}

void mr_rrt_smooth_path(const mr_path2d *raw, mr_path2d *smooth)
{
    if (raw->num_waypoints < 3) {
        *smooth = *raw;
        return;
    }

    smooth->num_waypoints = 0;
    smooth->waypoints[smooth->num_waypoints++] = raw->waypoints[0];

    for (int i = 1; i < raw->num_waypoints - 1; i++) {
        smooth->waypoints[smooth->num_waypoints].x
            = (raw->waypoints[i - 1].x + raw->waypoints[i].x
               + raw->waypoints[i + 1].x) / 3.0;
        smooth->waypoints[smooth->num_waypoints].y
            = (raw->waypoints[i - 1].y + raw->waypoints[i].y
               + raw->waypoints[i + 1].y) / 3.0;
        smooth->num_waypoints++;
    }

    smooth->waypoints[smooth->num_waypoints++]
        = raw->waypoints[raw->num_waypoints - 1];
}

void mr_trajectory_from_waypoints(const mr_path2d *path,
                                  double max_vel, double max_accel,
                                  mr_trajectory *traj)
{
    traj->num_points = 0;
    traj->duration = 0.0;
    double total_len = path->total_length;
    double accel_dist = 0.5 * max_vel * max_vel / max_accel;
    double cruise_dist = total_len - 2.0 * accel_dist;
    if (cruise_dist < 0.0) cruise_dist = 0.0;

    double t = 0.0, dt = 0.05;
    double vel = 0.0, dist = 0.0;

    for (int i = 0; i < path->num_waypoints && traj->num_points < MR_PATH_MAX_WP; i++) {
        if (dist < accel_dist)
            vel += max_accel * dt;
        else if (dist < accel_dist + cruise_dist)
            vel = max_vel;
        else if (dist < total_len)
            vel -= max_accel * dt;
        if (vel < 0.0) vel = 0.0;
        if (vel > max_vel) vel = max_vel;

        traj->points[traj->num_points].config.x = path->waypoints[i].x;
        traj->points[traj->num_points].config.y = path->waypoints[i].y;
        traj->points[traj->num_points].velocity = vel;
        traj->num_points++;
        t += dt;
        dist += vel * dt;
    }
    traj->duration = t;
}

void mr_trajectory_spline(const mr_path2d *path, int samples_per_seg,
                          mr_trajectory *traj)
{
    traj->num_points = 0;
    int n = path->num_waypoints;
    if (n < 2) return;

    for (int seg = 0; seg < n - 1; seg++) {
        double x0 = path->waypoints[seg].x,
               y0 = path->waypoints[seg].y;
        double x1 = path->waypoints[seg + 1].x,
               y1 = path->waypoints[seg + 1].y;

        for (int s = 0; s <= samples_per_seg; s++) {
            double t = (double)s / (double)samples_per_seg;
            if (traj->num_points >= MR_PATH_MAX_WP) return;

            traj->points[traj->num_points].config.x = x0 + (x1 - x0) * t;
            traj->points[traj->num_points].config.y = y0 + (y1 - y0) * t;
            traj->points[traj->num_points].velocity = 0.5;
            traj->num_points++;
        }
    }
}

int mr_collision_check_point(const mr_point2d *p, const mr_world2d *world)
{
    for (int i = 0; i < world->num_obstacles; i++) {
        const mr_obstacle *obs = &world->obstacles[i];
        if (obs->type == MR_OBSTACLE_CIRCLE) {
            double dx = p->x - obs->circle.center.x;
            double dy = p->y - obs->circle.center.y;
            if (dx * dx + dy * dy <= obs->circle.radius * obs->circle.radius)
                return 1;
        }
    }
    if (p->x < 0 || p->x > world->width || p->y < 0 || p->y > world->height)
        return 1;
    return 0;
}

int mr_collision_check_segment(const mr_point2d *a, const mr_point2d *b,
                               const mr_world2d *world)
{
    int steps = 10;
    for (int i = 0; i <= steps; i++) {
        double t = (double)i / (double)steps;
        mr_point2d p;
        p.x = a->x + (b->x - a->x) * t;
        p.y = a->y + (b->y - a->y) * t;
        if (mr_collision_check_point(&p, world)) return 1;
    }
    return 0;
}

double mr_repulsive_potential(const mr_point2d *robot,
                              const mr_world2d *world,
                              double eta, double d0,
                              mr_point2d *force_out)
{
    force_out->x = 0.0;
    force_out->y = 0.0;
    double total_potential = 0.0;

    for (int i = 0; i < world->num_obstacles; i++) {
        const mr_obstacle *obs = &world->obstacles[i];
        double ox, oy;

        if (obs->type == MR_OBSTACLE_CIRCLE) {
            ox = obs->circle.center.x;
            oy = obs->circle.center.y;
        } else {
            ox = obs->polygon.vertices[0].x;
            oy = obs->polygon.vertices[0].y;
        }

        double dx = robot->x - ox;
        double dy = robot->y - oy;
        double d = sqrt(dx * dx + dy * dy);

        if (d < d0 && d > 1e-6) {
            double pot = 0.5 * eta * (1.0 / d - 1.0 / d0)
                         * (1.0 / d - 1.0 / d0);
            total_potential += pot;
            double force_mag = eta * (1.0 / d - 1.0 / d0) / (d * d);
            force_out->x += force_mag * dx / d;
            force_out->y += force_mag * dy / d;
        }
    }

    return total_potential;
}
