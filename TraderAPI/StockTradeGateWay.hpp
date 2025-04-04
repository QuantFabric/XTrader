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
        FMTLOG(fmtlog::DBG, "{}, PrintAccountPosition Product:{} Account:{} Ticker:{} ExchangeID:{} LongYdPosition:{} LongPosition:{} "
                            "LongTdBuy:{} LongTdSell:{} MarginYdPosition:{} MarginPosition:{} MarginTdBuy:{} MarginTdSell:{} "
                            "ShortYdPosition:{} ShortPosition:{} ShortTdSell:{} ShortTdBuy:{} ShortDirectRepaid:{} SpecialPositionAvl:{}",
                op, Position.Product, Position.Account, Position.Ticker, Position.ExchangeID, 
                Position.StockPosition.LongYdPosition, Position.StockPosition.LongPosition, Position.StockPosition.LongTdBuy,
                Position.StockPosition.LongTdSell, Position.StockPosition.MarginYdPosition, Position.StockPosition.MarginPosition,
                Position.StockPosition.MarginTdBuy, Position.StockPosition.ShortYdPosition, Position.StockPosition.ShortPosition, 
                Position.StockPosition.ShortTdSell, Position.StockPosition.ShortTdBuy, Position.StockPosition.ShortDirectRepaid, 
                Position.StockPosition.SpecialPositionAvl);
    }
};

#endif // STOCKTRADEGATEWAY_HPP