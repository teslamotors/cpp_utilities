#include "fixed_containers/tuples.hpp"

#include "fixed_containers/int_math.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <tuple>

namespace fixed_containers::tuples::as_tuple_codegen
{
TEST(Tuples, AsTupleViewCodegenBranches)
{
    // `DISABLED_` would cause gtest to report that 1 test is disabled,
    // so using this instead.
    if constexpr ((false))
    {
        static constexpr std::size_t MAX_VARIABLE_COUNT = 720;
        static constexpr std::size_t GROUP_SIZE = 32;
        static constexpr std::size_t GROUP_COUNT =
            int_math::divide_integers_rounding_up(MAX_VARIABLE_COUNT, GROUP_SIZE);

        auto do_group =
            [](const std::size_t starting_i, const std::size_t ending_i, std::string& variables)
        {
            for (std::size_t i = starting_i; i <= ending_i; i++)
            {
                if (!variables.empty())
                {
                    variables += ",";
                }
                variables += "m" + std::to_string(i);
                if (i == starting_i)
                {
                    std::cout << "        if constexpr(FIELD_COUNT == " << i << ") {\n";
                }
                else
                {
                    std::cout << "        else if constexpr(FIELD_COUNT == " << i << ") {\n";
                }

                std::cout << "            auto& [" << variables << "] = data;\n"
                          << "            return std::tie(" << variables << ");\n"
                          << "        }\n";
            }
        };

        std::string variables{};

        std::cout << "    // codegen-start\n";
        std::cout << "    // clang-format off\n";
        for (std::size_t group_id = 0; group_id < GROUP_COUNT; group_id++)
        {
            const std::size_t starting_i = group_id * GROUP_SIZE + 1;
            const std::size_t ending_i =
                std::min(MAX_VARIABLE_COUNT, group_id * GROUP_SIZE + GROUP_SIZE);

            std::cout << "    else if constexpr(FIELD_COUNT <= " << ending_i << ") {\n";

            do_group(starting_i, ending_i, variables);
            std::cout << "    }\n";
        }
        std::cout << "    // clang-format on\n";
        std::cout << "    // codegen-end\n";
    }
}
}  // namespace fixed_containers::tuples::as_tuple_codegen

namespace fixed_containers
{
namespace
{
struct StructOfPrimitives
{
    int a1;
    std::size_t a2;
    double a3;
    std::int64_t a4;
    char a5;
};
}  // namespace

TEST(Tuples, AsTupleView)
{
    constexpr auto RESULT = []()
    {
        StructOfPrimitives instance{};
        std::tuple<int&, std::size_t&, double&, std::int64_t&, char&> tuple_view =
            tuples::as_tuple_view<5>(instance);
        std::get<0>(tuple_view) = 11;
        std::get<1>(tuple_view) = 13;
        std::get<2>(tuple_view) = 2.0;
        std::get<3>(tuple_view) = 17;
        std::get<4>(tuple_view) = 'z';
        return instance;
    }();
    static_assert(RESULT.a1 == 11);
    static_assert(RESULT.a2 == 13);
    static_assert(RESULT.a3 == 2.0);
    static_assert(RESULT.a4 == 17);
    static_assert(RESULT.a5 == 'z');
}

TEST(Tuples, ForEachEntryEmpty)
{
    constexpr auto RESULT = []()
    {
        std::tuple<> var{};
        tuples::for_each_entry(var, []<typename T>(T& /*t*/) {});
        tuples::for_each_entry(var, []<typename T>(T& /*t*/, std::size_t /*i*/) {});
        return var;
    }();

    static_assert(std::tuple_size_v<decltype(RESULT)> == 0);
}

TEST(Tuples, ForEachEntry)
{
    constexpr auto RESULT = []()
    {
        std::tuple<int, double> var{1, 2};
        tuples::for_each_entry(var,
                               []<typename T>(T& entry)
                               {
                                   entry *= 2;
                                   if constexpr (std::same_as<T, int>)
                                   {
                                       entry += 7;
                                   }
                               });
        return var;
    }();

    static_assert(std::get<0>(RESULT) == 9);
    static_assert(std::get<1>(RESULT) == 4.0);
}

TEST(Tuples, ForEachEntryWithIndex)
{
    constexpr auto RESULT = []()
    {
        std::tuple<int, double> var{1, 2};
        tuples::for_each_entry(var,
                               []<typename T>(std::size_t index, T& entry)
                               {
                                   if (index == 0)
                                   {
                                       entry *= 2;
                                   }
                                   else
                                   {
                                       entry *= 3;
                                   }

                                   if constexpr (std::same_as<T, int>)
                                   {
                                       entry += 7;
                                   }
                               });
        return var;
    }();

    static_assert(std::get<0>(RESULT) == 9);
    static_assert(std::get<1>(RESULT) == 6.0);
}

}  // namespace fixed_containers
