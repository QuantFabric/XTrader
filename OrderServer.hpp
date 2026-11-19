#ifndef ORDERSERVER_HPP
#define ORDERSERVER_HPP

#include <string>
#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <mutex>
#include <unistd.h>
#include "AccountTickerKey.hpp"
#include "PackMessage.hpp"
#include "FMTLogger.hpp"
#include "LockFreeQueue.hpp"
#include "SafeUtils.hpp"
#include "phmap.h"
#include <shared_mutex>

#include "SHMServer.hpp"

struct ServerConf : public SHMIPC::CommonConf
{
    static const bool Publish = false;
    static const bool Performance = true;
};

class OrderServer : public SHMIPC::SHMServer<Message::PackMessage, ServerConf>
{
public:
    OrderServer():SHMServer<Message::PackMessage, ServerConf>()
    {

    }

    virtual ~OrderServer()
    {

    }

    void HandleMsg()
    {

    }

    void UpdateAccountFund(const Message::PackMessage& msg)
    {
        memcpy(&m_AccountFundMsg, &msg, sizeof(m_AccountFundMsg));
    }
    
    void UpdateAccountPosition(const Message::PackMessage& msg)
    {
        const auto key = XTrader::MakeAccountTickerKey(
            msg.AccountPosition.Account, msg.AccountPosition.Ticker);
        XTrader::UpsertParallelMapEntry<Message::PackMessage>(
            m_AccountPositionMap, key, [&](Message::PackMessage& position) {
                position = msg;
            });
    }

protected:
    virtual void Push_Init_Data(uint16_t ChannelID)
    {
        SHMIPC::TChannelMsg<Message::PackMessage> Msg;
        Msg.MsgType = SHMIPC::EMsgType::EMSG_TYPE_DATA;
        Msg.ChannelID = ChannelID;
        {
            Msg.TimeStamp = SHMIPC::RDTSC();
            memcpy(&Msg.Data, &m_AccountFundMsg, sizeof(Msg.Data));
            m_AllChannel[ChannelID].SendQueue.Push(Msg);
        }

        m_AccountPositionMap.for_each([&](const AccountPositionMapT::value_type& item) {
            Msg.TimeStamp = SHMIPC::RDTSC();
            memcpy(&Msg.Data, &item.second, sizeof(Msg.Data));
            m_AllChannel[ChannelID].SendQueue.Push(Msg);
        });
        fprintf(stdout, "OrderServer Push Init Data Done\n");
    }

protected:
    Message::PackMessage m_AccountFundMsg;
    typedef phmap::parallel_flat_hash_map<XTrader::AccountTickerKey, Message::PackMessage,
                                        XTrader::AccountTickerKeyHash,
                                        XTrader::AccountTickerKeyEqual,
                                        std::allocator<std::pair<const XTrader::AccountTickerKey, Message::PackMessage>>,
                                        8, std::shared_mutex>
    AccountPositionMapT;
    AccountPositionMapT m_AccountPositionMap;
};


#endif // ORDERSERVER_HPP
