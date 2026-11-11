#pragma once

#include <cstddef>

struct Stat
{
    std::size_t object_count = 0;
    std::size_t array_count = 0;
    std::size_t number_count = 0;
    std::size_t string_count = 0;
    std::size_t true_count = 0;
    std::size_t false_count = 0;
    std::size_t null_count = 0;
    std::size_t member_count = 0;
    std::size_t element_count = 0;
    std::size_t string_length = 0;
};

inline bool operator== (const Stat& a, const Stat& b)
{
    return a.object_count == b.object_count && a.array_count == b.array_count && a.number_count == b.number_count &&
           a.string_count == b.string_count && a.true_count == b.true_count && a.false_count == b.false_count &&
           a.null_count == b.null_count && a.member_count == b.member_count && a.element_count == b.element_count &&
           a.string_length == b.string_length;
}
