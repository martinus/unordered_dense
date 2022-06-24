#pragma once

#include <string_view>

template <typename T>
constexpr auto name_of_type_raw() -> std::string_view {
#if defined(_MSC_VER)
    return __FUNCSIG__;
#else
    return __PRETTY_FUNCTION__;
#endif
}

template <typename T>
constexpr auto name_of_type() -> std::string_view {
    using namespace std::literals;

    // idea from https://github.com/TheLartians/StaticTypeInfo/blob/master/include/static_type_info/type_name.h
    auto for_double = name_of_type_raw<double>();
    auto n_before = for_double.find("double"sv);
    auto n_after = for_double.size() - (n_before + "double"sv.size());

    auto str = name_of_type_raw<T>();
    str.remove_prefix(n_before);
    str.remove_suffix(n_after);
    return str;
}