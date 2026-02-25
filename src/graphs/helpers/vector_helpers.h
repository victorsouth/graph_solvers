#pragma once


namespace graphlib {
;

/// @brief Помещает элементы из вектора src в конец вектора dest
/// @tparam T Тип элементов вектора
template<typename T>
void append_vector(std::vector<T>& dest, const std::vector<T>& src) {
    dest.insert(dest.end(), src.begin(), src.end());
}

/// @brief Извлекает элементы по указанным индексам из массива
/// @param iterator_begin Итератор на начало исходного массива
/// @param iterator_end Итератор на конец исходного массива
/// @param index_list Контейнер с индексами извлекаемых элементов
/// @param[out] removed_elements Опциональный выходной параметр для удаленных элементов
/// @return Вектор элементов, соответствующих индексам из index_list
template<typename Iterator, typename IndicesContainer>
inline auto filter_elements_by_indices(Iterator iterator_begin, Iterator iterator_end, const IndicesContainer& index_list,
    std::vector < typename std::remove_const<typename std::remove_reference< decltype(*iterator_begin)>::type>::type >*
    removed_elements = nullptr
) ->
std::vector < typename std::remove_const<typename std::remove_reference< decltype(*iterator_begin)>::type>::type > {
    typedef
        typename std::remove_const<typename std::remove_reference< decltype(*iterator_begin)>::type> ::type
        ElementType;

    using std::vector;

    std::vector<ElementType> filtered_elements;

    if (removed_elements == nullptr) {
        auto size = iterator_end - iterator_begin;

        typedef typename std::remove_reference<decltype(size)>::type signed_type;

        // Сложность O(размер подмножества)
        for (auto index : index_list) {
            auto iter = iterator_begin + index;
            if (static_cast<signed_type>(index) >= size)
                throw std::logic_error("filter_elements_by_indices(): subset index list out of range");
            filtered_elements.push_back(*iter);
        }

    }
    else {
        // Сложность O(размер буфера) = O(iterator_end - iterator_begin)
        // Выделяемы индексы в виде множества
        std::unordered_set<size_t> index_subset(index_list.begin(), index_list.end());
        for (Iterator iter = iterator_begin; iter != iterator_end; ++iter) {
            auto index = iter - iterator_begin;
            if (index_subset.find(index) != index_subset.end()) {
                filtered_elements.push_back(*iter);
            }
            else {
                // здесь removed_elements всегда != nullptr
                removed_elements->push_back(*iter);
            }
        }
    }



    return filtered_elements;
}



}