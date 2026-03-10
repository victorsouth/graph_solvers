#pragma once

namespace graphlib {
;


/// @brief Возвращает максимальное значение из пар "ключ-значение" в заданном std::unordered_map.
///  @tparam Key Тип ключа в словаре.
///  @tparam Value Тип значения, для которого должен быть определён оператор сравнения `>`.
template <typename Key, typename Value>
inline Value get_max_value(const std::unordered_map<Key, Value>& mapping) {
    Value result = std::numeric_limits<Value>::min();
    for (const auto& key_value_pair : mapping) {
        result = std::max(result, key_value_pair.second);
    }
    return result;
}


/// @brief Проверяет наличие ключа в словаре
/// @tparam MapType Тип словаря
/// @tparam KeyType Тип ключа
template <typename MapType, typename KeyType>
inline bool has_map_key(const MapType& mapping, const KeyType& key)
{
    return mapping.find(key) != mapping.end();
}


template <typename MapType>
inline std::unordered_set<typename MapType::key_type> get_map_keys(const MapType& mapping)
{
    std::unordered_set<typename MapType::key_type> keys;
    for (const auto& kvp : mapping) {
        keys.insert(kvp.first);
    }
    return keys;
}

template <typename MapType, typename ResultContainerType>
inline void get_map_keys(const MapType& mapping, ResultContainerType* result)
{
    typedef decltype(*mapping.begin()) KeyType;

    std::transform(mapping.begin(), mapping.end(),
        std::inserter(*result, result->end()),
        [](const KeyType& data) { return data.first; });
}

/// @brief Получает значение из unordered_map по ключу, если ключа нет - возвращает значение по умолчанию
/// @param mapping Словарь для поиска
/// @param key Ключ для поиска
template <typename Key, typename Value, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
inline Value get_value_or_default(const std::unordered_map<Key, Value, Hash, KeyEqual>& mapping, const Key& key) {
    auto it = mapping.find(key);
    if (it != mapping.end()) {
        return it->second;
    }
    return Value{};
}

}