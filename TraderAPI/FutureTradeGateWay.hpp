#ifndef FUTURETRADEGATEWAY_HPP
#define FUTURETRADEGATEWAY_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "TradeGateWay.hpp"
#include "PackMessage.hpp"


class FutureTradeGateWay : public TradeGateWay
{
public:
    virtual void RepayMarginDirect(double value)
    {
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EERROR;
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.App, "FutureTrader", sizeof(message.EventLog.App));
        strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event), 
                        "FutureTradeGateWay::RepayMarginDirect Account:{} invalid Command", m_XTraderConfig.Account);
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));
        FMTLOG(fmtlog::ERR, "FutureTradeGateWay::RepayMarginDirect Account:{} invalid Command", m_XTraderConfig.Account);
    }

    virtual void TransferFundIn(double value)
    {
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EERROR;
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.App, "FutureTrader", sizeof(message.EventLog.App));
        strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event), 
                        "FutureTradeGateWay::TransferFundIn Account:{} invalid Command", m_XTraderConfig.Account);
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));
        FMTLOG(fmtlog::ERR, "FutureTradeGateWay::TransferFundIn Account:{} invalid Command", m_XTraderConfig.Account);
    }

    virtual void TransferFundOut(double value)
    {
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EERROR;
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.App, "FutureTrader", sizeof(message.EventLog.App));
        strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event), 
                        "FutureTradeGateWay::TransferFundOut Account:{} invalid Command", m_XTraderConfig.Account);
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));
        FMTLOG(fmtlog::ERR, "FutureTradeGateWay::TransferFundOut Account:{} invalid Command", m_XTraderConfig.Account);
    }

    virtual void UpdatePosition(const Message::TOrderStatus& OrderStatus, Message::TAccountPosition& position)
    {
        // Position Update
        switch (OrderStatus.OrderSide)
        {
        case Message::EOrderSide::EOPEN_LONG:
            if(Message::EOrderStatusType::EPARTTRADED == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EALLTRADED == OrderStatus.OrderStatus)
            {
                position.FuturePosition.LongOpenVolume += OrderStatus.TradedVolume;
                position.FuturePosition.LongTdVolume += OrderStatus.TradedVolume;
                if(!m_XTraderConfig.CloseToday && position.FuturePosition.LongOpenVolume > 0)
                {
                    position.FuturePosition.LongTdVolume += position.FuturePosition.LongYdVolume;
                    position.FuturePosition.LongYdVolume = 0;
                }
                position.FuturePosition.LongOpeningVolume -= OrderStatus.TradedVolume;  
            }
            else if(Message::EOrderStatusType::EORDER_SENDED == OrderStatus.OrderStatus)
            {
                position.FuturePosition.LongOpeningVolume += OrderStatus.SendVolume;
            }
            else if(Message::EOrderStatusType::ECANCELLED == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EPARTTRADED_CANCELLED == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EBROKER_ERROR == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EEXCHANGE_ERROR == OrderStatus.OrderStatus)
            {
                position.FuturePosition.LongOpeningVolume -= OrderStatus.CanceledVolume;
            }
            break;
        case Message::EOrderSide::EOPEN_SHORT:
            if(Message::EOrderStatusType::EPARTTRADED == OrderStatus.OrderStatus  ||
                    Message::EOrderStatusType::EALLTRADED == OrderStatus.OrderStatus)
            {
                position.FuturePosition.ShortOpenVolume += OrderStatus.TradedVolume;
                position.FuturePosition.ShortTdVolume += OrderStatus.TradedVolume;
                if(!m_XTraderConfig.CloseToday && position.FuturePosition.ShortOpenVolume > 0)
                {
                    position.FuturePosition.ShortTdVolume += position.FuturePosition.ShortYdVolume;
                    position.FuturePosition.ShortYdVolume = 0;
                }
                position.FuturePosition.ShortOpeningVolume -= OrderStatus.TradedVolume;
            }
            else if(Message::EOrderStatusType::EORDER_SENDED == OrderStatus.OrderStatus)
            {
                position.FuturePosition.ShortOpeningVolume += OrderStatus.SendVolume;
            }
            else if(Message::EOrderStatusType::ECANCELLED == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EPARTTRADED_CANCELLED == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EBROKER_ERROR == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EEXCHANGE_ERROR == OrderStatus.OrderStatus)
            {
                position.FuturePosition.ShortOpeningVolume -= OrderStatus.CanceledVolume;
            }
            break;
        case Message::EOrderSide::ECLOSE_TD_LONG:
            if(Message::EOrderStatusType::EPARTTRADED == OrderStatus.OrderStatus  ||
                    Message::EOrderStatusType::EALLTRADED == OrderStatus.OrderStatus)
            {
                position.FuturePosition.LongTdVolume -= OrderStatus.TradedVolume;
                position.FuturePosition.LongClosingTdVolume -= OrderStatus.TradedVolume;
            }
            else if(Message::EOrderStatusType::EORDER_SENDED == OrderStatus.OrderStatus)
            {
                position.FuturePosition.LongClosingTdVolume += OrderStatus.SendVolume;
            }
            else if(Message::EOrderStatusType::ECANCELLED == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EPARTTRADED_CANCELLED == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EBROKER_ERROR == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EEXCHANGE_ERROR == OrderStatus.OrderStatus)
            {
                position.FuturePosition.LongClosingTdVolume -= OrderStatus.CanceledVolume;
            }
            break;
        case Message::EOrderSide::ECLOSE_TD_SHORT:
            if(Message::EOrderStatusType::EPARTTRADED == OrderStatus.OrderStatus  ||
                    Message::EOrderStatusType::EALLTRADED == OrderStatus.OrderStatus)
            {
                position.FuturePosition.ShortTdVolume -= OrderStatus.TradedVolume;
                position.FuturePosition.ShortClosingTdVolume -= OrderStatus.TradedVolume;
            }
            else if(Message::EOrderStatusType::EORDER_SENDED == OrderStatus.OrderStatus)
            {
                position.FuturePosition.ShortClosingTdVolume += OrderStatus.SendVolume;
            }
            else if(Message::EOrderStatusType::ECANCELLED == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EPARTTRADED_CANCELLED == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EBROKER_ERROR == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EEXCHANGE_ERROR == OrderStatus.OrderStatus)
            {
                position.FuturePosition.ShortClosingTdVolume -= OrderStatus.CanceledVolume;
            }
            break;
        case Message::EOrderSide::ECLOSE_YD_LONG:
            if(Message::EOrderStatusType::EPARTTRADED == OrderStatus.OrderStatus  ||
                    Message::EOrderStatusType::EALLTRADED == OrderStatus.OrderStatus)
            {
                position.FuturePosition.LongYdVolume -= OrderStatus.TradedVolume;
                position.FuturePosition.LongClosingYdVolume -= OrderStatus.TradedVolume;
            }
            else if(Message::EOrderStatusType::EORDER_SENDED == OrderStatus.OrderStatus)
            {
                position.FuturePosition.LongClosingYdVolume += OrderStatus.SendVolume;
            }
            else if(Message::EOrderStatusType::ECANCELLED == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EPARTTRADED_CANCELLED == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EBROKER_ERROR == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EEXCHANGE_ERROR == OrderStatus.OrderStatus)
            {
                position.FuturePosition.LongClosingYdVolume -= OrderStatus.CanceledVolume;
            }
            break;
        case Message::EOrderSide::ECLOSE_YD_SHORT:
            if(Message::EOrderStatusType::EPARTTRADED == OrderStatus.OrderStatus  ||
                    Message::EOrderStatusType::EALLTRADED == OrderStatus.OrderStatus)
            {
                position.FuturePosition.ShortYdVolume -= OrderStatus.TradedVolume;
                position.FuturePosition.ShortClosingYdVolume -= OrderStatus.TradedVolume;
            }
            else if(Message::EOrderStatusType::EORDER_SENDED == OrderStatus.OrderStatus)
            {
                position.FuturePosition.ShortClosingYdVolume += OrderStatus.SendVolume;
            }
            else if(Message::EOrderStatusType::ECANCELLED == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EPARTTRADED_CANCELLED == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EBROKER_ERROR == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EEXCHANGE_ERROR == OrderStatus.OrderStatus)
            {
                position.FuturePosition.ShortClosingYdVolume -= OrderStatus.CanceledVolume;
            }
            break;
        default:
            break;
        }
        strncpy(position.UpdateTime, Utils::getCurrentTimeUs(), sizeof(position.UpdateTime));
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EAccountPosition;
        memcpy(&message.AccountPosition, &position, sizeof(message.AccountPosition));
        while(!m_ReportMessageQueue.Push(message));
    }

    virtual void UpdateFrozenPosition(const Message::TOrderStatus& OrderStatus, Message::TAccountPosition& position)
    {
        // Position Update
        switch (OrderStatus.OrderSide)
        {
        case Message::EOrderSide::EOPEN_LONG:
            if(Message::EOrderStatusType::EPARTTRADED == OrderStatus.OrderStatus)
            {
                position.FuturePosition.LongOpeningVolume += (OrderStatus.SendVolume - OrderStatus.TotalTradedVolume);
            }
            else if(Message::EOrderStatusType::EEXCHANGE_ACK == OrderStatus.OrderStatus)
            {
                position.FuturePosition.LongOpeningVolume += OrderStatus.SendVolume;
            }
            break;
        case Message::EOrderSide::EOPEN_SHORT:
            if(Message::EOrderStatusType::EPARTTRADED == OrderStatus.OrderStatus)
            {
                position.FuturePosition.ShortOpeningVolume += (OrderStatus.SendVolume - OrderStatus.TotalTradedVolume);
            }
            else if(Message::EOrderStatusType::EEXCHANGE_ACK == OrderStatus.OrderStatus)
            {
                position.FuturePosition.ShortOpeningVolume += OrderStatus.SendVolume;
            }
            break;
        case Message::EOrderSide::ECLOSE_TD_LONG:
            if(Message::EOrderStatusType::EPARTTRADED == OrderStatus.OrderStatus)
            {
                position.FuturePosition.LongClosingTdVolume += (OrderStatus.SendVolume - OrderStatus.TotalTradedVolume);
            }
            else if(Message::EOrderStatusType::EEXCHANGE_ACK == OrderStatus.OrderStatus)
            {
                position.FuturePosition.LongClosingTdVolume += OrderStatus.SendVolume;
            }
            break;
        case Message::EOrderSide::ECLOSE_TD_SHORT:
            if(Message::EOrderStatusType::EPARTTRADED == OrderStatus.OrderStatus)
            {
                position.FuturePosition.ShortClosingTdVolume += (OrderStatus.SendVolume - OrderStatus.TotalTradedVolume);
            }
            else if(Message::EOrderStatusType::EEXCHANGE_ACK == OrderStatus.OrderStatus)
            {
                position.FuturePosition.ShortClosingTdVolume += OrderStatus.SendVolume;
            }
            break;
        case Message::EOrderSide::ECLOSE_YD_LONG:
            if(Message::EOrderStatusType::EPARTTRADED == OrderStatus.OrderStatus)
            {
                position.FuturePosition.LongClosingYdVolume += (OrderStatus.SendVolume - OrderStatus.TotalTradedVolume);
            }
            else if(Message::EOrderStatusType::EEXCHANGE_ACK == OrderStatus.OrderStatus)
            {
                position.FuturePosition.LongClosingYdVolume += OrderStatus.SendVolume;
            }
            break;
        case Message::EOrderSide::ECLOSE_YD_SHORT:
            if(Message::EOrderStatusType::EPARTTRADED == OrderStatus.OrderStatus)
            {
                position.FuturePosition.ShortClosingYdVolume += (OrderStatus.SendVolume - OrderStatus.TotalTradedVolume);
            }
            else if(Message::EOrderStatusType::EEXCHANGE_ACK == OrderStatus.OrderStatus)
            {
                position.FuturePosition.ShortClosingYdVolume += OrderStatus.SendVolume;
            }
            break;
        default:
            break;
        }
        strncpy(position.UpdateTime, Utils::getCurrentTimeUs(), sizeof(position.UpdateTime));
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EAccountPosition;
        memcpy(&message.AccountPosition, &position, sizeof(message.AccountPosition));
        while(!m_ReportMessageQueue.Push(message));
    }

    virtual void InitPosition()
    {
        for(auto it = m_TickerPropertyList.begin(); it != m_TickerPropertyList.end(); it++)
        {
            std::string Key = m_XTraderConfig.Account + ":" + it->Ticker;
            Message::TAccountPosition& AccountPosition = m_TickerAccountPositionMap[Key];
            AccountPosition.BusinessType = m_XTraderConfig.BusinessType;
            strncpy(AccountPosition.Product,  m_XTraderConfig.Product.c_str(), sizeof(AccountPosition.Product));
            strncpy(AccountPosition.Broker,  m_XTraderConfig.Broker.c_str(), sizeof(AccountPosition.Broker));
            strncpy(AccountPosition.Account, m_XTraderConfig.Account.c_str(), sizeof(AccountPosition.Account));
            strncpy(AccountPosition.Ticker, it->Ticker.c_str(), sizeof(AccountPosition.Ticker));
            strncpy(AccountPosition.ExchangeID, it->ExchangeID.c_str(), sizeof(AccountPosition.ExchangeID));
        }
        FMTLOG(fmtlog::INF, "FutureTradeGateWay::InitPosition Account:{} Tickers:{}", m_XTraderConfig.Account, m_TickerPropertyList.size());
    }
        
    virtual void UpdateFund(const Message::TOrderStatus& OrderStatus, Message::TAccountFund& fund)
    {

    }

    void PrintAccountPosition(const Message::TAccountPosition& Position, const std::string& op)
    {
        FMTLOG(fmtlog::DBG, "{}, PrintAccountPosition Product:{} Account:{} Ticker:{} ExchangeID:{} LongTdVolume:{} LongYdVolume:{} "
                            "LongOpenVolume:{} LongOpeningVolume:{} LongClosingTdVolume:{} LongClosingYdVolume:{} ShortTdVolume:{} "
                            "ShortYdVolume:{} ShortOpenVolume:{} ShortOpeningVolume:{} ShortClosingTdVolume:{} ShortClosingYdVolume:{} UpdateTime:{}",
                op, Position.Product, Position.Account, Position.Ticker, Position.ExchangeID, 
                Position.FuturePosition.LongTdVolume, Position.FuturePosition.LongYdVolume, Position.FuturePosition.LongOpenVolume,
                Position.FuturePosition.LongOpeningVolume, Position.FuturePosition.LongClosingTdVolume, Position.FuturePosition.LongClosingYdVolume,
                Position.FuturePosition.ShortTdVolume, Position.FuturePosition.ShortYdVolume, Position.FuturePosition.ShortOpenVolume,
                Position.FuturePosition.ShortOpeningVolume, Position.FuturePosition.ShortClosingTdVolume, Position.FuturePosition.ShortClosingYdVolume,
                Position.UpdateTime);
    }
};

#endif // FUTURETRADEGATEWAY_HPP