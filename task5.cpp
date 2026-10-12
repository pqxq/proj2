#include "task5.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <filesystem>
#include <cmath>

using namespace std;

void generate_and_save_data(const string &filename, size_t size)
{
    ofstream out(filename);
    if (!out.is_open())
    {
        throw runtime_error(format("Не вдалося створити файл {}.", filename));
    }

    mt19937_64 rng(0); // Фіксований сид для відтворюваності наборів
    uniform_int_distribution<int> dist(-1'000'000, 1'000'000);

    out << size << "\n";
    for (size_t i = 0; i < size; ++i)
    {
        out << dist(rng) << (i + 1 == size ? "" : " ");
    }
}

vector<int> load_data_from_file(const string &filename)
{
    ifstream in(filename);
    if (!in.is_open())
    {
        throw runtime_error(format("Не вдалося відкрити файл {}.", filename));
    }

    size_t size = 0;
    in >> size;

    vector<int> data(size);
    for (size_t i = 0; i < size; ++i)
    {
        in >> data[i];
    }
    return data;
}

int custom_parallel_min(const vector<int> &data, size_t k, function<bool(int, int)> comp)
{
    const size_t n = data.size();
    if (k <= 1 || n <= 1)
    {
        return *min_element(data.begin(), data.end(), comp);
    }

    k = min(k, n);

    vector<jthread> threads;
    threads.reserve(k);
    vector<int> local_mins(k);

    const size_t base_chunk = n / k;
    const size_t remainder = n % k;

    size_t start_idx = 0;
    for (size_t i = 0; i < k; ++i)
    {
        const size_t current_chunk = base_chunk + (i < remainder ? 1 : 0);
        const size_t end_idx = start_idx + current_chunk;

        threads.emplace_back([&data, &local_mins, i, start_idx, end_idx, comp]() {
            auto first = data.begin() + start_idx;
            auto last = data.begin() + end_idx;
            local_mins[i] = *min_element(first, last, comp);
        });

        start_idx = end_idx;
    }

    for (auto &th : threads)
    {
        if (th.joinable())
        {
            th.join();
        }
    }

    return *min_element(local_mins.begin(), local_mins.end(), comp);
}

template <typename Func>
static double measure_time_ms(Func &&func)
{
    func(); // Прогрів кешу

    double total_ms = 0.0;
    for (int i = 0; i < Config::MeasurementIterations; ++i)
    {
        auto t1 = chrono::steady_clock::now();
        func();
        auto t2 = chrono::steady_clock::now();
        total_ms += chrono::duration<double, milli>(t2 - t1).count();
    }
    return total_ms / Config::MeasurementIterations;
}

vector<BenchmarkResult> run_standard_benchmarks(const vector<int> &data)
{
    vector<BenchmarkResult> results;
    int min_val = 0;

    double t = measure_time_ms([&]() {
        min_val = *min_element(data.begin(), data.end());
    });
    results.push_back({"std::min_element (без політики)", t, min_val});

    t = measure_time_ms([&]() {
        min_val = *min_element(execution::seq, data.begin(), data.end());
    });
    results.push_back({"std::min_element (seq)", t, min_val});

    t = measure_time_ms([&]() {
        min_val = *min_element(execution::par, data.begin(), data.end());
    });
    results.push_back({"std::min_element (par)", t, min_val});

    t = measure_time_ms([&]() {
        min_val = *min_element(execution::par_unseq, data.begin(), data.end());
    });
    results.push_back({"std::min_element (par_unseq)", t, min_val});

    return results;
}

vector<BenchmarkResult> run_predicate_benchmarks(const vector<int> &data)
{
    vector<BenchmarkResult> results;
    int min_val = 0;

    double t_fast = measure_time_ms([&]() {
        min_val = *min_element(execution::par, data.begin(), data.end(), less<int>{});
    });
    results.push_back({"par (Швидкий: a < b)", t_fast, min_val});

    auto heavy_cmp = [](int a, int b) {
        return abs(a) < abs(b);
    };

    double t_heavy = measure_time_ms([&]() {
        min_val = *min_element(execution::par, data.begin(), data.end(), heavy_cmp);
    });
    results.push_back({"par (Важкий: |a| < |b|)", t_heavy, min_val});

    return results;
}

vector<CustomKResult> run_custom_k_benchmarks(const vector<int> &data)
{
    vector<CustomKResult> results;
    double t1 = 0.0;

    for (size_t i = 0; i < Config::KValues.size(); ++i)
    {
        size_t k = Config::KValues[i];
        double t = measure_time_ms([&]() {
            custom_parallel_min(data, k);
        });

        if (i == 0)
        {
            t1 = t;
        }

        results.push_back({k, t, t1 / t});
    }
    return results;
}

void run_task5()
{
    const unsigned int hw_threads = thread::hardware_concurrency();

    cout << string(70, '=') << '\n';
    cout << "  ДОСЛІДЖЕННЯ ПАРАЛЕЛЬНИХ АЛГОРИТМІВ: ПОШУК МІНІМУМУ (ВАРІАНТ 5)\n";
    cout << format("  Апаратна кількість потоків процесора: {}\n", hw_threads);
    cout << string(70, '=') << "\n\n";

    vector<pair<string, size_t>> datasets = {
        {"data_100k.txt", 100'000},
        {"data_1m.txt",   1'000'000},
        {"data_10m.txt",  10'000'000}
    };

    for (const auto &[filename, size] : datasets)
    {
        if (!filesystem::exists(filename))
        {
            cout << format("Генерація тестового файлу {} (N = {})...\n", filename, size);
            generate_and_save_data(filename, size);
        }
    }

    for (const auto &[filename, expected_size] : datasets)
    {
        cout << string(70, '#') << '\n';
        cout << format(" НАБІР ДАНИХ: {} (Обсяг N = {})\n", filename, expected_size);
        cout << string(70, '#') << "\n\n";

        const auto data = load_data_from_file(filename);

        cout << "--- 1 & 2. Бібліотечні алгоритми та політики виконання ---\n";
        auto std_results = run_standard_benchmarks(data);

        cout << format("{:<32} | {:>12} | {:>15}\n", "Алгоритм / Політика", "Час (мс)", "Знайдений мін.");
        cout << string(66, '-') << '\n';
        for (const auto &res : std_results)
        {
            cout << format("{:<32} | {:>12.3f} | {:>15}\n", res.name, res.time_ms, res.found_min);
        }
        cout << '\n';

        cout << "--- Порівняння швидкодії при різних предикатах ---\n";
        auto pred_results = run_predicate_benchmarks(data);
        for (const auto &res : pred_results)
        {
            cout << format("{:<32} | {:>12.3f} | {:>15}\n", res.name, res.time_ms, res.found_min);
        }
        cout << '\n';

        cout << "--- 3. Власний алгоритм з розбиттям на K частин ---\n";
        auto k_results = run_custom_k_benchmarks(data);

        cout << format("{:>6} | {:>12} | {:>18}\n", "K", "Час (мс)", "Прискорення S(K)");
        cout << string(43, '-') << '\n';

        size_t best_k = 1;
        double best_time = k_results.front().time_ms;

        for (const auto &res : k_results)
        {
            cout << format("{:>6} | {:>12.3f} | {:>17.2f}x\n", res.k, res.time_ms, res.speedup);

            if (res.time_ms < best_time)
            {
                best_time = res.time_ms;
                best_k = res.k;
            }
        }
        cout << string(43, '-') << '\n';

        const double k_ratio = static_cast<double>(best_k) / hw_threads;
        cout << format("Найкраще значення K: {}\n", best_k);
        cout << format("Найкращий час:       {:.3f} мс (прискорення: {:.2f}x)\n", best_time, k_results.front().time_ms / best_time);
        cout << format("Співвідношення K_opt / Hardware_Threads: {} / {} = {:.2f}\n\n", best_k, hw_threads, k_ratio);
    }
}