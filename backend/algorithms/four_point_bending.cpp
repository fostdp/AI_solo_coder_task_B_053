#include "four_point_bending.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace porcelain_monitor {
namespace algorithms {

FourPointBendingTest::FourPointBendingTest()
    : failure_load_n_(0.0) {
}

void FourPointBendingTest::set_config(const BendingTestConfig& config) {
    config_ = config;
}

double FourPointBendingTest::moment_of_inertia() const {
    double b = config_.specimen_width_mm * 1e-3;
    double h = config_.specimen_thickness_mm * 1e-3;
    return b * h * h * h / 12.0;
}

double FourPointBendingTest::crack_influence_factor(double crack_depth, double specimen_thickness) const {
    if (crack_depth <= 0.0 || specimen_thickness <= 0.0) return 1.0;
    double a_over_h = crack_depth / specimen_thickness;
    a_over_h = std::min(0.9, std::max(0.0, a_over_h));

    double f = 1.12 - 0.23 * a_over_h + 10.6 * a_over_h * a_over_h
             - 21.7 * a_over_h * a_over_h * a_over_h
             + 30.4 * a_over_h * a_over_h * a_over_h * a_over_h;
    return f;
}

void FourPointBendingTest::compute_bending_moment_distribution(
    std::vector<double>& moments, double load_n) const {
    int n = static_cast<int>(moments.size());
    double L1 = config_.support_span_mm;
    double L2 = config_.loading_span_mm;
    double dx = L1 / (n - 1);
    double support_left = 0.0;
    double support_right = L1;
    double load_left = (L1 - L2) / 2.0;
    double load_right = L1 - (L1 - L2) / 2.0;
    double reaction = load_n / 2.0;

    for (int i = 0; i < n; ++i) {
        double x = i * dx;
        if (x < load_left) {
            moments[i] = reaction * (x - support_left);
        } else if (x <= load_right) {
            moments[i] = reaction * (load_left - support_left);
        } else {
            moments[i] = reaction * (x - support_left) - load_n * (x - load_right);
        }
    }
}

double FourPointBendingTest::interpolate_stress(double y, double y_min, double y_max,
                                                 double moment, double inertia) const {
    if (inertia < 1e-18) return 0.0;
    double y_mid = (y_min + y_max) / 2.0;
    double y_from_na = y - y_mid;
    return moment * y_from_na / inertia;
}

void FourPointBendingTest::build_mesh(std::vector<BendingElement>& mesh, const CrackInfo& crack,
                                      const RepairMaterial& material, bool repaired) {
    int n_elem = config_.mesh_elements;
    mesh.clear();
    mesh.resize(n_elem);

    double L1 = config_.support_span_mm;
    double dx = L1 / n_elem;
    double h = config_.specimen_thickness_mm;

    double crack_center = L1 / 2.0;
    double crack_half_len = 0.0;
    if (!crack.points.empty()) {
        double min_x = 1e18, max_x = -1e18;
        for (const auto& p : crack.points) {
            min_x = std::min(min_x, p.x);
            max_x = std::max(max_x, p.x);
        }
        crack_center = (min_x + max_x) / 2.0;
        crack_half_len = (max_x - min_x) / 2.0;
        if (crack_half_len < 1e-6) crack_half_len = std::max(crack.total_length / 2.0, 1e-3);
    }

    double max_crack_depth = crack.max_depth * 1e-3;
    double bonding_strength = 0.0;
    try {
        if (material.properties.contains("bonding_strength")) {
            bonding_strength = material.properties["bonding_strength"].get<double>();
        }
    } catch (...) {
        bonding_strength = 0.0;
    }

    for (int i = 0; i < n_elem; ++i) {
        double x_center = (i + 0.5) * dx;
        mesh[i].x_center = x_center;
        mesh[i].length = dx;
        mesh[i].y_min = -h / 2.0;
        mesh[i].y_max = h / 2.0;
        mesh[i].damage = 0.0;
        mesh[i].has_crack = false;
        mesh[i].is_repaired = false;
        mesh[i].repair_bonding_strength = 0.0;

        double dist_to_crack = std::abs(x_center - crack_center);
        if (dist_to_crack <= crack_half_len + dx && max_crack_depth > 0.0) {
            mesh[i].has_crack = true;
            double depth_factor = std::exp(-dist_to_crack * dist_to_crack /
                                   (2.0 * (crack_half_len + dx) * (crack_half_len + dx)));
            mesh[i].damage = depth_factor * (max_crack_depth / h);

            if (repaired && bonding_strength > 0.0) {
                mesh[i].is_repaired = true;
                mesh[i].repair_bonding_strength = bonding_strength
                    * config_.repair_interface_strength_ratio;
            }
        }

        for (int k = 0; k < 6; ++k) {
            mesh[i].stress[k] = 0.0;
            mesh[i].strain[k] = 0.0;
        }
    }
}

void FourPointBendingTest::apply_four_point_loads(std::vector<BendingElement>& mesh, double load_n) {
    int n_elem = static_cast<int>(mesh.size());
    if (n_elem == 0) return;

    std::vector<double> moments(n_elem, 0.0);
    compute_bending_moment_distribution(moments, load_n);

    double I = moment_of_inertia();
    double h = config_.specimen_thickness_mm;
    double E = config_.porcelain_youngs_modulus_gpa * 1e9;

    for (int i = 0; i < n_elem; ++i) {
        double M = moments[i];
        double y_top = h / 2.0;
        double y_bottom = -h / 2.0;

        double sigma_top = interpolate_stress(y_top, y_bottom, y_top, M, I);
        double sigma_bottom = interpolate_stress(y_bottom, y_bottom, y_top, M, I);

        if (mesh[i].has_crack) {
            double a = mesh[i].damage * h * 1e-3;
            double f = crack_influence_factor(a, h * 1e-3);
            double sigma_eff_factor = std::sqrt(M_PI * std::max(a, 1e-9)) * f;

            double effective_strength = config_.porcelain_strength_mpa * 1e6;
            if (mesh[i].is_repaired) {
                double repair_strength = mesh[i].repair_bonding_strength * 1e6;
                effective_strength = config_.porcelain_strength_mpa * 1e6 * (1.0 - mesh[i].damage)
                    + repair_strength * mesh[i].damage;
            }

            double max_stress_allowed = effective_strength / std::max(sigma_eff_factor, 1.0);
            if (std::abs(sigma_top) > max_stress_allowed) {
                double scale = max_stress_allowed / std::max(std::abs(sigma_top), 1e-12);
                sigma_top *= scale;
                sigma_bottom *= scale;
            }
        }

        mesh[i].stress[0] = sigma_top;
        mesh[i].stress[1] = sigma_bottom;
        mesh[i].stress[2] = 0.0;
        mesh[i].stress[3] = 0.0;
        mesh[i].stress[4] = 0.0;
        mesh[i].stress[5] = 0.0;

        mesh[i].strain[0] = sigma_top / E;
        mesh[i].strain[1] = sigma_bottom / E;
        mesh[i].strain[2] = 0.0;
        mesh[i].strain[3] = 0.0;
        mesh[i].strain[4] = 0.0;
        mesh[i].strain[5] = 0.0;
    }
}

double FourPointBendingTest::compute_bending_strength(const std::vector<BendingElement>& mesh,
                                                       double bending_moment) const {
    if (mesh.empty()) return 0.0;

    double h = config_.specimen_thickness_mm * 1e-3;
    double I = moment_of_inertia();
    double y_max = h / 2.0;

    double max_stress = 0.0;
    for (const auto& elem : mesh) {
        double current_max = std::max(std::abs(elem.stress[0]), std::abs(elem.stress[1]));
        max_stress = std::max(max_stress, current_max);
    }

    if (max_stress < 1e-12) {
        return bending_moment * y_max / I / 1e6;
    }

    return max_stress / 1e6;
}

void FourPointBendingTest::update_damage(std::vector<BendingElement>& mesh, double load_n) {
    double strength_pa = config_.porcelain_strength_mpa * 1e6;

    for (auto& elem : mesh) {
        double max_stress = std::max(std::abs(elem.stress[0]), std::abs(elem.stress[1]));
        double effective_strength = strength_pa;

        if (elem.has_crack) {
            double h = config_.specimen_thickness_mm * 1e-3;
            double a = elem.damage * h;
            double f = crack_influence_factor(a, h);
            double sigma_eff_factor = std::sqrt(M_PI * std::max(a, 1e-9)) * f;
            effective_strength = strength_pa / std::max(sigma_eff_factor, 1.0);

            if (elem.is_repaired) {
                double repair_strength = elem.repair_bonding_strength * 1e6;
                effective_strength = strength_pa * (1.0 - elem.damage)
                    + repair_strength * elem.damage * config_.repair_interface_strength_ratio;
            }
        }

        if (max_stress > effective_strength) {
            double overload = (max_stress - effective_strength) / std::max(effective_strength, 1.0);
            elem.damage = std::min(1.0, elem.damage + overload * 0.1);
        }
    }
}

void FourPointBendingTest::run_load_steps(std::vector<BendingElement>& mesh,
                                           BendingTestResult& result) {
    int load_steps = config_.load_steps;
    double max_disp = config_.max_displacement_mm;
    double L1 = config_.support_span_mm * 1e-3;
    double L2 = config_.loading_span_mm * 1e-3;
    double h = config_.specimen_thickness_mm * 1e-3;
    double b = config_.specimen_width_mm * 1e-3;
    double E = config_.porcelain_youngs_modulus_gpa * 1e9;
    double I = moment_of_inertia();

    double strength_pa = config_.porcelain_strength_mpa * 1e6;
    double y_max = h / 2.0;
    double max_moment = strength_pa * I / y_max;
    double max_load_est = 4.0 * max_moment / (L1 - L2);

    result.load_displacement_load.clear();
    result.load_displacement_disp.clear();
    failure_load_n_ = 0.0;

    for (int step = 0; step <= load_steps; ++step) {
        double load_ratio = static_cast<double>(step) / load_steps;
        double load_n = load_ratio * max_load_est * 1.5;

        double disp_mm = 0.0;
        if (I > 1e-18) {
            double a = (L1 - L2) / 2.0;
            disp_mm = (load_n * a / (24.0 * E * I)) *
                (3.0 * L1 * L1 - 4.0 * a * a) * 1e3;
        }

        apply_four_point_loads(mesh, load_n);
        update_damage(mesh, load_n);

        double max_stress = 0.0;
        for (const auto& elem : mesh) {
            double s = std::max(std::abs(elem.stress[0]), std::abs(elem.stress[1]));
            max_stress = std::max(max_stress, s);
        }

        result.load_displacement_load.push_back(load_n);
        result.load_displacement_disp.push_back(disp_mm);

        double effective_strength_limit = strength_pa;
        for (const auto& elem : mesh) {
            if (elem.has_crack) {
                double elem_strength = strength_pa;
                if (elem.is_repaired) {
                    double repair_strength = elem.repair_bonding_strength * 1e6;
                    elem_strength = strength_pa * (1.0 - elem.damage)
                        + repair_strength * elem.damage * config_.repair_interface_strength_ratio;
                } else {
                    double ac = elem.damage * h;
                    double f = crack_influence_factor(ac, h);
                    double sigma_eff_factor = std::sqrt(M_PI * std::max(ac, 1e-9)) * f;
                    elem_strength = strength_pa / std::max(sigma_eff_factor, 1.0);
                }
                effective_strength_limit = std::min(effective_strength_limit, elem_strength);
            }
        }

        if (max_stress >= effective_strength_limit) {
            failure_load_n_ = load_n;
            break;
        }

        if (disp_mm > max_disp) {
            failure_load_n_ = load_n;
            break;
        }
    }

    if (failure_load_n_ <= 0.0 && !result.load_displacement_load.empty()) {
        failure_load_n_ = result.load_displacement_load.back();
    }
}

BendingTestResult FourPointBendingTest::simulate(int porcelain_id, int crack_id, int material_id,
                                                  const CrackInfo& crack,
                                                  const RepairMaterial& material,
                                                  bool repaired) {
    BendingTestResult result;
    result.id = 0;
    result.porcelain_id = porcelain_id;
    result.crack_id = crack_id;
    result.material_id = material_id;
    result.method = "FOUR_POINT_BENDING";
    result.created_at = std::chrono::system_clock::now();

    double h = config_.specimen_thickness_mm * 1e-3;
    double b = config_.specimen_width_mm * 1e-3;
    double I = b * h * h * h / 12.0;
    double L1 = config_.support_span_mm * 1e-3;
    double L2 = config_.loading_span_mm * 1e-3;
    double y_max = h / 2.0;
    double E = config_.porcelain_youngs_modulus_gpa * 1e9;
    double strength_pa = config_.porcelain_strength_mpa * 1e6;

    result.youngs_modulus_gpa = config_.porcelain_youngs_modulus_gpa;

    {
        double max_moment_orig = strength_pa * I / y_max;
        result.original_strength_mpa = config_.porcelain_strength_mpa;

        double a_crack = (crack.max_depth > 0) ? crack.max_depth * 1e-3 : 1e-9;
        double f = crack_influence_factor(a_crack, h);
        double sigma_eff_factor = std::sqrt(M_PI * std::max(a_crack, 1e-9)) * f;
        double unrepaired_strength_pa = strength_pa / std::max(sigma_eff_factor, 1.0);
        result.unrepaired_strength_mpa = unrepaired_strength_pa / 1e6;

        if (repaired) {
            double bonding_strength = 0.0;
            try {
                if (material.properties.contains("bonding_strength")) {
                    bonding_strength = material.properties["bonding_strength"].get<double>();
                }
            } catch (...) {
                bonding_strength = 0.0;
            }

            double crack_depth_ratio = std::min(1.0, a_crack / h);
            double repaired_strength_pa = strength_pa * (1.0 - crack_depth_ratio)
                + bonding_strength * 1e6 * crack_depth_ratio * config_.repair_interface_strength_ratio;
            result.repaired_strength_mpa = repaired_strength_pa / 1e6;
        } else {
            result.repaired_strength_mpa = result.unrepaired_strength_mpa;
        }

        if (result.original_strength_mpa > 0.0) {
            result.strength_recovery_ratio = result.repaired_strength_mpa / result.original_strength_mpa;
        } else {
            result.strength_recovery_ratio = 0.0;
        }

        double K_IC = strength_pa * std::sqrt(M_PI * std::max(a_crack, 1e-9)) * f;
        result.fracture_toughness_mpa_m05 = K_IC / 1e6;
    }

    std::vector<BendingElement> mesh;
    build_mesh(mesh, crack, material, repaired);
    run_load_steps(mesh, result);

    json stress_dist = json::array();
    for (const auto& elem : mesh) {
        json elem_json = {
            {"x_center", elem.x_center},
            {"length", elem.length},
            {"y_min", elem.y_min},
            {"y_max", elem.y_max},
            {"stress_top", elem.stress[0] / 1e6},
            {"stress_bottom", elem.stress[1] / 1e6},
            {"strain_top", elem.strain[0]},
            {"strain_bottom", elem.strain[1]},
            {"damage", elem.damage},
            {"has_crack", elem.has_crack},
            {"is_repaired", elem.is_repaired}
        };
        stress_dist.push_back(elem_json);
    }
    result.stress_distribution = stress_dist;

    result.parameters = {
        {"support_span_mm", config_.support_span_mm},
        {"loading_span_mm", config_.loading_span_mm},
        {"specimen_width_mm", config_.specimen_width_mm},
        {"specimen_thickness_mm", config_.specimen_thickness_mm},
        {"porcelain_youngs_modulus_gpa", config_.porcelain_youngs_modulus_gpa},
        {"porcelain_poissons_ratio", config_.porcelain_poissons_ratio},
        {"porcelain_strength_mpa", config_.porcelain_strength_mpa},
        {"repair_interface_strength_ratio", config_.repair_interface_strength_ratio},
        {"mesh_elements", config_.mesh_elements},
        {"load_steps", config_.load_steps},
        {"failure_load_n", failure_load_n_}
    };

    json summary = {
        {"original_strength_mpa", result.original_strength_mpa},
        {"unrepaired_strength_mpa", result.unrepaired_strength_mpa},
        {"repaired_strength_mpa", result.repaired_strength_mpa},
        {"strength_recovery_ratio", result.strength_recovery_ratio},
        {"youngs_modulus_gpa", result.youngs_modulus_gpa},
        {"fracture_toughness_mpa_m05", result.fracture_toughness_mpa_m05},
        {"failure_load_n", failure_load_n_},
        {"load_displacement_points", static_cast<int>(result.load_displacement_load.size())}
    };
    result.result = summary;

    return result;
}

json FourPointBendingTest::result_to_json(const BendingTestResult& result) const {
    json j;
    j["id"] = result.id;
    j["porcelain_id"] = result.porcelain_id;
    j["crack_id"] = result.crack_id;
    j["material_id"] = result.material_id;
    j["method"] = result.method;
    j["parameters"] = result.parameters;
    j["original_strength_mpa"] = result.original_strength_mpa;
    j["unrepaired_strength_mpa"] = result.unrepaired_strength_mpa;
    j["repaired_strength_mpa"] = result.repaired_strength_mpa;
    j["strength_recovery_ratio"] = result.strength_recovery_ratio;
    j["youngs_modulus_gpa"] = result.youngs_modulus_gpa;
    j["fracture_toughness_mpa_m05"] = result.fracture_toughness_mpa_m05;
    j["load_displacement_load"] = result.load_displacement_load;
    j["load_displacement_disp"] = result.load_displacement_disp;
    j["stress_distribution"] = result.stress_distribution;
    j["result"] = result.result;
    return j;
}

}
}
