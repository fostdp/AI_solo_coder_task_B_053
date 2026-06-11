#include "test_framework.h"
#include "four_point_bending.h"
#include <cmath>
#include <chrono>
#include <limits>
#include <vector>

using namespace porcelain_monitor;
using namespace porcelain_monitor::algorithms;

namespace {

CrackInfo createTestCrack(double depth_mm, double width_um) {
    CrackInfo crack;
    crack.id = 1;
    crack.porcelain_id = 1;
    crack.crack_code = "TEST_CRACK";
    crack.max_depth = depth_mm;
    crack.max_width = width_um * 1e-3;
    crack.total_length = 10.0;
    crack.status = "active";
    crack.detected_at = std::chrono::system_clock::now();

    Point3D p1, p2;
    p1.x = 15.0; p1.y = 0.0; p1.z = 0.0;
    p1.depth = depth_mm; p1.width = width_um * 1e-3;
    p2.x = 25.0; p2.y = 0.0; p2.z = 0.0;
    p2.depth = depth_mm; p2.width = width_um * 1e-3;
    crack.points.push_back(p1);
    crack.points.push_back(p2);

    return crack;
}

RepairMaterial createTestMaterial(double bonding_strength, double viscosity, double youngs_gpa) {
    RepairMaterial mat;
    mat.id = 1;
    mat.type = RepairMaterialType::SILICA;
    mat.name = "TestMaterial";
    mat.manufacturer = "Test";
    mat.particle_size_nm = 100.0;
    mat.purity = 0.99;
    mat.viscosity = viscosity;
    mat.refractive_index = 1.5;
    mat.thermal_expansion_coeff = 1e-6;
    mat.properties = {
        {"bonding_strength", bonding_strength},
        {"youngs_modulus_gpa", youngs_gpa}
    };
    return mat;
}

bool isMonotonicIncreasing(const std::vector<double>& v) {
    if (v.size() < 2) return true;
    for (size_t i = 1; i < v.size(); ++i) {
        if (v[i] <= v[i - 1]) return false;
    }
    return true;
}

}

TEST(FourPointBending, Bending_NoCrack_StrengthMatchesMaterial) {
    FourPointBendingTest test;
    BendingTestConfig config;
    config.porcelain_strength_mpa = 120.0;
    test.set_config(config);

    CrackInfo no_crack = createTestCrack(0.0, 0.0);
    RepairMaterial dummy_mat = createTestMaterial(0.0, 1.0, 70.0);

    auto result = test.simulate(1, 0, 0, no_crack, dummy_mat, false);

    ASSERT_RANGE(result.original_strength_mpa, 80.0, 150.0);
    ASSERT_NEAR(result.original_strength_mpa, 120.0, 1.0);
}

TEST(FourPointBending, Bending_WithCrack_StrengthReduced) {
    FourPointBendingTest test;
    BendingTestConfig config;
    config.specimen_thickness_mm = 5.0;
    config.porcelain_strength_mpa = 120.0;
    test.set_config(config);

    CrackInfo crack = createTestCrack(2.0, 50.0);
    RepairMaterial dummy_mat = createTestMaterial(0.0, 1.0, 70.0);

    auto result = test.simulate(1, 1, 0, crack, dummy_mat, false);

    ASSERT_LT(result.unrepaired_strength_mpa, result.original_strength_mpa);

    double ratio = result.unrepaired_strength_mpa / result.original_strength_mpa;
    ASSERT_RANGE(ratio, 0.3, 0.8);
}

TEST(FourPointBending, Bending_Repaired_HighPermeability_RecoveryAbove80pct) {
    FourPointBendingTest test;
    BendingTestConfig config;
    config.specimen_thickness_mm = 5.0;
    config.porcelain_strength_mpa = 120.0;
    config.repair_interface_strength_ratio = 0.85;
    test.set_config(config);

    CrackInfo crack = createTestCrack(2.0, 50.0);
    RepairMaterial high_perm_mat = createTestMaterial(80.0, 0.05, 70.0);

    auto result = test.simulate(1, 1, 1, crack, high_perm_mat, true);

    ASSERT_GE(result.strength_recovery_ratio, 0.80);
}

TEST(FourPointBending, Bending_LoadDisplacement_MonotonicallyIncreasing) {
    FourPointBendingTest test;
    BendingTestConfig config;
    config.load_steps = 50;
    config.max_displacement_mm = 2.0;
    test.set_config(config);

    CrackInfo crack = createTestCrack(1.0, 50.0);
    RepairMaterial mat = createTestMaterial(50.0, 1.0, 70.0);

    auto result = test.simulate(1, 1, 1, crack, mat, true);

    ASSERT_FALSE(result.load_displacement_load.empty());
    ASSERT_FALSE(result.load_displacement_disp.empty());

    ASSERT_TRUE(isMonotonicIncreasing(result.load_displacement_load));
    ASSERT_TRUE(isMonotonicIncreasing(result.load_displacement_disp));
}

TEST(FourPointBending, Bending_DifferentCrackDepths_DeeperCrackLowerStrength) {
    FourPointBendingTest test1, test2;
    BendingTestConfig config;
    config.specimen_thickness_mm = 5.0;
    config.porcelain_strength_mpa = 120.0;
    test1.set_config(config);
    test2.set_config(config);

    CrackInfo shallow_crack = createTestCrack(1.0, 50.0);
    CrackInfo deep_crack = createTestCrack(3.0, 50.0);
    RepairMaterial dummy_mat = createTestMaterial(0.0, 1.0, 70.0);

    auto result_shallow = test1.simulate(1, 1, 0, shallow_crack, dummy_mat, false);
    auto result_deep = test2.simulate(1, 2, 0, deep_crack, dummy_mat, false);

    ASSERT_LT(result_deep.unrepaired_strength_mpa, result_shallow.unrepaired_strength_mpa);
}

TEST(FourPointBending, Bending_MultipleMaterials_BetterMaterialHigherRecovery) {
    FourPointBendingTest testA, testB;
    BendingTestConfig config;
    config.specimen_thickness_mm = 5.0;
    config.porcelain_strength_mpa = 120.0;
    testA.set_config(config);
    testB.set_config(config);

    CrackInfo crack = createTestCrack(2.0, 50.0);
    RepairMaterial matA = createTestMaterial(80.0, 0.1, 70.0);
    RepairMaterial matB = createTestMaterial(30.0, 1.0, 70.0);

    auto resultA = testA.simulate(1, 1, 1, crack, matA, true);
    auto resultB = testB.simulate(1, 1, 2, crack, matB, true);

    ASSERT_GT(resultA.strength_recovery_ratio, resultB.strength_recovery_ratio);
}

TEST(FourPointBending, Bending_MinimalCrack_AlmostFullRecovery) {
    FourPointBendingTest test;
    BendingTestConfig config;
    config.specimen_thickness_mm = 5.0;
    config.porcelain_strength_mpa = 120.0;
    test.set_config(config);

    CrackInfo tiny_crack = createTestCrack(0.01, 10.0);
    RepairMaterial good_mat = createTestMaterial(80.0, 0.1, 70.0);

    auto result = test.simulate(1, 1, 1, tiny_crack, good_mat, true);

    ASSERT_GT(result.strength_recovery_ratio, 0.95);
}

TEST(FourPointBending, Bending_CrackThroughThickness_VeryLowStrength) {
    FourPointBendingTest test;
    BendingTestConfig config;
    config.specimen_thickness_mm = 5.0;
    config.porcelain_strength_mpa = 120.0;
    test.set_config(config);

    CrackInfo through_crack = createTestCrack(4.9, 100.0);
    RepairMaterial dummy_mat = createTestMaterial(0.0, 1.0, 70.0);

    auto result = test.simulate(1, 1, 0, through_crack, dummy_mat, false);

    ASSERT_LT(result.unrepaired_strength_mpa, 0.1 * result.original_strength_mpa);
}

TEST(FourPointBending, Bending_MaxDisplacementReached_TerminatesGracefully) {
    FourPointBendingTest test;
    BendingTestConfig config;
    config.max_displacement_mm = 0.5;
    config.load_steps = 100;
    test.set_config(config);

    CrackInfo no_crack = createTestCrack(0.0, 0.0);
    RepairMaterial dummy_mat = createTestMaterial(0.0, 1.0, 70.0);

    auto result = test.simulate(1, 0, 0, no_crack, dummy_mat, false);

    ASSERT_FALSE(result.load_displacement_disp.empty());
    ASSERT_GT(result.load_displacement_disp.size(), 0u);

    double max_disp_reached = result.load_displacement_disp.back();
    ASSERT_LE(max_disp_reached, config.max_displacement_mm * 1.1);
}

TEST(FourPointBending, Bending_ZeroBondingMaterial_NoImprovement) {
    FourPointBendingTest test_unrepaired, test_repaired;
    BendingTestConfig config;
    config.specimen_thickness_mm = 5.0;
    config.porcelain_strength_mpa = 120.0;
    test_unrepaired.set_config(config);
    test_repaired.set_config(config);

    CrackInfo crack = createTestCrack(2.0, 50.0);
    RepairMaterial zero_bond_mat = createTestMaterial(0.0, 1.0, 70.0);

    auto result_unrepaired = test_unrepaired.simulate(1, 1, 0, crack, zero_bond_mat, false);
    auto result_repaired = test_repaired.simulate(1, 1, 1, crack, zero_bond_mat, true);

    double rel_error = 0.0;
    if (result_unrepaired.unrepaired_strength_mpa > 1e-12) {
        rel_error = std::abs(result_repaired.repaired_strength_mpa - result_unrepaired.unrepaired_strength_mpa)
            / result_unrepaired.unrepaired_strength_mpa * 100.0;
    }
    ASSERT_LT(rel_error, 5.0);
}

TEST(FourPointBending, Bending_MomentDistribution_Symmetric) {
    FourPointBendingTest test;
    BendingTestConfig config;
    config.support_span_mm = 40.0;
    config.loading_span_mm = 20.0;
    config.mesh_elements = 100;
    test.set_config(config);

    std::vector<double> moments(config.mesh_elements, 0.0);
    test.compute_bending_moment_distribution(moments, 100.0);

    int n = static_cast<int>(moments.size());
    for (int i = 0; i < n / 2; ++i) {
        int mirror_idx = n - 1 - i;
        ASSERT_NEAR(moments[i], moments[mirror_idx], 1.0);
    }

    double load_left = (config.support_span_mm - config.loading_span_mm) / 2.0;
    double load_right = config.support_span_mm - load_left;
    double dx = config.support_span_mm / (n - 1);

    int first_load_idx = static_cast<int>(load_left / dx) + 1;
    int second_load_idx = static_cast<int>(load_right / dx);
    if (second_load_idx >= n) second_load_idx = n - 1;

    ASSERT_GE(first_load_idx, 0);
    ASSERT_LT(second_load_idx, n);
    if (first_load_idx < second_load_idx) {
        double plateau_moment = moments[first_load_idx];
        for (int i = first_load_idx; i <= second_load_idx; ++i) {
            ASSERT_NEAR(moments[i], plateau_moment, 1.0);
        }
    }
}

TEST(FourPointBending, Bending_NegativeCrackDepth_ClampedToZero) {
    FourPointBendingTest test;
    BendingTestConfig config;
    config.porcelain_strength_mpa = 120.0;
    test.set_config(config);

    CrackInfo neg_crack = createTestCrack(-1.0, 50.0);
    RepairMaterial dummy_mat = createTestMaterial(0.0, 1.0, 70.0);

    auto result = test.simulate(1, 1, 0, neg_crack, dummy_mat, false);

    ASSERT_FALSE(std::isnan(result.original_strength_mpa));
    ASSERT_FALSE(std::isinf(result.original_strength_mpa));
    ASSERT_GE(result.unrepaired_strength_mpa, 0.0);
    ASSERT_NEAR(result.unrepaired_strength_mpa, result.original_strength_mpa, 5.0);
}

TEST(FourPointBending, Bending_ThicknessZero_HandlesGracefully) {
    FourPointBendingTest test;
    BendingTestConfig config;
    config.specimen_thickness_mm = 0.0;
    test.set_config(config);

    CrackInfo crack = createTestCrack(1.0, 50.0);
    RepairMaterial dummy_mat = createTestMaterial(0.0, 1.0, 70.0);

    auto result = test.simulate(1, 1, 0, crack, dummy_mat, false);

    ASSERT_FALSE(std::isnan(result.original_strength_mpa));
    ASSERT_FALSE(std::isinf(result.original_strength_mpa));
    ASSERT_FALSE(std::isnan(result.unrepaired_strength_mpa));
    ASSERT_FALSE(std::isinf(result.unrepaired_strength_mpa));
}

TEST(FourPointBending, Bending_SpanGreaterThanSpecimen_Warns) {
    FourPointBendingTest test;
    BendingTestConfig config;
    config.support_span_mm = 100.0;
    config.loading_span_mm = 50.0;
    test.set_config(config);

    CrackInfo crack = createTestCrack(1.0, 50.0);
    crack.total_length = 50.0;
    RepairMaterial dummy_mat = createTestMaterial(0.0, 1.0, 70.0);

    auto result = test.simulate(1, 1, 0, crack, dummy_mat, false);

    ASSERT_FALSE(std::isnan(result.original_strength_mpa));
    ASSERT_FALSE(std::isinf(result.original_strength_mpa));
    ASSERT_GE(result.unrepaired_strength_mpa, 0.0);
}

TEST(FourPointBending, Bending_Acceptance_HighPermeabilityRecoveryAbove80pct) {
    FourPointBendingTest test;
    BendingTestConfig config;
    config.specimen_thickness_mm = 5.0;
    config.specimen_width_mm = 10.0;
    config.support_span_mm = 40.0;
    config.loading_span_mm = 20.0;
    config.porcelain_strength_mpa = 120.0;
    config.porcelain_youngs_modulus_gpa = 70.0;
    config.repair_interface_strength_ratio = 0.85;
    test.set_config(config);

    CrackInfo realistic_crack = createTestCrack(2.0, 50.0);
    RepairMaterial high_perm_mat = createTestMaterial(65.0, 0.05, 70.0);

    auto result = test.simulate(1, 1, 1, realistic_crack, high_perm_mat, true);

    ASSERT_GE(result.strength_recovery_ratio, 0.80);
    ASSERT_GT(result.repaired_strength_mpa, result.unrepaired_strength_mpa);
}

int main() {
    return RUN_ALL_TESTS();
}
