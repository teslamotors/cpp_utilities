#include "fixed_containers/fixed_map.hpp"
#include "fixed_containers/fixed_unordered_map.hpp"
#include "fixed_containers/memory.hpp"

#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <unordered_map>

namespace fixed_containers
{

namespace
{
template <typename MapType>
void benchmark_map_copy(benchmark::State& state)
{
    const int64_t nelem = state.range(0);
    const std::unique_ptr<MapType> instance_ptr = std::make_unique<MapType>();
    MapType& instance = *instance_ptr.get();

    using KeyType = typename MapType::key_type;
    for (int64_t i = 0; i < nelem; i++)
    {
        instance.try_emplace(static_cast<KeyType>(i));
    }

    const std::unique_ptr<MapType> instance_ptr2 = std::make_unique<MapType>();
    MapType& instance2 = *instance_ptr2.get();
    for (auto _ : state)
    {
        memory::destroy_and_construct_at_address_of(instance2, instance);
        benchmark::DoNotOptimize(instance2);
    }
}

template <typename MapType>
void benchmark_map_copy_then_clear(benchmark::State& state)
{
    using KeyType = typename MapType::key_type;
    const std::unique_ptr<MapType> instance_ptr = std::make_unique<MapType>();
    MapType& instance = *instance_ptr.get();
    const int64_t nelem = state.range(0);
    for (int64_t i = 0; i < nelem; i++)
    {
        instance.try_emplace(static_cast<KeyType>(i));
    }

    const std::unique_ptr<MapType> instance_ptr2 = std::make_unique<MapType>();
    MapType& instance2 = *instance_ptr2.get();
    for (auto _ : state)
    {
        memory::destroy_and_construct_at_address_of(instance2, instance);
        instance2.clear();
        benchmark::DoNotOptimize(instance2);
    }
}

template <typename MapType>
void benchmark_map_copy_then_reconstruct(benchmark::State& state)
{
    using KeyType = typename MapType::key_type;
    const std::unique_ptr<MapType> instance_ptr = std::make_unique<MapType>();
    MapType& instance = *instance_ptr.get();
    const int64_t nelem = state.range(0);
    for (int64_t i = 0; i < nelem; i++)
    {
        instance.try_emplace(static_cast<KeyType>(i));
    }

    const std::unique_ptr<MapType> instance_ptr2 = std::make_unique<MapType>();
    MapType& instance2 = *instance_ptr2.get();
    for (auto _ : state)
    {
        memory::destroy_and_construct_at_address_of(instance2, instance);
        memory::destroy_and_construct_at_address_of(instance2);
        benchmark::DoNotOptimize(instance2);
    }
}

template <typename ArrType>
void benchmark_array_clear(benchmark::State& state)
{
    const std::unique_ptr<ArrType> instance_ptr = std::make_unique<ArrType>();
    ArrType& instance = *instance_ptr.get();

    for (auto _ : state)
    {
        instance.fill(0);
        benchmark::DoNotOptimize(instance);
    }
}

constexpr std::size_t MAXIMUM_SIZE_LIMIT = 8 << 13;
constexpr std::size_t START = 16;
}  // namespace

BENCHMARK(benchmark_map_copy<std::map<int, int>>)->Range(START, MAXIMUM_SIZE_LIMIT);
BENCHMARK(benchmark_map_copy_then_clear<std::map<int, int>>)->Range(START, MAXIMUM_SIZE_LIMIT);
BENCHMARK(benchmark_map_copy_then_reconstruct<std::map<int, int>>)
    ->Range(START, MAXIMUM_SIZE_LIMIT);

BENCHMARK(benchmark_map_copy<std::unordered_map<int, int>>)->Range(START, MAXIMUM_SIZE_LIMIT);
BENCHMARK(benchmark_map_copy_then_clear<std::unordered_map<int, int>>)
    ->Range(START, MAXIMUM_SIZE_LIMIT);
BENCHMARK(benchmark_map_copy_then_reconstruct<std::unordered_map<int, int>>)
    ->Range(START, MAXIMUM_SIZE_LIMIT);

BENCHMARK(benchmark_map_copy<FixedMap<int, int, MAXIMUM_SIZE_LIMIT>>)
    ->Range(START, MAXIMUM_SIZE_LIMIT);
BENCHMARK(benchmark_map_copy_then_clear<FixedMap<int, int, MAXIMUM_SIZE_LIMIT>>)
    ->Range(START, MAXIMUM_SIZE_LIMIT);
BENCHMARK(benchmark_map_copy_then_reconstruct<FixedMap<int, int, MAXIMUM_SIZE_LIMIT>>)
    ->Range(START, MAXIMUM_SIZE_LIMIT);

BENCHMARK(benchmark_map_copy<FixedUnorderedMap<int, int, MAXIMUM_SIZE_LIMIT>>)
    ->Range(START, MAXIMUM_SIZE_LIMIT);
BENCHMARK(benchmark_map_copy_then_clear<FixedUnorderedMap<int, int, MAXIMUM_SIZE_LIMIT>>)
    ->Range(START, MAXIMUM_SIZE_LIMIT);
BENCHMARK(benchmark_map_copy_then_reconstruct<FixedUnorderedMap<int, int, MAXIMUM_SIZE_LIMIT>>)
    ->Range(START, MAXIMUM_SIZE_LIMIT);

// more-or-less the theoretical best performance we could possibly get for a full FixedUnorderedMap
// (just 0 out every bucket)
BENCHMARK(benchmark_array_clear<std::array<long, (MAXIMUM_SIZE_LIMIT * 130ULL) / 100>>);

}  // namespace fixed_containers

BENCHMARK_MAIN();
