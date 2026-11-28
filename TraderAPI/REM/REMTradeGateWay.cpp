#include "REMTradeGateWay.h"
#include "XPluginEngine.hpp"

CreateObjectFunc(REMTradeGateWay);

REMTradeGateWay::REMTradeGateWay()
{
    m_ConnectedStatus = Message::ELoginStatus::ELOGIN_PREPARED;
    m_RequestID = 0;
    m_TraderAPI = NULL;
    m_CreateTraderAPIFunc = NULL;
    m_DestroyFunc = NULL;
    m_TraderHandle = NULL;
}

REMTradeGateWay::~REMTradeGateWay()
{
    if (NULL != m_TraderAPI)
    {
        m_TraderAPI->DisConnServer();
    }
    UnloadEESTrader();
}

void REMTradeGateWay::LoadAPIConfig()
{
    std::string errorString;
    bool ok = Utils::LoadREMConfig(m_XTraderConfig.TraderAPIConfig.c_str(), m_REMConfig, errorString);
    if(ok)
    {
        m_APISoPath = m_REMConfig.EESTraderLibPath + "/" + EES_TRADER_DLL_NAME;
        FMTLOG(fmtlog::INF, "REMTrader::LoadAPIConfig InvestorID:{} LoadREMConfig successed", m_XTraderConfig.Account);
        FMTLOG(fmtlog::INF, "LoadREMConfig TradeServerIP:{} TradeServerPort:{} TradeServerUDPPort:{}", 
                            m_REMConfig.TradeServerIP, m_REMConfig.TradeServerPort, m_REMConfig.TradeServerUDPPort);
        FMTLOG(fmtlog::INF, "LoadREMConfig QueryServerIP:{} QueryServerPort:{} QuoteServerIP:{} QuoteServerPort:{}", 
                            m_REMConfig.QueryServerIP, m_REMConfig.QueryServerPort, m_REMConfig.QuoteServerIP, m_REMConfig.QuoteServerPort);
        FMTLOG(fmtlog::INF, "LoadREMConfig LocalTradeIP:{} LocalTradeUDPPort:{} EESTraderLibPath:{}", 
                            m_REMConfig.LocalTradeIP, m_REMConfig.LocalTradeUDPPort, m_APISoPath);
        LoadEESTrader();
    }
    else
    {
        FMTLOG(fmtlog::ERR, "REMTrader::LoadAPIConfig InvestorID:{} LoadREMConfig failed, {}", m_XTraderConfig.Account, errorString);
    }
    ok = Utils::LoadREMError(m_XTraderConfig.ErrorPath.c_str(), m_CodeErrorMap, errorString);
    if(ok)
    {
        FMTLOG(fmtlog::INF, "REMTrader::LoadAPIConfig InvestorID:{} LoadREMError {} successed", m_XTraderConfig.Account, m_XTraderConfig.ErrorPath);
    }
    else
    {
        FMTLOG(fmtlog::ERR, "REMTrader::LoadAPIConfig InvestorID:{} LoadREMError {} failed, {}", m_XTraderConfig.Account, m_XTraderConfig.ErrorPath, errorString);
    }
}

void REMTradeGateWay::GetCommitID(std::string& CommitID, std::string& UtilsCommitID)
{
    CommitID = SO_COMMITID;
    UtilsCommitID = SO_UTILS_COMMITID;
}

void REMTradeGateWay::GetAPIVersion(std::string& APIVersion)
{
    APIVersion = API_VERSION;
}

void REMTradeGateWay::CreateTraderAPI()
{
    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EEventLog;
    message.EventLog.Level = Message::EEventLogLevel::EINFO;
    strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
    strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
    strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
    strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
    strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));

    m_TraderAPI = m_CreateTraderAPIFunc();
    if (NULL == m_TraderAPI)
    {
        XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader::CreateTraderAPI Broker:{} InvestorID:{} create Trader API object failed",
                        m_XTraderConfig.Broker, m_XTraderConfig.Account);
        while(!m_ReportMessageQueue.Push(message));
        FMTLOG(fmtlog::WRN, "REMTrader::CreateTraderAPI Broker:{} InvestorID:{} create Trader API object failed",
                m_XTraderConfig.Broker, m_XTraderConfig.Account);
        sleep(1);
        exit(-1);
    }
    m_TraderAPI->SetLoggerSwitch(true);
    XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader::CreateTraderAPI Broker:{} InvestorID:{} create Trader API object successed {}",
                    m_XTraderConfig.Broker, m_XTraderConfig.Account, m_APISoPath);
    while(!m_ReportMessageQueue.Push(message));

    FMTLOG(fmtlog::INF, "REMTrader::CreateTraderAPI Broker:{} InvestorID:{} create Trader API object successed {}",
            m_XTraderConfig.Broker, m_XTraderConfig.Account, m_APISoPath);
}

void REMTradeGateWay::DestroyTraderAPI()
{
    if(NULL != m_TraderAPI)
    {
        m_TraderAPI->DisConnServer();
        delete m_TraderAPI;
        m_TraderAPI = NULL;
    }
}

void REMTradeGateWay::LoadTrader()
{
    ConnectServer();
    FMTLOG(fmtlog::INF, "REMTrader::LoadTrader InvestorID:{}", m_XTraderConfig.Account);
}

void REMTradeGateWay::ReLoadTrader()
{
    if(Message::ELoginStatus::ELOGIN_SUCCESSED != m_ConnectedStatus)
    {
        DestroyTraderAPI();
        CreateTraderAPI();
        LoadTrader();
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EWARNING;
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
        strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
        XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader::ReLoadTrader InvestorID:{}", m_XTraderConfig.Account);
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));

        FMTLOG(fmtlog::WRN, "REMTrader::ReLoadTrader InvestorID:{}", m_XTraderConfig.Account);
    }
}

void REMTradeGateWay::ReqUserLogin()
{
    int ret = m_TraderAPI->UserLogon(m_XTraderConfig.Account.c_str(), m_XTraderConfig.Password.c_str(),
                                     m_XTraderConfig.AppID.c_str(), m_XTraderConfig.AuthCode.c_str());
    std::string errorString;
    HandleLoginResult(ret, errorString);
    FMTLOG(fmtlog::INF, "HandleLoginResult {}", errorString);

    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EEventLog;
    message.EventLog.Level = Message::EEventLogLevel::EINFO;
    strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
    strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
    strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
    strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
    strncpy(message.EventLog.Event, errorString.c_str(), sizeof(message.EventLog.Event));
    strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
    while(!m_ReportMessageQueue.Push(message));
}

int REMTradeGateWay::ReqQryFund()
{
    int ret = 0;
    for(auto it = m_FundAccountSet.begin(); it != m_FundAccountSet.end(); it++)
    {
        int code = m_TraderAPI->QueryAccountBP(it->c_str(), m_RequestID++);
        std::string buffer = "REMTrader::ReqQryFund Account:" + *it;
        HandleRetResult(code, buffer);
        ret += code;
    }
    return ret;
}
int REMTradeGateWay::ReqQryPoistion()
{
    int ret = 0;
    for(auto it = m_FundAccountSet.begin(); it != m_FundAccountSet.end(); it++)
    {
        int code = m_TraderAPI->QueryAccountPosition(it->c_str(), m_RequestID++);
        std::string buffer = "REMTrader::ReqQryPoistion Account:" + *it;
        HandleRetResult(code, buffer);
        ret += code;
    }
    return ret;
}

int REMTradeGateWay::ReqQryOrder()
{
    int ret = 0;
    for(auto it = m_FundAccountSet.begin(); it != m_FundAccountSet.end(); it++)
    {
        int code = m_TraderAPI->QueryAccountOrder(it->c_str());
        std::string buffer = "REMTrader::ReqQryOrder Account:" + *it;
        HandleRetResult(code, buffer);
        ret += code;
    }
    return ret;
}

int REMTradeGateWay::ReqQryTrade()
{
    int ret = 0;
    for(auto it = m_FundAccountSet.begin(); it != m_FundAccountSet.end(); it++)
    {
        int code = m_TraderAPI->QueryAccountOrderExecution(it->c_str());
        std::string buffer = "REMTrader::ReqQryTrade Account:" + *it;
        HandleRetResult(code, buffer);
        ret += code;
    }
    return ret;
}

int REMTradeGateWay::ReqQryTickerRate()
{
    std::string buffer;
    int code;
    // 查询合约保证金率
    for(auto it = m_FundAccountSet.begin(); it != m_FundAccountSet.end(); it++)
    {
        code = m_TraderAPI->QueryAccountTradeMargin(it->c_str());
        buffer = "REMTrader::QueryAccountTradeMargin Account:" + *it;
        HandleRetResult(code, buffer);
    }
    // 查询合约信息
    code = m_TraderAPI->QuerySymbolList();
    buffer = "REMTrader::QuerySymbolList ";
    HandleRetResult(code, buffer);
    // 查询合约手续费率
    for(auto it = m_FundAccountSet.begin(); it != m_FundAccountSet.end(); it++)
    {
        code = m_TraderAPI->QueryAccountTradeFee(it->c_str());
        buffer = "REMTrader::QueryAccountTradeFee Account:" + *it;
        HandleRetResult(code, buffer);
    }
    return 0;
}

void REMTradeGateWay::ReqInsertOrder(const Message::TOrderRequest& request)
{
    EES_ClientToken order_token = 0;
    m_TraderAPI->GetMaxToken(&order_token);
    EES_EnterOrderField reqOrderField;
    memset(&reqOrderField, 0, sizeof(EES_EnterOrderField));
    reqOrderField.m_HedgeFlag = EES_HedgeFlag_Speculation;
    reqOrderField.m_Exchange = REMExchangeID(request.ExchangeID);
    reqOrderField.m_SecType = EES_SecType_fut;
    strcpy(reqOrderField.m_Account, request.Account);
    strcpy(reqOrderField.m_Symbol, request.Ticker);
    reqOrderField.m_Side = REMOrderSide(request);
    reqOrderField.m_Price = request.Price;
    reqOrderField.m_Qty = request.Volume;
    reqOrderField.m_CustomField = request.OrderToken;
    if(Message::EOrderType::ELIMIT == request.OrderType)
    {
        reqOrderField.m_Tif = EES_OrderTif_Day;
    }
    else if(Message::EOrderType::EFAK == request.OrderType)
    {
        reqOrderField.m_Tif = EES_OrderTif_IOC;
        reqOrderField.m_MinQty = 0;
    }
    else if(Message::EOrderType::EFOK == request.OrderType)
    {
        reqOrderField.m_Tif = EES_OrderTif_IOC;
        reqOrderField.m_MinQty = reqOrderField.m_Qty;
    }
    reqOrderField.m_ClientOrderToken = FutureOrderRef(request.EngineID, order_token + 1);
    std::string OrderRef = fmt::format("{}", reqOrderField.m_ClientOrderToken);
    Message::TOrderStatus OrderStatus{};
    OrderStatus.BusinessType = m_XTraderConfig.BusinessType;
    strncpy(OrderStatus.OrderRef, OrderRef.c_str(), sizeof(OrderStatus.OrderRef));
    strncpy(OrderStatus.Product, m_XTraderConfig.Product.c_str(), sizeof(OrderStatus.Product));
    strncpy(OrderStatus.Broker, m_XTraderConfig.Broker.c_str(), sizeof(OrderStatus.Broker));
    strncpy(OrderStatus.Account, reqOrderField.m_Account, sizeof(OrderStatus.Account));
    strncpy(OrderStatus.ExchangeID, request.ExchangeID, sizeof(OrderStatus.ExchangeID));
    strncpy(OrderStatus.Ticker, request.Ticker, sizeof(OrderStatus.Ticker));
    strncpy(OrderStatus.RiskID, request.RiskID, sizeof(OrderStatus.RiskID));
    OrderStatus.SendPrice = reqOrderField.m_Price;
    OrderStatus.SendVolume = reqOrderField.m_Qty;
    OrderStatus.OrderType = request.OrderType;
    const auto Key = XTrader::MakeAccountTickerKey(reqOrderField.m_Account, reqOrderField.m_Symbol);
    OrderStatus.OrderSide = OrderSide(reqOrderField.m_Side, Key);
    strncpy(OrderStatus.SendTime, request.SendTime, sizeof(OrderStatus.SendTime));
    strncpy(OrderStatus.InsertTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.InsertTime));
    OrderStatus.OrderStatus = Message::EOrderStatusType::EORDER_SENDED;
    OrderStatus.OrderToken = request.OrderToken;
    OrderStatus.EngineID = request.EngineID;
    XTrader::UpsertParallelMapEntry<Message::TOrderStatus>(
        m_OrderStatusMap, OrderRef,
        [&](Message::TOrderStatus& storedStatus) {
            storedStatus = OrderStatus;
        });
    RESULT ret = m_TraderAPI->EnterOrder(&reqOrderField);
    std::string token = fmt::format("{}", order_token);
    std::string buffer = std::string("REMTrader::ReqInsertOrder Account:") + reqOrderField.m_Account
                         + " Ticker:" + reqOrderField.m_Symbol + " OrderRef:" + OrderRef + " MaxToken:" + token;
    HandleRetResult(ret, buffer);
    if(0 == ret)
    {
        XTrader::UpsertParallelMapEntry<Message::TAccountPosition>(
            m_TickerAccountPositionMap, Key,
            [&](Message::TAccountPosition& AccountPosition) {
                UpdatePosition(OrderStatus, AccountPosition);
            });
        UpdateOrderStatus(OrderStatus);
        FMTLOG(fmtlog::INF, "REMTrader:ReqInsertOrder, m_Account:{} m_Side:{} m_Exchange:{} m_Symbol:{} m_SecType:{} m_Price:{} "
                            "m_Qty:{} m_ClientOrderToken:{} m_Tif:{} m_MinQty:{} m_MarketSessionId:{} m_HedgeFlag:{} "
                            "m_ForceMarketSessionId:{} m_DoNotAdjustCoverSide:{} MaxToken:{}",
                reqOrderField.m_Account, reqOrderField.m_Side, reqOrderField.m_Exchange, reqOrderField.m_Symbol, 
                reqOrderField.m_SecType, reqOrderField.m_Price, reqOrderField.m_Qty, 
                reqOrderField.m_ClientOrderToken, reqOrderField.m_Tif, reqOrderField.m_MinQty, reqOrderField.m_MarketSessionId, 
                reqOrderField.m_HedgeFlag, reqOrderField.m_ForceMarketSessionId, reqOrderField.m_DoNotAdjustCoverSide, order_token);
    }
    else
    {
        XTrader::EraseParallelMapEntry(m_OrderStatusMap, OrderRef);
    }
}

void REMTradeGateWay::ReqInsertOrderRejected(const Message::TOrderRequest& request)
{
    EES_ClientToken order_token = 0;
    m_TraderAPI->GetMaxToken(&order_token);
    std::string OrderRef = fmt::format("{}", FutureOrderRef(request.EngineID, order_token + 1));
    Message::TOrderStatus OrderStatus;
    memset(&OrderStatus, 0, sizeof(OrderStatus));
    OrderStatus.BusinessType = m_XTraderConfig.BusinessType;
    strncpy(OrderStatus.OrderRef, OrderRef.c_str(), sizeof(OrderStatus.OrderRef));
    strncpy(OrderStatus.Product, m_XTraderConfig.Product.c_str(), sizeof(OrderStatus.Product));
    strncpy(OrderStatus.Broker, m_XTraderConfig.Broker.c_str(), sizeof(OrderStatus.Broker));
    strncpy(OrderStatus.Account, m_XTraderConfig.Account.c_str(), sizeof(OrderStatus.Account));
    strncpy(OrderStatus.ExchangeID, request.ExchangeID, sizeof(OrderStatus.ExchangeID));
    strncpy(OrderStatus.Ticker, request.Ticker, sizeof(OrderStatus.Ticker));
    strncpy(OrderStatus.RiskID, request.RiskID, sizeof(OrderStatus.RiskID));
    OrderStatus.SendPrice = request.Price;
    OrderStatus.SendVolume = request.Volume;
    OrderStatus.OrderType = request.OrderType;
    const auto Key = XTrader::MakeAccountTickerKey(request.Account, request.Ticker);
    OrderStatus.OrderSide = OrderSide(REMOrderSide(request), Key);
    strncpy(OrderStatus.SendTime, request.SendTime, sizeof(OrderStatus.SendTime));
    strncpy(OrderStatus.InsertTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.InsertTime));
    OrderStatus.OrderToken = request.OrderToken;
    OrderStatus.EngineID = request.EngineID;
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
    PrintOrderStatus(OrderStatus, "REMTrader::ReqInsertOrderRejected ");
    UpdateOrderStatus(OrderStatus);
}

void REMTradeGateWay::ReqCancelOrder(const Message::TActionRequest& request)
{
    Message::TOrderStatus OrderStatus{};
    if(!XTrader::ReadParallelMapEntry(m_OrderStatusMap, request.OrderRef, OrderStatus))
    {
        FMTLOG(fmtlog::WRN, "REMTrader::ReqCancelOrder Account:{} OrderRef:{} not found.", request.Account, request.OrderRef);
        return ;
    }
    EES_CancelOrder  reqOrderField;
    memset(&reqOrderField, 0, sizeof(EES_CancelOrder));
    strcpy(reqOrderField.m_Account, OrderStatus.Account);
    reqOrderField.m_MarketOrderToken = atol(OrderStatus.OrderLocalID);
    RESULT ret = m_TraderAPI->CancelOrder(&reqOrderField);
    std::string op = std::string("REMTrader::ReqCancelOrder Account:") + OrderStatus.Account + " OrderLocalID:"
                     + OrderStatus.OrderLocalID + " OrderSysID:" + OrderStatus.OrderSysID;
    HandleRetResult(ret, op);
    if(0 == ret)
    {
        XTrader::ModifyParallelMapEntry(
            m_OrderStatusMap, request.OrderRef,
            [&](Message::TOrderStatus& storedStatus) {
                storedStatus.OrderStatus = Message::EOrderStatusType::ECANCELLING;
                OrderStatus = storedStatus;
            });
        UpdateOrderStatus(OrderStatus);
    }
}

void REMTradeGateWay::ReqCancelOrderRejected(const Message::TActionRequest& request)
{
    Message::TOrderStatus OrderStatus{};
    bool found = XTrader::ModifyParallelMapEntry(
        m_OrderStatusMap, request.OrderRef,
        [&](Message::TOrderStatus& storedStatus) {
            storedStatus.OrderStatus = Message::EOrderStatusType::ERISK_ACTION_REJECTED;
            storedStatus.ErrorID = request.ErrorID;
            strncpy(storedStatus.ErrorMsg, request.ErrorMsg, sizeof(storedStatus.ErrorMsg));
            OrderStatus = storedStatus;
        });
    if(!found)
    {
        FMTLOG(fmtlog::WRN, "REMTrader::ReqCancelOrderRejected Account:{} OrderRef:{} not found.", request.Account, request.OrderRef);
        return ;
    }
    UpdateOrderStatus(OrderStatus);
    PrintOrderStatus(OrderStatus, "REMTrader::ReqCancelOrderRejected ");
}

void REMTradeGateWay::InitPosition()
{
    for(auto it1 = m_FundAccountSet.begin(); it1 != m_FundAccountSet.end(); it1++)
    {
        for(auto it2 = m_TickerPropertyList.begin(); it2 != m_TickerPropertyList.end(); it2++)
        {
            const auto Key = XTrader::MakeAccountTickerKey(*it1, it2->Ticker);
            XTrader::UpsertParallelMapEntry<Message::TAccountPosition>(
                m_TickerAccountPositionMap, Key,
                [&](Message::TAccountPosition& AccountPosition) {
                    AccountPosition.BusinessType = m_XTraderConfig.BusinessType;
                    strncpy(AccountPosition.Product, m_XTraderConfig.Product.c_str(), sizeof(AccountPosition.Product));
                    strncpy(AccountPosition.Broker, m_XTraderConfig.Broker.c_str(), sizeof(AccountPosition.Broker));
                    strncpy(AccountPosition.Account, it1->c_str(), sizeof(AccountPosition.Account));
                    strncpy(AccountPosition.Ticker, it2->Ticker.c_str(), sizeof(AccountPosition.Ticker));
                    strncpy(AccountPosition.ExchangeID, it2->ExchangeID.c_str(), sizeof(AccountPosition.ExchangeID));
                });
        }
        FMTLOG(fmtlog::INF, "REMTrader::InitPosition Account:{} Tickers:{}", *it1, m_TickerPropertyList.size());
    }
    for(auto it2 = m_TickerPropertyList.begin(); it2 != m_TickerPropertyList.end(); it2++)
    {
        m_TickerSet.insert(it2->Ticker);
    }
}

void REMTradeGateWay::ConnectServer()
{
    if(NULL == m_TraderAPI)
    {
        FMTLOG(fmtlog::WRN, "REMTrader::ConnectServer Broker:{} InvestorID:{} Trader API object invalid.",
                m_XTraderConfig.Broker, m_XTraderConfig.Account);
        return;
    }
    memset(&m_EES_TradeSvrInfo, 0, sizeof(m_EES_TradeSvrInfo));
    strcpy(m_EES_TradeSvrInfo.m_remoteTradeIp, m_REMConfig.TradeServerIP.c_str());
    m_EES_TradeSvrInfo.m_remoteTradeTCPPort = m_REMConfig.TradeServerPort;
    m_EES_TradeSvrInfo.m_remoteTradeUDPPort = m_REMConfig.TradeServerUDPPort;
    strcpy(m_EES_TradeSvrInfo.m_remoteQueryIp, m_REMConfig.QueryServerIP.c_str());
    m_EES_TradeSvrInfo.m_remoteQueryTCPPort = m_REMConfig.QueryServerPort;
    strcpy(m_EES_TradeSvrInfo.m_LocalTradeIp, m_REMConfig.LocalTradeIP.c_str());
    m_EES_TradeSvrInfo.m_LocalTradeUDPPort = m_REMConfig.LocalTradeUDPPort;

    RESULT ret_err = m_TraderAPI->ConnServer(m_EES_TradeSvrInfo, this);
    std::string errorBuffer;
    auto it = m_CodeErrorMap.find(ret_err);
    if(it != m_CodeErrorMap.end())
    {
        errorBuffer = it->second;
    }
    else
    {
        errorBuffer = "Unkown Error";
    }
    char errorString[512] = {0};
    if (NO_ERROR != ret_err)
    {
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EERROR;
        XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader::ConnectServer Broker:{} InvestorID:{} Connected to TradeServer:{}:{} QueryServer:{}:{} failed, code:{}, {}",
                        m_XTraderConfig.Broker, m_XTraderConfig.Account,
                        m_REMConfig.TradeServerIP, m_REMConfig.TradeServerPort,
                        m_REMConfig.QueryServerIP, m_REMConfig.QueryServerPort,
                        ret_err, errorBuffer);
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
        strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));

        FMTLOG(fmtlog::WRN, "REMTrader::ConnectServer Broker:{} InvestorID:{} Connected to TradeServer:{}:{} QueryServer:{}:{} failed, code:{}, {}",
                m_XTraderConfig.Broker, m_XTraderConfig.Account,
                m_REMConfig.TradeServerIP, m_REMConfig.TradeServerPort,
                m_REMConfig.QueryServerIP, m_REMConfig.QueryServerPort,
                ret_err, errorBuffer);
        return;
    }

    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EEventLog;
    message.EventLog.Level = Message::EEventLogLevel::EINFO;
    XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader::ConnectServer Broker:{} InvestorID:{} Connected to TradeServer:{}:{} QueryServer:{}:{} succssed",
                    m_XTraderConfig.Broker, m_XTraderConfig.Account,
                    m_REMConfig.TradeServerIP, m_REMConfig.TradeServerPort,
                    m_REMConfig.QueryServerIP, m_REMConfig.QueryServerPort);
    strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
    strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
    strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
    strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
    strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
    while(!m_ReportMessageQueue.Push(message));

    FMTLOG(fmtlog::INF, "REMTrader::ConnectServer Broker:{} InvestorID:{} Connected to TradeServer:{}:{} QueryServer:{}:{} succssed",
            m_XTraderConfig.Broker, m_XTraderConfig.Account,
            m_REMConfig.TradeServerIP, m_REMConfig.TradeServerPort,
            m_REMConfig.QueryServerIP, m_REMConfig.QueryServerPort);
}

bool REMTradeGateWay::LoadEESTrader()
{
    char errorString[512] = {0};

    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EEventLog;
    message.EventLog.Level = Message::EEventLogLevel::EINFO;
    strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
    strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
    strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
    strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
    strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));

    m_TraderHandle = dlopen(m_APISoPath.c_str(), RTLD_NOW | RTLD_DEEPBIND);
    if (NULL == m_TraderHandle)
    {
        XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader::LoadEESTrader Broker:{} InvestorID:{} load library({}) failed, {}",
                        m_XTraderConfig.Broker, m_XTraderConfig.Account,
                        m_APISoPath, dlerror());
        while(!m_ReportMessageQueue.Push(message));

        FMTLOG(fmtlog::WRN, "REMTrader::LoadEESTrader Broker:{} InvestorID:{} load library({}) failed, {}",
                m_XTraderConfig.Broker, m_XTraderConfig.Account, m_APISoPath, dlerror());
        return false;
    }

    m_CreateTraderAPIFunc = (funcCreateEESTraderApi)dlsym(m_TraderHandle, CREATE_EES_TRADER_API_NAME);
    if (NULL == m_CreateTraderAPIFunc)
    {
        XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader::LoadEESTrader Broker:{} InvestorID:{} load CreateTraderAPIFunc({}) failed, {}",
                        m_XTraderConfig.Broker, m_XTraderConfig.Account,
                        CREATE_EES_TRADER_API_NAME, dlerror());
        while(!m_ReportMessageQueue.Push(message));

        FMTLOG(fmtlog::WRN, "REMTrader::LoadEESTrader Broker:{} InvestorID:{} load CreateTraderAPIFunc({}) failed, {}",
                m_XTraderConfig.Broker, m_XTraderConfig.Account, CREATE_EES_TRADER_API_NAME, dlerror());
        return false;
    }

    m_DestroyFunc = (funcDestroyEESTraderApi)dlsym(m_TraderHandle, DESTROY_EES_TRADER_API_NAME);
    if (NULL == m_CreateTraderAPIFunc)
    {
        XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader::LoadEESTrader Broker:{} InvestorID:{} load DestroyFunc({}) failed, {}",
                        m_XTraderConfig.Broker, m_XTraderConfig.Account,
                        DESTROY_EES_TRADER_API_NAME, dlerror());
        while(!m_ReportMessageQueue.Push(message));

        FMTLOG(fmtlog::WRN, "REMTrader::LoadEESTrader Broker:{} InvestorID:{} load DestroyFunc({}) failed, {}",
                m_XTraderConfig.Broker, m_XTraderConfig.Account, DESTROY_EES_TRADER_API_NAME, dlerror());
        return false;
    }
    FMTLOG(fmtlog::INF, "REMTrader::LoadEESTrader {} successed", m_APISoPath);
    return true;
}

void REMTradeGateWay::UnloadEESTrader()
{
    if (NULL != m_TraderAPI)
    {
        m_DestroyFunc(m_TraderAPI);
        m_TraderAPI = NULL;
        m_DestroyFunc = NULL;
    }
    if (NULL != m_TraderHandle)
    {
        dlclose(m_TraderHandle);
        m_TraderHandle = NULL;
    }
}

void REMTradeGateWay::HandleLoginResult(int code, std::string& errorString)
{
    switch (code)
    {
    case NO_ERROR:
        m_ConnectedStatus = Message::ELoginStatus::ELOGIN_SUCCESSED;
        break;
    case 1:
        m_ConnectedStatus = Message::ELoginStatus::ELOGIN_FAILED;
        errorString = "用户名或密码错误";
        break;
    case 2:
        m_ConnectedStatus = Message::ELoginStatus::ELOGIN_FAILED;
        errorString = "用户名未绑定任何资金账户";
        break;
    case 3:
        m_ConnectedStatus = Message::ELoginStatus::ELOGIN_FAILED;
        errorString = "用户重复登录（后台配置非互踢模式）";
        break;
    case 4:
        m_ConnectedStatus = Message::ELoginStatus::ELOGIN_FAILED;
        errorString = "用户已经成功登录后再次登录";
        break;
    case 5:
        m_ConnectedStatus = Message::ELoginStatus::ELOGIN_FAILED;
        errorString = "缺少客户端标识、MAC地址";
        break;
    case 6:
        m_ConnectedStatus = Message::ELoginStatus::ELOGIN_FAILED;
        errorString = "系统内部错误";
        break;
    case 8:
        m_ConnectedStatus = Message::ELoginStatus::ELOGIN_FAILED;
        errorString = "连接查询通道失败";
        break;
    case 11:
        m_ConnectedStatus = Message::ELoginStatus::ELOGIN_FAILED;
        errorString = "无合法采集信息，登录失败";
        break;
    case 12:
        m_ConnectedStatus = Message::ELoginStatus::ELOGIN_FAILED;
        errorString = "采集上报和交易端远端IP不同，登录失败";
        break;

    case 13:
        m_ConnectedStatus = Message::ELoginStatus::ELOGIN_FAILED;
        errorString = "校验授权码错误，登录失败";
        break;
    case 99:
        m_ConnectedStatus = Message::ELoginStatus::ELOGIN_FAILED;
        errorString = "服务端环境恶劣，CPU、内存、磁盘资源不足，所有登录被禁止";
        break;
    default:
        m_ConnectedStatus = Message::ELoginStatus::ELOGIN_FAILED;
        errorString = "未知错误";
        break;
    }
    errorString = fmt::format("REMTrader::ReqUserLogin Broker:{} InvestorID:{} UserLogon, AppID:{} AuthCode:{} ErrorMsg:{}",
                                m_XTraderConfig.Broker, m_XTraderConfig.Account, 
                                m_XTraderConfig.AppID, m_XTraderConfig.AuthCode, errorString);
}

void REMTradeGateWay::HandleRetResult(int code, const std::string& op)
{
    std::string errorString;
    auto it = m_CodeErrorMap.find(code);
    if(it != m_CodeErrorMap.end())
    {
        errorString = op + " " + it->second;
    }
    else
    {
        errorString = op + " Unkown Error";
    }
    // 错误报告发送监控EventLog
    if(0 == code)
        return ;
    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EEventLog;
    message.EventLog.Level = Message::EEventLogLevel::EWARNING;
    strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
    strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
    strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
    strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
    strncpy(message.EventLog.Event, errorString.c_str(), sizeof(message.EventLog.Event));
    strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
    while(!m_ReportMessageQueue.Push(message));
}

int REMTradeGateWay::OrderSide(unsigned char side, const XTrader::AccountTickerKey& Key)
{
    int ret = -1;
    Message::TAccountPosition AccountPosition{};
    XTrader::UpsertParallelMapEntry<Message::TAccountPosition>(
        m_TickerAccountPositionMap, Key,
        [&](Message::TAccountPosition& storedPosition) {
            AccountPosition = storedPosition;
        });
    switch (side)
    {
    case EES_SideType_open_long:
        ret = Message::EOrderSide::EOPEN_LONG;
        break;
    case EES_SideType_close_today_long:
        ret = Message::EOrderSide::ECLOSE_TD_LONG;
        break;
    case EES_SideType_close_today_short:
        ret = Message::EOrderSide::ECLOSE_TD_SHORT;
        break;
    case EES_SideType_open_short:
        ret = Message::EOrderSide::EOPEN_SHORT;
        break;
    case EES_SideType_close_ovn_short:
        ret = Message::EOrderSide::ECLOSE_YD_SHORT;
        break;
    case EES_SideType_close_ovn_long:
        ret = Message::EOrderSide::ECLOSE_YD_LONG;
        break;
    case EES_SideType_close_short:
        if(AccountPosition.FuturePosition.ShortTdVolume > 0)
        {
            ret = Message::EOrderSide::ECLOSE_TD_SHORT;
        }
        else
        {
            ret = Message::EOrderSide::ECLOSE_YD_SHORT;
        }
        break;
    case EES_SideType_close_long:
        if(AccountPosition.FuturePosition.LongTdVolume > 0)
        {
            ret = Message::EOrderSide::ECLOSE_TD_LONG;
        }
        else
        {
            ret = Message::EOrderSide::ECLOSE_YD_LONG;
        }
        break;
    default:
        break;
    }
    return ret;
}

int REMTradeGateWay::REMOrderSide(const Message::TOrderRequest& req)
{
    int side = -1;
    if(Message::EOrderDirection::EBUY == req.Direction && Message::EOrderOffset::EOPEN == req.Offset)
    {
        side = EES_SideType_open_long;
    }
    else if(Message::EOrderDirection::ESELL == req.Direction && Message::EOrderOffset::EOPEN == req.Offset)
    {
        side = EES_SideType_open_short;
    }
    else if(Message::EOrderDirection::ESELL == req.Direction  && Message::EOrderOffset::ECLOSE == req.Offset)
    {
        side = EES_SideType_close_long;
    }
    else if(Message::EOrderDirection::EBUY == req.Direction  &&  Message::EOrderOffset::ECLOSE == req.Offset)
    {
        side = EES_SideType_close_short;
    }
    else if(Message::EOrderDirection::ESELL == req.Direction && Message::EOrderOffset::ECLOSE_TODAY == req.Offset)
    {
        side = EES_SideType_close_today_long;
    }
    else if(Message::EOrderDirection::EBUY == req.Direction  && Message::EOrderOffset::ECLOSE_TODAY == req.Offset)
    {
        side = EES_SideType_close_today_short;
    }
    else if(Message::EOrderDirection::EBUY == req.Direction && Message::EOrderOffset::ECLOSE_YESTODAY == req.Offset)
    {
        side = EES_SideType_close_ovn_short;
    }
    else if(Message::EOrderDirection::ESELL == req.Direction && Message::EOrderOffset::ECLOSE_YESTODAY == req.Offset)
    {
        side = EES_SideType_close_ovn_long;
    }
    return side;
}

const char* REMTradeGateWay::ExchangeID(EES_ExchangeID ExchangeID)
{
    switch(ExchangeID)
    {
        case EES_ExchangeID_sh_cs:
            return "SH";
        case EES_ExchangeID_sz_cs:
            return "SZ";
        case EES_ExchangeID_cffex:
            return "CFFEX";
        case EES_ExchangeID_shfe:
            return "SHFE";
        case EES_ExchangeID_dce:
            return "DCE";
        case EES_ExchangeID_zcze:
            return "CZCE";
        case EES_ExchangeID_ine:
            return "INE";
        default:
            return "Unkown";
    }
}

EES_ExchangeID REMTradeGateWay::REMExchangeID(const std::string& ExchangeID)
{
    if(ExchangeID == "SH")
    {
        return EES_ExchangeID_sh_cs;
    }
    else if(ExchangeID == "SZ")
    {
        return EES_ExchangeID_sz_cs;
    }
    else if(ExchangeID == "CFFEX")
    {
        return EES_ExchangeID_cffex;
    }
    else if(ExchangeID == "SHFE")
    {
        return EES_ExchangeID_shfe;
    }
    else if(ExchangeID == "DCE")
    {
        return EES_ExchangeID_dce;
    }
    else if(ExchangeID == "CZCE")
    {
        return EES_ExchangeID_zcze;
    }
    else if(ExchangeID == "INE")
    {
        return EES_ExchangeID_ine;
    }
    else 
    {
        return -1;
    }
}

void REMTradeGateWay::OnConnection(ERR_NO errNo, const char* pErrStr)
{
    char errorString[512] = {0};
    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EEventLog;
    strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
    strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
    strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
    strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
    strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));

    if (NO_ERROR != errNo)
    {
        XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader::OnConnection Broker:{} InvestorID:{} connected to Server failed:{} API:{}",
                        m_XTraderConfig.Broker, m_XTraderConfig.Account, pErrStr, m_TraderAPI->GetTradeApiVersion());
        message.EventLog.Level = Message::EEventLogLevel::EERROR;
        while(!m_ReportMessageQueue.Push(message));

        FMTLOG(fmtlog::WRN, "REMTrader::OnConnection Broker:{} InvestorID:{} connected to Server failed:{} API:{}",
                m_XTraderConfig.Broker, m_XTraderConfig.Account, pErrStr, m_TraderAPI->GetTradeApiVersion());
        return;
    }
    m_ConnectedStatus = Message::ELoginStatus::ELOGIN_CONNECTED;
    XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader::OnConnection Broker:{} InvestorID:{} connected to Server successed API:{}",
                    m_XTraderConfig.Broker, m_XTraderConfig.Account, m_TraderAPI->GetTradeApiVersion());
    message.EventLog.Level = Message::EEventLogLevel::EINFO;
    while(!m_ReportMessageQueue.Push(message));

    FMTLOG(fmtlog::INF, "REMTrader::OnConnection Broker:{} InvestorID:{} connected to Server successed API:{}",
            m_XTraderConfig.Broker, m_XTraderConfig.Account, m_TraderAPI->GetTradeApiVersion());

    ReqUserLogin();
}

void REMTradeGateWay::OnDisConnection(ERR_NO errNo, const char* pErrStr )
{
    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EEventLog;
    message.EventLog.Level = Message::EEventLogLevel::EERROR;

    std::string errorBuffer = m_CodeErrorMap[errNo];
    XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader::OnDisConnection Broker:{} InvestorID:{} disconnect to Server:{} API:{}",
                    m_XTraderConfig.Broker, m_XTraderConfig.Account, pErrStr, m_TraderAPI->GetTradeApiVersion());
    strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
    strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
    strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
    strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
    strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
    while(!m_ReportMessageQueue.Push(message));

    m_ConnectedStatus = Message::ELoginStatus::ELOGIN_FAILED;
    FMTLOG(fmtlog::WRN, "REMTrader::OnDisConnection Broker:{} InvestorID:{} disconnect to Server:{} API:{}",
            m_XTraderConfig.Broker, m_XTraderConfig.Account, pErrStr, m_TraderAPI->GetTradeApiVersion());
}

void REMTradeGateWay::OnUserLogon(EES_LogonResponse* pLogon)
{
    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EEventLog;
    message.EventLog.Level = Message::EEventLogLevel::EINFO;
    std::string errorBuffer;
    HandleLoginResult(pLogon->m_Result, errorBuffer);
    if(Message::ELoginStatus::ELOGIN_SUCCESSED == m_ConnectedStatus)
    {
        XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader::OnUserLogon Broker:{} InvestorID:{} logon successed, TradingDay:{} Max Token:{}",
                        m_XTraderConfig.Broker, m_XTraderConfig.Account, pLogon->m_TradingDate, pLogon->m_MaxToken);
        FMTLOG(fmtlog::INF, "REMTrader::OnUserLogon Broker:{} InvestorID:{} logon successed, TradingDay:{} Max Token:{}",
                m_XTraderConfig.Broker, m_XTraderConfig.Account, pLogon->m_TradingDate, pLogon->m_MaxToken);
    }
    else if(Message::ELoginStatus::ELOGIN_FAILED == m_ConnectedStatus)
    {
        XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader::OnUserLogon Broker:{} InvestorID:{} logon failed, code:{} errMsg:{}",
                        m_XTraderConfig.Broker, m_XTraderConfig.Account, pLogon->m_Result, errorBuffer);
        FMTLOG(fmtlog::WRN, "REMTrader::OnUserLogon Broker:{} InvestorID:{} logon failed, code:{} errMsg:{}",
                m_XTraderConfig.Broker, m_XTraderConfig.Account, pLogon->m_Result, errorBuffer);
    }
    strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
    strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
    strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
    strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
    strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
    while(!m_ReportMessageQueue.Push(message));

    // 查询绑定账户
    m_TraderAPI->QueryUserAccount();
}

void REMTradeGateWay::OnOrderAccept(EES_OrderAcceptField* pAccept )
{
    std::string OrderRef = fmt::format("{}", pAccept->m_ClientOrderToken);
    Message::TOrderStatus OrderStatus{};
    if(!XTrader::ReadParallelMapEntry(m_OrderStatusMap, OrderRef, OrderStatus))
    {
        // Order Status
        OrderStatus = Message::TOrderStatus{};
        OrderStatus.BusinessType = m_XTraderConfig.BusinessType;
        strncpy(OrderStatus.Product, m_XTraderConfig.Product.c_str(), sizeof(OrderStatus.Product));
        strncpy(OrderStatus.Broker, m_XTraderConfig.Broker.c_str(), sizeof(OrderStatus.Broker));
        strncpy(OrderStatus.Account, pAccept->m_Account, sizeof(OrderStatus.Account));
        strncpy(OrderStatus.ExchangeID, ExchangeID(pAccept->m_Exchange), sizeof(OrderStatus.ExchangeID));
        strncpy(OrderStatus.Ticker, pAccept->m_Symbol, sizeof(OrderStatus.Ticker));
        strncpy(OrderStatus.OrderRef, OrderRef.c_str(), sizeof(OrderStatus.OrderRef));
        OrderStatus.SendPrice = pAccept->m_Price;
        OrderStatus.SendVolume = pAccept->m_Qty;
        OrderStatus.OrderToken = pAccept->m_CustomField;
        std::string Account = pAccept->m_Account;
        std::string Ticker = pAccept->m_Symbol;
        const auto Key = XTrader::MakeAccountTickerKey(Account, Ticker);
        strncpy(OrderStatus.SendTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.SendTime));
        strncpy(OrderStatus.UpdateTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.UpdateTime));
        OrderStatus.OrderStatus = Message::EOrderStatusType::EORDER_SENDED;
        OrderStatus.OrderSide = OrderSide(pAccept->m_Side, Key);
        XTrader::UpsertParallelMapEntry<Message::TOrderStatus>(
            m_OrderStatusMap, OrderRef,
            [&](Message::TOrderStatus& storedStatus) {
                storedStatus = OrderStatus;
            });
        XTrader::UpsertParallelMapEntry<Message::TAccountPosition>(
            m_TickerAccountPositionMap, Key,
            [&](Message::TAccountPosition& AccountPosition) {
                UpdatePosition(OrderStatus, AccountPosition);
            });
    }

    const auto Key = XTrader::MakeAccountTickerKey(OrderStatus.Account, OrderStatus.Ticker);
    const int orderSide = OrderSide(pAccept->m_Side, Key);
    bool found = XTrader::ModifyParallelMapEntry(
        m_OrderStatusMap, OrderRef,
        [&](Message::TOrderStatus& storedStatus) {
            // Update OrderStatus
            XTrader::FormatToFixedBuffer(storedStatus.OrderLocalID, "{}", pAccept->m_MarketOrderToken);
            storedStatus.OrderStatus = Message::EOrderStatusType::EBROKER_ACK;
            storedStatus.OrderSide = orderSide;
            strncpy(storedStatus.BrokerACKTime, Utils::getCurrentTimeUs(), sizeof(storedStatus.BrokerACKTime));
            OrderStatus = storedStatus;
        });
    if(found)
    {
        UpdateOrderStatus(OrderStatus);
        // Update Position
        XTrader::UpsertParallelMapEntry<Message::TAccountPosition>(
            m_TickerAccountPositionMap, Key, [](Message::TAccountPosition&) {});
        Message::TAccountPosition AccountPosition{};
        XTrader::ModifyParallelMapEntry(
            m_TickerAccountPositionMap, Key,
            [&](Message::TAccountPosition& storedPosition) {
                strncpy(storedPosition.UpdateTime, Utils::getCurrentTimeUs(), sizeof(storedPosition.UpdateTime));
                UpdatePosition(OrderStatus, storedPosition);
                AccountPosition = storedPosition;
            });
        PrintOrderStatus(OrderStatus, "REMTrader::OnOrderAccept ");
        PrintAccountPosition(AccountPosition, "REMTrader::OnOrderAccept ");
    }
    else
    {
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EWARNING;
        XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader:OnOrderMarketAccept, UserID:{} Account:{} not found Order, Ticker:{} OrderRef:{}  OrderLocalID:{}",
                        pAccept->m_UserID, pAccept->m_Account, pAccept->m_Symbol, pAccept->m_ClientOrderToken, pAccept->m_MarketOrderToken);
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
        strncpy(message.EventLog.Account, pAccept->m_Account, sizeof(message.EventLog.Account));
        strncpy(message.EventLog.Ticker, pAccept->m_Symbol, sizeof(message.EventLog.Ticker));
        strncpy(message.EventLog.ExchangeID, ExchangeID(pAccept->m_Exchange), sizeof(message.EventLog.ExchangeID));
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));
        FMTLOG(fmtlog::WRN, "REMTrader:OnOrderMarketAccept, UserID:{} Account:{} not found Order, Ticker:{} OrderRef:{}  OrderLocalID:{}",
                pAccept->m_UserID, pAccept->m_Account, pAccept->m_Symbol, pAccept->m_ClientOrderToken, pAccept->m_MarketOrderToken);
    }
    FMTLOG(fmtlog::INF, "REMTrader:OnOrderAccept, Broker:{}, InvestorID:{} m_Symbol:{} "
                        "m_ClientOrderToken:{} m_MarketOrderToken:{} m_OrderState:{} m_UserID:{} m_AcceptTime:{} "
                        "m_Account:{} m_Side:{} m_Exchange:{} m_SecType:{} m_Price:{} m_Qty:{} "
                        "m_Tif:{} m_MinQty:{} m_CustomField:{} m_MarketSessionId:{} m_HedgeFlag:{}",
            m_XTraderConfig.Broker, m_XTraderConfig.Account,pAccept->m_Symbol,
            pAccept->m_ClientOrderToken, pAccept->m_MarketOrderToken, pAccept->m_OrderState,
            pAccept->m_UserID, pAccept->m_AcceptTime, pAccept->m_Account, pAccept->m_Side,
            pAccept->m_Exchange, pAccept->m_SecType, pAccept->m_Price, pAccept->m_Qty,
            pAccept->m_Tif, pAccept->m_MinQty, pAccept->m_CustomField,
            pAccept->m_MarketSessionId, pAccept->m_HedgeFlag);
}

void REMTradeGateWay::OnOrderReject(EES_OrderRejectField* pReject )
{
    std::string errorString;
    if(EES_RejectedMan_by_shengli == pReject->m_RejectedMan)
    {
        errorString = "REM System rejected, ";
    }
    else
    {
        errorString = "Exchange rejected, ";
    }
    char errorBuffer[512] = {0};
    Utils::CodeConvert(pReject->m_GrammerResult, sizeof(pReject->m_GrammerResult), errorBuffer,
                       sizeof(errorBuffer), "gb2312", "utf-8");
    errorString += std::string("GrammerText:") + errorBuffer;
    memset(errorBuffer, 0, sizeof(errorBuffer));
    Utils::CodeConvert(pReject->m_RiskText, sizeof(pReject->m_RiskText), errorBuffer,
                       sizeof(errorBuffer), "gb2312", "utf-8");
    errorString += (std::string(" RiskText: ") + errorBuffer);
    std::string OrderRef = fmt::format("{}", pReject->m_ClientOrderToken);
    Message::TOrderStatus OrderStatus{};
    bool found = XTrader::ModifyParallelMapEntry(
        m_OrderStatusMap, OrderRef,
        [&](Message::TOrderStatus& storedStatus) {
            storedStatus.OrderStatus = Message::EOrderStatusType::EBROKER_ERROR;
            storedStatus.CanceledVolume = storedStatus.SendVolume;
            strncpy(storedStatus.ErrorMsg, errorString.c_str(), sizeof(storedStatus.ErrorMsg));
            storedStatus.ErrorID = pReject->m_ReasonCode;
            strncpy(storedStatus.BrokerACKTime, Utils::getCurrentTimeUs(), sizeof(storedStatus.BrokerACKTime));
            OrderStatus = storedStatus;
        });
    if(found)
    {
        const auto Key = XTrader::MakeAccountTickerKey(OrderStatus.Account, OrderStatus.Ticker);
        // Update Position
        XTrader::UpsertParallelMapEntry<Message::TAccountPosition>(
            m_TickerAccountPositionMap, Key, [](Message::TAccountPosition&) {});
        Message::TAccountPosition AccountPosition{};
        XTrader::ModifyParallelMapEntry(
            m_TickerAccountPositionMap, Key,
            [&](Message::TAccountPosition& storedPosition) {
                UpdatePosition(OrderStatus, storedPosition);
                AccountPosition = storedPosition;
            });
        PrintAccountPosition(AccountPosition, "REMTrader::OnOrderReject ");
        UpdateOrderStatus(OrderStatus);
        PrintOrderStatus(OrderStatus, "REMTrader::OnOrderReject ");
        XTrader::EraseParallelMapEntry(m_OrderStatusMap, OrderRef);
    }
    else
    {
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EWARNING;
        XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader:OnOrderReject, UserID:{} Account:{} not found Order, OrderRef:{}",
                        pReject->m_Userid, m_XTraderConfig.Account, pReject->m_ClientOrderToken);
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
        strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));
        FMTLOG(fmtlog::WRN, "REMTrader:OnOrderReject, UserID:{} Account:{} not found Order, OrderRef:{}",
                pReject->m_Userid, m_XTraderConfig.Account, pReject->m_ClientOrderToken);
    }
    FMTLOG(fmtlog::INF, "REMTrader:OnOrderReject, Broker:{}, InvestorID:{} m_Userid:{} "
                        "m_Timestamp:{} m_ClientOrderToken:{} m_RejectedMan:{} m_ReasonCode:{} m_GrammerResult:{} "
                        "m_RiskResult:{} m_GrammerText:{} m_RiskText:{} errorString:{}",
            m_XTraderConfig.Broker, m_XTraderConfig.Account, pReject->m_Userid,
            pReject->m_Timestamp, pReject->m_ClientOrderToken, pReject->m_RejectedMan,
            pReject->m_ReasonCode, pReject->m_GrammerResult, pReject->m_RiskResult,
            pReject->m_GrammerText, pReject->m_RiskText, errorString);
}

void REMTradeGateWay::OnOrderMarketAccept(EES_OrderMarketAcceptField* pAccept)
{
    std::string OrderRef = fmt::format("{}", pAccept->m_ClientOrderToken);
    Message::TOrderStatus OrderStatus{};
    bool found = XTrader::ModifyParallelMapEntry(
        m_OrderStatusMap, OrderRef,
        [&](Message::TOrderStatus& storedStatus) {
            XTrader::FormatToFixedBuffer(storedStatus.OrderSysID, "{}", pAccept->m_MarketOrderId);
            storedStatus.OrderStatus = Message::EOrderStatusType::EEXCHANGE_ACK;
            strncpy(storedStatus.ExchangeACKTime, Utils::getCurrentTimeUs(), sizeof(storedStatus.ExchangeACKTime));
            OrderStatus = storedStatus;
        });
    if(found)
    {
        UpdateOrderStatus(OrderStatus);

        PrintOrderStatus(OrderStatus, "REMTrader::OnOrderMarketAccept ");
        OnExchangeACK(OrderStatus);
    }
    else
    {
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EWARNING;
        XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader:OnOrderMarketAccept, UserID:{} Account:{} not found Order, OrderRef:{} OrderSysID:{} OrderLocalID:{}",
                        pAccept->m_UserID, pAccept->m_Account, pAccept->m_ClientOrderToken, pAccept->m_MarketOrderId, pAccept->m_MarketOrderToken);
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
        strncpy(message.EventLog.Account, pAccept->m_Account, sizeof(message.EventLog.Account));
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));
        FMTLOG(fmtlog::WRN, "REMTrader:OnOrderMarketAccept, UserID:{} Account:{} not found Order, OrderRef:{} OrderSysID:{} OrderLocalID:{}",
                pAccept->m_UserID, pAccept->m_Account, pAccept->m_ClientOrderToken, pAccept->m_MarketOrderId, pAccept->m_MarketOrderToken);
    }
    FMTLOG(fmtlog::INF, "REMTrader:OnOrderMarketAccept, Broker:{}, InvestorID:{} m_Account:{} "
                        "m_MarketOrderToken:{} m_MarketOrderId:{} m_MarketTime:{} m_UserID:{} m_ClientOrderToken:{}",
            m_XTraderConfig.Broker, m_XTraderConfig.Account,pAccept->m_Account, pAccept->m_MarketOrderToken, 
            pAccept->m_MarketOrderId, pAccept->m_MarketTime, pAccept->m_UserID, pAccept->m_ClientOrderToken);
}

void REMTradeGateWay::OnOrderMarketReject(EES_OrderMarketRejectField* pReject)
{
    char errorString[512] = {0};
    Utils::CodeConvert(pReject->m_ReasonText, sizeof(pReject->m_ReasonText), errorString,
                       sizeof(errorString), "gb2312", "utf-8");

    std::string OrderRef = fmt::format("{}", pReject->m_ClientOrderToken);
    Message::TOrderStatus OrderStatus{};
    bool found = XTrader::ModifyParallelMapEntry(
        m_OrderStatusMap, OrderRef,
        [&](Message::TOrderStatus& storedStatus) {
            storedStatus.OrderStatus = Message::EOrderStatusType::EEXCHANGE_ERROR;
            storedStatus.CanceledVolume = storedStatus.SendVolume;
            Utils::CodeConvert(pReject->m_ReasonText, sizeof(pReject->m_ReasonText), storedStatus.ErrorMsg,
                               sizeof(storedStatus.ErrorMsg), "gb2312", "utf-8");
            strncpy(storedStatus.ExchangeACKTime, Utils::getCurrentTimeUs(), sizeof(storedStatus.ExchangeACKTime));
            OrderStatus = storedStatus;
        });
    if(found)
    {
        UpdateOrderStatus(OrderStatus);
        // Update Position
        std::string Ticker = OrderStatus.Ticker;
        std::string Account = OrderStatus.Account;
        const auto Key = XTrader::MakeAccountTickerKey(Account, Ticker);
        XTrader::UpsertParallelMapEntry<Message::TAccountPosition>(
            m_TickerAccountPositionMap, Key, [](Message::TAccountPosition&) {});
        Message::TAccountPosition AccountPosition{};
        XTrader::ModifyParallelMapEntry(
            m_TickerAccountPositionMap, Key,
            [&](Message::TAccountPosition& storedPosition) {
                strncpy(storedPosition.UpdateTime, Utils::getCurrentTimeUs(), sizeof(storedPosition.UpdateTime));
                UpdatePosition(OrderStatus, storedPosition);
                AccountPosition = storedPosition;
            });
        PrintOrderStatus(OrderStatus, "REMTrader::OnOrderMarketReject ");
        PrintAccountPosition(AccountPosition, "REMTrader::OnOrderMarketReject ");
        XTrader::EraseParallelMapEntry(m_OrderStatusMap, OrderRef);
    }
    else
    {
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EWARNING;
        XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader:OnOrderMarketReject, UserID:{} Account:{} not found Order, OrderRef:{} OrderLocalID:{}",
                        pReject->m_UserID, pReject->m_Account, pReject->m_ClientOrderToken, pReject->m_MarketOrderToken);
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
        strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));
        FMTLOG(fmtlog::WRN, "REMTrader:OnOrderMarketReject, UserID:{} Account:{} not found Order, OrderRef:{} OrderLocalID:{}",
                pReject->m_UserID, pReject->m_Account, pReject->m_ClientOrderToken, pReject->m_MarketOrderToken);
    }
    FMTLOG(fmtlog::INF, "REMTrader:OnOrderMarketReject, Broker:{}, InvestorID:{} m_Account:{} "
                        "m_MarketOrderToken:{} m_MarketTimestamp:{} m_ReasonText:{} m_UserID:{} m_ClientOrderToken:{}",
            m_XTraderConfig.Broker, m_XTraderConfig.Account, pReject->m_Account, pReject->m_MarketOrderToken, 
            pReject->m_MarketTimestamp, errorString, pReject->m_UserID, pReject->m_ClientOrderToken);
}

void REMTradeGateWay::OnOrderExecution(EES_OrderExecutionField* pExec )
{
    // Order Status
    std::string OrderRef = fmt::format("{}", pExec->m_ClientOrderToken);
    Message::TOrderStatus OrderStatus{};
    bool found = XTrader::ModifyParallelMapEntry(
        m_OrderStatusMap, OrderRef,
        [&](Message::TOrderStatus& storedStatus) {
            storedStatus.TradedPrice = pExec->m_Price;
            storedStatus.TradedVolume = pExec->m_Quantity;
            storedStatus.TradedAvgPrice =
                (storedStatus.TotalTradedVolume * storedStatus.TradedAvgPrice +
                 pExec->m_Price * pExec->m_Quantity) /
                (storedStatus.TotalTradedVolume + pExec->m_Quantity);
            storedStatus.TotalTradedVolume += pExec->m_Quantity;
            if(storedStatus.TotalTradedVolume == storedStatus.SendVolume)
            {
                storedStatus.OrderStatus = Message::EOrderStatusType::EALLTRADED;
            }
            else
            {
                storedStatus.OrderStatus = Message::EOrderStatusType::EPARTTRADED;
            }
            OrderStatus = storedStatus;
        });
    if(found)
    {
        UpdateOrderStatus(OrderStatus);
        // Update Position
        std::string Ticker = OrderStatus.Ticker;
        std::string Account = OrderStatus.Account;
        const auto Key = XTrader::MakeAccountTickerKey(Account, Ticker);
        XTrader::UpsertParallelMapEntry<Message::TAccountPosition>(
            m_TickerAccountPositionMap, Key, [](Message::TAccountPosition&) {});
        Message::TAccountPosition AccountPosition{};
        XTrader::ModifyParallelMapEntry(
            m_TickerAccountPositionMap, Key,
            [&](Message::TAccountPosition& storedPosition) {
                UpdatePosition(OrderStatus, storedPosition);
                AccountPosition = storedPosition;
            });

        PrintOrderStatus(OrderStatus, "REMTrader::OnOrderExecution ");
        PrintAccountPosition(AccountPosition, "REMTrader::OnOrderExecution ");
        if(Message::EOrderStatusType::EALLTRADED == OrderStatus.OrderStatus)
        {
            XTrader::EraseParallelMapEntry(m_OrderStatusMap, OrderRef);
        }
    }
    else
    {
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EWARNING;
        XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader:OnOrderExecution, UserID:{} Account:{} not found Order, OrderRef:{} OrderLocalID:{}",
                        pExec->m_Userid, m_XTraderConfig.Account, pExec->m_ClientOrderToken, pExec->m_MarketOrderToken);
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
        strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));
        FMTLOG(fmtlog::WRN, "REMTrader:OnOrderExecution, UserID:{} Account:{} not found Order, OrderRef:{} OrderLocalID:{}",
                pExec->m_Userid, m_XTraderConfig.Account, pExec->m_ClientOrderToken, pExec->m_MarketOrderToken);
    }
    FMTLOG(fmtlog::INF, "REMTrader:OnOrderExecution, Broker:{}, InvestorID:{} m_Userid:{} "
                        "m_Timestamp:{} m_ClientOrderToken:{} m_MarketOrderToken:{} m_Quantity:{} m_Price:{} "
                        "m_ExecutionID:{} m_MarketExecID:{}",
            m_XTraderConfig.Broker, m_XTraderConfig.Account, pExec->m_Userid, pExec->m_Timestamp, pExec->m_ClientOrderToken, 
            pExec->m_MarketOrderToken, pExec->m_Quantity, pExec->m_Price, pExec->m_ExecutionID, pExec->m_MarketExecID);
}

void REMTradeGateWay::OnOrderCxled(EES_OrderCxled* pCxled )
{
    std::string OrderRef = fmt::format("{}", pCxled->m_ClientOrderToken);
    Message::TOrderStatus OrderStatus{};
    bool found = XTrader::ModifyParallelMapEntry(
        m_OrderStatusMap, OrderRef,
        [&](Message::TOrderStatus& storedStatus) {
            if(storedStatus.TotalTradedVolume > 0)
            {
                storedStatus.OrderStatus = Message::EOrderStatusType::EPARTTRADED_CANCELLED;
            }
            else
            {
                storedStatus.OrderStatus = Message::EOrderStatusType::ECANCELLED;
            }
            storedStatus.CanceledVolume = pCxled->m_Decrement;
            OrderStatus = storedStatus;
        });
    if(found)
    {
        UpdateOrderStatus(OrderStatus);
        // Update Position
        std::string Ticker = OrderStatus.Ticker;
        std::string Account = OrderStatus.Account;
        const auto Key = XTrader::MakeAccountTickerKey(Account, Ticker);
        XTrader::UpsertParallelMapEntry<Message::TAccountPosition>(
            m_TickerAccountPositionMap, Key, [](Message::TAccountPosition&) {});
        Message::TAccountPosition AccountPosition{};
        XTrader::ModifyParallelMapEntry(
            m_TickerAccountPositionMap, Key,
            [&](Message::TAccountPosition& storedPosition) {
                strncpy(storedPosition.UpdateTime, Utils::getCurrentTimeUs(), sizeof(storedPosition.UpdateTime));
                UpdatePosition(OrderStatus, storedPosition);
                AccountPosition = storedPosition;
            });

        PrintOrderStatus(OrderStatus, "REMTrader::OnOrderCxled ");
        PrintAccountPosition(AccountPosition, "REMTrader::OnOrderCxled ");

        XTrader::EraseParallelMapEntry(m_OrderStatusMap, OrderRef);
    }
    else
    {
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EWARNING;
        XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader:OnOrderCxled, UserID:{} Account:{} not found Order, OrderRef:{} OrderLocalID:{}",
                        pCxled->m_Userid, m_XTraderConfig.Account, pCxled->m_ClientOrderToken, pCxled->m_MarketOrderToken);
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));
        FMTLOG(fmtlog::WRN, "REMTrader:OnOrderCxled, UserID:{} Account:{} not found Order, OrderRef:{} OrderLocalID:{}",
                pCxled->m_Userid, m_XTraderConfig.Account, pCxled->m_ClientOrderToken, pCxled->m_MarketOrderToken);
    }
    FMTLOG(fmtlog::INF, "REMTrader:OnOrderCxled, Broker:{}, InvestorID:{} m_Userid:{} " 
                        "m_Timestamp:{} m_ClientOrderToken:{} m_MarketOrderToken:{} m_Decrement:{} m_Reason:{}",
            m_XTraderConfig.Broker, m_XTraderConfig.Account, pCxled->m_Userid, pCxled->m_Timestamp, 
            pCxled->m_ClientOrderToken, pCxled->m_MarketOrderToken, pCxled->m_Decrement, pCxled->m_Reason);
}

void REMTradeGateWay::OnCxlOrderReject(EES_CxlOrderRej* pReject )
{
    std::string OrderRef = fmt::format("{}", pReject->m_ClientOrderToken);
    Message::TOrderStatus OrderStatus{};
    bool found = XTrader::ModifyParallelMapEntry(
        m_OrderStatusMap, OrderRef,
        [&](Message::TOrderStatus& storedStatus) {
            storedStatus.OrderStatus = Message::EOrderStatusType::EACTION_ERROR;
            storedStatus.ErrorID = pReject->m_ReasonCode;
            Utils::CodeConvert(pReject->m_ReasonText, sizeof(pReject->m_ReasonText), storedStatus.ErrorMsg,
                               sizeof(storedStatus.ErrorMsg), "gb2312", "utf-8");
            OrderStatus = storedStatus;
        });
    if(found)
    {
        UpdateOrderStatus(OrderStatus);
        {
            Message::PackMessage message;
            memset(&message, 0, sizeof(message));
            message.MessageType = Message::EMessageType::EEventLog;
            message.EventLog.Level = Message::EEventLogLevel::EERROR;
            XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader:OnCxlOrderReject, Broker:{}, Account:{} Ticker:{} OrderRef:{} OrderSysID:{} OrderLocalID:{} ErrorID:{}, ErrorMessage:{}",
                            m_XTraderConfig.Broker, pReject->m_account, OrderStatus.Ticker, OrderStatus.OrderRef, 
                            OrderStatus.OrderSysID, OrderStatus.OrderLocalID, OrderStatus.ErrorID, OrderStatus.ErrorMsg);
            strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
            strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
            strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
            strncpy(message.EventLog.Account, pReject->m_account, sizeof(message.EventLog.Account));
            strncpy(message.EventLog.Ticker, OrderStatus.Ticker, sizeof(message.EventLog.Ticker));
            strncpy(message.EventLog.ExchangeID, ExchangeID(pReject->m_ExchangeID), sizeof(message.EventLog.ExchangeID));
            strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
            while(!m_ReportMessageQueue.Push(message));
            FMTLOG(fmtlog::WRN, "REMTrader:OnCxlOrderReject, Broker:{}, Account:{} Ticker:{} OrderRef:{} OrderSysID:{} OrderLocalID:{} ErrorID:{}, ErrorMessage:{}",
                    m_XTraderConfig.Broker, pReject->m_account, OrderStatus.Ticker, OrderStatus.OrderRef, 
                    OrderStatus.OrderSysID, OrderStatus.OrderLocalID, OrderStatus.ErrorID, OrderStatus.ErrorMsg);
        }
        PrintOrderStatus(OrderStatus, "REMTrader::OnCxlOrderReject ");
    }
    else
    {
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EWARNING;
        XTrader::FormatToFixedBuffer(message.EventLog.Event, "REMTrader:OnCxlOrderReject, UserID:{} Account:{} not found Order, OrderRef:{} OrderLocalID:{}",
                        pReject->m_UserID, pReject->m_account, pReject->m_ClientOrderToken, pReject->m_MarketOrderToken);
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
        strncpy(message.EventLog.Account, pReject->m_account, sizeof(message.EventLog.Account));
        strncpy(message.EventLog.ExchangeID, ExchangeID(pReject->m_ExchangeID), sizeof(message.EventLog.ExchangeID));
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));
        FMTLOG(fmtlog::WRN, "REMTrader:OnCxlOrderReject, UserID:{} Account:{} not found Order, OrderRef:{} OrderLocalID:{}",
                pReject->m_UserID, pReject->m_account, pReject->m_ClientOrderToken, pReject->m_MarketOrderToken);
    }
    FMTLOG(fmtlog::INF, "REMTrader:OnCxlOrderReject, Broker:{}, InvestorID:{} m_account:{} m_MarketOrderToken:{} "
                        "m_ReasonCode:{} m_ReasonText:{} m_UserID:{} m_ClientOrderToken:{} m_ExchangeID:{}",
            m_XTraderConfig.Broker, m_XTraderConfig.Account, pReject->m_account, pReject->m_MarketOrderToken, 
            pReject->m_ReasonCode, pReject->m_ReasonText, pReject->m_UserID, pReject->m_ClientOrderToken, pReject->m_ExchangeID);
}

void REMTradeGateWay::OnQueryUserAccount(EES_AccountInfo * pAccoutnInfo, bool bFinish)
{
    FMTLOG(fmtlog::INF, "REMTrader::OnQueryUserAccount, UserID:{}, Account:{} Finished:{}", m_XTraderConfig.Account, pAccoutnInfo->m_Account, bFinish);
    std::string Account = pAccoutnInfo->m_Account;
    if(!Account.empty())
    {
        m_FundAccountSet.insert(Account);
    }
    if(bFinish)
    {
        InitPosition();
    }
}

void REMTradeGateWay::OnQuerySymbol(EES_SymbolField* pSymbol, bool bFinish)
{
    auto it = m_TickerSet.find(pSymbol->m_symbol);
    if(it != m_TickerSet.end())
    {
        FMTLOG(fmtlog::INF, "REMTrader:OnQuerySymbol m_SecType:{} m_symbol:{} m_ProdID:{} m_VolumeMultiple:{} m_PriceTick:{}",
                pSymbol->m_SecType, pSymbol->m_symbol, pSymbol->m_ProdID, pSymbol->m_VolumeMultiple, pSymbol->m_PriceTick);
    }
}

void REMTradeGateWay::OnQueryAccountBP(const char* pAccount, EES_AccountBP* pAccountBP, int nReqId )
{
    if (NULL != pAccountBP)
    {
        std::string Account = pAccountBP->m_account;
        Message::TAccountFund& AccountFund = m_AccountFundMap[Account];
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EAccountFund;
        AccountFund.BusinessType = m_XTraderConfig.BusinessType;
        strncpy(AccountFund.Product, m_XTraderConfig.Product.c_str(), sizeof(AccountFund.Product));
        strncpy(AccountFund.Account, pAccountBP->m_account, sizeof(AccountFund.Account));
        AccountFund.Deposit = 0;
        AccountFund.Withdraw = 0;
        AccountFund.CurrMargin = pAccountBP->m_Margin;
        AccountFund.Commission = pAccountBP->m_CommissionFee;
        AccountFund.CloseProfit = pAccountBP->m_TotalLiquidPL;
        AccountFund.PositionProfit = pAccountBP->m_TotalMarketPL;
        AccountFund.Available = pAccountBP->m_AvailableBp;
        AccountFund.WithdrawQuota = 0;
        AccountFund.ExchangeMargin = 0;
        AccountFund.Balance = pAccountBP->m_Margin + pAccountBP->m_AvailableBp;
        AccountFund.PreBalance = pAccountBP->m_OvnInitMargin + pAccountBP->m_InitialBp;
        strncpy(AccountFund.UpdateTime, Utils::getCurrentTimeUs(), sizeof(AccountFund.UpdateTime));
        memcpy(&message.AccountFund, &AccountFund, sizeof(message.AccountFund));
        while(!m_ReportMessageQueue.Push(message));
        FMTLOG(fmtlog::INF, "REMTrader::OnQueryAccountBP, m_account:{} m_InitialBp:{} m_AvailableBp:{} m_OvnMargin:{} "
                            "m_FrozenMargin:{} m_CommissionFee:{} m_FrozenCommission:{} m_OvnInitMargin:{} "
                            "m_TotalLiquidPL:{} m_TotalMarketPL:{}",
                pAccountBP->m_account, pAccountBP->m_InitialBp, pAccountBP->m_AvailableBp, pAccountBP->m_Margin, pAccountBP->m_FrozenMargin, 
                pAccountBP->m_CommissionFee, pAccountBP->m_FrozenCommission, pAccountBP->m_OvnInitMargin, 
                pAccountBP->m_TotalLiquidPL, pAccountBP->m_TotalMarketPL);
    }
}

void REMTradeGateWay::OnQueryAccountPosition(const char* pAccount, EES_AccountPosition* pAccoutnPosition, int nReqId, bool bFinish)
{
    if (NULL != pAccoutnPosition)
    {
        if(!bFinish)
        {
            std::string Account = pAccoutnPosition->m_actId;
            std::string Ticker = pAccoutnPosition->m_Symbol;
            const auto Key = XTrader::MakeAccountTickerKey(Account, Ticker);
            XTrader::UpsertParallelMapEntry<Message::TAccountPosition>(
                m_TickerAccountPositionMap, Key,
                [&](Message::TAccountPosition& AccountPosition) {
                    if(EES_PosiDirection_long == pAccoutnPosition->m_PosiDirection)
                    {
                        AccountPosition.FuturePosition.LongTdVolume = pAccoutnPosition->m_TodayQty;
                        AccountPosition.FuturePosition.LongYdVolume = pAccoutnPosition->m_OvnQty;
                        AccountPosition.FuturePosition.LongOpenVolume = pAccoutnPosition->m_TodayQty - pAccoutnPosition->m_OvnQty;
                        if(AccountPosition.FuturePosition.LongOpenVolume > 0)
                        {
                            AccountPosition.FuturePosition.LongTdVolume += AccountPosition.FuturePosition.LongYdVolume;
                            AccountPosition.FuturePosition.LongYdVolume = 0;
                        }
                        AccountPosition.FuturePosition.LongOpeningVolume = 0;
                        AccountPosition.FuturePosition.LongClosingTdVolume = 0;
                        AccountPosition.FuturePosition.LongClosingYdVolume = 0;
                        strncpy(AccountPosition.UpdateTime, Utils::getCurrentTimeUs(), sizeof(AccountPosition.UpdateTime));
                    }
                    else if(EES_PosiDirection_short == pAccoutnPosition->m_PosiDirection)
                    {
                        AccountPosition.FuturePosition.ShortTdVolume = pAccoutnPosition->m_TodayQty;
                        AccountPosition.FuturePosition.ShortYdVolume = pAccoutnPosition->m_OvnQty;
                        AccountPosition.FuturePosition.ShortOpenVolume = pAccoutnPosition->m_TodayQty - pAccoutnPosition->m_OvnQty;
                        if(AccountPosition.FuturePosition.ShortOpenVolume > 0)
                        {
                            AccountPosition.FuturePosition.ShortTdVolume += AccountPosition.FuturePosition.ShortYdVolume;
                            AccountPosition.FuturePosition.ShortYdVolume = 0;
                        }
                        AccountPosition.FuturePosition.ShortOpeningVolume = 0;
                        AccountPosition.FuturePosition.ShortClosingTdVolume = 0;
                        AccountPosition.FuturePosition.ShortClosingYdVolume = 0;
                        strncpy(AccountPosition.UpdateTime, Utils::getCurrentTimeUs(), sizeof(AccountPosition.UpdateTime));
                    }
                });
        }
        FMTLOG(fmtlog::INF, "REMTrader::OnQueryAccountPosition, m_actId:{} m_Symbol:{} m_PosiDirection:{} "
                            "m_InitOvnQty:{} m_OvnQty:{} m_FrozenOvnQty:{} m_TodayQty:{} m_FrozenTodayQty:{} "
                            "m_OvnMargin:{} m_TodayMargin:{} m_PositionCost:{} m_HedgeFlag:{} Finished:{}",
                pAccoutnPosition->m_actId, pAccoutnPosition->m_Symbol, pAccoutnPosition->m_PosiDirection,
                pAccoutnPosition->m_InitOvnQty, pAccoutnPosition->m_OvnQty, pAccoutnPosition->m_FrozenOvnQty,
                pAccoutnPosition->m_TodayQty, pAccoutnPosition->m_FrozenTodayQty, pAccoutnPosition->m_OvnMargin,
                pAccoutnPosition->m_TodayMargin, pAccoutnPosition->m_PositionCost, pAccoutnPosition->m_HedgeFlag, bFinish);
    }

    if(bFinish)
    {
        const auto Positions =
            XTrader::SnapshotParallelMapValues<Message::TAccountPosition>(m_TickerAccountPositionMap);
        for(const auto& AccountPosition : Positions)
        {
            if(strnlen(AccountPosition.UpdateTime, sizeof(AccountPosition.UpdateTime)) > 0)
            {
                Message::PackMessage message;
                memset(&message, 0, sizeof(message));
                message.MessageType = Message::EMessageType::EAccountPosition;
                memcpy(&message.AccountPosition, &AccountPosition, sizeof(message.AccountPosition));
                while(!m_ReportMessageQueue.Push(message));
            }
        }
    }
}

void REMTradeGateWay::OnQueryTradeOrder(const char* pAccount, EES_QueryAccountOrder* pQueryOrder, bool bFinish)
{
    if(NULL == pQueryOrder)
        return;
    unsigned char status = pQueryOrder->m_OrderStatus & EES_OrderStatus_closed;
    if(!bFinish && EES_OrderStatus_closed != status)
    {
        std::string OrderRef = fmt::format("{}", pQueryOrder->m_ClientOrderToken);
        const auto Key = XTrader::MakeAccountTickerKey(pQueryOrder->m_account, pQueryOrder->m_symbol);
        const int orderSide = OrderSide(pQueryOrder->m_SideType, Key);
        Message::TOrderStatus OrderStatus{};
        XTrader::UpsertParallelMapEntry<Message::TOrderStatus>(
            m_OrderStatusMap, OrderRef,
            [&](Message::TOrderStatus& storedStatus) {
                storedStatus.BusinessType = m_XTraderConfig.BusinessType;
                // Update OrderStatus
                strncpy(storedStatus.Product, m_XTraderConfig.Product.c_str(), sizeof(storedStatus.Product));
                strncpy(storedStatus.Broker, m_XTraderConfig.Broker.c_str(), sizeof(storedStatus.Broker));
                strncpy(storedStatus.Account, pQueryOrder->m_account, sizeof(storedStatus.Account));
                strncpy(storedStatus.ExchangeID, ExchangeID(pQueryOrder->m_ExchengeID), sizeof(storedStatus.ExchangeID));
                strncpy(storedStatus.Ticker, pQueryOrder->m_symbol, sizeof(storedStatus.Ticker));
                strncpy(storedStatus.OrderRef, OrderRef.c_str(), sizeof(storedStatus.OrderRef));
                strncpy(storedStatus.OrderSysID, pQueryOrder->m_MarketOrderId, sizeof(storedStatus.OrderSysID));
                std::string OrderLocalID = fmt::format("{}", pQueryOrder->m_MarketOrderToken);
                strncpy(storedStatus.OrderLocalID, OrderLocalID.c_str(), sizeof(storedStatus.OrderLocalID));
                storedStatus.SendPrice = pQueryOrder->m_Price;
                storedStatus.SendVolume = pQueryOrder->m_Quantity;
                storedStatus.OrderToken = pQueryOrder->m_CustomField;

                if(EES_OrderTif_Day == pQueryOrder->m_Tif)
                {
                    storedStatus.OrderType = Message::EOrderType::ELIMIT;
                }
                else if(EES_OrderTif_IOC == pQueryOrder->m_Tif && 0 == pQueryOrder->m_MinQty)
                {
                    storedStatus.OrderType = Message::EOrderType::EFAK;
                }
                else if(EES_OrderTif_IOC == pQueryOrder->m_Tif && pQueryOrder->m_Quantity == pQueryOrder->m_MinQty)
                {
                    storedStatus.OrderType = Message::EOrderType::EFOK;
                }
                struct tm Timestamp;
                unsigned int NanoSec;
                m_TraderAPI->ConvertFromTimestamp(pQueryOrder->m_Timestamp, Timestamp, NanoSec);
                char szBuffer[32] = {0};
                strftime(szBuffer, sizeof(szBuffer), "%Y-%m-%d %H:%M:%S", &Timestamp);
                std::string InsertTime = std::string(szBuffer) + ".0000000";
                strncpy(storedStatus.InsertTime, InsertTime.c_str(), sizeof(storedStatus.InsertTime));
                strncpy(storedStatus.BrokerACKTime, InsertTime.c_str(), sizeof(storedStatus.BrokerACKTime));
                strncpy(storedStatus.ExchangeACKTime, InsertTime.c_str(), sizeof(storedStatus.ExchangeACKTime));
                status = pQueryOrder->m_OrderStatus & EES_OrderStatus_mkt_accept;
                if(EES_OrderStatus_mkt_accept == status)
                {
                    storedStatus.OrderStatus = Message::EOrderStatusType::EEXCHANGE_ACK;
                }
                status = pQueryOrder->m_OrderStatus & EES_OrderStatus_executed;
                if(EES_OrderStatus_executed == status)
                {
                    storedStatus.OrderStatus = Message::EOrderStatusType::EPARTTRADED;
                    storedStatus.TradedVolume = pQueryOrder->m_FilledQty;
                    storedStatus.TotalTradedVolume = pQueryOrder->m_FilledQty;
                    storedStatus.TradedPrice = pQueryOrder->m_Price;
                    storedStatus.TradedAvgPrice = pQueryOrder->m_Price;
                }
                storedStatus.OrderSide = orderSide;
                OrderStatus = storedStatus;
            });
        UpdateOrderStatus(OrderStatus);
        // Update Position
        XTrader::UpsertParallelMapEntry<Message::TAccountPosition>(
            m_TickerAccountPositionMap, Key, [](Message::TAccountPosition&) {});
        Message::TAccountPosition AccountPosition{};
        XTrader::ModifyParallelMapEntry(
            m_TickerAccountPositionMap, Key,
            [&](Message::TAccountPosition& storedPosition) {
                strncpy(storedPosition.UpdateTime, Utils::getCurrentTimeUs(), sizeof(storedPosition.UpdateTime));
                UpdateFrozenPosition(OrderStatus, storedPosition);
                AccountPosition = storedPosition;
            });

        PrintOrderStatus(OrderStatus, "REMTrader::OnQueryTradeOrder ");
        PrintAccountPosition(AccountPosition, "REMTrader::OnQueryTradeOrder ");
    }
    if(bFinish)
    {
        if(m_XTraderConfig.CancelAll)
        {
            const auto Orders =
                XTrader::SnapshotParallelMapValues<Message::TOrderStatus>(m_OrderStatusMap);
            for(const auto& OrderStatus : Orders)
            {
                CancelOrder(OrderStatus);
            }
        }
    }
    FMTLOG(fmtlog::INF, "REMTrader:OnQueryTradeOrder, Broker:{}, InvestorID:{} Account:{} InstrumentID:{} "
                        "m_Userid:{} m_Timestamp:{} m_ClientOrderToken:{} m_SideType:{} m_Quantity:{} "
                        "m_InstrumentType:{} m_Price:{} m_ExchengeID:{} m_MarketOrderToken:{} "
                        "m_OrderStatus:{} m_CloseTime:{} m_FilledQty:{} m_Tif:{} m_MinQty:{} "
                        "m_CustomField:{} m_MarketOrderId:{} m_HedgeFlag:{} bFinish:{}",
                m_XTraderConfig.Broker, m_XTraderConfig.Account,
                pQueryOrder->m_symbol, pQueryOrder->m_account, pQueryOrder->m_Userid, pQueryOrder->m_Timestamp,
                pQueryOrder->m_ClientOrderToken, pQueryOrder->m_SideType, pQueryOrder->m_Quantity,
                pQueryOrder->m_InstrumentType, pQueryOrder->m_Price, pQueryOrder->m_ExchengeID,
                pQueryOrder->m_MarketOrderToken, pQueryOrder->m_OrderStatus,
                pQueryOrder->m_CloseTime,  pQueryOrder->m_FilledQty,  pQueryOrder->m_Tif,  pQueryOrder->m_MinQty,
                pQueryOrder->m_CustomField, pQueryOrder->m_MarketOrderId, pQueryOrder->m_HedgeFlag, bFinish);
}

void REMTradeGateWay::OnQueryTradeOrderExec(const char* pAccount, EES_QueryOrderExecution* pQueryOrderExec, bool bFinish)
{
    if(NULL == pQueryOrderExec)
        return;
    FMTLOG(fmtlog::INF, "REMTrader:OnQueryTradeOrderExec, Broker:{}, InvestorID:{} Account:{} "
                        "m_Userid:{} m_Timestamp:{} m_ClientOrderToken:{} m_MarketOrderToken:{} m_ExecutedQuantity:{} "
                        "m_ExecutionPrice:{} m_ExecutionID:{} m_MarketExecID:{} bFinish:{}",
            m_XTraderConfig.Broker, m_XTraderConfig.Account, pAccount, pQueryOrderExec->m_Userid, pQueryOrderExec->m_Timestamp,
            pQueryOrderExec->m_ClientOrderToken, pQueryOrderExec->m_MarketOrderToken, pQueryOrderExec->m_ExecutedQuantity,
            pQueryOrderExec->m_ExecutionPrice, pQueryOrderExec->m_ExecutionID, pQueryOrderExec->m_MarketExecID, bFinish);
}

void REMTradeGateWay::OnQueryAccountTradeMargin(const char* pAccount, EES_AccountMargin* pSymbolMargin, bool bFinish)
{
    auto it = m_TickerSet.find(pSymbolMargin->m_symbol);
    if(it != m_TickerSet.end())
    {
        FMTLOG(fmtlog::INF, "REMTrader:OnQueryAccountTradeMargin, Account:{} m_symbol:{} "
                            "m_ExchangeID:{} m_ProdID:{} m_LongMarginRatio:{} m_ShortMarginRatio:{} bFinish:{}",
                pAccount, pSymbolMargin->m_symbol, pSymbolMargin->m_ExchangeID, 
                pSymbolMargin->m_ProdID, pSymbolMargin->m_LongMarginRatio, pSymbolMargin->m_ShortMarginRatio, bFinish);
    }
}

void REMTradeGateWay::OnQueryAccountTradeFee(const char* pAccount, EES_AccountFee* pSymbolFee, bool bFinish)
{
    auto it = m_TickerSet.find(pSymbolFee->m_symbol);
    if(it != m_TickerSet.end())
    {
        FMTLOG(fmtlog::INF, "REMTrader:OnQueryAccountTradeFee, Account:{}  m_ExchangeID:{} m_ProdID:{} m_symbol:{} "
                            "m_OpenRatioByMoney:{} m_OpenRatioByVolume:{} m_CloseYesterdayRatioByMoney:{} "
                            "m_CloseYesterdayRatioByVolume:{} m_CloseTodayRatioByMoney:{} m_CloseTodayRatioByVolume:{} m_PositionDir:{}",
                pAccount, pSymbolFee->m_ExchangeID, pSymbolFee->m_ProdID, pSymbolFee->m_symbol, 
                pSymbolFee->m_OpenRatioByMoney, pSymbolFee->m_OpenRatioByVolume, pSymbolFee->m_CloseYesterdayRatioByMoney, 
                pSymbolFee->m_CloseYesterdayRatioByVolume, pSymbolFee->m_CloseTodayRatioByMoney, pSymbolFee->m_CloseTodayRatioByVolume, 
                pSymbolFee->m_PositionDir);
    }
}
