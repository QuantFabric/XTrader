#ifndef XTRADER_ACCOUNT_TICKER_KEY_HPP
#define XTRADER_ACCOUNT_TICKER_KEY_HPP

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

#include "phmap.h"

namespace XTrader
{

struct AccountTickerKey
{
    char Account[16];
    char Ticker[20];
};

static_assert(std::is_trivial<AccountTickerKey>::value,
              "AccountTickerKey must be trivial");
static_assert(std::is_standard_layout<AccountTickerKey>::value,
              "AccountTickerKey must be standard layout");

inline AccountTickerKey MakeAccountTickerKey(const char* account, const char* ticker)
{
    AccountTickerKey key{};
    if(account != nullptr)
    {
        std::strncpy(key.Account, account, sizeof(key.Account) - 1);
    }
    if(ticker != nullptr)
    {
        std::strncpy(key.Ticker, ticker, sizeof(key.Ticker) - 1);
    }
    return key;
}

inline AccountTickerKey MakeAccountTickerKey(const std::string& account, const std::string& ticker)
{
    return MakeAccountTickerKey(account.c_str(), ticker.c_str());
}

struct AccountTickerKeyHash
{
    std::size_t operator()(const AccountTickerKey& key) const noexcept
    {
        return phmap::HashState().combine(
            0, std::string_view(key.Account), std::string_view(key.Ticker));
    }
};

struct AccountTickerKeyEqual
{
    bool operator()(const AccountTickerKey& lhs, const AccountTickerKey& rhs) const noexcept
    {
        return std::memcmp(lhs.Account, rhs.Account, sizeof(lhs.Account)) == 0 &&
               std::memcmp(lhs.Ticker, rhs.Ticker, sizeof(lhs.Ticker)) == 0;
    }
};

} // namespace XTrader

#endif // XTRADER_ACCOUNT_TICKER_KEY_HPP
