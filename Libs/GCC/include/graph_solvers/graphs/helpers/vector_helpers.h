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

template <typename T, typename = void>
struct is_contiguous_resizable_container : std::false_type {};

template <typename T>
struct is_contiguous_resizable_container<T,
    std::void_t<
        decltype(std::declval<const T&>().size()),
        decltype(std::declval<T&>().resize(std::declval<std::size_t>())),
        decltype(std::declval<const T&>().data()),
        decltype(std::declval<T&>().data())>> : std::true_type {};

template <typename T>
using contiguous_value_type_t = std::remove_pointer_t<decltype(std::declval<T&>().data())>;

/// @brief В original записывает результат перестановки элементов из renumbered
/// согласно маппингу renumbered_to_original_indices для одного контейнера
template <typename Container,
    std::enable_if_t<is_contiguous_resizable_container<Container>::value, int> = 0>
void restore_original_order(const Container& renumbered,
    const std::vector<std::size_t>& renumbered_to_original_indices,
    Container& original)
{
    using value_type = contiguous_value_type_t<Container>;

    const std::size_t n = static_cast<std::size_t>(renumbered.size());

    original.resize(static_cast<decltype(original.size())>(n));

    const value_type* renumbered_ptr = renumbered.data();
    value_type* original_ptr = original.data();
    const std::size_t* map_ptr = renumbered_to_original_indices.data();

    for (std::size_t i = 0; i < n; ++i) {
        original_ptr[map_ptr[i]] = renumbered_ptr[i];
    }
}

/// @brief В original записывает результат перестановки элементов из renumbered 
/// согласно маппингу renumbered_to_original_indices для вектора контейнеров
template <typename InnerContainer,
    std::enable_if_t<is_contiguous_resizable_container<InnerContainer>::value, int> = 0>
void restore_original_order(const std::vector<InnerContainer>& renumbered,
    const std::vector<std::size_t>& renumbered_to_original_indices,
    std::vector<InnerContainer>& original)
{
    const std::size_t n = static_cast<std::size_t>(renumbered.size());
    original.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        restore_original_order(renumbered[i], renumbered_to_original_indices, original[i]);
    }
}



}