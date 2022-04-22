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
    m_Logger->Log->info("TestTradeGateWay Account:{} LoadAPIConfig {}", m_XTraderConfig.Account, m_XTraderConfig.TraderAPIConfig);
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
    m_Logger->Log->info("TestTradeGateWay Account:{} CreateTraderAPI", m_XTraderConfig.Account);
}

void TestTradeGateWay::DestroyTraderAPI()
{
    m_Logger->Log->info("TestTradeGateWay Account:{} DestroyTraderAPI", m_XTraderConfig.Account);
}

void TestTradeGateWay::ReqUserLogin()
{
    m_ConnectedStatus = Message::ELoginStatus::ELOGIN_SUCCESSED;
    m_Logger->Log->info("TestTradeGateWay Account:{} ReqUserLogin", m_XTraderConfig.Account);
}

void TestTradeGateWay::LoadTrader()
{
    m_Logger->Log->info("TestTradeGateWay Account:{} LoadTrader", m_XTraderConfig.Account);
    ReqUserLogin();
}

void TestTradeGateWay::ReLoadTrader()
{
    if(Message::ELoginStatus::ELOGIN_SUCCESSED != m_ConnectedStatus)
    {
        DestroyTraderAPI();
        CreateTraderAPI();
        LoadTrader();

        m_Logger->Log->info("TestTradeGateWay Account:{} ReLoadTrader", m_XTraderConfig.Account);
    }
}

int TestTradeGateWay::ReqQryFund()
{
    m_Logger->Log->info("TestTradeGateWay Account:{} ReqQryFund", m_XTraderConfig.Account);
    return 0;
}

int TestTradeGateWay::ReqQryPoistion()
{
    m_Logger->Log->info("TestTradeGateWay Account:{} ReqQryPoistion", m_XTraderConfig.Account);
    return 0;
}

int TestTradeGateWay::ReqQryTrade()
{
    m_Logger->Log->info("TestTradeGateWay Account:{} ReqQryTrade", m_XTraderConfig.Account);
    return 0;
}

int TestTradeGateWay::ReqQryOrder()
{
    m_Logger->Log->info("TestTradeGateWay Account:{} ReqQryOrder", m_XTraderConfig.Account);
    return 0;
}

int TestTradeGateWay::ReqQryTickerRate()
{
    m_Logger->Log->info("TestTradeGateWay Account:{} ReqQryTickerRate", m_XTraderConfig.Account);
    return 0;
}

void TestTradeGateWay::ReqInsertOrder(const Message::TOrderRequest& request)
{
    m_Logger->Log->info("TestTradeGateWay Account:{} ReqInsertOrder", m_XTraderConfig.Account);
}

void TestTradeGateWay::ReqInsertOrderRejected(const Message::TOrderRequest& request)
{
    m_Logger->Log->info("TestTradeGateWay Account:{} ReqInsertOrderRejected", m_XTraderConfig.Account);
}

void TestTradeGateWay::ReqCancelOrder(const Message::TActionRequest& request)
{
    m_Logger->Log->info("TestTradeGateWay Account:{} ReqCancelOrder", m_XTraderConfig.Account);
}

void TestTradeGateWay::ReqCancelOrderRejected(const Message::TActionRequest& request)
{
    m_Logger->Log->info("TestTradeGateWay Account:{} ReqCancelOrderRejected", m_XTraderConfig.Account);
}

void TestTradeGateWay::RepayMarginDirect(double value)
{
    m_Logger->Log->info("TestTradeGateWay Account:{} RepayMarginDirect", m_XTraderConfig.Account);
}

void TestTradeGateWay::TransferFundIn(double value)
{
    m_Logger->Log->info("TestTradeGateWay Account:{} TransferFundIn", m_XTraderConfig.Account);
}

void TestTradeGateWay::TransferFundOut(double value)
{
    m_Logger->Log->info("TestTradeGateWay Account:{} TransferFundOut", m_XTraderConfig.Account);
}

void TestTradeGateWay::UpdatePosition(const Message::TOrderStatus& OrderStatus, Message::TAccountPosition& Position)
{
    m_Logger->Log->info("TestTradeGateWay Account:{} UpdatePosition", m_XTraderConfig.Account);
}

void TestTradeGateWay::UpdateFund(const Message::TOrderStatus& OrderStatus, Message::TAccountFund& Fund)
{
    m_Logger->Log->info("TestTradeGateWay Account:{} UpdateFund", m_XTraderConfig.Account);
}