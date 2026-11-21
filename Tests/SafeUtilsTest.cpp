#include "AccountTickerKey.hpp"
#include "SafeUtils.hpp"
#include "phmap.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace
{

#ifndef XTRADER_TEST_ITERATIONS
#define XTRADER_TEST_ITERATIONS 20000
#endif

using TestMap = phmap::parallel_flat_hash_map<
    int,
    int,
    phmap::Hash<int>,
    phmap::EqualTo<int>,
    std::allocator<std::pair<const int, int>>,
    8,
    std::shared_mutex>;

using TestAccountTickerMap = phmap::parallel_flat_hash_map<
    XTrader::AccountTickerKey,
    int,
    XTrader::AccountTickerKeyHash,
    XTrader::AccountTickerKeyEqual,
    std::allocator<std::pair<const XTrader::AccountTickerKey, int>>,
    8,
    std::shared_mutex>;

void Check(bool condition, const char* message)
{
    if(!condition)
    {
        std::fprintf(stderr, "XTraderSafeUtilsTest failed: %s\n", message);
        std::abort();
    }
}

void TestFixedBufferFormatting()
{
    char shortBuffer[16] = "xxxxxxxxxxxxxxx";
    XTrader::FormatToFixedBuffer(shortBuffer, "{}", "abc");
    Check(std::strcmp(shortBuffer, "abc") == 0, "short format result");
    Check(shortBuffer[3] == '\0', "short format terminator");

    char exactBuffer[5] = "xxxx";
    XTrader::FormatToFixedBuffer(exactBuffer, "{}", "1234");
    Check(std::strcmp(exactBuffer, "1234") == 0, "exact format result");
    Check(exactBuffer[4] == '\0', "exact format terminator");

    char longBuffer[5] = "xxxx";
    XTrader::FormatToFixedBuffer(longBuffer, "{}", "123456789");
    Check(std::strcmp(longBuffer, "1234") == 0, "truncated format result");
    Check(longBuffer[4] == '\0', "truncated buffer terminator");

    char emptyBuffer[1] = {'x'};
    XTrader::FormatToFixedBuffer(emptyBuffer, "");
    Check(emptyBuffer[0] == '\0', "one-byte buffer terminator");
}

void TestConcurrentMapUpdates()
{
    constexpr int kThreads = 8;
    constexpr int kIterations = XTRADER_TEST_ITERATIONS;
    TestMap map;
    bool stopSnapshotter = false;
    std::mutex stopSnapshotterMutex;

    std::thread snapshotter([&]() {
        while(true)
        {
            {
                std::lock_guard<std::mutex> lock(stopSnapshotterMutex);
                if(stopSnapshotter)
                {
                    break;
                }
            }
            (void)XTrader::SnapshotParallelMapValues<int>(map);
            (void)XTrader::SnapshotParallelMapEntries(map);
            std::this_thread::yield();
        }
    });

    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for(int threadIndex = 0; threadIndex < kThreads; ++threadIndex)
    {
        workers.emplace_back([&, threadIndex]() {
            for(int i = 0; i < kIterations; ++i)
            {
                XTrader::UpsertParallelMapEntry<int>(
                    map, -1, [](int& value) { ++value; });
                XTrader::UpsertParallelMapEntry<int>(
                    map, threadIndex, [](int& value) { ++value; });
            }
        });
    }

    for(auto& worker : workers)
    {
        worker.join();
    }
    {
        std::lock_guard<std::mutex> lock(stopSnapshotterMutex);
        stopSnapshotter = true;
    }
    snapshotter.join();

    Check(map[-1] == kThreads * kIterations, "same-key update count");
    for(int threadIndex = 0; threadIndex < kThreads; ++threadIndex)
    {
        Check(map[threadIndex] == kIterations, "per-key update count");
    }

    int readValue = 0;
    Check(XTrader::ReadParallelMapEntry(map, -1, readValue), "existing entry read");
    Check(readValue == kThreads * kIterations, "read entry value");
    Check(XTrader::ModifyParallelMapEntry(map, -1, [](int& value) { value += 1; }),
          "existing entry modification");
    Check(map[-1] == kThreads * kIterations + 1, "modified entry value");
    Check(XTrader::EraseParallelMapEntryIf(map, 0,
                                          [=](int value) { return value == kIterations; }),
          "conditional erase");
    Check(!map.contains(0), "conditional erase removes entry");
    Check(XTrader::EraseParallelMapEntry(map, 1), "unconditional erase");
    Check(!map.contains(1), "unconditional erase removes entry");

    Check(XTrader::EmplaceParallelMapEntryIfAbsent(map, 100, 41), "insert absent entry");
    Check(!XTrader::EmplaceParallelMapEntryIfAbsent(map, 100, 99), "duplicate insert is ignored");
    int insertedValue = 0;
    Check(XTrader::ReadParallelMapEntry(map, 100, insertedValue), "read inserted entry");
    Check(insertedValue == 41, "duplicate insert preserves original value");

    int missingValue = 0;
    Check(!XTrader::ReadParallelMapEntry(map, 999, missingValue), "missing entry read");
    Check(missingValue == 0, "missing entry does not change output");

    auto entries = XTrader::SnapshotParallelMapEntries(map);
    auto entry = std::find_if(entries.begin(), entries.end(), [](const auto& item) {
        return item.first == 100;
    });
    Check(entry != entries.end() && entry->second == 41, "key-value snapshot");

    auto modifiedEntries = XTrader::ModifyParallelMapEntries(map, [](int& value) {
        value += 1;
    });
    entry = std::find_if(modifiedEntries.begin(), modifiedEntries.end(), [](const auto& item) {
        return item.first == 100;
    });
    Check(entry != modifiedEntries.end() && entry->second == 42, "batch modification");
    Check(map[100] == 42, "batch modification stored");
}

void TestAccountTickerKey()
{
    static_assert(std::is_trivial<XTrader::AccountTickerKey>::value,
                  "account/ticker key must be trivial");
    static_assert(std::is_standard_layout<XTrader::AccountTickerKey>::value,
                  "account/ticker key must be standard layout");

    const auto key = XTrader::MakeAccountTickerKey("account", "ticker");
    const auto sameKey = XTrader::MakeAccountTickerKey(std::string("account"), std::string("ticker"));
    const XTrader::AccountTickerKeyEqual equal;
    const XTrader::AccountTickerKeyHash hash;
    Check(equal(key, sameKey), "equal account/ticker keys");
    Check(hash(key) == hash(sameKey), "equal keys hash equally");

    const auto colonInAccount = XTrader::MakeAccountTickerKey("A:B", "C");
    const auto colonInTicker = XTrader::MakeAccountTickerKey("A", "B:C");
    Check(!equal(colonInAccount, colonInTicker), "colon-delimited keys stay distinct");

    TestAccountTickerMap collisionMap;
    XTrader::UpsertParallelMapEntry<int>(
        collisionMap, colonInAccount, [](int& value) { value = 1; });
    int collisionValue = 0;
    Check(!XTrader::ReadParallelMapEntry(collisionMap, colonInTicker, collisionValue),
          "colon-delimited map keys stay distinct");

    const auto truncated = XTrader::MakeAccountTickerKey(
        std::string(40, 'a'), std::string(40, 't'));
    Check(::strnlen(truncated.Account, sizeof(truncated.Account)) == 15,
          "account key truncation");
    Check(::strnlen(truncated.Ticker, sizeof(truncated.Ticker)) == 19,
          "ticker key truncation");

    TestAccountTickerMap map;
    XTrader::UpsertParallelMapEntry<int>(map, key, [](int& value) { value = 7; });
    int value = 0;
    Check(XTrader::ReadParallelMapEntry(map, sameKey, value), "POD key map read");
    Check(value == 7, "POD key map value");
    Check(XTrader::ModifyParallelMapEntry(map, key, [](int& stored) { stored += 1; }),
          "POD key map modify");
    Check(XTrader::EraseParallelMapEntry(map, sameKey), "POD key map erase");
    Check(!map.contains(key), "POD key map erased");
}

} // namespace

int main()
{
    TestFixedBufferFormatting();
    TestConcurrentMapUpdates();
    TestAccountTickerKey();
    return 0;
}
