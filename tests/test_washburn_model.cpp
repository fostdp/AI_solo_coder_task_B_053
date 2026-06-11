#include "test_framework.h"
#include "washburn_model.h"
#include <cmath>
#include <chrono>
#include <limits>

using namespace porcelain_monitor;
using namespace porcelain_monitor::algorithms;

namespace {

constexpr double DEFAULT_GAMMA = 0.072;
constexpr double DEFAULT_THETA_DEG = 30.0;
constexpr double DEFAULT_ETA = 1.0;
constexpr double DEFAULT_CRACK_WIDTH_UM = 10.0;
constexpr double DEFAULT_TORTUOSITY = 1.5;
constexpr double DEFAULT_ROUGHNESS = 1.2;

double computeEffectiveRadius(double crack_width_um) {
    double crack_width_m = crack_width_um * 1e-6;
    double r_parallel_plate = crack_width_m / 4.0;
    return r_parallel_plate / DEFAULT_ROUGHNESS;
}

double computeAnalyticalDepth(double gamma, double theta_deg, double crack_width_um,
                              double eta, double tau, double t) {
    if (t <= 0.0) return 0.0;
    double r = computeEffectiveRadius(crack_width_um);
    double theta_rad = theta_deg * M_PI / 180.0;
    double cos_theta = std::cos(theta_rad);
    double numerator = gamma * cos_theta * r * t;
    double denominator = 2.0 * eta * tau;
    if (denominator < 1e-20 || numerator <= 0.0) return 0.0;
    double h_m = std::sqrt(numerator / denominator);
    return h_m * 1e6;
}

CrackInfo createTestCrack(double max_width_m) {
    CrackInfo crack;
    crack.id = 1;
    crack.porcelain_id = 1;
    crack.crack_code = "TEST_CRACK";
    crack.max_depth = 0.001;
    crack.max_width = max_width_m;
    crack.total_length = 0.01;
    crack.status = "active";
    crack.detected_at = std::chrono::system_clock::now();
    return crack;
}

RepairMaterial createTestMaterial(double viscosity, double surface_tension, double contact_angle) {
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
        {"surface_tension_n_m", surface_tension},
        {"contact_angle_deg", contact_angle}
    };
    return mat;
}

}

TEST(Washburn, StandardParams_DepthMatchesAnalytical) {
    WashburnPenetrationModel model;
    WashburnConfig cfg;
    cfg.default_surface_tension_n_m = DEFAULT_GAMMA;
    cfg.default_contact_angle_deg = DEFAULT_THETA_DEG;
    cfg.default_viscosity_pa_s = DEFAULT_ETA;
    cfg.tortuosity_factor = DEFAULT_TORTUOSITY;
    cfg.surface_roughness_factor = DEFAULT_ROUGHNESS;
    model.set_config(cfg);

    double t = 100.0;
    double h_pred = model.penetration_depth_at_time(
        t, DEFAULT_CRACK_WIDTH_UM, DEFAULT_ETA, DEFAULT_GAMMA, DEFAULT_THETA_DEG
    );
    double h_analytical = computeAnalyticalDepth(
        DEFAULT_GAMMA, DEFAULT_THETA_DEG, DEFAULT_CRACK_WIDTH_UM,
        DEFAULT_ETA, DEFAULT_TORTUOSITY, t
    );

    ASSERT_GT(h_pred, 0.0);
    ASSERT_NEAR(h_pred, h_analytical, 5.0);
}

TEST(Washburn, TimeReachDepth_MatchesDepthFormula) {
    WashburnPenetrationModel model;
    WashburnConfig cfg;
    cfg.tortuosity_factor = DEFAULT_TORTUOSITY;
    cfg.surface_roughness_factor = DEFAULT_ROUGHNESS;
    model.set_config(cfg);

    double t_input = 100.0;
    double h = model.penetration_depth_at_time(
        t_input, DEFAULT_CRACK_WIDTH_UM, DEFAULT_ETA, DEFAULT_GAMMA, DEFAULT_THETA_DEG
    );
    ASSERT_GT(h, 0.0);

    double t_recovered = model.time_to_reach_depth(
        h, DEFAULT_CRACK_WIDTH_UM, DEFAULT_ETA, DEFAULT_GAMMA, DEFAULT_THETA_DEG
    );

    ASSERT_GT(t_recovered, 0.0);
    ASSERT_NEAR(t_recovered, t_input, 5.0);
}

TEST(Washburn, PenetrationRate_DecreasesWithDepth) {
    WashburnPenetrationModel model;
    WashburnConfig cfg;
    cfg.tortuosity_factor = DEFAULT_TORTUOSITY;
    cfg.surface_roughness_factor = DEFAULT_ROUGHNESS;
    model.set_config(cfg);

    double rate_shallow = model.penetration_rate(
        50.0, DEFAULT_CRACK_WIDTH_UM, DEFAULT_ETA, DEFAULT_GAMMA, DEFAULT_THETA_DEG
    );
    double rate_deep = model.penetration_rate(
        200.0, DEFAULT_CRACK_WIDTH_UM, DEFAULT_ETA, DEFAULT_GAMMA, DEFAULT_THETA_DEG
    );

    ASSERT_GT(rate_shallow, 0.0);
    ASSERT_GT(rate_deep, 0.0);
    ASSERT_GT(rate_shallow, rate_deep);
}

TEST(Washburn, DifferentViscosities_LowerViscosityFaster) {
    WashburnPenetrationModel model;
    WashburnConfig cfg;
    cfg.tortuosity_factor = DEFAULT_TORTUOSITY;
    cfg.surface_roughness_factor = DEFAULT_ROUGHNESS;
    model.set_config(cfg);

    double t = 100.0;
    double h_low_eta = model.penetration_depth_at_time(
        t, DEFAULT_CRACK_WIDTH_UM, 0.5, DEFAULT_GAMMA, DEFAULT_THETA_DEG
    );
    double h_high_eta = model.penetration_depth_at_time(
        t, DEFAULT_CRACK_WIDTH_UM, 2.0, DEFAULT_GAMMA, DEFAULT_THETA_DEG
    );

    ASSERT_GT(h_low_eta, 0.0);
    ASSERT_GT(h_high_eta, 0.0);
    ASSERT_GT(h_low_eta, h_high_eta);

    double ratio = h_low_eta / h_high_eta;
    ASSERT_RANGE(ratio, 1.7, 2.3);
}

TEST(Washburn, DifferentContactAngles_SmallerAngleBetter) {
    WashburnPenetrationModel model;
    WashburnConfig cfg;
    cfg.tortuosity_factor = DEFAULT_TORTUOSITY;
    cfg.surface_roughness_factor = DEFAULT_ROUGHNESS;
    model.set_config(cfg);

    double t = 100.0;
    double h_10deg = model.penetration_depth_at_time(
        t, DEFAULT_CRACK_WIDTH_UM, DEFAULT_ETA, DEFAULT_GAMMA, 10.0
    );
    double h_60deg = model.penetration_depth_at_time(
        t, DEFAULT_CRACK_WIDTH_UM, DEFAULT_ETA, DEFAULT_GAMMA, 60.0
    );
    double h_90deg = model.penetration_depth_at_time(
        t, DEFAULT_CRACK_WIDTH_UM, DEFAULT_ETA, DEFAULT_GAMMA, 90.0
    );

    ASSERT_GT(h_10deg, 0.0);
    ASSERT_GT(h_60deg, 0.0);
    ASSERT_GT(h_10deg, h_60deg);
    ASSERT_LT(h_90deg, 0.01);
}

TEST(Washburn, FullPredict_ValidOutputFields) {
    WashburnPenetrationModel model;
    WashburnConfig cfg;
    cfg.time_series_points = 100;
    model.set_config(cfg);

    CrackInfo crack = createTestCrack(50e-6);
    RepairMaterial mat = createTestMaterial(DEFAULT_ETA, DEFAULT_GAMMA, DEFAULT_THETA_DEG);

    auto result = model.predict(1, 1, crack, mat, 200.0);

    ASSERT_GT(result.predicted_time_s, 0.0);
    ASSERT_GT(result.penetration_rate_um_s, 0.0);
    ASSERT_EQ(result.time_series.size(), static_cast<size_t>(101));
    ASSERT_EQ(result.depth_series.size(), static_cast<size_t>(101));

    for (size_t i = 1; i < result.time_series.size(); ++i) {
        ASSERT_GT(result.time_series[i], result.time_series[i - 1]);
    }
    for (size_t i = 1; i < result.depth_series.size(); ++i) {
        ASSERT_GE(result.depth_series[i], result.depth_series[i - 1]);
    }
}

TEST(Washburn, ZeroTime_DepthIsZero) {
    WashburnPenetrationModel model;
    double h = model.penetration_depth_at_time(
        0.0, DEFAULT_CRACK_WIDTH_UM, DEFAULT_ETA, DEFAULT_GAMMA, DEFAULT_THETA_DEG
    );
    ASSERT_NEAR(h, 0.0, 1e-10);
}

TEST(Washburn, ZeroViscosity_InvalidButNoCrash) {
    WashburnPenetrationModel model;
    double h = model.penetration_depth_at_time(
        100.0, DEFAULT_CRACK_WIDTH_UM, 0.0, DEFAULT_GAMMA, DEFAULT_THETA_DEG
    );
    ASSERT_FALSE(std::isnan(h));
    ASSERT_FALSE(std::isinf(h));
    ASSERT_GE(h, 0.0);
}

TEST(Washburn, 90DegreeContact_NoPenetration) {
    WashburnPenetrationModel model;
    double h1 = model.penetration_depth_at_time(
        10.0, DEFAULT_CRACK_WIDTH_UM, DEFAULT_ETA, DEFAULT_GAMMA, 90.0
    );
    double h2 = model.penetration_depth_at_time(
        1000.0, DEFAULT_CRACK_WIDTH_UM, DEFAULT_ETA, DEFAULT_GAMMA, 90.0
    );
    ASSERT_LT(h1, 0.01);
    ASSERT_LT(h2, 0.01);
}

TEST(Washburn, VeryThinCrack_SlowerPenetration) {
    WashburnPenetrationModel model;
    WashburnConfig cfg;
    cfg.tortuosity_factor = DEFAULT_TORTUOSITY;
    cfg.surface_roughness_factor = DEFAULT_ROUGHNESS;
    model.set_config(cfg);

    double t = 100.0;
    double h_thin = model.penetration_depth_at_time(
        t, 0.5, DEFAULT_ETA, DEFAULT_GAMMA, DEFAULT_THETA_DEG
    );
    double h_wide = model.penetration_depth_at_time(
        t, 50.0, DEFAULT_ETA, DEFAULT_GAMMA, DEFAULT_THETA_DEG
    );

    ASSERT_GT(h_thin, 0.0);
    ASSERT_GT(h_wide, 0.0);
    ASSERT_LT(h_thin, h_wide);
}

TEST(Washburn, LargeTime_DepthSaturationByGravity) {
    WashburnPenetrationModel model;
    WashburnConfig cfg;
    cfg.tortuosity_factor = DEFAULT_TORTUOSITY;
    cfg.surface_roughness_factor = DEFAULT_ROUGHNESS;
    model.set_config(cfg);

    double t1 = 1e5;
    double t2 = 1e7;
    double h1 = model.penetration_depth_at_time(
        t1, DEFAULT_CRACK_WIDTH_UM, DEFAULT_ETA, DEFAULT_GAMMA, DEFAULT_THETA_DEG
    );
    double h2 = model.penetration_depth_at_time(
        t2, DEFAULT_CRACK_WIDTH_UM, DEFAULT_ETA, DEFAULT_GAMMA, DEFAULT_THETA_DEG
    );

    ASSERT_GT(h1, 0.0);
    ASSERT_GT(h2, 0.0);

    double h_ideal_t1 = computeAnalyticalDepth(
        DEFAULT_GAMMA, DEFAULT_THETA_DEG, DEFAULT_CRACK_WIDTH_UM,
        DEFAULT_ETA, DEFAULT_TORTUOSITY, t1
    );
    double ratio_actual = h2 / h1;
    double ratio_ideal = std::sqrt(static_cast<double>(t2) / t1);

    ASSERT_LT(ratio_actual, ratio_ideal * 0.5);
    ASSERT_LT(h2, h_ideal_t1 * 2.0);
}

TEST(Washburn, NegativeTime_HandlesGracefully) {
    WashburnPenetrationModel model;
    double h = model.penetration_depth_at_time(
        -10.0, DEFAULT_CRACK_WIDTH_UM, DEFAULT_ETA, DEFAULT_GAMMA, DEFAULT_THETA_DEG
    );
    ASSERT_FALSE(std::isnan(h));
    ASSERT_FALSE(std::isinf(h));
    ASSERT_NEAR(h, 0.0, 1e-10);
}

TEST(Washburn, NegativeViscosity_NoCrash) {
    WashburnPenetrationModel model;
    double h = model.penetration_depth_at_time(
        100.0, DEFAULT_CRACK_WIDTH_UM, -1.0, DEFAULT_GAMMA, DEFAULT_THETA_DEG
    );
    ASSERT_FALSE(std::isnan(h));
    ASSERT_FALSE(std::isinf(h));
    ASSERT_GE(h, 0.0);

    double t = model.time_to_reach_depth(
        100.0, DEFAULT_CRACK_WIDTH_UM, -1.0, DEFAULT_GAMMA, DEFAULT_THETA_DEG
    );
    ASSERT_FALSE(std::isnan(t));
    ASSERT_FALSE(std::isinf(t));

    double rate = model.penetration_rate(
        50.0, DEFAULT_CRACK_WIDTH_UM, -1.0, DEFAULT_GAMMA, DEFAULT_THETA_DEG
    );
    ASSERT_FALSE(std::isnan(rate));
    ASSERT_FALSE(std::isinf(rate));
}

TEST(Washburn, 180DegreeContact_Repulsive) {
    WashburnPenetrationModel model;
    double h = model.penetration_depth_at_time(
        100.0, DEFAULT_CRACK_WIDTH_UM, DEFAULT_ETA, DEFAULT_GAMMA, 180.0
    );
    ASSERT_FALSE(std::isnan(h));
    ASSERT_FALSE(std::isinf(h));
    ASSERT_LT(h, 0.01);

    double h2 = model.penetration_depth_at_time(
        100.0, DEFAULT_CRACK_WIDTH_UM, DEFAULT_ETA, DEFAULT_GAMMA, 120.0
    );
    ASSERT_FALSE(std::isnan(h2));
    ASSERT_FALSE(std::isinf(h2));
    ASSERT_LT(h2, 0.01);
}

TEST(Washburn, Acceptance_PredictionErrorLessThan15pct) {
    WashburnPenetrationModel model;
    WashburnConfig cfg;
    cfg.default_surface_tension_n_m = 0.072;
    cfg.default_contact_angle_deg = 30.0;
    cfg.default_viscosity_pa_s = 1.0;
    cfg.tortuosity_factor = DEFAULT_TORTUOSITY;
    cfg.surface_roughness_factor = DEFAULT_ROUGHNESS;
    model.set_config(cfg);

    double t = 100.0;
    double crack_width_um = 0.5;
    double eta_pa_s = 1.0;
    double gamma_n_m = 0.072;
    double theta_deg = 30.0;

    double h_pred = model.penetration_depth_at_time(
        t, crack_width_um, eta_pa_s, gamma_n_m, theta_deg
    );

    double expected_um = 470.0;
    ASSERT_GT(h_pred, 0.0);
    ASSERT_NEAR(h_pred, expected_um, 15.0);
}

int main() {
    return RUN_ALL_TESTS();
}
