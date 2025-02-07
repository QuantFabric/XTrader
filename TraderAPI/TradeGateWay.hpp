#ifndef TRADEGATEWAY_HPP
#define TRADEGATEWAY_HPP

#include <string>
#include "LockFreeQueue.hpp"
#include "PackMessage.hpp"
#include "FMTLogger.hpp"
#include "YMLConfig.hpp"
#include "phmap.h"
#include <shared_mutex>

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
    void SetTraderConfig(const Utils::XTraderConfig& config)
    {
        m_XTraderConfig = config;
        LoadAPIConfig();
        std::string errorString;
        bool ok = Utils::LoadTickerList(m_XTraderConfig.TickerListPath.c_str(), m_TickerPropertyList, errorString);
        if(ok)
        {
            FMTLOG(fmtlog::INF, "TradeGateWay Account:{} LoadTickerList {} successed", m_XTraderConfig.Account, m_XTraderConfig.TickerListPath);
            for (auto it = m_TickerPropertyList.begin(); it != m_TickerPropertyList.end(); it++)
            {
                m_TickerExchangeMap[it->Ticker] = it->ExchangeID;
            }
        }
        else
        {
            FMTLOG(fmtlog::WRN, "TradeGateWay Account:{} LoadTickerList {} failed, {}", m_XTraderConfig.Account, m_XTraderConfig.TickerListPath, errorString);
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
                // sleep(2);
                // ReqQryTickerRate();
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
        switch(request.MessageType)
        {
            CheckOrderRequest(request.OrderRequest);
            case Message::EMessageType::EOrderRequest:
            {
                if(request.OrderRequest.RiskStatus == Message::ERiskStatusType::ENOCHECKED)
                {
                    ReqInsertOrder(request.OrderRequest);
                }
                else if(request.OrderRequest.RiskStatus == Message::ERiskStatusType::ECHECKED_PASS)
                {
                    ReqInsertOrder(request.OrderRequest);
                }
                // 处理风控拒单
                else if(request.OrderRequest.RiskStatus == Message::ERiskStatusType::ECHECKED_NOPASS)
                {
                    ReqInsertOrderRejected(request.OrderRequest);
                }
                else if(request.OrderRequest.RiskStatus == Message::ERiskStatusType::ECHECK_INIT)
                {
                    Message::PackMessage message;
                    memcpy(&message, &request, sizeof(message));
                    if(m_TickerPropertyList.size() > 0)
                    {
                        std::string Ticker = m_TickerPropertyList.at(0).Ticker;
                        strncpy(message.OrderRequest.Ticker, Ticker.c_str(), sizeof(message.OrderRequest.Ticker));
                        strncpy(message.OrderRequest.ExchangeID, m_TickerExchangeMap[Ticker].c_str(), sizeof(message.OrderRequest.ExchangeID));
                    }
                    if(Message::ELoginStatus::ELOGIN_SUCCESSED == m_ConnectedStatus)
                    {
                        message.OrderRequest.ErrorID = 0;
                        strncpy(message.OrderRequest.ErrorMsg, "API Connect Successed", sizeof(message.OrderRequest.ErrorMsg));
                    }
                    else
                    {
                        message.OrderRequest.ErrorID = -1;
                        strncpy(message.OrderRequest.ErrorMsg, "API Connect failed", sizeof(message.OrderRequest.ErrorMsg));
                    }
                    FMTLOG(fmtlog::INF, "TradeGateWay::SendRequest Account:{} Ticker:{} Init Risk Check", message.OrderRequest.Account, message.OrderRequest.Ticker);
                    ReqInsertOrderRejected(message.OrderRequest);
                }
                break;
            }
            case Message::EMessageType::EActionRequest:
            {
                if(request.ActionRequest.RiskStatus == Message::ERiskStatusType::ENOCHECKED)
                {
                    ReqCancelOrder(request.ActionRequest);
                }
                else if(request.ActionRequest.RiskStatus == Message::ERiskStatusType::ECHECKED_PASS)
                {
                    ReqCancelOrder(request.ActionRequest);
                }
                // 处理风控拒单
                else if(request.ActionRequest.RiskStatus == Message::ERiskStatusType::ECHECKED_NOPASS)
                {
                    ReqCancelOrderRejected(request.ActionRequest);
                }
                break;
            }
            default:
            {
                FMTLOG(fmtlog::WRN, "TradeGateWay::SendRequest Unkown Message Type:{:#X}", request.MessageType);
                break;
            }
        }
    }
protected:
    void CheckOrderRequest(const Message::TOrderRequest& req)
    {
        if(0 == req.Offset)
        {
            Message::TOrderRequest& request = const_cast<Message::TOrderRequest&>(req);
            
            std::string Key = std::string(request.Account) + ":" + request.Ticker;
            Message::TAccountPosition& AccountPosition = m_TickerAccountPositionMap[Key];
            if(Message::EOrderDirection::EBUY == request.Direction)
            {
                if(AccountPosition.FuturePosition.ShortYdVolume - AccountPosition.FuturePosition.ShortClosingYdVolume >= request.Volume)
                {
                    request.Offset = Message::EOrderOffset::ECLOSE_YESTODAY;
                }
                else if(m_XTraderConfig.CloseToday && AccountPosition.FuturePosition.ShortTdVolume - AccountPosition.FuturePosition.ShortClosingTdVolume >= request.Volume)
                {
                    request.Offset = Message::EOrderOffset::ECLOSE_TODAY;
                }
                else
                {
                    request.Offset = Message::EOrderOffset::EOPEN;
                }
            }
            else if(Message::EOrderDirection::ESELL == request.Direction)
            {
                if(AccountPosition.FuturePosition.LongYdVolume - AccountPosition.FuturePosition.LongClosingYdVolume >= request.Volume)
                {
                    request.Offset = Message::EOrderOffset::ECLOSE_YESTODAY;
                }
                else if(m_XTraderConfig.CloseToday && AccountPosition.FuturePosition.LongTdVolume - AccountPosition.FuturePosition.LongClosingTdVolume >= request.Volume)
                {
                    request.Offset = Message::EOrderOffset::ECLOSE_TODAY;
                }
                else
                {
                    request.Offset = Message::EOrderOffset::EOPEN;
                }
            }
        }
    }

    void UpdateOrderStatus(Message::TOrderStatus& OrderStatus)
    {
        strncpy(OrderStatus.UpdateTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.UpdateTime));
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EOrderStatus;
        memcpy(&message.OrderStatus, &OrderStatus, sizeof(message.OrderStatus));
        while(!m_ReportMessageQueue.Push(message));
    }

    void CancelOrder(const Message::TOrderStatus& OrderStatus)
    {
        Message::TActionRequest ActionRequest;
        memset(&ActionRequest, 0, sizeof (ActionRequest));
        ActionRequest.RiskStatus = Message::ERiskStatusType::ENOCHECKED;
        strncpy(ActionRequest.Account, OrderStatus.Account, sizeof(ActionRequest.Account));
        strncpy(ActionRequest.OrderRef, OrderStatus.OrderRef, sizeof(ActionRequest.OrderRef));

        ReqCancelOrder(ActionRequest);
    }
    
    virtual void OnExchangeACK(const Message::TOrderStatus& OrderStatus)
    {
        long send = Utils::getTimeStampUs(OrderStatus.SendTime + 11);
        long insert = Utils::getTimeStampUs(OrderStatus.InsertTime + 11);
        long broker = Utils::getTimeStampUs(OrderStatus.BrokerACKTime + 11);
        long end = Utils::getTimeStampUs(OrderStatus.ExchangeACKTime + 11);
        FMTLOG(fmtlog::INF, "TraderGateWay::OnExchangeACK OrderRef:{}, OrderLocalID:{}, TraderLatency:{}, BrokerLatency:{}, ExchangeLatency:{}",
                OrderStatus.OrderRef, OrderStatus.OrderLocalID, insert - send, broker - insert, end - insert);
    }

    void PrintOrderStatus(const Message::TOrderStatus& OrderStatus, const std::string& op)
    {
        FMTLOG(fmtlog::DBG, "{}, PrintOrderStatus Product:{} Broker:{} Account:{} ExchangeID:{} Ticker:{} OrderRef:{} OrderSysID:{} "
                            "OrderLocalID:{} OrderToken:{} OrderSide:{} SendPrice:{} SendVolume:{} OrderType:{} TotalTradedVolume:{} "
                            "TradedAvgPrice:{} TradedVolume:{} TradedPrice:{} OrderStatus:{} CanceledVolume:{} RecvMarketTime:{} SendTime:{} "
                            "InsertTime:{} BrokerACKTime:{} ExchangeACKTime:{} UpdateTime:{} ErrorID:{} ErrorMsg:{} RiskID:{}",
                op, OrderStatus.Product, OrderStatus.Broker, OrderStatus.Account,
                OrderStatus.ExchangeID, OrderStatus.Ticker, OrderStatus.OrderRef,
                OrderStatus.OrderSysID, OrderStatus.OrderLocalID, OrderStatus.OrderToken, OrderStatus.OrderSide, OrderStatus.SendPrice,
                OrderStatus.SendVolume, OrderStatus.OrderType, OrderStatus.TotalTradedVolume,
                OrderStatus.TradedAvgPrice, OrderStatus.TradedVolume, OrderStatus.TradedPrice,
                OrderStatus.OrderStatus, OrderStatus.CanceledVolume, OrderStatus.RecvMarketTime, OrderStatus.SendTime, OrderStatus.InsertTime,
                OrderStatus.BrokerACKTime, OrderStatus.ExchangeACKTime, OrderStatus.UpdateTime,
                OrderStatus.ErrorID, OrderStatus.ErrorMsg, OrderStatus.RiskID);
    }
public:
    Utils::LockFreeQueue<Message::PackMessage> m_ReportMessageQueue;
protected:
    Message::ELoginStatus m_ConnectedStatus;
    std::vector<Utils::TickerProperty> m_TickerPropertyList;
    Utils::XTraderConfig m_XTraderConfig;
    std::unordered_map<std::string, std::string> m_TickerExchangeMap;

    typedef phmap::parallel_node_hash_map<std::string, Message::TAccountPosition, phmap::priv::hash_default_hash<std::string>,
                                     phmap::priv::hash_default_eq<std::string>,
                                     std::allocator<std::pair<const std::string, Message::TAccountPosition>>, 12, std::shared_mutex>
    AccountPositionMapT;
    AccountPositionMapT m_TickerAccountPositionMap;
    std::unordered_map<std::string, Message::TAccountFund> m_AccountFundMap;
    typedef phmap::parallel_node_hash_map<std::string, Message::TOrderStatus, phmap::priv::hash_default_hash<std::string>,
                                     phmap::priv::hash_default_eq<std::string>,
                                     std::allocator<std::pair<const std::string, Message::TOrderStatus>>, 10, std::shared_mutex>
    OrderStatusMapT;
    OrderStatusMapT m_OrderStatusMap;
    std::unordered_map<int, std::string> m_CodeErrorMap;
};

#endif // TRADEGATEWAY_HPP