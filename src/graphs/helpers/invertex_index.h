
namespace graphlib {
;



/// @brief Возвращает обратное отображение "значение → индекс" в виде std::unordered_map<size_t, size_t>
inline std::unordered_map<size_t, size_t> get_inverted_index(const std::vector<size_t>& indices_list);

/// @brief Возвращает обратное отображение "значение → индекс" для заданного вектора.
/// @tparam ResultMap Тип возвращаемого контейнера (например, std::unordered_map<size_t, size_t>).
/// @warning Если в списке есть дубликаты, будет учтён только последний индекс для каждого значения.
template <typename ResultMap>
inline ResultMap get_inverted_index(const std::vector<size_t>& indices_list)
{
    ResultMap inverted_index;
    for (size_t index_index = 0; index_index < indices_list.size(); ++index_index) {
        size_t index = indices_list[index_index];
        inverted_index[index] = index_index;
    }
    return inverted_index;
}

/// @brief Возвращает обратное отображение "значение → индекс" в виде std::unordered_map для заданного std::unordered_map
/// @tparam Key Тип ключа в std::unordered_map
/// @tparam Value Тип значения в std::unordered_map
template <typename Key, typename Value>
inline std::unordered_map<Value, Key> get_inverted_index(const std::unordered_map<Key, Value>& index) {
    std::unordered_map<Value, Key> inverted_index;
    for (const auto& iter : index) {
        inverted_index[iter->second] = iter->first;
    }
    return inverted_index;
}

inline std::unordered_map <size_t, size_t> get_inverted_index(
    const std::unordered_map<size_t, std::unordered_set<size_t>>& subsets)
{
    std::unordered_map <size_t, size_t> inverted_index;
    for (const auto& kvp : subsets) {
        size_t subgraph = kvp.first;
        for (size_t vertex : kvp.second) {
            inverted_index[vertex] = subgraph;
        }
    }
    return inverted_index;
}

/// @brief Строит обратное отображение из словаря, превращая "ключ → значение" в "значение → ключ".
/// @tparam Map Тип исходного словаря (например, std::unordered_map<Key, Value>).
/// @tparam InverseMap Тип целевого словаря (например, std::unordered_map<Value, Key>).
/// @param index Исходный словарь, содержащий пары "ключ → значение".
/// @param result Указатель на словарь, в который будет записан результат: "значение → ключ".
template <typename Map, typename InverseMap>
inline void get_inverted_index(const Map& index, InverseMap* result) {
    InverseMap& inverted_index = *result;
    for (const auto& iter : index) {
        inverted_index.emplace(iter.second, iter.first);
    }
}


/// @brief Создаёт обратное отображение "значение → индекс" для заданного контейнера значений.
/// @tparam IndexContainer Тип входного контейнера (например, std::vector<size_t> или std::unordered_set<size_t>).
/// @param indices_list Входной контейнер с последовательностью значений.
/// @return std::unordered_map<size_t, size_t> — карта соответствий: значение → его индекс в списке.
/// @note Если в списке есть дубликаты, будет учтён только последний индекс для каждого значения.
template <typename IndexContainer>
inline std::unordered_map<size_t, size_t> get_inverted_index_as_unordered_map(const IndexContainer& indices_list)
{
    std::unordered_map<size_t, size_t> inverted_index;
    size_t index_index = 0;
    for (size_t index : indices_list) {
        inverted_index[index] = index_index++;
    }
    return inverted_index;
}

}