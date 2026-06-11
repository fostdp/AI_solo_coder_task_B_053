#pragma once

#include <string>
#include <cstdint>
#include <cstdlib>

namespace porcelain_monitor {
namespace config {

struct DatabaseConfig {
    std::string host = "localhost";
    uint16_t port = 5432;
    std::string name = "porcelain_monitor";
    std::string user = "postgres";
    std::string password = "postgres";
    int pool_size = 10;
    int max_pool_size = 50;
    int connection_timeout_ms = 5000;

    void apply_env() {
        const char* env = std::getenv("PM_DB_HOST");
        if (env) host = env;
        env = std::getenv("PM_DB_PORT");
        if (env) port = static_cast<uint16_t>(std::atoi(env));
        env = std::getenv("PM_DB_NAME");
        if (env) name = env;
        env = std::getenv("PM_DB_USER");
        if (env) user = env;
        env = std::getenv("PM_DB_PASSWORD");
        if (env) password = env;
        env = std::getenv("PM_DB_POOL_SIZE");
        if (env) {
            pool_size = std::atoi(env);
            pool_size = std::max(1, std::min(pool_size, max_pool_size));
        }
    }
};

struct ServerConfig {
    uint16_t profinet_port = 34964;
    uint16_t http_port = 8080;
    uint16_t websocket_port = 8081;
    std::string bind_address = "0.0.0.0";
    int thread_pool_size = 8;

    void apply_env() {
        const char* env = std::getenv("PM_PROFINET_PORT");
        if (env) profinet_port = static_cast<uint16_t>(std::atoi(env));
        env = std::getenv("PM_HTTP_PORT");
        if (env) http_port = static_cast<uint16_t>(std::atoi(env));
        env = std::getenv("PM_WS_PORT");
        if (env) websocket_port = static_cast<uint16_t>(std::atoi(env));
        env = std::getenv("PM_THREAD_POOL");
        if (env) thread_pool_size = std::max(1, std::atoi(env));
    }
};

struct AlertConfig {
    double depth_threshold = 200.0;
    double width_threshold = 50.0;
    bool sms_enabled = true;
    bool websocket_enabled = true;
    std::string sms_gateway_url = "http://sms-gateway.example.com/send";
    std::string alert_phone_number = "+8613800138000";

    void apply_env() {
        const char* env = std::getenv("PM_DEPTH_THRESHOLD");
        if (env) depth_threshold = std::atof(env);
        env = std::getenv("PM_WIDTH_THRESHOLD");
        if (env) width_threshold = std::atof(env);
        env = std::getenv("PM_SMS_ENABLED");
        if (env) sms_enabled = (std::string(env) == "1" || std::string(env) == "true");
    }
};

struct AlgorithmConfig {
    struct ParisLaw {
        double default_C = 1.5e-10;
        double default_m = 3.0;
        double stress_ratio = 0.1;
        int prediction_horizon_hours = 720;
    } paris_law;

    struct DEM {
        double particle_radius_nm = 25.0;
        double youngs_modulus = 70e9;
        double poissons_ratio = 0.22;
        double density = 3950.0;
        int max_particles = 10000;
        int simulation_steps = 1000;
        double time_step = 1e-9;
    } dem;
};

struct Config {
    DatabaseConfig database;
    ServerConfig server;
    AlertConfig alerts;
    AlgorithmConfig algorithms;

    void apply_env() {
        database.apply_env();
        server.apply_env();
        alerts.apply_env();
    }
};

inline Config& get_config() {
    static Config config;
    return config;
}

}
}
