#include "TestTradeGateWay.h"
#include "XPluginEngine.hpp"

CreateObjectFunc(TestTradeGateWay);

TestTradeGateWay::TestTradeGateWay()
{
    m_ConnectedStatus = Message::ELoginStatus::ELOGIN_PREPARED;
}

TestTradeGateWay::~TestTradeGateWay()
{
    DestroyTraderAPI();
}

void TestTradeGateWay::LoadAPIConfig()
{
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} LoadAPIConfig {}", m_XTraderConfig.Account, m_XTraderConfig.TraderAPIConfig);
}

void TestTradeGateWay::GetCommitID(std::string& CommitID, std::string& UtilsCommitID)
{
    CommitID = SO_COMMITID;
    UtilsCommitID = SO_UTILS_COMMITID;
}
void TestTradeGateWay::GetAPIVersion(std::string& APIVersion)
{
    APIVersion = API_VERSION;
}

void TestTradeGateWay::CreateTraderAPI()
{
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} CreateTraderAPI", m_XTraderConfig.Account);
}

void TestTradeGateWay::DestroyTraderAPI()
{
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} DestroyTraderAPI", m_XTraderConfig.Account);
}

void TestTradeGateWay::ReqUserLogin()
{
    m_ConnectedStatus = Message::ELoginStatus::ELOGIN_SUCCESSED;
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} ReqUserLogin", m_XTraderConfig.Account);
}

void TestTradeGateWay::LoadTrader()
{
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} LoadTrader", m_XTraderConfig.Account);
    ReqUserLogin();
}

void TestTradeGateWay::ReLoadTrader()
{
    if(Message::ELoginStatus::ELOGIN_SUCCESSED != m_ConnectedStatus)
    {
        DestroyTraderAPI();
        CreateTraderAPI();
        LoadTrader();

        FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} ReLoadTrader", m_XTraderConfig.Account);
    }
}

int TestTradeGateWay::ReqQryFund()
{
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} ReqQryFund", m_XTraderConfig.Account);

    Message::TAccountFund& AccountFund = m_AccountFundMap[m_XTraderConfig.Account];
    AccountFund.BusinessType = m_XTraderConfig.BusinessType;
    strncpy(AccountFund.Product, m_XTraderConfig.Product.c_str(), sizeof(AccountFund.Product));
    strncpy(AccountFund.Broker, m_XTraderConfig.Broker.c_str(), sizeof(AccountFund.Broker));
    strncpy(AccountFund.Account, m_XTraderConfig.Account.c_str(), sizeof(AccountFund.Account));
    AccountFund.Deposit = 0;
    AccountFund.Withdraw = 0;
    AccountFund.CurrMargin = 0;
    AccountFund.Commission = 0;
    AccountFund.CloseProfit = 0;
    AccountFund.PositionProfit = 0;
    AccountFund.Available = 1000000;
    AccountFund.WithdrawQuota = 0;
    AccountFund.ExchangeMargin = 0;
    AccountFund.Balance = 1000000;
    AccountFund.PreBalance = 1000000;
    strncpy(AccountFund.UpdateTime, Utils::getCurrentTimeUs(), sizeof(AccountFund.UpdateTime));

    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EAccountFund;
    memcpy(&message.AccountFund, &AccountFund, sizeof(AccountFund));
    while(!m_ReportMessageQueue.Push(message));
    return 0;
}

int TestTradeGateWay::ReqQryPoistion()
{
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} ReqQryPoistion", m_XTraderConfig.Account);
    return 0;
}

int TestTradeGateWay::ReqQryTrade()
{
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} ReqQryTrade", m_XTraderConfig.Account);
    return 0;
}

int TestTradeGateWay::ReqQryOrder()
{
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} ReqQryOrder", m_XTraderConfig.Account);
    return 0;
}

int TestTradeGateWay::ReqQryTickerRate()
{
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} ReqQryTickerRate", m_XTraderConfig.Account);
    return 0;
}

void TestTradeGateWay::ReqInsertOrder(const Message::TOrderRequest& request)
{
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} ReqInsertOrder", m_XTraderConfig.Account);
}

void TestTradeGateWay::ReqInsertOrderRejected(const Message::TOrderRequest& request)
{
    int orderID = (uint64_t(Utils::getTimeSec() + 8 * 3600 - 17 * 3600) % 86400) * 10000;
    // Order Status
    Message::TOrderStatus OrderStatus;
    memset(&OrderStatus, 0, sizeof(OrderStatus));
    OrderStatus.BusinessType = m_XTraderConfig.BusinessType;
    strncpy(OrderStatus.Product, m_XTraderConfig.Product.c_str(), sizeof(OrderStatus.Product));
    strncpy(OrderStatus.Broker, m_XTraderConfig.Broker.c_str(), sizeof(OrderStatus.Broker));
    strncpy(OrderStatus.Account, m_XTraderConfig.Account.c_str(), sizeof(OrderStatus.Account));
    strncpy(OrderStatus.ExchangeID, request.ExchangeID, sizeof(OrderStatus.ExchangeID));
    strncpy(OrderStatus.Ticker, request.Ticker, sizeof(OrderStatus.Ticker));
    fmt::format_to_n(OrderStatus.OrderRef, sizeof(OrderStatus.OrderRef), "{:09d}", orderID);
    strncpy(OrderStatus.RiskID, request.RiskID, sizeof(OrderStatus.RiskID));
    OrderStatus.SendPrice = request.Price;
    OrderStatus.SendVolume = request.Volume;
    OrderStatus.OrderType = request.OrderType;
    OrderStatus.OrderToken = request.OrderToken;
    OrderStatus.EngineID = request.EngineID;
    strncpy(OrderStatus.SendTime, request.SendTime, sizeof(OrderStatus.SendTime));
    strncpy(OrderStatus.InsertTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.InsertTime));
    strncpy(OrderStatus.RecvMarketTime, request.RecvMarketTime, sizeof(OrderStatus.RecvMarketTime));
    if(Message::ERiskStatusType::ECHECK_INIT == request.RiskStatus)
    {
        OrderStatus.OrderStatus = Message::EOrderStatusType::ERISK_CHECK_INIT;
    }
    else
    {
        OrderStatus.OrderStatus = Message::EOrderStatusType::ERISK_ORDER_REJECTED;
    }
    OrderStatus.ErrorID = request.ErrorID;
    strncpy(OrderStatus.ErrorMsg, request.ErrorMsg, sizeof(OrderStatus.ErrorMsg));
    PrintOrderStatus(OrderStatus, "TestTrader::ReqInsertOrderRejected ");
    UpdateOrderStatus(OrderStatus);
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} ReqInsertOrderRejected", m_XTraderConfig.Account);
}

void TestTradeGateWay::ReqCancelOrder(const Message::TActionRequest& request)
{
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} ReqCancelOrder", m_XTraderConfig.Account);
}

void TestTradeGateWay::ReqCancelOrderRejected(const Message::TActionRequest& request)
{
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} ReqCancelOrderRejected", m_XTraderConfig.Account);
}

void TestTradeGateWay::RepayMarginDirect(double value)
{
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} RepayMarginDirect", m_XTraderConfig.Account);
}

void TestTradeGateWay::TransferFundIn(double value)
{
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} TransferFundIn", m_XTraderConfig.Account);
}

void TestTradeGateWay::TransferFundOut(double value)
{
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} TransferFundOut", m_XTraderConfig.Account);
}

void TestTradeGateWay::UpdatePosition(const Message::TOrderStatus& OrderStatus, Message::TAccountPosition& Position)
{
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} UpdatePosition", m_XTraderConfig.Account);
}

void TestTradeGateWay::UpdateFund(const Message::TOrderStatus& OrderStatus, Message::TAccountFund& Fund)
{
    FMTLOG(fmtlog::INF, "TestTradeGateWay Account:{} UpdateFund", m_XTraderConfig.Account);
}