#include "CTPTradeGateWay.h"
#include "XPluginEngine.hpp"

CreateObjectFunc(CTPTradeGateWay);

CTPTradeGateWay::CTPTradeGateWay()
{
    m_RequestID = 0;
    m_ConnectedStatus = Message::ELoginStatus::ELOGIN_PREPARED;
}

CTPTradeGateWay::~CTPTradeGateWay()
{
    DestroyTraderAPI();
}

void CTPTradeGateWay::LoadAPIConfig()
{
    std::string errorString;
    bool ok = Utils::LoadCTPConfig(m_XTraderConfig.TraderAPIConfig.c_str(), m_CTPConfig, errorString);
    if(ok)
    {
        FMTLOG(fmtlog::INF, "CTPTrader LoadAPIConfig Account:{} LoadCTPConfig successed, FrontAddr:{}", m_XTraderConfig.Account, m_CTPConfig.FrontAddr);
    }
    else
    {
        FMTLOG(fmtlog::ERR, "CTPTrader LoadAPIConfig Account:{} LoadCTPConfig failed, FrontAddr:{} {}", m_XTraderConfig.Account, m_CTPConfig.FrontAddr, errorString);
    }
    ok = Utils::LoadCTPError(m_XTraderConfig.ErrorPath.c_str(), m_CodeErrorMap, errorString);
    if(ok)
    {
        FMTLOG(fmtlog::INF, "CTPTrader LoadAPIConfig Account:{} LoadCTPError {} successed", m_XTraderConfig.Account, m_XTraderConfig.ErrorPath);
    }
    else
    {
        FMTLOG(fmtlog::ERR, "CTPTrader LoadAPIConfig Account:{} LoadCTPError {} failed, {}", m_XTraderConfig.Account, m_XTraderConfig.ErrorPath, errorString);
    }

    std::string app_log_path;
    char* p = getenv("APP_LOG_PATH");
    if(p == NULL)
    {
        char buffer[256] = {0};
        getcwd(buffer, sizeof(buffer));
        app_log_path = buffer;
    }
    else
    {
        app_log_path = p;
    }
    // 创建flow目录
    std::string flowPath = app_log_path;
    flowPath = app_log_path + "/flow/";
    mkdir(flowPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

void CTPTradeGateWay::GetCommitID(std::string& CommitID, std::string& UtilsCommitID)
{
    CommitID = SO_COMMITID;
    UtilsCommitID = SO_UTILS_COMMITID;
}

void CTPTradeGateWay::GetAPIVersion(std::string& APIVersion)
{
    APIVersion = API_VERSION;
}

void CTPTradeGateWay::CreateTraderAPI()
{
    std::string app_log_path;
    char* p = getenv("APP_LOG_PATH");
    if(p == NULL)
    {
        char buffer[256] = {0};
        getcwd(buffer, sizeof(buffer));
        app_log_path = buffer;
    }
    else
    {
        app_log_path = p;
    }
    // 指定CTP flow目录
    std::string flow = app_log_path + "/flow/" + m_XTraderConfig.Account;
    m_CTPTraderAPI = CThostFtdcTraderApi::CreateFtdcTraderApi(flow.c_str());
}

void CTPTradeGateWay::DestroyTraderAPI()
{
    if(NULL != m_CTPTraderAPI)
    {
        m_CTPTraderAPI->RegisterSpi(NULL);
        m_CTPTraderAPI->Release();
        m_CTPTraderAPI = NULL;
    }
}

void CTPTradeGateWay::LoadTrader()
{
    // 注册事件类
    m_CTPTraderAPI->RegisterSpi(this);
    m_CTPTraderAPI->SubscribePublicTopic(THOST_TERT_QUICK);
    m_CTPTraderAPI->SubscribePrivateTopic(THOST_TERT_QUICK);
    m_CTPTraderAPI->RegisterFront(const_cast<char*>(m_CTPConfig.FrontAddr.c_str()));
    m_CTPTraderAPI->Init();
    FMTLOG(fmtlog::INF, "CTPTrader::LoadTrader Account:{} Front:{}", m_XTraderConfig.Account, m_CTPConfig.FrontAddr);
}

void CTPTradeGateWay::ReLoadTrader()
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
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event), "CTPTrader::ReLoadTrader Account:{} ", m_XTraderConfig.Account);
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));
        FMTLOG(fmtlog::WRN, "CTPTrader::ReLoadTrader Account:{} ", m_XTraderConfig.Account);
    }
}

void CTPTradeGateWay::ReqUserLogin()
{
    CThostFtdcReqUserLoginField reqUserLogin;
    memset(&reqUserLogin, 0, sizeof(reqUserLogin));
    strcpy(reqUserLogin.BrokerID, m_XTraderConfig.BrokerID.c_str());
    strcpy(reqUserLogin.UserID, m_XTraderConfig.Account.c_str());
    strcpy(reqUserLogin.Password, m_XTraderConfig.Password.c_str());
    int ret = m_CTPTraderAPI->ReqUserLogin(&reqUserLogin, m_RequestID++);
    std::string buffer = "CTPTrader:ReqUserLogin Account:" + m_XTraderConfig.Account;
    HandleRetCode(ret, buffer);
}

int CTPTradeGateWay::ReqQryFund()
{
    CThostFtdcQryTradingAccountField  reqQryTradingAccountField;
    memset(&reqQryTradingAccountField, 0, sizeof(reqQryTradingAccountField));
    strcpy(reqQryTradingAccountField.BrokerID, m_XTraderConfig.BrokerID.c_str());
    strcpy(reqQryTradingAccountField.InvestorID, m_XTraderConfig.Account.c_str());
    int ret = m_CTPTraderAPI->ReqQryTradingAccount(&reqQryTradingAccountField, m_RequestID++);
    std::string buffer = "CTPTrader:ReqQryTradingAccount Account:" + m_XTraderConfig.Account;
    HandleRetCode(ret, buffer);
    return ret;
}
int CTPTradeGateWay::ReqQryOrder()
{
    CThostFtdcQryOrderField reqQryOrderField;
    memset(&reqQryOrderField, 0, sizeof(reqQryOrderField));
    strcpy(reqQryOrderField.BrokerID, m_XTraderConfig.BrokerID.c_str());
    strcpy(reqQryOrderField.InvestorID, m_XTraderConfig.Account.c_str());

    int ret = m_CTPTraderAPI->ReqQryOrder(&reqQryOrderField, m_RequestID++);
    std::string buffer = "CTPTrader:ReqQryOrder Account:" + m_XTraderConfig.Account;
    HandleRetCode(ret, buffer);
    return ret;
}
int CTPTradeGateWay::ReqQryTrade()
{
    CThostFtdcQryTradeField reqQryTradeField;
    memset(&reqQryTradeField, 0, sizeof(reqQryTradeField));
    strcpy(reqQryTradeField.BrokerID, m_XTraderConfig.BrokerID.c_str());
    strcpy(reqQryTradeField.InvestorID, m_XTraderConfig.Account.c_str());

    int ret = m_CTPTraderAPI->ReqQryTrade(&reqQryTradeField, m_RequestID++);
    std::string buffer = "CTPTrader:ReqQryTrade Account:" + m_XTraderConfig.Account;
    HandleRetCode(ret, buffer);
    return ret;
}
int CTPTradeGateWay::ReqQryPoistion()
{
    CThostFtdcQryInvestorPositionField  reqQryInvestorPositioField;
    memset(&reqQryInvestorPositioField, 0, sizeof(reqQryInvestorPositioField));
    strcpy(reqQryInvestorPositioField.BrokerID, m_XTraderConfig.BrokerID.c_str());
    strcpy(reqQryInvestorPositioField.InvestorID, m_XTraderConfig.Account.c_str());

    int ret = m_CTPTraderAPI->ReqQryInvestorPosition(&reqQryInvestorPositioField, m_RequestID++);
    std::string buffer = "CTPTrader:ReqQryInvestorPosition Account:" + m_XTraderConfig.Account;
    HandleRetCode(ret, buffer);
    return ret;
}

int CTPTradeGateWay::ReqQryTickerRate()
{
    std::string buffer;
    int ret = 0;
    // 查询合约保证金率
    CThostFtdcQryInstrumentMarginRateField MarginRateField;
    strncpy(MarginRateField.BrokerID, m_XTraderConfig.BrokerID.c_str(), sizeof(MarginRateField.BrokerID));
    strncpy(MarginRateField.InvestorID, m_XTraderConfig.Account.c_str(), sizeof(MarginRateField.InvestorID));
    MarginRateField.HedgeFlag = THOST_FTDC_HF_Speculation;
    for(int i = 0; i < m_TickerPropertyList.size(); i++)
    {
        std::string Ticker = m_TickerPropertyList.at(i).Ticker;
        strncpy(MarginRateField.InstrumentID, Ticker.c_str(), sizeof(MarginRateField.InstrumentID));
        strncpy(MarginRateField.ExchangeID, m_TickerPropertyList.at(i).ExchangeID.c_str(), sizeof(MarginRateField.ExchangeID));
        ret = m_CTPTraderAPI->ReqQryInstrumentMarginRate(&MarginRateField, m_RequestID++);
        buffer = "CTPTrader:ReqQryInstrumentMarginRate Account:" + m_XTraderConfig.Account + " Ticker:" + Ticker;
        HandleRetCode(ret, buffer);
        usleep(1000*1000);
    }
    // 查询保证金计算价格类型
    CThostFtdcQryBrokerTradingParamsField BrokerTradingParamsField;
    strncpy(BrokerTradingParamsField.BrokerID, m_XTraderConfig.BrokerID.c_str(), sizeof(BrokerTradingParamsField.BrokerID));
    strncpy(BrokerTradingParamsField.InvestorID, m_XTraderConfig.Account.c_str(), sizeof(BrokerTradingParamsField.InvestorID));
    ret = m_CTPTraderAPI->ReqQryBrokerTradingParams(&BrokerTradingParamsField, m_RequestID++);
    buffer = "CTPTrader:ReqQryBrokerTradingParams Account:" + m_XTraderConfig.Account;
    HandleRetCode(ret, buffer);
    usleep(1000*1000);
    // 查询合约单向大边
    CThostFtdcQryInstrumentField InstrumentField;
    for(int i = 0; i < m_TickerPropertyList.size(); i++)
    {
        std::string Ticker = m_TickerPropertyList.at(i).Ticker;
        strncpy(InstrumentField.InstrumentID, Ticker.c_str(), sizeof(InstrumentField.InstrumentID));
        strncpy(InstrumentField.ExchangeID, m_TickerPropertyList.at(i).ExchangeID.c_str(), sizeof(InstrumentField.ExchangeID));
        ret = m_CTPTraderAPI->ReqQryInstrument(&InstrumentField, m_RequestID++);
        buffer = "CTPTrader:ReqQryInstrument Account:" + m_XTraderConfig.Account + " Ticker:" + Ticker;
        HandleRetCode(ret, buffer);
        usleep(1000*1000);
    }
    // 查询合约手续费率
    CThostFtdcQryInstrumentCommissionRateField CommissionRateField;
    strncpy(CommissionRateField.BrokerID, m_XTraderConfig.BrokerID.c_str(), sizeof(CommissionRateField.BrokerID));
    strncpy(CommissionRateField.InvestorID, m_XTraderConfig.Account.c_str(), sizeof(CommissionRateField.InvestorID));
    for(int i = 0; i < m_TickerPropertyList.size(); i++)
    {
        std::string Ticker = m_TickerPropertyList.at(i).Ticker;
        strncpy(CommissionRateField.InstrumentID, Ticker.c_str(), sizeof(CommissionRateField.InstrumentID));
        strncpy(CommissionRateField.ExchangeID, m_TickerPropertyList.at(i).ExchangeID.c_str(), sizeof(CommissionRateField.ExchangeID));
        ret = m_CTPTraderAPI->ReqQryInstrumentCommissionRate(&CommissionRateField, m_RequestID++);
        buffer = "CTPTrader:ReqQryInstrumentCommissionRate Account:" + m_XTraderConfig.Account + " Ticker:" + Ticker;
        HandleRetCode(ret, buffer);
        usleep(1000*1000);
    }
    // 查询合约报单手续费
    CThostFtdcQryInstrumentOrderCommRateField OrderCommRateField;
    strncpy(OrderCommRateField.BrokerID, m_XTraderConfig.BrokerID.c_str(), sizeof(OrderCommRateField.BrokerID));
    strncpy(OrderCommRateField.InvestorID, m_XTraderConfig.Account.c_str(), sizeof(OrderCommRateField.InvestorID));
    for(int i = 0; i < m_TickerPropertyList.size(); i++)
    {
        std::string Ticker = m_TickerPropertyList.at(i).Ticker;
        strncpy(OrderCommRateField.InstrumentID, Ticker.c_str(), sizeof(OrderCommRateField.InstrumentID));
        ret = m_CTPTraderAPI->ReqQryInstrumentOrderCommRate(&OrderCommRateField, m_RequestID++);
        buffer = "CTPTrader:ReqQryInstrumentOrderCommRate Account:" + m_XTraderConfig.Account + " Ticker:" + Ticker;
        HandleRetCode(ret, buffer);
        usleep(1000*1000);
    }
    return 0;
}

void CTPTradeGateWay::ReqInsertOrder(const Message::TOrderRequest& request)
{
    CThostFtdcInputOrderField  reqOrderField;
    memset(&reqOrderField, 0, sizeof(reqOrderField));
    strcpy(reqOrderField.BrokerID, m_XTraderConfig.BrokerID.c_str());
    strcpy(reqOrderField.InvestorID, m_XTraderConfig.Account.c_str());
    strcpy(reqOrderField.InstrumentID, request.Ticker);
    strcpy(reqOrderField.ExchangeID, request.ExchangeID);
    strcpy(reqOrderField.UserID, m_UserID.c_str());
    int orderID = Utils::getCurrentTodaySec() * 10000 + m_RequestID++;
    fmt::format_to_n(reqOrderField.OrderRef, sizeof(reqOrderField.OrderRef), "{:09}", orderID);
    reqOrderField.OrderPriceType = THOST_FTDC_OPT_LimitPrice;//限价
    if(Message::EOrderDirection::EBUY == request.Direction)
    {
        reqOrderField.Direction = THOST_FTDC_D_Buy;
    }
    else
    {
        reqOrderField.Direction = THOST_FTDC_D_Sell;
    }
    if(Message::EOrderOffset::EOPEN == request.Offset)
    {
        reqOrderField.CombOffsetFlag[0] = THOST_FTDC_OF_Open;
    }
    else if(Message::EOrderOffset::ECLOSE == request.Offset)
    {
        reqOrderField.CombOffsetFlag[0] = THOST_FTDC_OF_Close;
    }
    else if(Message::EOrderOffset::ECLOSE_TODAY == request.Offset)
    {
        reqOrderField.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;
    }
    else if(Message::EOrderOffset::ECLOSE_YESTODAY == request.Offset)
    {
        reqOrderField.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;//
    }

    reqOrderField.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;//投机
    reqOrderField.LimitPrice = request.Price;
    reqOrderField.VolumeTotalOriginal = request.Volume;
    // reqOrderField.MinVolume = 1;
    reqOrderField.ContingentCondition = THOST_FTDC_CC_Immediately;
    reqOrderField.StopPrice = 0;
    reqOrderField.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
    reqOrderField.IsAutoSuspend = 0;
    reqOrderField.RequestID = request.OrderToken;

    if(Message::EOrderType::EFOK == request.OrderType)
    {
        reqOrderField.TimeCondition = THOST_FTDC_TC_IOC;
        reqOrderField.VolumeCondition = THOST_FTDC_VC_CV;
    }
    else if (Message::EOrderType::EFAK == request.OrderType)
    {
        reqOrderField.TimeCondition = THOST_FTDC_TC_IOC;
        reqOrderField.VolumeCondition = THOST_FTDC_VC_AV;
    }
    else if (Message::EOrderType::ELIMIT == request.OrderType)
    {
        reqOrderField.TimeCondition = THOST_FTDC_TC_GFD;
        reqOrderField.VolumeCondition = THOST_FTDC_VC_AV;
    }
    // Order Status
    Message::TOrderStatus& OrderStatus = m_OrderStatusMap[reqOrderField.OrderRef];
    OrderStatus.BusinessType = m_XTraderConfig.BusinessType;
    strncpy(OrderStatus.Product, m_XTraderConfig.Product.c_str(), sizeof(OrderStatus.Product));
    strncpy(OrderStatus.Broker, m_XTraderConfig.Broker.c_str(), sizeof(OrderStatus.Broker));
    strncpy(OrderStatus.Account, reqOrderField.InvestorID, sizeof(OrderStatus.Account));
    strncpy(OrderStatus.ExchangeID, request.ExchangeID, sizeof(OrderStatus.ExchangeID));
    strncpy(OrderStatus.Ticker, reqOrderField.InstrumentID, sizeof(OrderStatus.Ticker));
    strncpy(OrderStatus.OrderRef, reqOrderField.OrderRef, sizeof(OrderStatus.OrderRef));
    strncpy(OrderStatus.RiskID, request.RiskID, sizeof(OrderStatus.RiskID));
    OrderStatus.SendPrice = reqOrderField.LimitPrice;
    OrderStatus.SendVolume = reqOrderField.VolumeTotalOriginal;
    OrderStatus.OrderType = request.OrderType;
    OrderStatus.OrderToken = request.OrderToken;
    OrderStatus.EngineID = request.EngineID;
    std::string Account = reqOrderField.InvestorID;
    std::string Ticker = reqOrderField.InstrumentID;
    std::string Key = Account + ":" + Ticker;
    OrderStatus.OrderSide = OrderSide(reqOrderField.Direction, reqOrderField.CombOffsetFlag[0], Key);
    strncpy(OrderStatus.SendTime, request.SendTime, sizeof(OrderStatus.SendTime));
    strncpy(OrderStatus.InsertTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.InsertTime));
    strncpy(OrderStatus.RecvMarketTime, request.RecvMarketTime, sizeof(OrderStatus.RecvMarketTime));
    OrderStatus.OrderStatus = Message::EOrderStatusType::EORDER_SENDED;
    PrintOrderStatus(OrderStatus, "CTPTrader::ReqInsertOrder ");
    
    int ret = m_CTPTraderAPI->ReqOrderInsert(&reqOrderField, m_RequestID);
    std::string op = std::string("CTPTrader:ReqOrderInsert Account:") + request.Account + " Ticker:"
                     + request.Ticker + " OrderRef:" + reqOrderField.OrderRef;
    HandleRetCode(ret, op);
    if(0 == ret)
    {
        Message::TAccountPosition& AccountPosition = m_TickerAccountPositionMap[Key];
        UpdatePosition(OrderStatus, AccountPosition);
        UpdateOrderStatus(OrderStatus);
        FMTLOG(fmtlog::DBG, "CTPTrader:ReqInsertOrder, InvestorID:{} ExchangeID:{} Ticker:{} UserID:{} OrderPriceType:{} Direction:{} "
                            "CombOffsetFlag:{} CombHedgeFlag:{} LimitPrice:{} VolumeTotalOriginal:{} MinVolume:{} ContingentCondition:{} "
                            "StopPrice:{} ForceCloseReason:{} IsAutoSuspend:{} TimeCondition:{} VolumeCondition:{} RequestID:{}",
                reqOrderField.InvestorID, reqOrderField.ExchangeID, reqOrderField.InstrumentID,
                reqOrderField.UserID, reqOrderField.OrderPriceType, reqOrderField.Direction, reqOrderField.CombOffsetFlag,
                reqOrderField.CombHedgeFlag, reqOrderField.LimitPrice, reqOrderField.VolumeTotalOriginal,
                reqOrderField.MinVolume, reqOrderField.ContingentCondition, reqOrderField.StopPrice, reqOrderField.ForceCloseReason,
                reqOrderField.IsAutoSuspend, reqOrderField.TimeCondition, reqOrderField.VolumeCondition, reqOrderField.RequestID);
    }
    else
    {
        m_OrderStatusMap.erase(reqOrderField.OrderRef);
    }
}

void CTPTradeGateWay::ReqInsertOrderRejected(const Message::TOrderRequest& request)
{
    CThostFtdcInputOrderField  reqOrderField;
    int orderID = Utils::getCurrentTodaySec() * 10000 + m_RequestID++;
    if(Message::EOrderDirection::EBUY == request.Direction)
    {
        reqOrderField.Direction = THOST_FTDC_D_Buy;
    }
    else
    {
        reqOrderField.Direction = THOST_FTDC_D_Sell;
    }
    if(Message::EOrderOffset::EOPEN == request.Offset)
    {
        reqOrderField.CombOffsetFlag[0] = THOST_FTDC_OF_Open;
    }
    else if(Message::EOrderOffset::ECLOSE == request.Offset)
    {
        reqOrderField.CombOffsetFlag[0] = THOST_FTDC_OF_Close;
    }
    else if(Message::EOrderOffset::ECLOSE_TODAY == request.Offset)
    {
        reqOrderField.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;
    }
    else if(Message::EOrderOffset::ECLOSE_YESTODAY == request.Offset)
    {
        reqOrderField.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;//
    }
    // Order Status
    Message::TOrderStatus OrderStatus;
    memset(&OrderStatus, 0, sizeof(OrderStatus));
    OrderStatus.BusinessType = m_XTraderConfig.BusinessType;
    strncpy(OrderStatus.Product, m_XTraderConfig.Product.c_str(), sizeof(OrderStatus.Product));
    strncpy(OrderStatus.Broker, m_XTraderConfig.Broker.c_str(), sizeof(OrderStatus.Broker));
    strncpy(OrderStatus.Account, m_XTraderConfig.Account.c_str(), sizeof(OrderStatus.Account));
    strncpy(OrderStatus.ExchangeID, request.ExchangeID, sizeof(OrderStatus.ExchangeID));
    strncpy(OrderStatus.Ticker, request.Ticker, sizeof(OrderStatus.Ticker));
    fmt::format_to_n(OrderStatus.OrderRef, sizeof(OrderStatus.OrderRef), "{:09}", orderID);
    strncpy(OrderStatus.RiskID, request.RiskID, sizeof(OrderStatus.RiskID));
    OrderStatus.SendPrice = request.Price;
    OrderStatus.SendVolume = request.Volume;
    OrderStatus.OrderType = request.OrderType;
    OrderStatus.OrderToken = request.OrderToken;
    OrderStatus.EngineID = request.EngineID;
    std::string Account = request.Account;
    std::string Ticker = request.Ticker;
    std::string Key = Account + ":" + Ticker;
    OrderStatus.OrderSide = OrderSide(reqOrderField.Direction, reqOrderField.CombOffsetFlag[0], Key);
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
    PrintOrderStatus(OrderStatus, "CTPTrader::ReqInsertOrderRejected ");
    UpdateOrderStatus(OrderStatus);
}

void CTPTradeGateWay::ReqCancelOrder(const Message::TActionRequest& request)
{
    auto it = m_OrderStatusMap.find(request.OrderRef);
    if(it == m_OrderStatusMap.end())
    {
        FMTLOG(fmtlog::WRN, "CTPTrader::ReqCancelOrder Account:{} OrderRef:{} not found.", request.Account, request.OrderRef);
        return ;
    }
    Message::TOrderStatus& OrderStatus = m_OrderStatusMap[request.OrderRef];
    CThostFtdcInputOrderActionField   reqOrderField;
    memset(&reqOrderField, 0, sizeof(reqOrderField));
    strcpy(reqOrderField.BrokerID, m_XTraderConfig.BrokerID.c_str());
    strcpy(reqOrderField.InvestorID, OrderStatus.Account);
    strcpy(reqOrderField.UserID, m_UserID.c_str());
    strcpy(reqOrderField.ExchangeID, OrderStatus.ExchangeID);
    reqOrderField.ActionFlag = THOST_FTDC_AF_Delete;
    strcpy(reqOrderField.OrderSysID, OrderStatus.OrderSysID);
    strcpy(reqOrderField.OrderRef, OrderStatus.OrderRef);
    reqOrderField.OrderActionRef = Utils::getCurrentTodaySec() * 10000 + m_RequestID++;

    int ret = m_CTPTraderAPI->ReqOrderAction(&reqOrderField, m_RequestID);
    std::string op = std::string("CTPTrader:ReqOrderAction Account:") + OrderStatus.Account + " OrderRef:"
                     + OrderStatus.OrderRef + " OrderLocalID:" + OrderStatus.OrderLocalID + " OrderSysID:" + OrderStatus.OrderSysID;
    HandleRetCode(ret, op);
    if(0 == ret)
    {
        OrderStatus.OrderStatus = Message::EOrderStatusType::ECANCELLING;
    }
    UpdateOrderStatus(OrderStatus);
    PrintOrderStatus(OrderStatus, "CTPTrader::ReqCancelOrder ");
}

void CTPTradeGateWay::ReqCancelOrderRejected(const Message::TActionRequest& request)
{
    auto it = m_OrderStatusMap.find(request.OrderRef);
    if(it == m_OrderStatusMap.end())
    {
        FMTLOG(fmtlog::WRN, "CTPTrader::ReqCancelOrderRejected Account:{} OrderRef:{} not found.", request.Account, request.OrderRef);
        return ;
    }
    Message::TOrderStatus& OrderStatus = m_OrderStatusMap[request.OrderRef];
    OrderStatus.OrderStatus = Message::EOrderStatusType::ERISK_ACTION_REJECTED;
    OrderStatus.ErrorID = request.ErrorID;
    strncpy(OrderStatus.ErrorMsg, request.ErrorMsg, sizeof(OrderStatus.ErrorMsg));
    UpdateOrderStatus(OrderStatus);
    PrintOrderStatus(OrderStatus, "CTPTrader::ReqCancelOrderRejected ");
}

void CTPTradeGateWay::ReqAuthenticate()
{
    CThostFtdcReqAuthenticateField reqAuthenticateField;
    memset(&reqAuthenticateField, 0, sizeof(reqAuthenticateField));
    strcpy(reqAuthenticateField.BrokerID, m_XTraderConfig.BrokerID.c_str());
    strcpy(reqAuthenticateField.UserID, m_XTraderConfig.Account.c_str());
    strcpy(reqAuthenticateField.AppID, m_XTraderConfig.AppID.c_str());
    strcpy(reqAuthenticateField.AuthCode, m_XTraderConfig.AuthCode.c_str());
    int ret = m_CTPTraderAPI->ReqAuthenticate(&reqAuthenticateField, m_RequestID++);
    std::string buffer = "CTPTrader:ReqAuthenticate Account:" + m_XTraderConfig.Account;
    HandleRetCode(ret, buffer);
}

void CTPTradeGateWay::ReqSettlementInfoConfirm()
{
    CThostFtdcSettlementInfoConfirmField confirm;
    memset(&confirm, 0, sizeof(confirm));
    strcpy(confirm.BrokerID, m_XTraderConfig.BrokerID.c_str());
    strcpy(confirm.InvestorID, m_XTraderConfig.Account.c_str());
    int ret = m_CTPTraderAPI->ReqSettlementInfoConfirm(&confirm, m_RequestID++);
    std::string buffer = "CTPTrader:ReqSettlementInfoConfirm Account:" + m_XTraderConfig.Account;
    HandleRetCode(ret, buffer);
}

void CTPTradeGateWay::HandleRetCode(int code, const std::string& op)
{
    std::string Account = m_XTraderConfig.Account;
    std::string errorString = Account + " ";
    switch(code)
    {
    case 0:
        errorString = op + " successed";
        FMTLOG(fmtlog::INF, "{} successed", op);
        break;
    case -1:
        errorString = op + " failed, 网络连接失败.";
        FMTLOG(fmtlog::WRN, "{} failed, 网络连接失败.", op);
        break;
    case -2:
        errorString = op + " failed, 未处理请求超过许可数.";
        FMTLOG(fmtlog::WRN, "{} failed, 未处理请求超过许可数.", op);
        break;
    case -3:
        errorString = op + " failed, 每秒发送请求数超过许可数.";
        FMTLOG(fmtlog::WRN, "{} failed, 每秒发送请求数超过许可数.", op);
        break;
    default:
        errorString = op + " failed, unkown error.";
        FMTLOG(fmtlog::WRN, "{} failed, unkown error.", op);
        break;
    }
    // 错误发送监控EventLog
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

int CTPTradeGateWay::OrderSide(char direction, char offset, const std::string& Key)
{
    int side = -1;
    Message::TAccountPosition& position = m_TickerAccountPositionMap[Key];
    if(THOST_FTDC_D_Buy == direction && THOST_FTDC_OF_Open == offset)
    {
        side = Message::EOrderSide::EOPEN_LONG;
    }
    else if(THOST_FTDC_D_Sell == direction && THOST_FTDC_OF_CloseToday == offset)
    {
        side = Message::EOrderSide::ECLOSE_TD_LONG;
    }
    else if(THOST_FTDC_D_Sell == direction && THOST_FTDC_OF_CloseYesterday == offset)
    {
        side = Message::EOrderSide::ECLOSE_YD_LONG;
    }
    if(THOST_FTDC_D_Sell == direction && THOST_FTDC_OF_Open == offset)
    {
        side = Message::EOrderSide::EOPEN_SHORT;
    }
    else if(THOST_FTDC_D_Buy == direction && THOST_FTDC_OF_CloseToday == offset)
    {
        side = Message::EOrderSide::ECLOSE_TD_SHORT;
    }
    else if(THOST_FTDC_D_Buy == direction && THOST_FTDC_OF_CloseYesterday == offset)
    {
        side = Message::EOrderSide::ECLOSE_YD_SHORT;
    }
    else if(THOST_FTDC_D_Buy == direction && THOST_FTDC_OF_Close == offset)
    {
        if(position.FuturePosition.ShortTdVolume > 0)
        {
            side = Message::EOrderSide::ECLOSE_TD_SHORT;
        }
        else
        {
            side = Message::EOrderSide::ECLOSE_YD_SHORT;
        }
    }
    else if(THOST_FTDC_D_Sell == direction && THOST_FTDC_OF_Close == offset)
    {
        if(position.FuturePosition.LongTdVolume > 0)
        {
            side = Message::EOrderSide::ECLOSE_TD_LONG;
        }
        else
        {
            side = Message::EOrderSide::ECLOSE_YD_LONG;
        }
    }
    return side;
}

int CTPTradeGateWay::Ordertype(char timec, char volumec)
{
    int ret = 0;
    if(THOST_FTDC_TC_GFD == timec && THOST_FTDC_VC_AV == volumec)
    {
        ret = Message::EOrderType::ELIMIT;
    }
    else if(THOST_FTDC_TC_IOC == timec && THOST_FTDC_VC_AV == volumec)
    {
        ret = Message::EOrderType::EFAK;
    }
    else if(THOST_FTDC_TC_IOC == timec && THOST_FTDC_VC_CV == volumec)
    {
        ret = Message::EOrderType::EFOK;
    }
    return ret;
}

bool CTPTradeGateWay::IsRspError(CThostFtdcRspInfoField *pRspInfo)
{
    return pRspInfo != NULL && pRspInfo->ErrorID > 0;
}

void CTPTradeGateWay::OnFrontConnected()
{
    m_ConnectedStatus = Message::ELoginStatus::ELOGIN_CONNECTED;
    FMTLOG(fmtlog::INF, "CTPTrader::OnFrontConnected Account:{} TradingDay:{} Connected to Front:{}, API:{}",
            m_XTraderConfig.Account, m_CTPTraderAPI->GetTradingDay(), m_CTPConfig.FrontAddr, m_CTPTraderAPI->GetApiVersion());
    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EEventLog;
    message.EventLog.Level = Message::EEventLogLevel::EINFO;
    strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
    strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
    strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
    strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
    fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event), 
                    "CTPTrader::OnFrontConnected Account:{} TradingDay:{} Connected to Front:{}, API:{}",
                    m_XTraderConfig.Account, m_CTPTraderAPI->GetTradingDay(), m_CTPConfig.FrontAddr, m_CTPTraderAPI->GetApiVersion());
    strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
    while(!m_ReportMessageQueue.Push(message));
    // 请求身份认证
    ReqAuthenticate();
}

void CTPTradeGateWay::OnFrontDisconnected(int nReason)
{
    m_ConnectedStatus = Message::ELoginStatus::ELOGIN_FAILED;
    std::string buffer;
    auto it = m_CodeErrorMap.find(nReason);
    if(it != m_CodeErrorMap.end())
    {
        buffer = it->second;
    }
    else
    {
        buffer = "Unkown Error";
    }
    FMTLOG(fmtlog::WRN, "CTPTrader::OnFrontDisconnected Account:{} Code:{:#X}, Error:{}",
            m_XTraderConfig.Account, nReason, buffer);
    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EEventLog;
    message.EventLog.Level = Message::EEventLogLevel::EERROR;
    strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
    strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
    strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
    strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
    fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event), 
                    "CTPTrader::OnFrontDisconnected Account:{} Code:{:#X}, Error:{}",
                    m_XTraderConfig.Account, nReason, buffer);
    strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
    while(!m_ReportMessageQueue.Push(message));
}

void CTPTradeGateWay::OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
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
    if(!IsRspError(pRspInfo) && pRspAuthenticateField != NULL)
    {
        FMTLOG(fmtlog::INF, "CTPTrader::OnRspAuthenticate Authenticate successed, BrokerID:{}, Account:{}, UserID:{}, AppID:{}",
                        pRspAuthenticateField->BrokerID,  m_XTraderConfig.Account, pRspAuthenticateField->UserID, pRspAuthenticateField->AppID);
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event), 
                        "CTPTrader::OnRspAuthenticate Authenticate successed, BrokerID:{}, Account:{}, UserID:{}, AppID:{}",
                        pRspAuthenticateField->BrokerID,  m_XTraderConfig.Account, pRspAuthenticateField->UserID, pRspAuthenticateField->AppID);
        while(!m_ReportMessageQueue.Push(message));

        ReqUserLogin();
    }
    else if(NULL != pRspInfo)
    {
        char errorBuffer[512] = {0};
        Utils::CodeConvert(pRspInfo->ErrorMsg, sizeof(pRspInfo->ErrorMsg), errorBuffer,
                       sizeof(errorBuffer), "gb2312", "utf-8");
        FMTLOG(fmtlog::WRN, "CTPTrader::OnRspAuthenticate Authenticate failed, BrokerID:{}, Account:{}, ErrorID:{}, ErrorMessage:{}",
                m_XTraderConfig.BrokerID, m_XTraderConfig.Account, pRspInfo->ErrorID, errorBuffer);
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event),
                        "CTPTrader::OnRspAuthenticate Authenticate failed, BrokerID:{}, Account:{}, ErrorID:{}, ErrorMessage:{}",
                        m_XTraderConfig.BrokerID, m_XTraderConfig.Account, pRspInfo->ErrorID, errorBuffer);
        while(!m_ReportMessageQueue.Push(message));
    }
}

void CTPTradeGateWay::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    m_UserID = pRspUserLogin->UserID;
    m_FrontID = pRspUserLogin->FrontID;
    m_SessionID = pRspUserLogin->SessionID;
    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EEventLog;
    message.EventLog.Level = Message::EEventLogLevel::EINFO;
    strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
    strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
    strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
    strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
    strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
    // 登录成功
    if(!IsRspError(pRspInfo) && pRspUserLogin != NULL)
    {
        FMTLOG(fmtlog::INF, "CTPTrader::OnRspUserLogin Login successed, BrokerID:{}, Account:{}, TradingDay:{}, LoginTime:{}, SystemName:{}, MaxOrderRef:{}, FrontID:{}, SessionID:{}",
                        pRspUserLogin->BrokerID,  m_XTraderConfig.Account, pRspUserLogin->TradingDay, pRspUserLogin->LoginTime,
                        pRspUserLogin->SystemName, pRspUserLogin->MaxOrderRef, pRspUserLogin->FrontID, pRspUserLogin->SessionID);
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event),
                        "CTPTrader::OnRspUserLogin Login successed, BrokerID:{}, Account:{}, TradingDay:{}, LoginTime:{}, SystemName:{}, MaxOrderRef:{}, FrontID:{}, SessionID:{}",
                        pRspUserLogin->BrokerID,  m_XTraderConfig.Account, pRspUserLogin->TradingDay, pRspUserLogin->LoginTime,
                        pRspUserLogin->SystemName, pRspUserLogin->MaxOrderRef, pRspUserLogin->FrontID, pRspUserLogin->SessionID);
        while(!m_ReportMessageQueue.Push(message));
        // 请求结算单确认
        ReqSettlementInfoConfirm();
    }
    else if(NULL != pRspInfo)
    {
        char errorBuffer[512] = {0};
        Utils::CodeConvert(pRspInfo->ErrorMsg, sizeof(pRspInfo->ErrorMsg), errorBuffer,
                       sizeof(errorBuffer), "gb2312", "utf-8");
        FMTLOG(fmtlog::WRN, "CTPTrader::OnRspUserLogin Login failed, BrokerID:{}, Account:{}, ErrorID:{}, ErrorMessage:{}",
                m_XTraderConfig.BrokerID, m_XTraderConfig.Account, pRspInfo->ErrorID, errorBuffer);
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event),
                        "CTPTrader::OnRspUserLogin Login failed, BrokerID:{}, Account:{}, ErrorID:{}, ErrorMessage:{}",
                        m_XTraderConfig.BrokerID, m_XTraderConfig.Account, pRspInfo->ErrorID, errorBuffer);
        while(!m_ReportMessageQueue.Push(message));
    }
}

void CTPTradeGateWay::OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EEventLog;
    message.EventLog.Level = Message::EEventLogLevel::EINFO;

    if(!IsRspError(pRspInfo) && pUserLogout != NULL)
    {
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event),
                        "CTPTrader::Logout successed BrokerID:{}, Account:{}, UserID:{}", 
                        pUserLogout->BrokerID, m_XTraderConfig.Account, pUserLogout->UserID);
        FMTLOG(fmtlog::INF, "CTPTrader::Logout successed, BrokerID:{}, Account:{}, UserID:{}", 
                pUserLogout->BrokerID, m_XTraderConfig.Account, pUserLogout->UserID);
    }
    else if(NULL != pRspInfo)
    {
        char errorBuffer[512] = {0};
        Utils::CodeConvert(pRspInfo->ErrorMsg, sizeof(pRspInfo->ErrorMsg), errorBuffer,
                       sizeof(errorBuffer), "gb2312", "utf-8");
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event),
                        "CTPTrader::Logout failed BrokerID:{}, Account:{}, ErrorID:{}, ErrorMessage:{}", 
                        pUserLogout->BrokerID, m_XTraderConfig.Account, pRspInfo->ErrorID, errorBuffer);
        FMTLOG(fmtlog::WRN, "CTPTrader::Logout failed BrokerID:{}, Account:{}, ErrorID:{}, ErrorMessage:{}", 
                pUserLogout->BrokerID, m_XTraderConfig.Account, pRspInfo->ErrorID, errorBuffer);
    }
    strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
    strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
    strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
    strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
    strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
    while(!m_ReportMessageQueue.Push(message));
}

void CTPTradeGateWay::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
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
    if(!IsRspError(pRspInfo) && pSettlementInfoConfirm != NULL)
    {
        m_ConnectedStatus = Message::ELoginStatus::ELOGIN_SUCCESSED;
        FMTLOG(fmtlog::INF, "CTPTrader::OnRspSettlementInfoConfirm successed, BrokerID:{} Account:{}", 
                pSettlementInfoConfirm->BrokerID, pSettlementInfoConfirm->InvestorID);
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event), 
                        "CTPTrader::OnRspSettlementInfoConfirm successed, BrokerID:{} Account:{}", 
                        pSettlementInfoConfirm->BrokerID, pSettlementInfoConfirm->InvestorID);
        while(!m_ReportMessageQueue.Push(message));
        // 初始化仓位
        InitPosition();
    }
    else if(NULL != pRspInfo)
    {
        char errorBuffer[512] = {0};
        Utils::CodeConvert(pRspInfo->ErrorMsg, sizeof(pRspInfo->ErrorMsg), errorBuffer,
                       sizeof(errorBuffer), "gb2312", "utf-8");
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event),
                        "CTPTrader::OnRspSettlementInfoConfirm failed, BrokerID:{} Account:{}, ErrorID:{}, ErrorMessage:{}",
                        m_XTraderConfig.BrokerID, m_XTraderConfig.Account, pRspInfo->ErrorID, errorBuffer);
        FMTLOG(fmtlog::WRN, "CTPTrader::OnRspSettlementInfoConfirm failed, BrokerID:{} Account:{}, ErrorID:{}, ErrorMessage:{}",
                m_XTraderConfig.BrokerID, m_XTraderConfig.Account, pRspInfo->ErrorID, errorBuffer);
        while(!m_ReportMessageQueue.Push(message));
    }
}

void CTPTradeGateWay::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(IsRspError(pRspInfo) && pInputOrder != NULL)
    {
        char errorBuffer[512] = {0};
        Utils::CodeConvert(pRspInfo->ErrorMsg, sizeof(pRspInfo->ErrorMsg), errorBuffer,
                        sizeof(errorBuffer), "gb2312", "utf-8");
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspOrderInsert, BrokerID:{}, Account:{} InstrumentID:{} OrderRef:{} UserID:{} OrderPriceType:{} "
                            "Direction:{} CombOffsetFlag:{} CombHedgeFlag:{} LimitPrice:{} VolumeTotalOriginal:{} TimeCondition:{} "
                            "VolumeCondition:{} MinVolume:{} ContingentCondition:{} StopPrice:{} ForceCloseReason:{} IsAutoSuspend:{} "
                            "BusinessUnit:{} RequestID:{} UserForceClose:{} IsSwapOrder:{} ExchangeID:{} InvestUnitID:{} AccountID:{} "
                            "ClientID:{} MacAddress:{} IPAddress:{} ErrorID:{} ErrorMsg:{}",
                pInputOrder->BrokerID, pInputOrder->InvestorID, pInputOrder->InstrumentID, pInputOrder->OrderRef,
                pInputOrder->UserID, pInputOrder->OrderPriceType, pInputOrder->Direction, pInputOrder->CombOffsetFlag,
                pInputOrder->CombHedgeFlag, pInputOrder->LimitPrice, pInputOrder->VolumeTotalOriginal,
                pInputOrder->TimeCondition, pInputOrder->VolumeCondition, pInputOrder->MinVolume,
                pInputOrder->ContingentCondition, pInputOrder->StopPrice, pInputOrder->ForceCloseReason,
                pInputOrder->IsAutoSuspend, pInputOrder->BusinessUnit, pInputOrder->RequestID, pInputOrder->UserForceClose,
                pInputOrder->IsSwapOrder, pInputOrder->ExchangeID, pInputOrder->InvestUnitID, pInputOrder->AccountID,
                pInputOrder->ClientID, pInputOrder->MacAddress, pInputOrder->IPAddress, pRspInfo->ErrorID, errorBuffer);
    }
}

void CTPTradeGateWay::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo)
{
    if(IsRspError(pRspInfo) && pInputOrder != NULL)
    {
        // OrderStatus
        std::string OrderRef = pInputOrder->OrderRef;
        auto it1 = m_OrderStatusMap.find(OrderRef);
        if(it1 != m_OrderStatusMap.end())
        {
            Message::TOrderStatus& OrderStatus = it1->second;
            strncpy(OrderStatus.BrokerACKTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.BrokerACKTime));
            OrderStatus.OrderStatus = Message::EOrderStatusType::EBROKER_ERROR;
            OrderStatus.CanceledVolume = OrderStatus.SendVolume;
            Utils::CodeConvert(pRspInfo->ErrorMsg, sizeof(pRspInfo->ErrorMsg), OrderStatus.ErrorMsg,
                            sizeof(OrderStatus.ErrorMsg), "gb2312", "utf-8");
            OrderStatus.ErrorID = pRspInfo->ErrorID;
            std::string Key = std::string(OrderStatus.Account) + ":" + OrderStatus.Ticker;
            // Update Position
            Message::TAccountPosition& AccountPosition = m_TickerAccountPositionMap[Key];
            UpdatePosition(OrderStatus, AccountPosition);
            UpdateOrderStatus(OrderStatus);
            {
                Message::PackMessage message;
                memset(&message, 0, sizeof(message));
                message.MessageType = Message::EMessageType::EEventLog;
                message.EventLog.Level = Message::EEventLogLevel::EERROR;
                strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
                strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
                strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
                strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
                strncpy(message.EventLog.Ticker, pInputOrder->InstrumentID, sizeof(message.EventLog.Ticker));
                strncpy(message.EventLog.ExchangeID, pInputOrder->ExchangeID, sizeof(message.EventLog.ExchangeID));
                fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event),
                                "CTPTrader:OnErrRtnOrderInsert, BrokerID:{}, Account:{} InstrumentID:{} OrderRef:{} ErrorID:{}, ErrorMessage:{}",
                                m_XTraderConfig.BrokerID, m_XTraderConfig.Account,
                                pInputOrder->InstrumentID, pInputOrder->OrderRef, pRspInfo->ErrorID, OrderStatus.ErrorMsg);
                strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
                while(!m_ReportMessageQueue.Push(message));
                FMTLOG(fmtlog::WRN, "CTPTrader:OnErrRtnOrderInsert, BrokerID:{}, Account:{} InstrumentID:{} OrderRef:{} ErrorID:{}, ErrorMessage:{}",
                        m_XTraderConfig.BrokerID, m_XTraderConfig.Account,
                        pInputOrder->InstrumentID, pInputOrder->OrderRef, pRspInfo->ErrorID, OrderStatus.ErrorMsg);
            }

            PrintOrderStatus(OrderStatus, "CTPTrader::OnErrRtnOrderInsert ");
            m_OrderStatusMap.erase(it1);
        }
        else
        {
            Message::PackMessage message;
            memset(&message, 0, sizeof(message));
            message.MessageType = Message::EMessageType::EEventLog;
            message.EventLog.Level = Message::EEventLogLevel::EWARNING;
            strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
            strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
            strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
            strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
            strncpy(message.EventLog.Ticker, pInputOrder->InstrumentID, sizeof(message.EventLog.Ticker));
            strncpy(message.EventLog.ExchangeID, pInputOrder->ExchangeID, sizeof(message.EventLog.ExchangeID));
            fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event),
                            "CTPTrader:OnErrRtnOrderInsert, BrokerID:{}, Account:{} InstrumentID:{} not found Order, OrderRef:{}",
                            pInputOrder->BrokerID, pInputOrder->InvestorID, pInputOrder->InstrumentID, pInputOrder->OrderRef);
            strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
            while(!m_ReportMessageQueue.Push(message));
            FMTLOG(fmtlog::WRN, "CTPTrader:OnErrRtnOrderInsert, BrokerID:{}, Account:{} InstrumentID:{} not found Order, OrderRef:{}",
                    pInputOrder->BrokerID, pInputOrder->InvestorID, pInputOrder->InstrumentID, pInputOrder->OrderRef);
        }
        FMTLOG(fmtlog::INF, "CTPTrader:OnErrRtnOrderInsert, BrokerID:{}, Account:{} InstrumentID:{} OrderRef:{} UserID:{} OrderPriceType:{} "
                            "Direction:{} CombOffsetFlag:{} CombHedgeFlag:{} LimitPrice:{} VolumeTotalOriginal:{} TimeCondition:{} "
                            "VolumeCondition:{} MinVolume:{} ContingentCondition:{} StopPrice:{} ForceCloseReason:{} IsAutoSuspend:{} "
                            "BusinessUnit:{} RequestID:{} UserForceClose:{} IsSwapOrder:{} ExchangeID:{} InvestUnitID:{} AccountID:{} "
                            "ClientID:{} MacAddress:{} IPAddress:{}",
                pInputOrder->BrokerID, pInputOrder->InvestorID, pInputOrder->InstrumentID, pInputOrder->OrderRef,
                pInputOrder->UserID, pInputOrder->OrderPriceType, pInputOrder->Direction, pInputOrder->CombOffsetFlag,
                pInputOrder->CombHedgeFlag, pInputOrder->LimitPrice, pInputOrder->VolumeTotalOriginal,
                pInputOrder->TimeCondition, pInputOrder->VolumeCondition, pInputOrder->MinVolume,
                pInputOrder->ContingentCondition, pInputOrder->StopPrice, pInputOrder->ForceCloseReason,
                pInputOrder->IsAutoSuspend, pInputOrder->BusinessUnit, pInputOrder->RequestID, pInputOrder->UserForceClose,
                pInputOrder->IsSwapOrder, pInputOrder->ExchangeID, pInputOrder->InvestUnitID, pInputOrder->AccountID,
                pInputOrder->ClientID, pInputOrder->MacAddress, pInputOrder->IPAddress);
    }
}

void CTPTradeGateWay::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(IsRspError(pRspInfo) && pInputOrderAction != NULL)
    {
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspOrderAction, BrokerID:{}, Account:{} InstrumentID:{} OrderActionRef:{} OrderRef:{} RequestID:{} "
                            "FrontID:{} SessionID:{} ExchangeID:{} OrderSysID:{} ActionFlag:{} LimitPrice:{} VolumeChange:{} UserID:{} "
                            "InvestUnitID:{} MacAddress:{} IPAddress:{} ErrorID:{} ErrorMsg:{}",
                pInputOrderAction->BrokerID, pInputOrderAction->InvestorID, pInputOrderAction->InstrumentID,
                pInputOrderAction->OrderActionRef, pInputOrderAction->OrderRef, pInputOrderAction->RequestID,
                pInputOrderAction->FrontID, pInputOrderAction->SessionID, pInputOrderAction->ExchangeID,
                pInputOrderAction->OrderSysID, pInputOrderAction->ActionFlag, pInputOrderAction->LimitPrice,
                pInputOrderAction->VolumeChange, pInputOrderAction->UserID, pInputOrderAction->InvestUnitID,
                pInputOrderAction->MacAddress, pInputOrderAction->IPAddress, pRspInfo->ErrorID, pRspInfo->ErrorMsg);
        std::string OrderRef = pInputOrderAction->OrderRef;
        auto it = m_OrderStatusMap.find(OrderRef);
        if(it != m_OrderStatusMap.end())
        {
            // Update OrderStatus
            it->second.OrderStatus = Message::EOrderStatusType::EACTION_ERROR;
            strncpy(it->second.Product, m_XTraderConfig.Product.c_str(), sizeof(it->second.Product));
            strncpy(it->second.Broker, m_XTraderConfig.Broker.c_str(), sizeof(it->second.Broker));
            strncpy(it->second.Account, pInputOrderAction->InvestorID, sizeof(it->second.Account));
            strncpy(it->second.ExchangeID, pInputOrderAction->ExchangeID, sizeof(it->second.ExchangeID));
            strncpy(it->second.Ticker, pInputOrderAction->InstrumentID, sizeof(it->second.Ticker));
            strncpy(it->second.OrderRef, pInputOrderAction->OrderRef, sizeof(it->second.OrderRef));
            strncpy(it->second.OrderSysID, pInputOrderAction->OrderSysID, sizeof(it->second.OrderSysID));

            Utils::CodeConvert(pRspInfo->ErrorMsg, sizeof(pRspInfo->ErrorMsg), it->second.ErrorMsg,
                            sizeof(it->second.ErrorMsg), "gb2312", "utf-8");
            it->second.ErrorID = pRspInfo->ErrorID;
            UpdateOrderStatus(it->second);
            PrintOrderStatus(it->second, "CTPTrader::OnRspOrderAction ");
        }
    }
}

void CTPTradeGateWay::OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo)
{
    if(IsRspError(pRspInfo) && pOrderAction != NULL)
    {
        // OrderStatus
        // 撤单时指定才能返回OrderRef
        std::string OrderRef = pOrderAction->OrderRef;
        auto it1 = m_OrderStatusMap.find(OrderRef);
        if(it1 != m_OrderStatusMap.end())
        {
            Message::TOrderStatus& OrderStatus = it1->second;
            OrderStatus.OrderStatus = Message::EOrderStatusType::EACTION_ERROR;
            Utils::CodeConvert(pRspInfo->ErrorMsg, sizeof(pRspInfo->ErrorMsg), OrderStatus.ErrorMsg,
                            sizeof(OrderStatus.ErrorMsg), "gb2312", "utf-8");
            OrderStatus.ErrorID = pRspInfo->ErrorID;
            UpdateOrderStatus(OrderStatus);
            {
                Message::PackMessage message;
                memset(&message, 0, sizeof(message));
                message.MessageType = Message::EMessageType::EEventLog;
                message.EventLog.Level = Message::EEventLogLevel::EWARNING;
                strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
                strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
                strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
                strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
                strncpy(message.EventLog.Ticker, pOrderAction->InstrumentID, sizeof(message.EventLog.Ticker));
                strncpy(message.EventLog.ExchangeID, pOrderAction->ExchangeID, sizeof(message.EventLog.ExchangeID));
                fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event), 
                                "CTPTrader:OnErrRtnOrderAction, BrokerID:{}, Account:{} InstrumentID:{} OrderRef:{} OrderActionRef:{} OrderSysID:{} OrderLocalID:{} ErrorID:{}, ErrorMessage:{}",
                                m_XTraderConfig.BrokerID, m_XTraderConfig.Account, pOrderAction->InstrumentID,
                                pOrderAction->OrderRef, pOrderAction->OrderActionRef, pOrderAction->OrderSysID,  pOrderAction->OrderLocalID,
                                pRspInfo->ErrorID, OrderStatus.ErrorMsg);
                strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
                while(!m_ReportMessageQueue.Push(message));
                FMTLOG(fmtlog::WRN, "CTPTrader:OnErrRtnOrderAction, BrokerID:{}, Account:{} InstrumentID:{} OrderRef:{} OrderActionRef:{} OrderSysID:{} OrderLocalID:{} ErrorID:{}, ErrorMessage:{}",
                        m_XTraderConfig.BrokerID, m_XTraderConfig.Account, pOrderAction->InstrumentID,
                        pOrderAction->OrderRef, pOrderAction->OrderActionRef, pOrderAction->OrderSysID,  pOrderAction->OrderLocalID,
                        pRspInfo->ErrorID, OrderStatus.ErrorMsg);
            }

            PrintOrderStatus(OrderStatus, "CTPTrader::OnErrRtnOrderAction ");
        }
        else
        {
            Message::PackMessage message;
            memset(&message, 0, sizeof(message));
            message.MessageType = Message::EMessageType::EEventLog;
            message.EventLog.Level = Message::EEventLogLevel::EWARNING;
            strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
            strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
            strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
            strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
            strncpy(message.EventLog.Ticker, pOrderAction->InstrumentID, sizeof(message.EventLog.Ticker));
            strncpy(message.EventLog.ExchangeID, pOrderAction->ExchangeID, sizeof(message.EventLog.ExchangeID));
            fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event),
                            "CTPTrader:OnErrRtnOrderAction, BrokerID:{}, Account:{} InstrumentID:{} not found Order, OrderRef:{}",
                            m_XTraderConfig.BrokerID, m_XTraderConfig.Account, pOrderAction->InstrumentID, pOrderAction->OrderRef);
            strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
            while(!m_ReportMessageQueue.Push(message));
            FMTLOG(fmtlog::WRN, "CTPTrader:OnErrRtnOrderAction, BrokerID:{}, Account:{} InstrumentID:{} not found Order, OrderRef:{}",
                    m_XTraderConfig.BrokerID, m_XTraderConfig.Account, pOrderAction->InstrumentID, pOrderAction->OrderRef);
        }
        FMTLOG(fmtlog::INF, "CTPTrader:OnErrRtnOrderAction, BrokerID:{}, Account:{} InstrumentID:{} OrderActionRef:{} OrderRef:{} "
                            "RequestID:{} FrontID:{} SessionID:{} ExchangeID:{} OrderSysID:{} ActionFlag:{} LimitPrice:{} VolumeChange:{} "
                            "UserID:{} InvestUnitID:{} MacAddress:{} IPAddress:{}",
                pOrderAction->BrokerID, pOrderAction->InvestorID,pOrderAction->InstrumentID,
                pOrderAction->OrderActionRef, pOrderAction->OrderRef, pOrderAction->RequestID,
                pOrderAction->FrontID, pOrderAction->SessionID, pOrderAction->ExchangeID,
                pOrderAction->OrderSysID, pOrderAction->ActionFlag, pOrderAction->LimitPrice,
                pOrderAction->VolumeChange, pOrderAction->UserID, pOrderAction->InvestUnitID,
                pOrderAction->MacAddress, pOrderAction->IPAddress);
    }
}

void CTPTradeGateWay::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(IsRspError(pRspInfo))
    {
        char errorBuffer[512] = {0};
        Utils::CodeConvert(pRspInfo->ErrorMsg, sizeof(pRspInfo->ErrorMsg), errorBuffer,
                        sizeof(errorBuffer), "gb2312", "utf-8");
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspError, BrokerID:{}, Account:{}, ErrorID:{}, ErrorMessage:{}",
                m_XTraderConfig.BrokerID, m_XTraderConfig.Account, pRspInfo->ErrorID, errorBuffer);
    }
}

void CTPTradeGateWay::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
    if(NULL == pOrder)
    {
        return;
    }
    char errorBuffer[512] = {0};
    Utils::CodeConvert(pOrder->StatusMsg, sizeof(pOrder->StatusMsg), errorBuffer,
                       sizeof(errorBuffer), "gb2312", "utf-8");
    std::string OrderRef = pOrder->OrderRef;
    auto it1 = m_OrderStatusMap.find(OrderRef);
    if(m_OrderStatusMap.end() == it1)
    {
        // Order Status
        Message::TOrderStatus& OrderStatus = m_OrderStatusMap[OrderRef];
        OrderStatus.BusinessType = m_XTraderConfig.BusinessType;
        strncpy(OrderStatus.Product, m_XTraderConfig.Product.c_str(), sizeof(OrderStatus.Product));
        strncpy(OrderStatus.Broker, m_XTraderConfig.Broker.c_str(), sizeof(OrderStatus.Broker));
        strncpy(OrderStatus.Account, pOrder->InvestorID, sizeof(OrderStatus.Account));
        strncpy(OrderStatus.ExchangeID, pOrder->ExchangeID, sizeof(OrderStatus.ExchangeID));
        strncpy(OrderStatus.Ticker, pOrder->InstrumentID, sizeof(OrderStatus.Ticker));
        strncpy(OrderStatus.OrderRef, pOrder->OrderRef, sizeof(OrderStatus.OrderRef));
        OrderStatus.SendPrice = pOrder->LimitPrice;
        OrderStatus.SendVolume = pOrder->VolumeTotalOriginal;
        OrderStatus.OrderStatus = Message::EOrderStatusType::EORDER_SENDED;
        OrderStatus.OrderType = Ordertype(pOrder->TimeCondition, pOrder->VolumeCondition);
        OrderStatus.OrderToken = pOrder->RequestID;
        std::string Account = pOrder->InvestorID;
        std::string Ticker = pOrder->InstrumentID;
        std::string Key = Account + ":" + Ticker;
        strncpy(OrderStatus.SendTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.SendTime));
        OrderStatus.OrderSide = OrderSide(pOrder->Direction, pOrder->CombOffsetFlag[0], Key);
        Message::TAccountPosition& AccountPosition = m_TickerAccountPositionMap[Key];
        UpdatePosition(OrderStatus, AccountPosition); 
    }
    auto it = m_OrderStatusMap.find(OrderRef);
    if(it != m_OrderStatusMap.end())
    {
        Message::TOrderStatus& OrderStatus = it->second;
        std::string Account = pOrder->InvestorID;
        std::string Ticker = pOrder->InstrumentID;
        std::string Key = Account + ":" + Ticker;
        OrderStatus.OrderSide = OrderSide(pOrder->Direction, pOrder->CombOffsetFlag[0], Key);
        std::string OrderSysID = pOrder->OrderSysID;
        Message::TAccountPosition& AccountPosition = m_TickerAccountPositionMap[Key];

        PrintOrderStatus(OrderStatus, "CTPTrader::OnRtnOrder start ");
        PrintAccountPosition(AccountPosition, "CTPTrader::OnRtnOrder start ");
        if(THOST_FTDC_OSS_InsertSubmitted == pOrder->OrderSubmitStatus)
        {
            if(THOST_FTDC_OST_Unknown == pOrder->OrderStatus)
            {
                // Broker ACK
                if(OrderSysID.empty())
                {
                    strncpy(OrderStatus.OrderLocalID, pOrder->OrderLocalID, sizeof(OrderStatus.OrderLocalID));
                    strncpy(OrderStatus.BrokerACKTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.BrokerACKTime));
                    OrderStatus.OrderStatus = Message::EOrderStatusType::EBROKER_ACK;
                }
                else // Exchange ACK
                {
                    strncpy(OrderStatus.ExchangeACKTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.ExchangeACKTime));
                    strncpy(OrderStatus.OrderSysID, pOrder->OrderSysID, sizeof(OrderStatus.OrderSysID));
                    OrderStatus.OrderStatus = Message::EOrderStatusType::EEXCHANGE_ACK;
                    OnExchangeACK(OrderStatus);
                }
            }
            // 报单提交到交易所立即部分成交
            else if(THOST_FTDC_OST_PartTradedQueueing == pOrder->OrderStatus)
            {
                OrderStatus.OrderStatus = Message::EOrderStatusType::EPARTTRADED;
            }
            // 报单提交到交易所立即全部成交
            else if(THOST_FTDC_OST_AllTraded == pOrder->OrderStatus)
            {
                OrderStatus.OrderStatus = Message::EOrderStatusType::EALLTRADED;
            }
            // 撤单状态， FAK、FOK
            else if(THOST_FTDC_OST_Canceled == pOrder->OrderStatus)
            {
                strncpy(OrderStatus.ExchangeACKTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.ExchangeACKTime));
                strncpy(OrderStatus.OrderSysID, pOrder->OrderSysID, sizeof(OrderStatus.OrderSysID));
                if(pOrder->VolumeTraded > 0)
                {
                    OrderStatus.OrderStatus = Message::EOrderStatusType::EPARTTRADED_CANCELLED;
                    OrderStatus.CanceledVolume = pOrder->VolumeTotalOriginal - pOrder->VolumeTraded;
                }
                else
                {
                    OrderStatus.OrderStatus = Message::EOrderStatusType::ECANCELLED;
                    OrderStatus.CanceledVolume = pOrder->VolumeTotalOriginal;
                }
            }
        }
        else if(THOST_FTDC_OSS_CancelSubmitted == pOrder->OrderSubmitStatus)
        {
            // Nothing，撤单请求被CTP校验通过，CTP返回当前订单状态给客户端
        }
        else if(THOST_FTDC_OSS_Accepted == pOrder->OrderSubmitStatus)
        {
            // Exchange ACK
            if(THOST_FTDC_OST_NoTradeQueueing == pOrder->OrderStatus)
            {
                strncpy(OrderStatus.ExchangeACKTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.ExchangeACKTime));
                strncpy(OrderStatus.OrderSysID, pOrder->OrderSysID, sizeof(OrderStatus.OrderSysID));
                OrderStatus.OrderStatus = Message::EOrderStatusType::EEXCHANGE_ACK;
                OnExchangeACK(OrderStatus);
            }
            // Order PartTraded
            else if(THOST_FTDC_OST_PartTradedQueueing == pOrder->OrderStatus)
            {
                OrderStatus.OrderStatus = Message::EOrderStatusType::EPARTTRADED;
            }
            // Order AllTraded
            else if(THOST_FTDC_OST_AllTraded == pOrder->OrderStatus)
            {
                OrderStatus.OrderStatus = Message::EOrderStatusType::EALLTRADED;
            }
            // Order Cancelled
            else if(THOST_FTDC_OST_Canceled == pOrder->OrderStatus)
            {
                if(pOrder->VolumeTraded > 0)
                {
                    OrderStatus.OrderStatus = Message::EOrderStatusType::EPARTTRADED_CANCELLED;
                    OrderStatus.CanceledVolume = pOrder->VolumeTotalOriginal - pOrder->VolumeTraded;
                }
                else
                {
                    OrderStatus.OrderStatus = Message::EOrderStatusType::ECANCELLED;
                    OrderStatus.CanceledVolume = pOrder->VolumeTotalOriginal;
                }
            }
        }
        // 报单被交易所拒绝
        else if(THOST_FTDC_OSS_InsertRejected == pOrder->OrderSubmitStatus)
        {
            strncpy(OrderStatus.ExchangeACKTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.ExchangeACKTime));
            strncpy(OrderStatus.OrderSysID, pOrder->OrderSysID, sizeof(OrderStatus.OrderSysID));
            OrderStatus.OrderStatus = Message::EOrderStatusType::EEXCHANGE_ERROR;
        }
        // 撤单被交易所拒绝
        else if(THOST_FTDC_OSS_CancelRejected == pOrder->OrderSubmitStatus)
        {
            Message::PackMessage message;
            memset(&message, 0, sizeof(message));
            message.MessageType = Message::EMessageType::EEventLog;
            message.EventLog.Level = Message::EEventLogLevel::EWARNING;
            strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
            strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
            strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
            strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
            strncpy(message.EventLog.Ticker, pOrder->InstrumentID, sizeof(message.EventLog.Ticker));
            strncpy(message.EventLog.ExchangeID, pOrder->ExchangeID, sizeof(message.EventLog.ExchangeID));
            fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event),
                            "CTPTrader:OnRtnOrder, BrokerID:{}, Account:{} Ticker:{} Order OrderRef:{} OrderSysID:{} Cancel Rejected",
                            pOrder->BrokerID, pOrder->InvestorID, pOrder->InstrumentID, pOrder->OrderRef, pOrder->OrderSysID);
            strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
            while(!m_ReportMessageQueue.Push(message));
            FMTLOG(fmtlog::WRN, "CTPTrader:OnRtnOrder, BrokerID:{}, Account:{} Ticker:{} Order OrderRef:{} OrderSysID:{} Cancel Rejected",
                    pOrder->BrokerID, pOrder->InvestorID, pOrder->InstrumentID, pOrder->OrderRef, pOrder->OrderSysID);
        }
        Utils::CodeConvert(pOrder->StatusMsg, sizeof(pOrder->StatusMsg), OrderStatus.ErrorMsg,
                           sizeof(OrderStatus.ErrorMsg), "gb2312", "utf-8");
        // Update OrderStatus
        if(OrderStatus.OrderType == Message::EOrderType::ELIMIT)
        {
            // 对于LIMIT订单，在成交回报更新时更新订单状态
            if(Message::EOrderStatusType::EALLTRADED != OrderStatus.OrderStatus && Message::EOrderStatusType::EPARTTRADED != OrderStatus.OrderStatus)
            {
                UpdateOrderStatus(OrderStatus);
            }
            // Update Position
            if(Message::EOrderStatusType::EPARTTRADED_CANCELLED == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::ECANCELLED == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EEXCHANGE_ERROR == OrderStatus.OrderStatus)
            {
                UpdatePosition(OrderStatus, AccountPosition);
                // remove Order
                m_OrderStatusMap.erase(it);
            }
        }
        else if(OrderStatus.OrderType == Message::EOrderType::EFAK || OrderStatus.OrderType == Message::EOrderType::EFOK)
        {
            if(Message::EOrderStatusType::EALLTRADED != OrderStatus.OrderStatus && 
                    Message::EOrderStatusType::EPARTTRADED_CANCELLED != OrderStatus.OrderStatus)
            {
                UpdateOrderStatus(OrderStatus);
            }
            // Update Position
            if(Message::EOrderStatusType::ECANCELLED == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EEXCHANGE_ERROR == OrderStatus.OrderStatus)
            {
                UpdatePosition(OrderStatus, AccountPosition);
                // remove Order
                m_OrderStatusMap.erase(it);
            }
        }
    }
    else
    {
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EWARNING;
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
        strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
        strncpy(message.EventLog.Ticker, pOrder->InstrumentID, sizeof(message.EventLog.Ticker));
        strncpy(message.EventLog.ExchangeID, pOrder->ExchangeID, sizeof(message.EventLog.ExchangeID));
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event), 
                        "CTPTrader:OnRtnOrder, BrokerID:{}, Account:{} Ticker:{} not found Order, OrderRef:{} OrderSysID:{}",
                        pOrder->BrokerID, pOrder->InvestorID, pOrder->InstrumentID, pOrder->OrderRef, pOrder->OrderSysID);
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));
        FMTLOG(fmtlog::WRN, "CTPTrader:OnRtnOrder, BrokerID:{}, Account:{} Ticker:{} not found Order, OrderRef:{} OrderSysID:{}",
                pOrder->BrokerID, pOrder->InvestorID, pOrder->InstrumentID, pOrder->OrderRef, pOrder->OrderSysID);
    }
    FMTLOG(fmtlog::INF, "CTPTrader:OnRtnOrder, BrokerID:{}, Account:{} InstrumentID:{} OrderRef:{} UserID:{} OrderPriceType:{} Direction:{} "
                        "CombOffsetFlag:{} CombHedgeFlag:{} LimitPrice:{} VolumeTotalOriginal:{} TimeCondition:{} VolumeCondition:{} "
                        "MinVolume:{} ContingentCondition:{} StopPrice:{} ForceCloseReason:{} IsAutoSuspend:{} BusinessUnit:{} "
                        "RequestID:{} OrderLocalID:{} ExchangeID:{} ParticipantID:{} ClientID:{} TraderID:{} InstallID:{} "
                        "OrderSubmitStatus:{} NotifySequence:{} SettlementID:{} OrderSysID:{} OrderSource:{} OrderStatus:{} "
                        "OrderType:{} VolumeTraded:{} VolumeTotal:{} InsertDate:{} InsertTime:{} ActiveTime:{} SuspendTime:{} "
                        "UpdateTime:{} CancelTime:{} ActiveTraderID:{} ClearingPartID:{} SequenceNo:{} FrontID:{} SessionID:{} "
                        "UserProductInfo:{} StatusMsg:{} UserForceClose:{} ActiveUserID:{} BrokerOrderSeq:{} RelativeOrderSysID:{} "
                        "ZCETotalTradedVolume:{} IsSwapOrder:{} BranchID:{} InvestUnitID:{} AccountID:{} MacAddress:{} ExchangeInstID:{} IPAddress:{}",
            pOrder->BrokerID, pOrder->InvestorID,pOrder->InstrumentID,pOrder->OrderRef, pOrder->UserID,
            pOrder->OrderPriceType, pOrder->Direction, pOrder->CombOffsetFlag,
            pOrder->CombHedgeFlag, pOrder->LimitPrice, pOrder->VolumeTotalOriginal,
            pOrder->TimeCondition, pOrder->VolumeCondition, pOrder->MinVolume,
            pOrder->ContingentCondition, pOrder->StopPrice, pOrder->ForceCloseReason,
            pOrder->IsAutoSuspend, pOrder->BusinessUnit,  pOrder->RequestID, pOrder->OrderLocalID,
            pOrder->ExchangeID, pOrder->ParticipantID, pOrder->ClientID, pOrder->TraderID,
            pOrder->InstallID, pOrder->OrderSubmitStatus, pOrder->NotifySequence, pOrder->SettlementID,
            pOrder->OrderSysID, pOrder->OrderSource, pOrder->OrderStatus, pOrder->OrderType,
            pOrder->VolumeTraded, pOrder->VolumeTotal, pOrder->InsertDate, pOrder->InsertTime,
            pOrder->ActiveTime, pOrder->SuspendTime, pOrder->UpdateTime, pOrder->CancelTime,
            pOrder->ActiveTraderID, pOrder->ClearingPartID, pOrder->SequenceNo, pOrder->FrontID,
            pOrder->SessionID, pOrder->UserProductInfo, errorBuffer, pOrder->UserForceClose,
            pOrder->ActiveUserID, pOrder->BrokerOrderSeq, pOrder->RelativeOrderSysID, pOrder->ZCETotalTradedVolume,
            pOrder->IsSwapOrder, pOrder->BranchID, pOrder->InvestUnitID, pOrder->AccountID, pOrder->MacAddress,
            pOrder->ExchangeInstID, pOrder->IPAddress);
}

void CTPTradeGateWay::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
    if(NULL == pTrade)
    {
        return;
    }
    std::string OrderRef = pTrade->OrderRef;
    auto it = m_OrderStatusMap.find(OrderRef);
    if(it != m_OrderStatusMap.end())
    {
        Message::TOrderStatus& OrderStatus = it->second;
        // Upadte OrderStatus
        double prevTotalAmount = OrderStatus.TotalTradedVolume * OrderStatus.TradedAvgPrice;
        OrderStatus.TotalTradedVolume +=  pTrade->Volume;
        OrderStatus.TradedVolume =  pTrade->Volume;
        OrderStatus.TradedPrice = pTrade->Price;
        OrderStatus.TradedAvgPrice = (pTrade->Price * pTrade->Volume + prevTotalAmount) / OrderStatus.TotalTradedVolume;

        std::string Account = pTrade->InvestorID;
        std::string Ticker = pTrade->InstrumentID;
        std::string Key = Account + ":" + Ticker;
        Message::TAccountPosition& AccountPosition = m_TickerAccountPositionMap[Key];
        if(Message::EOrderType::ELIMIT ==  OrderStatus.OrderType)
        {
            // Upadte OrderStatus
            if(OrderStatus.TotalTradedVolume == OrderStatus.SendVolume)
            {
                OrderStatus.OrderStatus = Message::EOrderStatusType::EALLTRADED;
            }
            else
            {
                OrderStatus.OrderStatus = Message::EOrderStatusType::EPARTTRADED;
            }
            UpdateOrderStatus(OrderStatus);
            // Position Update
            UpdatePosition(OrderStatus, AccountPosition);
            // remove Order When AllTraded
            if(Message::EOrderStatusType::EALLTRADED == OrderStatus.OrderStatus && OrderStatus.TotalTradedVolume == OrderStatus.SendVolume)
            {
                m_OrderStatusMap.erase(it);
            }
        }
        else if(Message::EOrderType::EFAK == OrderStatus.OrderType || Message::EOrderType::EFOK == OrderStatus.OrderType)
        {
            if(OrderStatus.TotalTradedVolume == OrderStatus.SendVolume)
            {
                OrderStatus.OrderStatus = Message::EOrderStatusType::EALLTRADED;
                UpdatePosition(OrderStatus, AccountPosition);
                UpdateOrderStatus(OrderStatus);
            }
            else if(OrderStatus.TotalTradedVolume == OrderStatus.SendVolume - OrderStatus.CanceledVolume)
            {
                // 更新成交数量仓位
                OrderStatus.OrderStatus = Message::EOrderStatusType::EPARTTRADED;
                UpdatePosition(OrderStatus, AccountPosition);
                // 更新订单终结状态冻结仓位
                OrderStatus.OrderStatus = Message::EOrderStatusType::EPARTTRADED_CANCELLED;
                UpdatePosition(OrderStatus, AccountPosition);
                UpdateOrderStatus(OrderStatus);
            }
            else
            {
                OrderStatus.OrderStatus = Message::EOrderStatusType::EPARTTRADED;
                UpdatePosition(OrderStatus, AccountPosition);
            }
            PrintOrderStatus(OrderStatus, "CTPTrader::OnRtnTrade ");
            if(Message::EOrderStatusType::EALLTRADED == OrderStatus.OrderStatus ||
                    Message::EOrderStatusType::EPARTTRADED_CANCELLED == OrderStatus.OrderStatus)
            {
                m_OrderStatusMap.erase(it);
            }
        }
    }
    else
    {
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EWARNING;
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
        strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
        strncpy(message.EventLog.Ticker, pTrade->InstrumentID, sizeof(message.EventLog.Ticker));
        strncpy(message.EventLog.ExchangeID,  pTrade->ExchangeID, sizeof(message.EventLog.ExchangeID));
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event), 
                        "CTPTrader:OnRtnTrade, BrokerID:{}, Account:{} Ticker:{} not found Order, OrderRef:{} OrderSysID:{}",
                        pTrade->BrokerID, pTrade->InvestorID, pTrade->InstrumentID, pTrade->OrderRef, pTrade->OrderSysID);
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));
        FMTLOG(fmtlog::WRN, "CTPTrader:OnRtnTrade, BrokerID:{}, Account:{} Ticker:{} not found Order, OrderRef:{} OrderSysID:{}",
                pTrade->BrokerID, pTrade->InvestorID, pTrade->InstrumentID, pTrade->OrderRef, pTrade->OrderSysID);
    }
    FMTLOG(fmtlog::INF, "CTPTrader:OnRtnTrade, BrokerID:{}, Account:{} InstrumentID:{} OrderRef:{} UserID:{} ExchangeID:{} TradeID:{} "
                        "Direction:{} OrderSysID:{} ParticipantID:{} ClientID:{} TradingRole:{} OffsetFlag:{} HedgeFlag:{} Price:{} "
                        "Volume:{} TradeDate:{} TradeTime:{} TradeType:{} PriceSource:{} TraderID:{} OrderLocalID:{} ClearingPartID:{} "
                        "BusinessUnit:{} SequenceNo:{} SettlementID:{} BrokerOrderSeq:{} TradeSource:{} InvestUnitID:{} ExchangeInstID:{}",
            pTrade->BrokerID, pTrade->InvestorID, pTrade->InstrumentID,pTrade->OrderRef, pTrade->UserID,
            pTrade->ExchangeID, pTrade->TradeID, pTrade->Direction, pTrade->OrderSysID, pTrade->ParticipantID, 
            pTrade->ClientID, pTrade->TradingRole, pTrade->OffsetFlag, pTrade->HedgeFlag, pTrade->Price, 
            pTrade->Volume, pTrade->TradeDate, pTrade->TradeTime, pTrade->TradeType,  pTrade->PriceSource, 
            pTrade->TraderID, pTrade->OrderLocalID, pTrade->ClearingPartID, pTrade->BusinessUnit, pTrade->SequenceNo,
            pTrade->SettlementID, pTrade->BrokerOrderSeq, pTrade->TradeSource, pTrade->InvestUnitID, pTrade->ExchangeInstID);
}

void CTPTradeGateWay::OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(!IsRspError(pRspInfo) && pTradingAccount != NULL)
    {
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspQryTradingAccount successed, BrokerID:{} AccountID:{} PreMortgage:{} PreCredit:{} PreDeposit:{} "
                            "PreBalance:{} PreMargin:{} InterestBase:{} Interest:{} Deposit:{} Withdraw:{} FrozenMargin:{} FrozenCash:{} "
                            "FrozenCommission:{} CurrMargin:{} CashIn:{} Commission:{} CloseProfit:{} PositionProfit:{} Balance:{} Available:{} "
                            "WithdrawQuota:{} SettlementID:{} Credit:{} Mortgage:{} ExchangeMargin:{} DeliveryMargin:{} ExchangeDeliveryMargin:{} "
                            "ReserveBalance:{} CurrencyID:{} PreFundMortgageIn:{} PreFundMortgageOut:{} FundMortgageIn:{} FundMortgageOut:{} "
                            "FundMortgageAvailable:{} MortgageableFund:{} SpecProductMargin:{} SpecProductFrozenMargin:{} SpecProductCommission:{} "
                            "SpecProductFrozenCommission:{} SpecProductPositionProfit:{} SpecProductCloseProfit:{} SpecProductPositionProfitByAlg:{} "
                            "SpecProductExchangeMargin:{} BizType:{} FrozenSwap:{} RemainSwap:{} nRequestID:{} bIsLast:{}",
                pTradingAccount->BrokerID, pTradingAccount->AccountID, pTradingAccount->PreMortgage, pTradingAccount->PreCredit,
                pTradingAccount->PreDeposit, pTradingAccount->PreBalance, pTradingAccount->PreMargin, pTradingAccount->InterestBase,
                pTradingAccount->Interest, pTradingAccount->Deposit, pTradingAccount->Withdraw, pTradingAccount->FrozenMargin,
                pTradingAccount->FrozenCash, pTradingAccount->FrozenCommission, pTradingAccount->CurrMargin, pTradingAccount->CashIn,
                pTradingAccount->Commission, pTradingAccount->CloseProfit, pTradingAccount->PositionProfit, pTradingAccount->Balance,
                pTradingAccount->Available, pTradingAccount->WithdrawQuota, pTradingAccount->SettlementID, pTradingAccount->Credit,
                pTradingAccount->Mortgage, pTradingAccount->ExchangeMargin, pTradingAccount->DeliveryMargin, pTradingAccount->ExchangeDeliveryMargin,
                pTradingAccount->ReserveBalance, pTradingAccount->CurrencyID, pTradingAccount->PreFundMortgageIn, pTradingAccount->PreFundMortgageOut,
                pTradingAccount->FundMortgageIn, pTradingAccount->FundMortgageOut, pTradingAccount->FundMortgageAvailable, pTradingAccount->MortgageableFund,
                pTradingAccount->SpecProductMargin, pTradingAccount->SpecProductFrozenMargin, pTradingAccount->SpecProductCommission, pTradingAccount->SpecProductFrozenCommission,
                pTradingAccount->SpecProductPositionProfit, pTradingAccount->SpecProductCloseProfit, pTradingAccount->SpecProductPositionProfitByAlg,
                pTradingAccount->SpecProductExchangeMargin, pTradingAccount->BizType, pTradingAccount->FrozenSwap, pTradingAccount->RemainSwap,
                nRequestID, bIsLast);
        std::string Account = pTradingAccount->AccountID;
        Message::TAccountFund& AccountFund = m_AccountFundMap[Account];
        AccountFund.BusinessType = m_XTraderConfig.BusinessType;
        strncpy(AccountFund.Product, m_XTraderConfig.Product.c_str(), sizeof(AccountFund.Product));
        strncpy(AccountFund.Broker, m_XTraderConfig.Broker.c_str(), sizeof(AccountFund.Broker));
        strncpy(AccountFund.Account, pTradingAccount->AccountID, sizeof(AccountFund.Account));
        AccountFund.Deposit = pTradingAccount->Deposit;
        AccountFund.Withdraw = pTradingAccount->Withdraw;
        AccountFund.CurrMargin = pTradingAccount->CurrMargin;
        AccountFund.Commission = pTradingAccount->Commission;
        AccountFund.CloseProfit = pTradingAccount->CloseProfit;
        AccountFund.PositionProfit = pTradingAccount->PositionProfit;
        AccountFund.Available = pTradingAccount->Available;
        AccountFund.WithdrawQuota = pTradingAccount->WithdrawQuota;
        AccountFund.ExchangeMargin = pTradingAccount->ExchangeMargin;
        AccountFund.Balance = pTradingAccount->Balance;
        AccountFund.PreBalance = pTradingAccount->PreBalance;
        strncpy(AccountFund.UpdateTime, Utils::getCurrentTimeUs(), sizeof(AccountFund.UpdateTime));

        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EAccountFund;
        memcpy(&message.AccountFund, &AccountFund, sizeof(AccountFund));
        while(!m_ReportMessageQueue.Push(message));
    }
    else if(NULL != pRspInfo)
    {
        char errorBuffer[512] = {0};
        Utils::CodeConvert(pRspInfo->ErrorMsg, sizeof(pRspInfo->ErrorMsg), errorBuffer,
                       sizeof(errorBuffer), "gb2312", "utf-8");
        FMTLOG(fmtlog::WRN, "CTPTrader:OnRspQryTradingAccount failed, Broker:{}, Account:{}, ErrorID:{}, ErrorMessage:{}",
                m_XTraderConfig.BrokerID, m_XTraderConfig.Account, pRspInfo->ErrorID, errorBuffer);
    }
}

void CTPTradeGateWay::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(!IsRspError(pRspInfo) && pInvestorPosition != NULL)
    {
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspQryInvestorPosition BrokerID:{} Account:{} PosiDirection:{} HedgeFlag:{} PositionDate:{} YdPosition:{} "
                            "Position:{} LongFrozen:{} ShortFrozen:{} LongFrozenAmount:{} ShortFrozenAmount:{} OpenVolume:{} CloseVolume:{} "
                            "OpenAmount:{} CloseAmount:{} PositionCost:{} PreMargin:{} UseMargin:{} FrozenMargin:{} FrozenCash:{} FrozenCommission:{} "
                            "CashIn:{} Commission:{} CloseProfit:{} PositionProfit:{} PreSettlementPrice:{} SettlementPrice:{} SettlementID:{} "
                            "OpenCost:{} ExchangeMargin:{} CombPosition:{} CombLongFrozen:{} CombShortFrozen:{} CloseProfitByDate:{} "
                            "CloseProfitByTrade:{} TodayPosition:{} MarginRateByMoney:{} MarginRateByVolume:{} StrikeFrozen:{} "
                            "StrikeFrozenAmount:{} AbandonFrozen:{} ExchangeID:{} YdStrikeFrozen:{} InvestUnitID:{} PositionCostOffset:{} "
                            "TasPosition:{} TasPositionCost:{} InstrumentID:{} nRequestID:{} bIsLast:{}",
                pInvestorPosition->BrokerID, pInvestorPosition->InvestorID, pInvestorPosition->PosiDirection,
                pInvestorPosition->HedgeFlag, pInvestorPosition->PositionDate, pInvestorPosition->YdPosition,
                pInvestorPosition->Position, pInvestorPosition->LongFrozen, pInvestorPosition->ShortFrozen,
                pInvestorPosition->LongFrozenAmount, pInvestorPosition->ShortFrozenAmount, pInvestorPosition->OpenVolume,
                pInvestorPosition->CloseVolume, pInvestorPosition->OpenAmount, pInvestorPosition->CloseAmount,
                pInvestorPosition->PositionCost, pInvestorPosition->PreMargin, pInvestorPosition->UseMargin,
                pInvestorPosition->FrozenMargin, pInvestorPosition->FrozenCash, pInvestorPosition->FrozenCommission,
                pInvestorPosition->CashIn, pInvestorPosition->Commission, pInvestorPosition->CloseProfit,
                pInvestorPosition->PositionProfit, pInvestorPosition->PreSettlementPrice, pInvestorPosition->SettlementPrice,
                pInvestorPosition->SettlementID, pInvestorPosition->OpenCost, pInvestorPosition->ExchangeMargin,
                pInvestorPosition->CombPosition, pInvestorPosition->CombLongFrozen, pInvestorPosition->CombShortFrozen,
                pInvestorPosition->CloseProfitByDate, pInvestorPosition->CloseProfitByTrade, pInvestorPosition->TodayPosition,
                pInvestorPosition->MarginRateByMoney, pInvestorPosition->MarginRateByVolume, pInvestorPosition->StrikeFrozen,
                pInvestorPosition->StrikeFrozenAmount, pInvestorPosition->AbandonFrozen, pInvestorPosition->ExchangeID,
                pInvestorPosition->YdStrikeFrozen, pInvestorPosition->InvestUnitID, pInvestorPosition->PositionCostOffset,
                pInvestorPosition->TasPosition, pInvestorPosition->TasPositionCost, pInvestorPosition->InstrumentID,
                nRequestID, bIsLast);
        std::string Account = pInvestorPosition->InvestorID;
        std::string Ticker = pInvestorPosition->InstrumentID;
        std::string Key = Account + ":" + Ticker;
        auto it = m_TickerAccountPositionMap.find(Key);
        if(m_TickerAccountPositionMap.end() == it)
        {
            Message::TAccountPosition AccountPosition;
            memset(&AccountPosition, 0, sizeof(AccountPosition));
            AccountPosition.BusinessType = m_XTraderConfig.BusinessType;
            strncpy(AccountPosition.Product,  m_XTraderConfig.Product.c_str(), sizeof(AccountPosition.Product));
            strncpy(AccountPosition.Broker,  m_XTraderConfig.Broker.c_str(), sizeof(AccountPosition.Broker));
            strncpy(AccountPosition.Account, pInvestorPosition->InvestorID, sizeof(AccountPosition.Account));
            strncpy(AccountPosition.Ticker, pInvestorPosition->InstrumentID, sizeof(AccountPosition.Ticker));
            strncpy(AccountPosition.ExchangeID, pInvestorPosition->ExchangeID, sizeof(AccountPosition.ExchangeID));
            m_TickerAccountPositionMap[Key] = AccountPosition;
        }
        Message::TAccountPosition& AccountPosition = m_TickerAccountPositionMap[Key];
        std::string ExchangeID = pInvestorPosition->ExchangeID;
        if(THOST_FTDC_PD_Long == pInvestorPosition->PosiDirection)
        {
            if(ExchangeID == "SHFE" || ExchangeID == "INE")
            {
                if(pInvestorPosition->PositionDate == THOST_FTDC_PSD_Today)
                {
                    AccountPosition.FuturePosition.LongTdVolume = pInvestorPosition->Position;
                    AccountPosition.FuturePosition.LongOpenVolume = pInvestorPosition->OpenVolume;
                }
                else if(pInvestorPosition->PositionDate == THOST_FTDC_PSD_History)
                {
                    AccountPosition.FuturePosition.LongYdVolume = pInvestorPosition->Position;
                }
            }
            else
            {
                AccountPosition.FuturePosition.LongTdVolume = pInvestorPosition->TodayPosition;
                // 昨仓 = 总持仓 - 今仓；
                AccountPosition.FuturePosition.LongYdVolume = pInvestorPosition->Position - pInvestorPosition->TodayPosition;
                AccountPosition.FuturePosition.LongOpenVolume = pInvestorPosition->OpenVolume;
                if(AccountPosition.FuturePosition.LongOpenVolume > 0)
                {
                    AccountPosition.FuturePosition.LongTdVolume += AccountPosition.FuturePosition.LongYdVolume;
                    AccountPosition.FuturePosition.LongYdVolume = 0;
                }
            }
            AccountPosition.FuturePosition.LongOpeningVolume = 0;
            AccountPosition.FuturePosition.LongClosingTdVolume = 0;
            AccountPosition.FuturePosition.LongClosingYdVolume = 0;
            strncpy(AccountPosition.UpdateTime, Utils::getCurrentTimeUs(), sizeof(AccountPosition.UpdateTime));
        }
        else if(THOST_FTDC_PD_Short == pInvestorPosition->PosiDirection)
        {
            if(ExchangeID == "SHFE" || ExchangeID == "INE")
            {
                if(pInvestorPosition->PositionDate == THOST_FTDC_PSD_Today)
                {
                    AccountPosition.FuturePosition.ShortTdVolume = pInvestorPosition->Position;
                    AccountPosition.FuturePosition.ShortOpenVolume = pInvestorPosition->OpenVolume;
                }
                else if(pInvestorPosition->PositionDate == THOST_FTDC_PSD_History)
                {
                    AccountPosition.FuturePosition.ShortYdVolume = pInvestorPosition->Position;
                }
            }
            else
            {
                AccountPosition.FuturePosition.ShortTdVolume = pInvestorPosition->TodayPosition;
                // 昨仓 = 总持仓 - 今仓；
                AccountPosition.FuturePosition.ShortYdVolume = pInvestorPosition->Position - pInvestorPosition->TodayPosition;
                AccountPosition.FuturePosition.ShortOpenVolume = pInvestorPosition->OpenVolume;
                if(AccountPosition.FuturePosition.ShortOpenVolume > 0)
                {
                    AccountPosition.FuturePosition.ShortTdVolume += AccountPosition.FuturePosition.ShortYdVolume;
                    AccountPosition.FuturePosition.ShortYdVolume = 0;
                }
            }
            AccountPosition.FuturePosition.ShortOpeningVolume  = 0;
            AccountPosition.FuturePosition.ShortClosingTdVolume  = 0;
            AccountPosition.FuturePosition.ShortClosingYdVolume  = 0;
            strncpy(AccountPosition.UpdateTime, Utils::getCurrentTimeUs(), sizeof(AccountPosition.UpdateTime));
        }
    }
    else if(NULL != pRspInfo)
    {
        char errorBuffer[512] = {0};
        Utils::CodeConvert(pRspInfo->ErrorMsg, sizeof(pRspInfo->ErrorMsg), errorBuffer,
                       sizeof(errorBuffer), "gb2312", "utf-8");
        FMTLOG(fmtlog::WRN, "CTPTrader:OnRspQryInvestorPosition BrokerID:{}, Account:{}, ErrorID:{}, ErrorMessage:{}",
                m_XTraderConfig.BrokerID, m_XTraderConfig.Account, pRspInfo->ErrorID, errorBuffer);
    }
    if(bIsLast)
    {
        for (auto it = m_TickerAccountPositionMap.begin(); it != m_TickerAccountPositionMap.end(); it++)
        {
            if(strnlen(it->second.UpdateTime, sizeof(it->second.UpdateTime)) > 0)
            {
                Message::PackMessage message;
                memset(&message, 0, sizeof(message));
                message.MessageType = Message::EMessageType::EAccountPosition;
                memcpy(&message.AccountPosition, &(it->second), sizeof(message.AccountPosition));
                while(!m_ReportMessageQueue.Push(message));
            }
        }
    }
}

void CTPTradeGateWay::OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(!IsRspError(pRspInfo) && pOrder != NULL)
    {
        char errorBuffer[512] = {0};
        Utils::CodeConvert(pOrder->StatusMsg, sizeof(pOrder->StatusMsg), errorBuffer,
                        sizeof(errorBuffer), "gb2312", "utf-8");
        std::string OrderLocalID = pOrder->OrderLocalID;
        if(!(THOST_FTDC_OST_Canceled == pOrder->OrderStatus || THOST_FTDC_OST_AllTraded == pOrder->OrderStatus))
        {
            Message::TOrderStatus& OrderStatus = m_OrderStatusMap[pOrder->OrderRef];
            OrderStatus.BusinessType = m_XTraderConfig.BusinessType;
            // Update OrderStatus
            strncpy(OrderStatus.Product, m_XTraderConfig.Product.c_str(), sizeof(OrderStatus.Product));
            strncpy(OrderStatus.Broker, m_XTraderConfig.Broker.c_str(), sizeof(OrderStatus.Broker));
            strncpy(OrderStatus.Account, pOrder->InvestorID, sizeof(OrderStatus.Account));
            strncpy(OrderStatus.ExchangeID, pOrder->ExchangeID, sizeof(OrderStatus.ExchangeID));
            strncpy(OrderStatus.Ticker, pOrder->InstrumentID, sizeof(OrderStatus.Ticker));
            strncpy(OrderStatus.OrderRef, pOrder->OrderRef, sizeof(OrderStatus.OrderRef));
            strncpy(OrderStatus.OrderSysID, pOrder->OrderSysID, sizeof(OrderStatus.OrderSysID));
            strncpy(OrderStatus.OrderLocalID, pOrder->OrderLocalID, sizeof(OrderStatus.OrderLocalID));
            OrderStatus.SendPrice = pOrder->LimitPrice;
            OrderStatus.SendVolume = pOrder->VolumeTotalOriginal;
            OrderStatus.OrderType = Ordertype(pOrder->TimeCondition, pOrder->VolumeCondition);
            Utils::CodeConvert(pOrder->StatusMsg, sizeof(pOrder->StatusMsg), OrderStatus.ErrorMsg,
                            sizeof(OrderStatus.ErrorMsg), "gb2312", "utf-8");
            std::string InsertTime = std::string(Utils::getCurrentDay()) + " " + pOrder->InsertTime + ".000000";
            strncpy(OrderStatus.InsertTime, InsertTime.c_str(), sizeof(OrderStatus.InsertTime));
            strncpy(OrderStatus.BrokerACKTime, InsertTime.c_str(), sizeof(OrderStatus.BrokerACKTime));
            strncpy(OrderStatus.ExchangeACKTime, InsertTime.c_str(), sizeof(OrderStatus.ExchangeACKTime));
            if(THOST_FTDC_OST_PartTradedQueueing == pOrder->OrderStatus)
            {
                OrderStatus.OrderStatus = Message::EOrderStatusType::EPARTTRADED;
                OrderStatus.TradedVolume =  pOrder->VolumeTraded - OrderStatus.TotalTradedVolume;
                OrderStatus.TotalTradedVolume =  pOrder->VolumeTraded;
                OrderStatus.TradedPrice = pOrder->LimitPrice;
                OrderStatus.TradedAvgPrice = pOrder->LimitPrice;
            }
            else if(THOST_FTDC_OST_NoTradeQueueing == pOrder->OrderStatus)
            {
                OrderStatus.OrderStatus = Message::EOrderStatusType::EEXCHANGE_ACK;
            }
            std::string Account = pOrder->InvestorID;
            std::string Ticker = pOrder->InstrumentID;
            std::string Key = Account + ":" + Ticker;
            OrderStatus.OrderSide = OrderSide(pOrder->Direction, pOrder->CombOffsetFlag[0], Key);
            OrderStatus.OrderToken = pOrder->RequestID;
            UpdateOrderStatus(OrderStatus);

            // Update Position
            Message::TAccountPosition& AccountPosition = m_TickerAccountPositionMap[Key];
            strncpy(AccountPosition.UpdateTime, Utils::getCurrentTimeUs(), sizeof(AccountPosition.UpdateTime));
            UpdateFrozenPosition(OrderStatus, AccountPosition);

            PrintOrderStatus(OrderStatus, "CTPTrader::OnRspQryOrder ");
            PrintAccountPosition(AccountPosition, "CTPTrader::OnRspQryOrder ");
        }
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspQryOrder, BrokerID:{}, Account:{} InstrumentID:{} OrderRef:{} UserID:{} OrderPriceType:{} Direction:{} "
                            "CombOffsetFlag:{} CombHedgeFlag:{} LimitPrice:{} VolumeTotalOriginal:{} TimeCondition:{} VolumeCondition:{} MinVolume:{} "
                            "ContingentCondition:{} StopPrice:{} ForceCloseReason:{} IsAutoSuspend:{} BusinessUnit:{} RequestID:{} OrderLocalID:{} "
                            "ExchangeID:{} ParticipantID:{} ClientID:{} TraderID:{} InstallID:{} OrderSubmitStatus:{} NotifySequence:{} SettlementID:{} "
                            "OrderSysID:{} OrderSource:{} OrderStatus:{} OrderType:{} VolumeTraded:{} VolumeTotal:{} InsertDate:{} InsertTime:{} "
                            "ActiveTime:{} SuspendTime:{} UpdateTime:{} CancelTime:{} ActiveTraderID:{} ClearingPartID:{} SequenceNo:{} "
                            "FrontID:{} SessionID:{} UserProductInfo:{} StatusMsg:{} UserForceClose:{} ActiveUserID:{} BrokerOrderSeq:{} "
                            "RelativeOrderSysID:{} ZCETotalTradedVolume:{} IsSwapOrder:{} BranchID:{} InvestUnitID:{} AccountID:{} "
                            "MacAddress:{} ExchangeInstID:{} IPAddress:{} nRequestID:{} bIsLast:{}",
                pOrder->BrokerID, pOrder->InvestorID,pOrder->InstrumentID,pOrder->OrderRef, pOrder->UserID,
                pOrder->OrderPriceType, pOrder->Direction, pOrder->CombOffsetFlag,
                pOrder->CombHedgeFlag, pOrder->LimitPrice, pOrder->VolumeTotalOriginal,
                pOrder->TimeCondition, pOrder->VolumeCondition, pOrder->MinVolume,
                pOrder->ContingentCondition, pOrder->StopPrice, pOrder->ForceCloseReason,
                pOrder->IsAutoSuspend, pOrder->BusinessUnit,  pOrder->RequestID, pOrder->OrderLocalID,
                pOrder->ExchangeID, pOrder->ParticipantID, pOrder->ClientID, pOrder->TraderID,
                pOrder->InstallID, pOrder->OrderSubmitStatus, pOrder->NotifySequence, pOrder->SettlementID,
                pOrder->OrderSysID, pOrder->OrderSource, pOrder->OrderStatus, pOrder->OrderType,
                pOrder->VolumeTraded, pOrder->VolumeTotal, pOrder->InsertDate, pOrder->InsertTime,
                pOrder->ActiveTime, pOrder->SuspendTime, pOrder->UpdateTime, pOrder->CancelTime,
                pOrder->ActiveTraderID, pOrder->ClearingPartID, pOrder->SequenceNo, pOrder->FrontID,
                pOrder->SessionID, pOrder->UserProductInfo, errorBuffer, pOrder->UserForceClose,
                pOrder->ActiveUserID, pOrder->BrokerOrderSeq, pOrder->RelativeOrderSysID, pOrder->ZCETotalTradedVolume,
                pOrder->IsSwapOrder, pOrder->BranchID, pOrder->InvestUnitID, pOrder->AccountID, pOrder->MacAddress,
                pOrder->ExchangeInstID, pOrder->IPAddress, nRequestID, bIsLast);
    }
    else if(pRspInfo != NULL)
    {
        char errorBuffer[512] = {0};
        Utils::CodeConvert(pOrder->StatusMsg, sizeof(pOrder->StatusMsg), errorBuffer,
                        sizeof(errorBuffer), "gb2312", "utf-8");
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspQryOrder, Account:{}, ErrorID:{}, ErrorMessage:{}",
                m_XTraderConfig.Account, pRspInfo->ErrorID, errorBuffer);
    }

    if(bIsLast)
    {
        if(m_XTraderConfig.CancelAll)
        {
            for(auto it = m_OrderStatusMap.begin(); it != m_OrderStatusMap.end(); it++)
            {
                CancelOrder(it->second);
            }
        }
    }
}

void CTPTradeGateWay::OnRspQryTrade(CThostFtdcTradeField *pTrade, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(!IsRspError(pRspInfo) && pTrade != NULL)
    {
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspQryTrade, BrokerID:{}, Account:{} InstrumentID:{} OrderRef:{} UserID:{} ExchangeID:{} "
                            "TradeID:{} Direction:{} OrderSysID:{} ParticipantID:{} ClientID:{} TradingRole:{} OffsetFlag:{} HedgeFlag:{} "
                            "Price:{} Volume:{} TradeDate:{} TradeTime:{} TradeType:{} PriceSource:{} TraderID:{} OrderLocalID:{} "
                            "ClearingPartID:{} BusinessUnit:{} SequenceNo:{} SettlementID:{} BrokerOrderSeq:{} TradeSource:{} InvestUnitID:{} "
                            "ExchangeInstID:{}",
                pTrade->BrokerID, pTrade->InvestorID, pTrade->InstrumentID,pTrade->OrderRef, pTrade->UserID,
                pTrade->ExchangeID, pTrade->TradeID, pTrade->Direction,
                pTrade->OrderSysID, pTrade->ParticipantID, pTrade->ClientID,
                pTrade->TradingRole, pTrade->OffsetFlag, pTrade->HedgeFlag,
                pTrade->Price, pTrade->Volume, pTrade->TradeDate,
                pTrade->TradeTime, pTrade->TradeType,  pTrade->PriceSource, pTrade->TraderID,
                pTrade->OrderLocalID, pTrade->ClearingPartID, pTrade->BusinessUnit, pTrade->SequenceNo,
                pTrade->SettlementID, pTrade->BrokerOrderSeq, pTrade->TradeSource, pTrade->InvestUnitID,
                pTrade->ExchangeInstID);
    }
    else if(pRspInfo != NULL)
    {
        char errorBuffer[512] = {0};
        Utils::CodeConvert(pRspInfo->ErrorMsg, sizeof(pRspInfo->ErrorMsg), errorBuffer,
                        sizeof(errorBuffer), "gb2312", "utf-8");
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspQryTrade, Account:{}, ErrorID:{}, ErrorMessage:{}",
                m_XTraderConfig.Account, pRspInfo->ErrorID, errorBuffer);
    }
}

void CTPTradeGateWay::OnRspQryInstrumentMarginRate(CThostFtdcInstrumentMarginRateField *pInstrumentMarginRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(!IsRspError(pRspInfo) && pInstrumentMarginRate != NULL)
    {
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspQryInstrumentMarginRate, BrokerID:{}, Account:{} InstrumentID:{} HedgeFlag:{} "
                            "LongMarginRatioByMoney:{} LongMarginRatioByVolume:{} ShortMarginRatioByMoney:{} ShortMarginRatioByVolume:{} ExchangeID:{}",
                pInstrumentMarginRate->BrokerID, pInstrumentMarginRate->InvestorID, pInstrumentMarginRate->InstrumentID,
                pInstrumentMarginRate->HedgeFlag, pInstrumentMarginRate->LongMarginRatioByMoney,
                pInstrumentMarginRate->LongMarginRatioByVolume, pInstrumentMarginRate->ShortMarginRatioByMoney,
                pInstrumentMarginRate->ShortMarginRatioByVolume, pInstrumentMarginRate->ExchangeID);
    }
    else if(pRspInfo != NULL)
    {
        char errorBuffer[512] = {0};
        Utils::CodeConvert(pRspInfo->ErrorMsg, sizeof(pRspInfo->ErrorMsg), errorBuffer,
                        sizeof(errorBuffer), "gb2312", "utf-8");
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspQryInstrumentMarginRate, Account:{}, ErrorID:{}, ErrorMessage:{}",
                m_XTraderConfig.Account, pRspInfo->ErrorID, errorBuffer);
    }
}

void CTPTradeGateWay::OnRspQryBrokerTradingParams(CThostFtdcBrokerTradingParamsField *pBrokerTradingParams, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(!IsRspError(pRspInfo) && pBrokerTradingParams != NULL)
    {
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspQryBrokerTradingParams, BrokerID:{}, Account:{} MarginPriceType:{} Algorithm:{} AvailIncludeCloseProfit:{}",
                pBrokerTradingParams->BrokerID, pBrokerTradingParams->InvestorID, pBrokerTradingParams->MarginPriceType,
                pBrokerTradingParams->Algorithm, pBrokerTradingParams->AvailIncludeCloseProfit);
    }
    else if(pRspInfo != NULL)
    {
        char errorBuffer[512] = {0};
        Utils::CodeConvert(pRspInfo->ErrorMsg, sizeof(pRspInfo->ErrorMsg), errorBuffer,
                        sizeof(errorBuffer), "gb2312", "utf-8");
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspQryBrokerTradingParams, Account:{}, ErrorID:{}, ErrorMessage:{}",
                m_XTraderConfig.Account, pRspInfo->ErrorID, errorBuffer);
    }
}

void CTPTradeGateWay::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(!IsRspError(pRspInfo) && pInstrument != NULL)
    {
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspQryInstrument, ExchangeID:{} InstrumentID:{} VolumeMultiple:{} LongMarginRatio:{} "
                            "ShortMarginRatio:{} MaxMarginSideAlgorithm:{} ProductID:{}",
                pInstrument->ExchangeID, pInstrument->InstrumentID, pInstrument->VolumeMultiple, pInstrument->LongMarginRatio, 
                pInstrument->ShortMarginRatio, pInstrument->MaxMarginSideAlgorithm, pInstrument->ProductID);
    }
    else if(pRspInfo != NULL)
    {
        char errorBuffer[512] = {0};
        Utils::CodeConvert(pRspInfo->ErrorMsg, sizeof(pRspInfo->ErrorMsg), errorBuffer,
                        sizeof(errorBuffer), "gb2312", "utf-8");
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspQryInstrument, Account:{}, ErrorID:{}, ErrorMessage:{}",
                m_XTraderConfig.Account, pRspInfo->ErrorID, errorBuffer);
    }
}

void CTPTradeGateWay::OnRspQryInstrumentCommissionRate(CThostFtdcInstrumentCommissionRateField *pInstrumentCommissionRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(!IsRspError(pRspInfo) && pInstrumentCommissionRate != NULL)
    {
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspQryInstrumentCommissionRate, ExchangeID:{} BrokerID:{}, Account:{} InstrumentID:{} "
                            "OpenRatioByMoney:{} OpenRatioByVolume:{} CloseRatioByMoney:{} CloseRatioByVolume:{} CloseTodayRatioByMoney:{} "
                            "CloseTodayRatioByVolume:{}",
                pInstrumentCommissionRate->ExchangeID, pInstrumentCommissionRate->BrokerID, pInstrumentCommissionRate->InvestorID, 
                pInstrumentCommissionRate->InstrumentID, pInstrumentCommissionRate->OpenRatioByMoney, pInstrumentCommissionRate->OpenRatioByVolume,
                pInstrumentCommissionRate->CloseRatioByMoney, pInstrumentCommissionRate->CloseRatioByVolume, 
                pInstrumentCommissionRate->CloseTodayRatioByMoney, pInstrumentCommissionRate->CloseTodayRatioByVolume);
    }
    else if(pRspInfo != NULL)
    {
        char errorBuffer[512] = {0};
        Utils::CodeConvert(pRspInfo->ErrorMsg, sizeof(pRspInfo->ErrorMsg), errorBuffer,
                        sizeof(errorBuffer), "gb2312", "utf-8");
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspQryInstrumentCommissionRate, Account:{}, ErrorID:{}, ErrorMessage:{}",
                m_XTraderConfig.Account, pRspInfo->ErrorID, errorBuffer);
    }
}

void CTPTradeGateWay::OnRspQryInstrumentOrderCommRate(CThostFtdcInstrumentOrderCommRateField *pInstrumentOrderCommRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(!IsRspError(pRspInfo) && pInstrumentOrderCommRate != NULL)
    {
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspQryInstrumentOrderCommRate, ExchangeID:{} BrokerID:{}, Account:{} InstrumentID:{} HedgeFlag:{} "
                            "OrderCommByVolume:{} OrderActionCommByVolume:{}",
                pInstrumentOrderCommRate->ExchangeID, pInstrumentOrderCommRate->BrokerID, pInstrumentOrderCommRate->InvestorID, 
                pInstrumentOrderCommRate->InstrumentID, pInstrumentOrderCommRate->HedgeFlag, pInstrumentOrderCommRate->OrderCommByVolume,
                pInstrumentOrderCommRate->OrderActionCommByVolume);
    }
    else if(pRspInfo != NULL)
    {
        char errorBuffer[512] = {0};
        Utils::CodeConvert(pRspInfo->ErrorMsg, sizeof(pRspInfo->ErrorMsg), errorBuffer,
                        sizeof(errorBuffer), "gb2312", "utf-8");
        FMTLOG(fmtlog::INF, "CTPTrader:OnRspQryInstrumentOrderCommRate, Account:{}, ErrorID:{}, ErrorMessage:{}",
                m_XTraderConfig.Account, pRspInfo->ErrorID, errorBuffer);
    }
}