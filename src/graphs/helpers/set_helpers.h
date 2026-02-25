#pragma once

namespace graphlib {
;



/// @brief Возвращает общие элементы двух множеств
/// @tparam SmallerSetContainer Тип данных меньшего множества
/// @tparam SetContainer Тип данных большего множества
/// @return Множество типа SetContainer из обших элементов
template <typename SmallerSetContainer, typename SetContainer>
inline SetContainer set_intersection(const SmallerSetContainer& smaller_set, const SetContainer& larger_set)
{
    SetContainer result;
    for (const auto& element : smaller_set) {
        auto iter = larger_set.find(element);
        if (iter != larger_set.end()) {
            result.insert(*iter);
        }
    }
    return result;
}


}