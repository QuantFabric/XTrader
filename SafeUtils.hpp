#ifndef XTRADER_SAFEUTILS_HPP
#define XTRADER_SAFEUTILS_HPP

#include <cstddef>
#include <utility>
#include <vector>

#include <fmt/core.h>

namespace XTrader
{

template <std::size_t N, typename... Args>
void FormatToFixedBuffer(char (&buffer)[N], fmt::format_string<Args...> format, Args&&... args)
{
    static_assert(N > 0, "fixed-size format buffer must not be empty");
    auto result = fmt::format_to_n(buffer, N - 1, format, std::forward<Args>(args)...);
    *result.out = '\0';
}

template <typename Value, typename Map, typename Key, typename Updater>
bool UpsertParallelMapEntry(Map& map, const Key& key, Updater&& updater)
{
    return map.lazy_emplace_l(
        key,
        [&](typename Map::value_type& item) {
            std::forward<Updater>(updater)(item.second);
        },
        [&](const typename Map::constructor& constructor) {
            Value value{};
            std::forward<Updater>(updater)(value);
            constructor(key, std::move(value));
        });
}

template <typename Value, typename Map, typename Key>
bool EmplaceParallelMapEntryIfAbsent(Map& map, const Key& key, const Value& value)
{
    return map.lazy_emplace_l(
        key,
        [](typename Map::value_type&) {},
        [&](const typename Map::constructor& constructor) {
            constructor(key, value);
        });
}

template <typename Map, typename Key, typename Updater>
bool ModifyParallelMapEntry(Map& map, const Key& key, Updater&& updater)
{
    return map.modify_if(key, [&](typename Map::value_type& item) {
        std::forward<Updater>(updater)(item.second);
    });
}

template <typename Map, typename Key>
bool EraseParallelMapEntry(Map& map, const Key& key)
{
    return map.erase_if(key, [](typename Map::value_type&) { return true; });
}

template <typename Map, typename Key, typename Predicate>
bool EraseParallelMapEntryIf(Map& map, const Key& key, Predicate&& predicate)
{
    return map.erase_if(key, [&](typename Map::value_type& item) {
        return std::forward<Predicate>(predicate)(item.second);
    });
}

template <typename Value, typename Map, typename Key>
bool ReadParallelMapEntry(const Map& map, const Key& key, Value& value)
{
    bool found = false;
    map.if_contains(key, [&](const typename Map::value_type& item) {
        value = item.second;
        found = true;
    });
    return found;
}

template <typename Value, typename Map>
std::vector<Value> SnapshotParallelMapValues(const Map& map)
{
    std::vector<Value> values;
    map.for_each([&](const typename Map::value_type& item) {
        values.push_back(item.second);
    });
    return values;
}

template <typename Map>
std::vector<std::pair<typename Map::key_type, typename Map::mapped_type>>
SnapshotParallelMapEntries(const Map& map)
{
    using Entry = std::pair<typename Map::key_type, typename Map::mapped_type>;
    std::vector<Entry> entries;
    map.for_each([&](const typename Map::value_type& item) {
        entries.emplace_back(item.first, item.second);
    });
    return entries;
}

template <typename Map, typename Updater>
std::vector<std::pair<typename Map::key_type, typename Map::mapped_type>>
ModifyParallelMapEntries(Map& map, Updater&& updater)
{
    using Entry = std::pair<typename Map::key_type, typename Map::mapped_type>;
    std::vector<Entry> entries;
    map.for_each_m([&](typename Map::value_type& item) {
        updater(item.second);
        entries.emplace_back(item.first, item.second);
    });
    return entries;
}

} // namespace XTrader

#endif // XTRADER_SAFEUTILS_HPP
