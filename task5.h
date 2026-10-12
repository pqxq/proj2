#pragma once

#include <vector>
#include <array>
#include <string>
#include <random>
#include <cstdint>
#include <stdexcept>
#include <format>
#include <functional>
#include <thread>
#include <execution>

namespace Config
{
    constexpr int MeasurementIterations = 5;
    constexpr std::array<size_t, 11> KValues = {1, 2, 4, 8, 12, 16, 24, 32, 64, 128, 256};
}

struct BenchmarkResult
{
    std::string name;
    double time_ms = 0.0;
    int found_min = 0;
};

struct CustomKResult
{
    size_t k = 0;
    double time_ms = 0.0;
    double speedup = 1.0;
};

void generate_and_save_data(const std::string &filename, size_t size);
std::vector<int> load_data_from_file(const std::string &filename);

int custom_parallel_min(const std::vector<int> &data, size_t k, std::function<bool(int, int)> comp = std::less<int>{});

std::vector<BenchmarkResult> run_standard_benchmarks(const std::vector<int> &data);
std::vector<BenchmarkResult> run_predicate_benchmarks(const std::vector<int> &data);
std::vector<CustomKResult> run_custom_k_benchmarks(const std::vector<int> &data);

void run_task5();