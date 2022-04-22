#ifndef TRADEGATEWAY_HPP
#define TRADEGATEWAY_HPP

#include <string>
#include "RingBuffer.hpp"
#include "PackMessage.hpp"
#include "Logger.h"
#include "YMLConfig.hpp"

class TradeGateWay
{
public:
    virtual void LoadAPIConfig() = 0;
    virtual void GetCommitID(std::string& CommitID, std::string& UtilsCommitID) = 0;
    virtual void GetAPIVersion(std::string& APIVersion) = 0;
    virtual void CreateTraderAPI() = 0;
    virtual void DestroyTraderAPI() = 0;
    virtual void ReqUserLogin() = 0;
    virtual void LoadTrader() = 0;
    virtual void ReLoadTrader() = 0;
    virtual int ReqQryFund() = 0;
    virtual int ReqQryPoistion() = 0;
    virtual int ReqQryTrade() = 0;
    virtual int ReqQryOrder() = 0;
    virtual int ReqQryTickerRate() = 0;
    virtual void ReqInsertOrder(const Message::TOrderRequest& request) = 0;
    virtual void ReqInsertOrderRejected(const Message::TOrderRequest& request) = 0;
    virtual void ReqCancelOrder(const Message::TActionRequest& request) = 0;
    virtual void ReqCancelOrderRejected(const Message::TActionRequest& request) = 0;
    virtual void RepayMarginDirect(double value) = 0;
    virtual void TransferFundIn(double value) = 0;
    virtual void TransferFundOut(double value) = 0;
    virtual void UpdatePosition(const Message::TOrderStatus& OrderStatus, Message::TAccountPosition& Position) = 0;
    virtual void UpdateFund(const Message::TOrderStatus& OrderStatus, Message::TAccountFund& Fund) = 0;
public:
    void SetLogger(Utils::Logger* logger)
    {
        if(NULL == logger)
        {
            printf("Logger is NULL\n");
            exit(-1);
        }
        m_Logger = logger;
    }
    void SetTraderConfig(const Utils::XTraderConfig& config)
    {
        m_XTraderConfig = config;
        LoadAPIConfig();
        std::string errorString;
        bool ok = Utils::LoadTickerList(m_XTraderConfig.TickerListPath.c_str(), m_TickerPropertyList, errorString);
        if(ok)
        {
            m_Logger->Log->info("TradeGateWay Account:{} LoadTickerList {} successed", m_XTraderConfig.Account, m_XTraderConfig.TickerListPath);
            for (auto it = m_TickerPropertyList.begin(); it != m_TickerPropertyList.end(); it++)
            {
                m_TickerExchangeMap[it->Ticker] = it->ExchangeID;
            }
        }
        else
        {
            m_Logger->Log->warn("TradeGateWay Account:{} LoadTickerList {} failed, {}", m_XTraderConfig.Account, m_XTraderConfig.TickerListPath, errorString);
        }
    }

    void Qry()
    {
        int n = 0;
        while(n++ < 5)
        {
            if(Message::ELoginStatus::ELOGIN_SUCCESSED == m_ConnectedStatus)
            {
                ReqQryFund();
                sleep(2);
                ReqQryPoistion();
                sleep(2);
                ReqQryTrade();
                sleep(2);
                ReqQryOrder();
                sleep(2);
                ReqQryTickerRate();
                break;
            }
            else
            {
                sleep(1);
            }
        } 
    }

    void SendRequest(const Message::PackMessage& request)
    {
        // TODO: Send Request
        m_Logger->Log->info("TradeGateWay::SendRequest Account:{}", m_XTraderConfig.Account);
    }
public:
    Utils::RingBuffer<Message::PackMessage> m_ReportMessageQueue;
protected:
    Message::ELoginStatus m_ConnectedStatus;
    std::vector<Utils::TickerProperty> m_TickerPropertyList;
    Utils::XTraderConfig m_XTraderConfig;
    Utils::Logger* m_Logger;
    std::unordered_map<std::string, std::string> m_TickerExchangeMap;
};

#endif // TRADEGATEWAY_HPP