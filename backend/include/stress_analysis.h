#pragma once

#include <vector>
#include <string>
#include <array>
#include <nlohmann/json.hpp>
#include "common.h"

namespace porcelain_monitor {
namespace algorithms {

using json = nlohmann::json;

struct FEMConfig {
    int grid_resolution = 50;
    double youngs_modulus_gpa = 70.0;
    double poissons_ratio = 0.22;
    double stress_relaxation_time_h = 1000.0;
    double crack_density_sensitivity = 50.0;
    double directional_coupling = 0.7;
    double max_stress_mpa = 300.0;
};

struct FEMNode {
    double x, y, z;
    double displacement[3] = {0, 0, 0};
    double strain[6] = {0, 0, 0, 0, 0, 0};
    StressTensor stress;
    double crack_density = 0.0;
    double crack_direction[3] = {1, 0, 0};
    bool is_boundary = false;
};

struct FEAMesh {
    std::vector<FEMNode> nodes;
    int nx, ny, nz;
    double dx, dy, dz;
    double bbox_min[3];
    double bbox_max[3];
};

class StressAnalysisFEM {
public:
    StressAnalysisFEM();

    void set_config(const FEMConfig& config);

    StressAnalysisResult analyze(int porcelain_id,
                                  const std::vector<CrackInfo>& cracks);

    json result_to_json(const StressAnalysisResult& result) const;

private:
    void build_mesh(FEAMesh& mesh, const std::vector<CrackInfo>& cracks);
    void compute_crack_density_field(FEAMesh& mesh, const std::vector<CrackInfo>& cracks);
    void solve_elasticity(FEAMesh& mesh);
    void apply_crack_stress_coupling(FEAMesh& mesh);
    void smooth_stress_field(FEAMesh& mesh);
    void compute_von_mises(FEAMesh& mesh);
    void assemble_result(const FEAMesh& mesh, StressAnalysisResult& result, int porcelain_id);
    void principal_stress_direction(FEMNode& node);

    double gaussian_weight(double dist, double sigma) const;
    double hookes_law_3d(double strain_xx, double strain_yy, double strain_zz,
                         double strain_xy, double strain_yz, double strain_zx,
                         int component) const;

    FEMConfig config_;
    double lambda_;
    double mu_;
};

}
}
