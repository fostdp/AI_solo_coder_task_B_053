#pragma once

#include <vector>
#include <string>
#include <array>
#include <nlohmann/json.hpp>
#include "common.h"

namespace porcelain_monitor {
namespace algorithms {

using json = nlohmann::json;

struct BendingTestConfig {
    double support_span_mm = 40.0;
    double loading_span_mm = 20.0;
    double specimen_width_mm = 10.0;
    double specimen_thickness_mm = 5.0;
    double porcelain_youngs_modulus_gpa = 70.0;
    double porcelain_poissons_ratio = 0.22;
    double porcelain_strength_mpa = 120.0;
    double repair_interface_strength_ratio = 0.85;
    int mesh_elements = 100;
    int load_steps = 50;
    double max_displacement_mm = 2.0;
};

struct BendingElement {
    double x_center;
    double length;
    double y_min, y_max;
    double stress[6];
    double strain[6];
    double damage = 0.0;
    bool has_crack = false;
    bool is_repaired = false;
    double repair_bonding_strength = 0.0;
};

class FourPointBendingTest {
public:
    FourPointBendingTest();

    void set_config(const BendingTestConfig& config);

    BendingTestResult simulate(int porcelain_id, int crack_id, int material_id,
                                const CrackInfo& crack,
                                const RepairMaterial& material,
                                bool repaired = true);

    double compute_bending_strength(const std::vector<BendingElement>& mesh,
                                     double bending_moment) const;

    double crack_influence_factor(double crack_depth, double specimen_thickness) const;

    json result_to_json(const BendingTestResult& result) const;

private:
    void build_mesh(std::vector<BendingElement>& mesh, const CrackInfo& crack,
                    const RepairMaterial& material, bool repaired);
    void apply_four_point_loads(std::vector<BendingElement>& mesh, double load_n);
    void compute_bending_moment_distribution(std::vector<double>& moments,
                                              double load_n) const;
    void update_damage(std::vector<BendingElement>& mesh, double load_n);
    void run_load_steps(std::vector<BendingElement>& mesh,
                         BendingTestResult& result);
    double interpolate_stress(double y, double y_min, double y_max,
                               double moment, double inertia) const;
    double moment_of_inertia() const;

    BendingTestConfig config_;
    double failure_load_n_;
};

}
}
