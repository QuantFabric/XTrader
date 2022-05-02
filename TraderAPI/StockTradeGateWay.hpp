#ifndef STOCKTRADEGATEWAY_HPP
#define STOCKTRADEGATEWAY_HPP

#include <string>
#include <unordered_map>
#include "TradeGateWay.hpp"
#include "PackMessage.hpp"


class StockTradeGateWay : public TradeGateWay
{
public:
    virtual void UpdatePosition(const Message::TOrderStatus& OrderStatus, Message::TAccountPosition& Position)
    {
        // Position Update
    }

    virtual void UpdateFrozenPosition(const Message::TOrderStatus& OrderStatus, Message::TAccountPosition& Position)
    {
        // nothing
    }
    
    virtual void UpdateFund(const Message::TOrderStatus& OrderStatus, Message::TAccountFund& Fund)
    {
        // nothing
    }

    void PrintAccountPosition(const Message::TAccountPosition& Position, const std::string& op)
    {
        m_Logger->Log->debug("{}, PrintAccountPosition Product:{} Account:{}\n"
                    "\t\t\t\t\t\tTicker:{} ExchangeID:{} LongYdPosition:{} LongPosition:{}\n"
                    "\t\t\t\t\t\tLongTdBuy:{} LongTdSell:{} LongTdSell4RePay:{} ShortYdPosition:{}\n"
                    "\t\t\t\t\t\tShortPosition:{} TdBuy4RePay:{} TdDirectRepay:{} MarginPosition:{}",
                    op, Position.Product, Position.Account, Position.Ticker, Position.ExchangeID, 
                    Position.StockPosition.LongYdPosition, Position.StockPosition.LongPosition, Position.StockPosition.LongTdBuy,
                    Position.StockPosition.LongTdSell, Position.StockPosition.LongTdSell4RePay, Position.StockPosition.ShortYdPosition,
                    Position.StockPosition.ShortPosition, Position.StockPosition.TdBuy4RePay, Position.StockPosition.TdDirectRepay,
                    Position.StockPosition.MarginPosition);
    }
};

#endif // STOCKTRADEGATEWAY_HPP