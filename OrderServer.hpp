#ifndef ORDERSERVER_HPP
#define ORDERSERVER_HPP

#include <string>
#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <mutex>
#include <unordered_map>
#include <unistd.h>
#include "PackMessage.hpp"
#include "FMTLogger.hpp"
#include "LockFreeQueue.hpp"
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
        std::string key = std::string(msg.AccountPosition.Account) + ":" + msg.AccountPosition.Ticker;
        m_AccountPositionMap[key] = msg;
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

        for(auto it = m_AccountPositionMap.begin(); it != m_AccountPositionMap.end(); it++)
        {
            Msg.TimeStamp = SHMIPC::RDTSC();
            memcpy(&Msg.Data, &(it->second), sizeof(Msg.Data));
            m_AllChannel[ChannelID].SendQueue.Push(Msg);
        }
        fprintf(stdout, "OrderServer Push Init Data Done\n");
    }

protected:
    Message::PackMessage m_AccountFundMsg;
    typedef phmap::parallel_flat_hash_map<std::string, Message::PackMessage, 
                                        phmap::priv::hash_default_hash<std::string>,
                                        phmap::priv::hash_default_eq<std::string>,
                                        std::allocator<std::pair<std::string, Message::PackMessage>>, 
                                        8, std::shared_mutex>
    AccountPositionMapT;
    AccountPositionMapT m_AccountPositionMap;
};


#endif // ORDERSERVER_HPP