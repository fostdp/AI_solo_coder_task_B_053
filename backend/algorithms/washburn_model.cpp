#include "washburn_model.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace porcelain_monitor {
namespace algorithms {

WashburnPenetrationModel::WashburnPenetrationModel() = default;

void WashburnPenetrationModel::set_config(const WashburnConfig& config) {
    config_ = config;
}

double WashburnPenetrationModel::effective_radius(double crack_width_um) const {
    double crack_width_m = crack_width_um * 1e-6;
    double r_parallel_plate = crack_width_m / 4.0;
    return r_parallel_plate / config_.surface_roughness_factor;
}

double WashburnPenetrationModel::capillary_pressure(double crack_width_um,
                                                    double surface_tension_n_m,
                                                    double contact_angle_deg) const {
    double r = effective_radius(crack_width_um);
    double theta_rad = contact_angle_deg * M_PI / 180.0;
    if (r < 1e-12) return 0.0;
    return (2.0 * surface_tension_n_m * cos(theta_rad)) / r;
}

double WashburnPenetrationModel::penetration_depth_at_time(double time_s,
                                                           double crack_width_um,
                                                           double viscosity_pa_s,
                                                           double surface_tension_n_m,
                                                           double contact_angle_deg) const {
    if (time_s <= 0.0) return 0.0;

    double r = effective_radius(crack_width_um);
    double theta_rad = contact_angle_deg * M_PI / 180.0;
    double tau = config_.tortuosity_factor;

    double cos_theta = cos(theta_rad);
    double capillary_term = surface_tension_n_m * cos_theta * r;
    double viscous_term = 2.0 * viscosity_pa_s * tau;

    if (viscous_term < 1e-20) return 0.0;

    double h_ideal_sq = (capillary_term * time_s) / viscous_term;
    if (h_ideal_sq <= 0.0) return 0.0;

    double h_ideal_m = sqrt(h_ideal_sq);

    double gravity_threshold_m = 1e-4;
    if (h_ideal_m < gravity_threshold_m) {
        return h_ideal_m * 1e6;
    }

    double rho = 1000.0;
    double g = config_.gravity_m_s2;
    double h_eq = (2.0 * surface_tension_n_m * cos_theta) / (rho * g * r);

    if (h_eq < 1e-6) {
        return h_ideal_m * 1e6;
    }

    double h = h_ideal_m;
    double dt = time_s / 200.0;
    double t = 0.0;
    double h_prev = 0.0;

    while (t < time_s && h < h_eq * 0.999) {
        double dh_dt_numerator = capillary_term - rho * g * r * r * h;
        double dh_dt_denominator = 4.0 * viscosity_pa_s * tau * h;
        if (dh_dt_denominator < 1e-20) break;
        if (dh_dt_numerator <= 0.0) break;

        double dh_dt = dh_dt_numerator / dh_dt_denominator;
        double dh = dh_dt * dt;

        h_prev = h;
        h += dh;
        t += dt;

        if (dh < 1e-15) break;
    }

    return h * 1e6;
}

double WashburnPenetrationModel::time_to_reach_depth(double target_depth_um,
                                                     double crack_width_um,
                                                     double viscosity_pa_s,
                                                     double surface_tension_n_m,
                                                     double contact_angle_deg) const {
    if (target_depth_um <= 0.0) return 0.0;

    double r = effective_radius(crack_width_um);
    double theta_rad = contact_angle_deg * M_PI / 180.0;
    double tau = config_.tortuosity_factor;

    double target_depth_m = target_depth_um * 1e-6;
    double cos_theta = cos(theta_rad);

    double numerator = 2.0 * viscosity_pa_s * tau * target_depth_m * target_depth_m;
    double denominator = surface_tension_n_m * cos_theta * r;

    if (denominator < 1e-20) return 1e20;

    double t_ideal = numerator / denominator;

    double gravity_threshold_m = 1e-4;
    if (target_depth_m < gravity_threshold_m) {
        return t_ideal;
    }

    double rho = 1000.0;
    double g = config_.gravity_m_s2;
    double h_eq = (2.0 * surface_tension_n_m * cos_theta) / (rho * g * r);

    if (target_depth_m >= h_eq * 0.99) {
        return 1e20;
    }

    double h = 0.0;
    double t = 0.0;
    double dt = t_ideal / 500.0;
    if (dt < 1e-6) dt = 1e-6;

    while (h < target_depth_m && t < 1e8) {
        double dh_dt_numerator = surface_tension_n_m * cos_theta * r - rho * g * r * r * h;
        double dh_dt_denominator = 4.0 * viscosity_pa_s * tau * std::max(h, 1e-9);
        if (dh_dt_denominator < 1e-20) break;
        if (dh_dt_numerator <= 0.0) break;

        double dh_dt = dh_dt_numerator / dh_dt_denominator;
        double dh = dh_dt * dt;

        if (h + dh > target_depth_m) {
            double remaining = target_depth_m - h;
            t += remaining / dh_dt;
            h = target_depth_m;
            break;
        }

        h += dh;
        t += dt;

        if (dh < 1e-15) break;
    }

    return t;
}

double WashburnPenetrationModel::penetration_rate(double depth_um,
                                                  double crack_width_um,
                                                  double viscosity_pa_s,
                                                  double surface_tension_n_m,
                                                  double contact_angle_deg) const {
    if (depth_um <= 0.0) return 0.0;

    double r = effective_radius(crack_width_um);
    double theta_rad = contact_angle_deg * M_PI / 180.0;
    double tau = config_.tortuosity_factor;

    double depth_m = depth_um * 1e-6;
    double cos_theta = cos(theta_rad);

    double numerator = surface_tension_n_m * cos_theta * r;
    double denominator = 4.0 * viscosity_pa_s * tau * depth_m;

    if (denominator < 1e-20) return 0.0;

    double dh_dt_ideal = numerator / denominator;

    double gravity_threshold_m = 1e-4;
    if (depth_m < gravity_threshold_m) {
        return dh_dt_ideal * 1e6;
    }

    double rho = 1000.0;
    double g = config_.gravity_m_s2;
    double gravity_term = (rho * g * r * r) / (4.0 * viscosity_pa_s * tau);

    double dh_dt = std::max(0.0, dh_dt_ideal - gravity_term);
    return dh_dt * 1e6;
}

PenetrationPrediction WashburnPenetrationModel::predict(int crack_id,
                                                        int material_id,
                                                        const CrackInfo& crack,
                                                        const RepairMaterial& material,
                                                        double target_depth_um) {
    PenetrationPrediction result;
    result.crack_id = crack_id;
    result.material_id = material_id;
    result.method = "Washburn";
    result.target_depth_um = target_depth_um;
    result.created_at = std::chrono::system_clock::now();

    double crack_width_um = crack.max_width > 0 ? crack.max_width * 1e6 : config_.default_surface_tension_n_m;
    if (crack.max_width <= 0) crack_width_um = 50.0;

    double viscosity_pa_s = material.viscosity > 0 ? material.viscosity : config_.default_viscosity_pa_s;
    double surface_tension_n_m = config_.default_surface_tension_n_m;
    double contact_angle_deg = config_.default_contact_angle_deg;

    if (material.properties.contains("surface_tension_n_m")) {
        surface_tension_n_m = material.properties["surface_tension_n_m"].get<double>();
    }
    if (material.properties.contains("contact_angle_deg")) {
        contact_angle_deg = material.properties["contact_angle_deg"].get<double>();
    }

    result.crack_width_um = crack_width_um;
    result.viscosity_pa_s = viscosity_pa_s;
    result.surface_tension_n_m = surface_tension_n_m;
    result.contact_angle_deg = contact_angle_deg;

    double predicted_time_s = time_to_reach_depth(
        target_depth_um, crack_width_um, viscosity_pa_s,
        surface_tension_n_m, contact_angle_deg
    );
    result.predicted_time_s = predicted_time_s;

    double rate_at_target = penetration_rate(
        target_depth_um, crack_width_um, viscosity_pa_s,
        surface_tension_n_m, contact_angle_deg
    );
    result.penetration_rate_um_s = rate_at_target;

    int n_points = config_.time_series_points;
    double t_max = std::min(predicted_time_s * 1.2, predicted_time_s + 10.0);
    if (!std::isfinite(t_max) || t_max <= 0) t_max = 10.0;

    result.time_series.reserve(n_points + 1);
    result.depth_series.reserve(n_points + 1);

    for (int i = 0; i <= n_points; ++i) {
        double t = (t_max * i) / n_points;
        double h = penetration_depth_at_time(
            t, crack_width_um, viscosity_pa_s,
            surface_tension_n_m, contact_angle_deg
        );
        result.time_series.push_back(t);
        result.depth_series.push_back(h);
    }

    result.parameters = {
        {"tortuosity_factor", config_.tortuosity_factor},
        {"surface_roughness_factor", config_.surface_roughness_factor},
        {"gravity_m_s2", config_.gravity_m_s2},
        {"time_series_points", config_.time_series_points},
        {"effective_radius_um", effective_radius(crack_width_um) * 1e6},
        {"capillary_pressure_pa", capillary_pressure(crack_width_um, surface_tension_n_m, contact_angle_deg)}
    };

    result.result = result_to_json(result);

    return result;
}

json WashburnPenetrationModel::result_to_json(const PenetrationPrediction& prediction) const {
    json j;
    j["crack_id"] = prediction.crack_id;
    j["material_id"] = prediction.material_id;
    j["method"] = prediction.method;
    j["target_depth_um"] = prediction.target_depth_um;
    j["crack_width_um"] = prediction.crack_width_um;
    j["viscosity_pa_s"] = prediction.viscosity_pa_s;
    j["surface_tension_n_m"] = prediction.surface_tension_n_m;
    j["contact_angle_deg"] = prediction.contact_angle_deg;
    j["predicted_time_s"] = prediction.predicted_time_s;
    j["penetration_rate_um_s"] = prediction.penetration_rate_um_s;
    j["time_series"] = prediction.time_series;
    j["depth_series"] = prediction.depth_series;
    j["parameters"] = prediction.parameters;

    double effective_r_um = 0.0;
    double capillary_p = 0.0;
    try {
        effective_r_um = prediction.parameters.at("effective_radius_um").get<double>();
        capillary_p = prediction.parameters.at("capillary_pressure_pa").get<double>();
    } catch (...) {}

    double theta_rad = prediction.contact_angle_deg * M_PI / 180.0;
    double cos_theta = std::cos(theta_rad);
    double r_m = effective_r_um * 1e-6;
    double rho = 1000.0;
    double equilibrium_h_um = 0.0;
    if (r_m > 1e-15 && config_.gravity_m_s2 > 0) {
        equilibrium_h_um = (2.0 * prediction.surface_tension_n_m * cos_theta) /
                          (rho * config_.gravity_m_s2 * r_m) * 1e6;
    }

    j["derived_quantities"] = {
        {"effective_radius_um", effective_r_um},
        {"capillary_pressure_pa", capillary_p},
        {"tortuosity_factor", config_.tortuosity_factor},
        {"surface_roughness_factor", config_.surface_roughness_factor},
        {"equilibrium_penetration_depth_um", equilibrium_h_um},
        {"gravity_correction_applied", prediction.target_depth_um > 100.0}
    };

    return j;
}

}
}
