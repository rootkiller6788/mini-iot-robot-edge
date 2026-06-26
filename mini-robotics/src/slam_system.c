#include "slam_system.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double random_gaussian(void)
{
    double u1 = (double)rand() / (double)RAND_MAX;
    double u2 = (double)rand() / (double)RAND_MAX;
    return sqrt(-2.0 * log(u1 + 1e-12)) * cos(2.0 * M_PI * u2);
}

void mr_ekf_slam_init(mr_ekf_slam *slam)
{
    memset(slam, 0, sizeof(mr_ekf_slam));
    slam->cov.size = MR_SLAM_STATE_DIM;
    int i;
    for (i = 0; i < MR_SLAM_STATE_DIM; i++)
        slam->cov.m[i][i] = 0.01;
}

void mr_ekf_slam_predict(mr_ekf_slam *slam, double dx, double dy, double dt)
{
    double ct = cos(slam->pose.theta);
    double st = sin(slam->pose.theta);

    slam->pose.x += dx * ct - dy * st;
    slam->pose.y += dx * st + dy * ct;
    slam->pose.theta += dt;

    while (slam->pose.theta > M_PI) slam->pose.theta -= 2.0 * M_PI;
    while (slam->pose.theta < -M_PI) slam->pose.theta += 2.0 * M_PI;

    double G[3][3] = {
        {1.0, 0.0, -dx * st - dy * ct},
        {0.0, 1.0,  dx * ct - dy * st},
        {0.0, 0.0, 1.0}
    };

    int i, j, k;
    double Q[3] = {0.01, 0.01, 0.005};
    double F[MR_SLAM_STATE_DIM + MR_SLAM_LM_DIM * MR_SLAM_MAX_LANDMARKS]
            [MR_SLAM_STATE_DIM + MR_SLAM_LM_DIM * MR_SLAM_MAX_LANDMARKS];
    int dim = slam->cov.size;

    memset(F, 0, sizeof(F));
    for (i = 0; i < dim; i++) F[i][i] = 1.0;
    if (dim >= 3) {
        for (i = 0; i < 3; i++)
            for (j = 0; j < 3; j++)
                F[i][j] = G[i][j];
    }

    double tmp[256][256] = {{0}};
    for (i = 0; i < dim && i < 256; i++)
        for (j = 0; j < dim && j < 256; j++)
            for (k = 0; k < dim && k < 256; k++)
                tmp[i][j] += F[i][k] * slam->cov.m[k][j];

    for (i = 0; i < dim && i < 256; i++)
        for (j = 0; j < dim && j < 256; j++) {
            slam->cov.m[i][j] = 0.0;
            for (k = 0; k < dim && k < 256; k++)
                slam->cov.m[i][j] += tmp[i][k] * F[j][k];
        }

    for (i = 0; i < 3 && i < dim; i++)
        slam->cov.m[i][i] += Q[i];
}

int mr_ekf_slam_update(mr_ekf_slam *slam,
                       const mr_landmark_obs *observations,
                       int num_obs)
{
    int updated = 0;

    for (int o = 0; o < num_obs; o++) {
        const mr_landmark_obs *obs = &observations[o];

        if (obs->id >= 0) {
            int lm_idx = -1;
            for (int li = 0; li < slam->num_landmarks; li++)
                if (slam->lm_ids[li] == obs->id) { lm_idx = li; break; }

            if (lm_idx < 0) {
                mr_ekf_slam_add_landmark(slam, obs->id,
                                         obs->range, obs->bearing);
                lm_idx = slam->num_landmarks - 1;
            }

            double lx = slam->lm_x[lm_idx], ly = slam->lm_y[lm_idx];
            double dx = lx - slam->pose.x, dy = ly - slam->pose.y;
            double q = dx * dx + dy * dy;
            double z_hat_range = sqrt(q);
            double z_hat_bearing = atan2(dy, dx) - slam->pose.theta;

            while (z_hat_bearing > M_PI) z_hat_bearing -= 2.0 * M_PI;
            while (z_hat_bearing < -M_PI) z_hat_bearing += 2.0 * M_PI;

            double y_range = obs->range - z_hat_range;
            double y_bearing = obs->bearing - z_hat_bearing;

            double innov[2] = {y_range, y_bearing};
            double R[2][2] = {{0.01, 0.0}, {0.0, 0.001}};

            double H[2][3] = {
                {-dx / z_hat_range, -dy / z_hat_range, 0.0},
                { dy / q,           -dx / q,          -1.0}
            };

            double S[2][2] = {{R[0][0], R[0][1]}, {R[1][0], R[1][1]}};
            int i, j, k;
            for (i = 0; i < 2; i++)
                for (j = 0; j < 2; j++)
                    for (k = 0; k < 3; k++)
                        S[i][j] += H[i][k] * slam->cov.m[k][j] * H[j][k];

            double detS = S[0][0] * S[1][1] - S[0][1] * S[1][0];
            if (fabs(detS) < 1e-12) continue;

            double invS[2][2] = {
                { S[1][1] / detS, -S[0][1] / detS},
                {-S[1][0] / detS,  S[0][0] / detS}
            };

            double K[3][2] = {{0}};
            for (i = 0; i < 3; i++)
                for (j = 0; j < 2; j++)
                    for (k = 0; k < 2; k++)
                        K[i][j] += slam->cov.m[i][k] * H[k][j] * invS[k][j]
                                   + slam->cov.m[i][k + 1] * H[k + 1][j]
                                   * invS[k + 1][j];

            double dx_pose = K[0][0] * innov[0] + K[0][1] * innov[1];
            double dy_pose = K[1][0] * innov[0] + K[1][1] * innov[1];
            double dt_pose = K[2][0] * innov[0] + K[2][1] * innov[1];

            slam->pose.x += dx_pose;
            slam->pose.y += dy_pose;
            slam->pose.theta += dt_pose;

            updated = 1;
        } else {
            mr_ekf_slam_add_landmark(slam, -(obs->id + 1),
                                     obs->range, obs->bearing);
        }
    }

    return updated;
}

void mr_ekf_slam_get_pose(const mr_ekf_slam *slam, mr_robot_pose *pose)
{
    *pose = slam->pose;
}

int mr_ekf_slam_get_landmark(const mr_ekf_slam *slam, int id,
                             double *x, double *y)
{
    for (int i = 0; i < slam->num_landmarks; i++)
        if (slam->lm_ids[i] == id) {
            *x = slam->lm_x[i];
            *y = slam->lm_y[i];
            return 1;
        }
    return 0;
}

int mr_ekf_slam_add_landmark(mr_ekf_slam *slam, int id,
                             double range, double bearing)
{
    if (slam->num_landmarks >= MR_SLAM_MAX_LANDMARKS) return 0;

    double global_bearing = slam->pose.theta + bearing;
    int idx = slam->num_landmarks;

    slam->lm_x[idx] = slam->pose.x + range * cos(global_bearing);
    slam->lm_y[idx] = slam->pose.y + range * sin(global_bearing);
    slam->lm_ids[idx] = id;
    slam->num_landmarks++;

    slam->cov.size = MR_SLAM_STATE_DIM + MR_SLAM_LM_DIM * slam->num_landmarks;

    int bi = MR_SLAM_STATE_DIM + MR_SLAM_LM_DIM * idx;
    slam->cov.m[bi][bi] = 1.0;
    slam->cov.m[bi + 1][bi + 1] = 1.0;

    return 1;
}

void mr_fast_slam_init(mr_fast_slam *fs, int num_particles)
{
    fs->num_particles = num_particles;
    int i;
    for (i = 0; i < num_particles && i < MR_SLAM_MAX_PARTICLES; i++) {
        fs->particles[i].weight = 1.0 / num_particles;
        fs->particles[i].pose.x = 0.0;
        fs->particles[i].pose.y = 0.0;
        fs->particles[i].pose.theta = 0.0;
        fs->lm_counts[i] = 0;
    }
}

void mr_fast_slam_predict(mr_fast_slam *fs,
                          double dx, double dy, double dt,
                          double noise_std)
{
    int i;
    for (i = 0; i < fs->num_particles; i++) {
        double ndx = dx + random_gaussian() * noise_std;
        double ndy = dy + random_gaussian() * noise_std;
        double ndt = dt + random_gaussian() * noise_std * 0.5;

        double ct = cos(fs->particles[i].pose.theta);
        double st = sin(fs->particles[i].pose.theta);

        fs->particles[i].pose.x += ndx * ct - ndy * st;
        fs->particles[i].pose.y += ndx * st + ndy * ct;
        fs->particles[i].pose.theta += ndt;
    }
}

void mr_fast_slam_update(mr_fast_slam *fs,
                         const mr_landmark_obs *observations,
                         int num_obs)
{
    int i, o;
    double total_weight = 0.0;

    for (i = 0; i < fs->num_particles; i++) {
        fs->particles[i].weight = 1.0;

        for (o = 0; o < num_obs; o++) {
            const mr_landmark_obs *obs = &observations[o];
            int lm_idx = -1;

            for (int li = 0; li < fs->lm_counts[i]; li++)
                if (obs->id == li) { lm_idx = li; break; }

            if (lm_idx < 0 && fs->lm_counts[i] < MR_SLAM_MAX_LANDMARKS) {
                double gb = fs->particles[i].pose.theta + obs->bearing;
                lm_idx = fs->lm_counts[i];
                fs->landmarks[i][lm_idx].x
                    = fs->particles[i].pose.x + obs->range * cos(gb);
                fs->landmarks[i][lm_idx].y
                    = fs->particles[i].pose.y + obs->range * sin(gb);
                fs->lm_counts[i]++;
            }

            if (lm_idx >= 0) {
                double lx = fs->landmarks[i][lm_idx].x;
                double ly = fs->landmarks[i][lm_idx].y;
                double dx = lx - fs->particles[i].pose.x;
                double dy = ly - fs->particles[i].pose.y;
                double pred_range = sqrt(dx * dx + dy * dy);

                double diff = obs->range - pred_range;
                double likelihood = exp(-0.5 * diff * diff / 0.05) / sqrt(2.0 * M_PI * 0.05);
                fs->particles[i].weight *= (likelihood + 1e-9);
            }
        }

        total_weight += fs->particles[i].weight;
    }

    if (total_weight > 1e-12)
        for (i = 0; i < fs->num_particles; i++)
            fs->particles[i].weight /= total_weight;
}

void mr_fast_slam_resample(mr_fast_slam *fs)
{
    mr_particle new_particles[MR_SLAM_MAX_PARTICLES];
    mr_lm_estimate new_landmarks[MR_SLAM_MAX_PARTICLES][MR_SLAM_MAX_LANDMARKS];
    int new_lm_counts[MR_SLAM_MAX_PARTICLES];

    double cdf[MR_SLAM_MAX_PARTICLES];
    cdf[0] = fs->particles[0].weight;
    int i;
    for (i = 1; i < fs->num_particles; i++)
        cdf[i] = cdf[i - 1] + fs->particles[i].weight;

    for (i = 0; i < fs->num_particles; i++) {
        double r = (double)rand() / (double)RAND_MAX;
        int j = 0;
        while (j < fs->num_particles - 1 && cdf[j] < r) j++;

        new_particles[i] = fs->particles[j];
        new_particles[i].weight = 1.0 / fs->num_particles;
        new_lm_counts[i] = fs->lm_counts[j];
        memcpy(new_landmarks[i], fs->landmarks[j],
               sizeof(mr_lm_estimate) * (size_t)fs->lm_counts[j]);
    }

    memcpy(fs->particles, new_particles,
           sizeof(mr_particle) * (size_t)fs->num_particles);
    memcpy(fs->landmarks, new_landmarks,
           sizeof(fs->landmarks));
    memcpy(fs->lm_counts, new_lm_counts,
           sizeof(int) * (size_t)fs->num_particles);
}

void mr_fast_slam_best_pose(const mr_fast_slam *fs, mr_robot_pose *pose)
{
    double best_w = -1.0;
    int best = 0;
    int i;
    for (i = 0; i < fs->num_particles; i++)
        if (fs->particles[i].weight > best_w) {
            best_w = fs->particles[i].weight;
            best = i;
        }
    *pose = fs->particles[best].pose;
}

void mr_graph_slam_init(mr_graph_slam *gs, const mr_robot_pose *initial)
{
    memset(gs, 0, sizeof(mr_graph_slam));
    if (initial) {
        gs->nodes[0] = *initial;
        gs->num_nodes = 1;
    }
}

int mr_graph_slam_add_node(mr_graph_slam *gs, const mr_robot_pose *pose)
{
    if (gs->num_nodes >= MR_SLAM_MAX_LANDMARKS + MR_SLAM_MAX_PARTICLES)
        return -1;
    int idx = gs->num_nodes;
    gs->nodes[idx] = *pose;
    gs->num_nodes++;
    return idx;
}

int mr_graph_slam_add_edge(mr_graph_slam *gs, int from, int to,
                           double dx, double dy, double dtheta)
{
    if (gs->num_edges >= MR_SLAM_MAX_LANDMARKS * 4) return 0;
    int idx = gs->num_edges;
    gs->edges[idx].from = from;
    gs->edges[idx].to = to;
    gs->edges[idx].dx = dx;
    gs->edges[idx].dy = dy;
    gs->edges[idx].dtheta = dtheta;
    gs->num_edges++;
    return 1;
}

int mr_graph_slam_optimize(mr_graph_slam *gs, int max_iters)
{
    int iter, e;
    double alpha = 1.0;

    for (iter = 0; iter < max_iters; iter++) {
        double dx[MR_SLAM_MAX_LANDMARKS + MR_SLAM_MAX_PARTICLES][3];
        memset(dx, 0, sizeof(dx));

        for (e = 0; e < gs->num_edges; e++) {
            int a = gs->edges[e].from;
            int b = gs->edges[e].to;
            if (a >= gs->num_nodes || b >= gs->num_nodes) continue;

            double meas[3] = {gs->edges[e].dx, gs->edges[e].dy,
                              gs->edges[e].dtheta};
            double na[3] = {gs->nodes[a].x, gs->nodes[a].y,
                            gs->nodes[a].theta};
            double nb[3] = {gs->nodes[b].x, gs->nodes[b].y,
                            gs->nodes[b].theta};

            double pred[3] = {
                nb[0] - na[0],
                nb[1] - na[1],
                nb[2] - na[2]
            };
            while (pred[2] > M_PI) pred[2] -= 2.0 * M_PI;
            while (pred[2] < -M_PI) pred[2] += 2.0 * M_PI;

            double err[3] = {meas[0] - pred[0], meas[1] - pred[1],
                             meas[2] - pred[2]};

            dx[a][0] += -alpha * err[0];
            dx[a][1] += -alpha * err[1];
            dx[a][2] += -alpha * err[2];
            dx[b][0] += alpha * err[0];
            dx[b][1] += alpha * err[1];
            dx[b][2] += alpha * err[2];
        }

        int i;
        double max_d = 0.0;
        for (i = 0; i < gs->num_nodes; i++) {
            gs->nodes[i].x += dx[i][0];
            gs->nodes[i].y += dx[i][1];
            gs->nodes[i].theta += dx[i][2];
            double d = dx[i][0] * dx[i][0] + dx[i][1] * dx[i][1]
                       + dx[i][2] * dx[i][2];
            if (d > max_d) max_d = d;
        }
        if (max_d < 1e-6) break;
        alpha *= 0.9;
    }
    return 1;
}

int mr_graph_slam_loop_closure(mr_graph_slam *gs, int node_a, int node_b,
                               double dx, double dy, double dtheta)
{
    return mr_graph_slam_add_edge(gs, node_a, node_b, dx, dy, dtheta);
}

void mr_occ_grid_init(mr_occ_grid *grid, int w, int h,
                      double resolution, double ox, double oy)
{
    grid->width = w;
    grid->height = h;
    grid->resolution = resolution;
    grid->origin_x = ox;
    grid->origin_y = oy;
    memset(grid->cells, 127, sizeof(grid->cells));
}

void mr_occ_grid_update_from_scan(mr_occ_grid *grid,
                                  const mr_robot_pose *pose,
                                  const double *ranges, int num_ranges,
                                  double angle_min, double angle_inc,
                                  double max_range)
{
    int i;
    for (i = 0; i < num_ranges; i++) {
        double angle = angle_min + (double)i * angle_inc;
        double range = ranges[i];

        if (range < 0.01 || range > max_range) continue;

        double wx = pose->x + range * cos(pose->theta + angle);
        double wy = pose->y + range * sin(pose->theta + angle);

        int gx = (int)((wx - grid->origin_x) / grid->resolution);
        int gy = (int)((wy - grid->origin_y) / grid->resolution);

        if (gx >= 0 && gx < grid->width && gy >= 0 && gy < grid->height) {
            int idx = gy * grid->width + gx;
            if (idx < MR_SLAM_MAX_LANDMARKS * MR_SLAM_MAX_LANDMARKS) {
                grid->cells[idx] = (uint8_t)(
                    grid->cells[idx] > 200 ? 250
                    : (grid->cells[idx] + 10));
            }
        }
    }
}

uint8_t mr_occ_grid_get(const mr_occ_grid *grid, int x, int y, int *valid)
{
    if (x >= 0 && x < grid->width && y >= 0 && y < grid->height) {
        *valid = 1;
        return grid->cells[y * grid->width + x];
    }
    *valid = 0;
    return 0;
}

double mr_occ_grid_occupied(const mr_occ_grid *grid, double wx, double wy)
{
    int gx = (int)((wx - grid->origin_x) / grid->resolution);
    int gy = (int)((wy - grid->origin_y) / grid->resolution);
    int valid;
    uint8_t v = mr_occ_grid_get(grid, gx, gy, &valid);
    return valid ? (double)v / 255.0 : -1.0;
}

mr_descriptor mr_compute_scan_descriptor(const double *ranges, int num_ranges)
{
    mr_descriptor d;
    d.length = num_ranges < 64 ? num_ranges : 64;
    d.data = (uint8_t *)malloc((size_t)d.length);
    if (!d.data) { d.length = 0; return d; }

    int step = num_ranges / d.length;
    if (step < 1) step = 1;

    int i;
    for (i = 0; i < d.length; i++) {
        double v = ranges[i * step];
        d.data[i] = (uint8_t)(v > 10.0 ? 255 : (uint8_t)(v * 25.5));
    }
    return d;
}

double mr_descriptor_similarity(const mr_descriptor *a, const mr_descriptor *b)
{
    if (!a->data || !b->data) return 0.0;
    int len = a->length < b->length ? a->length : b->length;
    if (len == 0) return 0.0;

    double sum = 0.0;
    int i;
    for (i = 0; i < len; i++) {
        double diff = (double)a->data[i] - (double)b->data[i];
        sum += diff * diff;
    }
    return 1.0 / (1.0 + sqrt(sum / len));
}

void mr_descriptor_free(mr_descriptor *d)
{
    if (d->data) {
        free(d->data);
        d->data = NULL;
    }
    d->length = 0;
}
