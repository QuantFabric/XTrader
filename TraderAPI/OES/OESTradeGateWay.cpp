#include "OESTradeGateWay.h"
#include "XPluginEngine.hpp"

CreateObjectFunc(OESTradeGateWay);

OESTradeGateWay::OESTradeGateWay()
{
    m_ConnectedStatus = Message::ELoginStatus::ELOGIN_PREPARED;
}

OESTradeGateWay::~OESTradeGateWay()
{
    DestroyTraderAPI();
}

void OESTradeGateWay::LoadAPIConfig()
{
    std::string errorString;
    bool ok = Utils::LoadOESError(m_XTraderConfig.ErrorPath.c_str(), m_CodeErrorMap, errorString);
    if(ok)
    {
        FMTLOG(fmtlog::INF, "OESTrader::LoadAPIConfig Account:{} LoadOESError {} successed", m_XTraderConfig.Account, m_XTraderConfig.ErrorPath);
    }
    else
    {
        FMTLOG(fmtlog::WRN, "OESTrader::LoadAPIConfig Account:{} LoadOESError {} failed, {}", m_XTraderConfig.Account, m_XTraderConfig.ErrorPath, errorString);
    }
}

void OESTradeGateWay::GetCommitID(std::string& CommitID, std::string& UtilsCommitID)
{
    CommitID = SO_COMMITID;
    UtilsCommitID = SO_UTILS_COMMITID;
}

void OESTradeGateWay::GetAPIVersion(std::string& APIVersion)
{
    APIVersion = API_VERSION;
}

void OESTradeGateWay::CreateTraderAPI()
{
    m_OESTraderAPI = new OESTraderAPI();
    FMTLOG(fmtlog::INF, "OESTrader::CreateTraderAPI Account:{} API Version:{}", m_XTraderConfig.Account, m_OESTraderAPI->GetVersion());
}

void OESTradeGateWay::DestroyTraderAPI()
{
    if(NULL != m_OESTraderAPI) 
    {
        m_OESTraderAPI->Stop();
        delete m_OESTraderAPI;
        m_OESTraderAPI = NULL;
    }
}

void OESTradeGateWay::LoadTrader()
{
    m_OESTraderAPI->RegisterSpi(this);
    m_OESTraderAPI->LoadConfig(m_XTraderConfig.TraderAPIConfig.c_str());
    bool ret = m_OESTraderAPI->Start();
    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EEventLog;
    strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
    strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
    strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
    strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
    strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
    if(!ret) 
    {
        m_ConnectedStatus = Message::ELoginStatus::ELOGIN_FAILED;
        message.EventLog.Level = Message::EEventLogLevel::EWARNING;
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event), 
                        "OESTrader::LoadTrader Start failed, Account:{} ret={}", m_XTraderConfig.Account, ret);
        while(!m_ReportMessageQueue.Push(message));

        FMTLOG(fmtlog::ERR, "OESTrader::LoadTrader Start failed, Account:{} ret={}", m_XTraderConfig.Account, ret);
    }
    else
    {
        m_ConnectedStatus = Message::ELoginStatus::ELOGIN_SUCCESSED;
        message.EventLog.Level = Message::EEventLogLevel::EINFO;
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event), 
                        "OESTrader::LoadTrader Start successed, Account:{} OrderChannel:{} lastOutMsgSeq:{} ReportChannel:{}", 
                        m_XTraderConfig.Account, m_OESTraderAPI->GetOrdChannelCount(), m_OESTraderAPI->m_DefaultClSeqNo, m_OESTraderAPI->GetRptChannelCount());
        while(!m_ReportMessageQueue.Push(message));

        FMTLOG(fmtlog::INF, "OESTrader::LoadTrader Start successed, Account:{} OrderChannel:{} lastOutMsgSeq:{} ReportChannel:{}", 
                m_XTraderConfig.Account, m_OESTraderAPI->GetOrdChannelCount(), m_OESTraderAPI->m_DefaultClSeqNo, m_OESTraderAPI->GetRptChannelCount());
        // 查询账户信息
        QueryClientOverview();
        // 查询新股新债发行
        QueryIssue();
        // 查询新股新债中签
        QueryLotWinning();
    }
}

void OESTradeGateWay::ReLoadTrader()
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
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event), "OESTrader::ReLoadTrader Account:{}", m_XTraderConfig.Account);
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));

        FMTLOG(fmtlog::WRN, "OESTrader::ReLoadTrader Account:{}", m_XTraderConfig.Account);
    }
}

void OESTradeGateWay::ReqUserLogin()
{

}

int OESTradeGateWay::ReqQryFund()
{
    OesQryCashAssetFilterT  qryFilter = {NULLOBJ_OES_QRY_CASH_ASSET_FILTER};
    int ret = m_OESTraderAPI->QueryCashAsset(&qryFilter);
    std::string buffer = "OESTrader:ReqQryFund QueryCashAsset Account:" + m_XTraderConfig.Account;
    HandleRetCode(ret, buffer);
    return 0;
}

int OESTradeGateWay::ReqQryPoistion()
{
    OesQryStkHoldingFilterT qryFilter = {NULLOBJ_OES_QRY_STK_HOLDING_FILTER};
    int ret = m_OESTraderAPI->QueryStkHolding(&qryFilter);
    std::string buffer = "OESTrader:ReqQryPoistion QueryStkHolding Account:" + m_XTraderConfig.Account;
    HandleRetCode(ret, buffer);
    return 0;
}

int OESTradeGateWay::ReqQryOrder()
{
    OesQryOrdFilterT qryFilter = {NULLOBJ_OES_QRY_ORD_FILTER};
    // 查询未完成订单
    qryFilter.isUnclosedOnly = 1;
    int ret = m_OESTraderAPI->QueryOrder(&qryFilter);
    std::string buffer = "OESTrader:ReqQryOrder QueryOrder Account:" + m_XTraderConfig.Account;
    HandleRetCode(ret, buffer);
    return 0;
}
int OESTradeGateWay::ReqQryTrade()
{
    OesQryTrdFilterT qryFilter = {NULLOBJ_OES_QRY_TRD_FILTER};
    int ret = m_OESTraderAPI->QueryTrade(&qryFilter);
    std::string buffer = "OESTrader:ReqQryTrade QueryTrade Account:" + m_XTraderConfig.Account;
    HandleRetCode(ret, buffer);
    return 0;
}

int OESTradeGateWay::ReqQryTickerRate()
{
    return 0;
}

void OESTradeGateWay::ReqInsertOrder(const Message::TOrderRequest& request)
{
    OesOrdReqT OrderReq = {NULLOBJ_OES_ORD_REQ};
    OrderReq.clSeqNo = ++m_OESTraderAPI->m_DefaultClSeqNo;
    OrderReq.mktId = OESExchangeID(m_TickerExchangeMap[request.Ticker]);
    // Ticker不在TickerList内
    if(0 == OrderReq.mktId)
    {
        if(Utils::startWith(request.Ticker, "SH"))
        {
            OrderReq.mktId = eOesMarketIdT::OES_MKT_SH_ASHARE;
            m_TickerExchangeMap[request.Ticker] = "SH";
        }
        else if(Utils::startWith(request.Ticker, "SZ"))
        {
            OrderReq.mktId = eOesMarketIdT::OES_MKT_SZ_ASHARE;
            m_TickerExchangeMap[request.Ticker] = "SZ";
        }
    }
    OrderReq.ordType = OESOrderType(request.OrderType);
    OrderReq.bsType = OESOrderDirection(request);
    OrderReq.ordQty = request.Volume;
    OrderReq.ordPrice = request.Price * 10000;
    strncpy(OrderReq.securityId, request.Ticker + 2, 6);
    if(1)
    {
        if(request.Volume == 100)
        {
            OrderReq.userInfo.i32[0] = 0x7BEE00D0; // 挂单
        }
        else if(request.Volume == 200)
        {
            OrderReq.userInfo.i32[0] = 0x7BEE00D1; // 委托未上报
        }
        else if(request.Volume == 300)
        {
            OrderReq.userInfo.i32[0] = 0x7BEE00E0; // 交易所废单
        }
        else if(request.Volume == 400)
        {
            OrderReq.userInfo.i32[0] = 0x7BEE00E1; // 交易所拒绝 (平台未开放)
        }
        else if(request.Volume == 500)
        {
            OrderReq.userInfo.i32[0] = 0x7BEE00F1; // 部分成交
        }
        else if(request.Volume == 600)
        {
            OrderReq.userInfo.i32[0] = 0x7BEE00F2; // 部分成交,指定成交数量生成单笔成交, 成交数量通过 ordReq.userInfo.i32[1] 指定
            OrderReq.userInfo.i32[1] = 200;
        }
        else if(request.Volume == 700)
        {
            OrderReq.userInfo.i32[0] = 0x7BEE00F3; // 部分成交, 指定总成交数量生成多笔成交(2~5笔), 总成交数量通过 ordReq.userInfo.i32[1] 指定
            OrderReq.userInfo.i32[1] = 500;
        }
        else if(request.Volume >= 800)
        {
            OrderReq.userInfo.i32[0] = 0x7BEE00F0; // 全部成交
        }
    }

    std::string OrderRef = fmt::format("{:012}", OrderReq.clSeqNo);
    // Order Status
    Message::TOrderStatus& OrderStatus = m_OrderStatusMap[OrderRef];
    OrderStatus.BusinessType = m_XTraderConfig.BusinessType;
    strncpy(OrderStatus.Product, m_XTraderConfig.Product.c_str(), sizeof(OrderStatus.Product));
    strncpy(OrderStatus.Broker, m_XTraderConfig.Broker.c_str(), sizeof(OrderStatus.Broker));
    strncpy(OrderStatus.Account, request.Account, sizeof(OrderStatus.Account));
    strncpy(OrderStatus.ExchangeID, m_TickerExchangeMap[request.Ticker].c_str(), sizeof(OrderStatus.ExchangeID));
    strncpy(OrderStatus.Ticker, request.Ticker, sizeof(OrderStatus.Ticker));
    strncpy(OrderStatus.OrderRef, OrderRef.c_str(), sizeof(OrderStatus.OrderRef));
    strncpy(OrderStatus.RiskID, request.RiskID, sizeof(OrderStatus.RiskID));
    OrderStatus.SendPrice = request.Price;
    OrderStatus.SendVolume = request.Volume;
    OrderStatus.OrderType = request.OrderType;
    OrderStatus.OrderToken = request.OrderToken;
    OrderStatus.OrderSide = OrderSide(OrderReq.bsType);
    OrderStatus.OrderStatus = Message::EOrderStatusType::EORDER_SENDED;
    OrderStatus.EngineID = request.EngineID;
    strncpy(OrderStatus.SendTime, request.SendTime, sizeof(OrderStatus.SendTime));
    strncpy(OrderStatus.InsertTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.InsertTime));
    strncpy(OrderStatus.UpdateTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.UpdateTime));
    PrintOrderStatus(OrderStatus, "OESTrader::ReqInsertOrder ");

    int ret = m_OESTraderAPI->SendOrder(&OrderReq);
    std::string op = std::string("OESTrader:ReqInsertOrder SendOrder Account:") + request.Account + " Ticker:"
                     + request.Ticker + " OrderRef:" + OrderRef;
    HandleRetCode(ret, op);
    if(0 == ret)
    {
        UpdateOrderStatus(OrderStatus);
    }
    else
    {
        m_OrderStatusMap.erase(OrderRef);
    }
}

void OESTradeGateWay::ReqInsertOrderRejected(const Message::TOrderRequest& request)
{
    OesOrdReqT OrderReq = {NULLOBJ_OES_ORD_REQ};
    OrderReq.mktId = OESExchangeID(m_TickerExchangeMap[request.Ticker]);
    OrderReq.ordType = OESOrderType(request.OrderType);
    OrderReq.bsType = OESOrderDirection(request);
    OrderReq.ordQty = request.Volume;
    OrderReq.ordPrice = request.Price * 10000;
    // Order Status
    Message::TOrderStatus OrderStatus;
    memset(&OrderStatus, 0, sizeof(OrderStatus));
    OrderStatus.BusinessType = m_XTraderConfig.BusinessType;
    strncpy(OrderStatus.Product, m_XTraderConfig.Product.c_str(), sizeof(OrderStatus.Product));
    strncpy(OrderStatus.Broker, m_XTraderConfig.Broker.c_str(), sizeof(OrderStatus.Broker));
    strncpy(OrderStatus.Account, m_XTraderConfig.Account.c_str(), sizeof(OrderStatus.Account));
    strncpy(OrderStatus.Ticker, request.Ticker, sizeof(OrderStatus.Ticker));
    strncpy(OrderStatus.ExchangeID, request.ExchangeID, sizeof(OrderStatus.ExchangeID));
    strncpy(OrderStatus.RiskID, request.RiskID, sizeof(OrderStatus.RiskID));
    OrderStatus.SendPrice = request.Price;
    OrderStatus.SendVolume = request.Volume;
    OrderStatus.OrderType = request.OrderType;
    OrderStatus.OrderToken = request.OrderToken;
    OrderStatus.OrderSide = OrderSide(OrderReq.bsType);
    strncpy(OrderStatus.SendTime, request.SendTime, sizeof(OrderStatus.SendTime));
    strncpy(OrderStatus.InsertTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.InsertTime));
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
    strncpy(OrderStatus.UpdateTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.UpdateTime));
    UpdateOrderStatus(OrderStatus);
    PrintOrderStatus(OrderStatus, "OESTrader::ReqInsertOrderRejected ");
}

void OESTradeGateWay::ReqCancelOrder(const Message::TActionRequest& request)
{
    auto it = m_OrderStatusMap.find(request.OrderRef);
    if(it == m_OrderStatusMap.end())
    {
        FMTLOG(fmtlog::WRN, "OESTrader::ReqCancelOrder Account:{} OrderRef:{} not found.", request.Account, request.OrderRef);
        return ;
    }
    Message::TOrderStatus& OrderStatus = m_OrderStatusMap[request.OrderRef];
    OesOrdCancelReqT CancelReq = {NULLOBJ_OES_ORD_CANCEL_REQ};
    CancelReq.clSeqNo = (int32)++m_OESTraderAPI->m_DefaultClSeqNo;
    CancelReq.mktId = OESExchangeID(m_TickerExchangeMap[OrderStatus.Ticker]);
    if(0 == CancelReq.mktId)
    {
        if(Utils::startWith(OrderStatus.Ticker, "SH"))
        {
            CancelReq.mktId = eOesMarketIdT::OES_MKT_SH_ASHARE;
        }
        else if(Utils::startWith(OrderStatus.Ticker, "SZ"))
        {
            CancelReq.mktId = eOesMarketIdT::OES_MKT_SZ_ASHARE;
        }
    }
    CancelReq.origClSeqNo = atoi(request.OrderRef);
    int ret = m_OESTraderAPI->SendCancelOrder(&CancelReq);
    std::string op = std::string("OESTrader:ReqCancelOrder SendCancelOrder Account:") + OrderStatus.Account + " OrderRef:"
                     + OrderStatus.OrderRef + " OrderLocalID:" + OrderStatus.OrderLocalID + " OrderSysID:" + OrderStatus.OrderSysID;
    HandleRetCode(ret, op);
    if(0 == ret)
    {
        OrderStatus.OrderStatus = Message::EOrderStatusType::ECANCELLING;
        strncpy(OrderStatus.RiskID, request.RiskID, sizeof(OrderStatus.RiskID));
    }
    UpdateOrderStatus(OrderStatus);
    PrintOrderStatus(OrderStatus, "OESTrader::ReqCancelOrder ");
}

void OESTradeGateWay::ReqCancelOrderRejected(const Message::TActionRequest& request)
{
    auto it = m_OrderStatusMap.find(request.OrderRef);
    if(it == m_OrderStatusMap.end())
    {
        FMTLOG(fmtlog::WRN, "OESTrader::ReqCancelOrderRejected Account:{} OrderRef:{} not found.", request.Account, request.OrderRef);
        return ;
    }
    Message::TOrderStatus& OrderStatus = m_OrderStatusMap[request.OrderRef];
    OrderStatus.OrderStatus = Message::EOrderStatusType::ERISK_ACTION_REJECTED;
    OrderStatus.ErrorID = request.ErrorID;
    strncpy(OrderStatus.ErrorMsg, request.ErrorMsg, sizeof(OrderStatus.ErrorMsg));
    strncpy(OrderStatus.RiskID, request.RiskID, sizeof(OrderStatus.RiskID));
    UpdateOrderStatus(OrderStatus);
    PrintOrderStatus(OrderStatus, "OESTrader::ReqCancelOrderRejected ");
}

void OESTradeGateWay::RepayMarginDirect(double value)
{
    int ret = m_OESTraderAPI->SendCreditCashRepayReq(value * 10000, OES_CRD_ASSIGNABLE_REPAY_MODE_DEFAULT, NULL, NULL);
    std::string stringBuffer = fmt::format("OESTrader:RepayMarginDirect SendCreditCashRepayReq Account:{} money:{:.2f}", m_XTraderConfig.Account, value);
    HandleRetCode(ret, stringBuffer);
    if(ret == 0)
    {
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EINFO;
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
        strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
        strncpy(message.EventLog.Event, stringBuffer.c_str(), sizeof(message.EventLog.Event));
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));
    }
}

void OESTradeGateWay::TransferFundIn(double value)
{
    OesFundTrsfReqT FundTrsfReq = {NULLOBJ_OES_FUND_TRSF_REQ};
    FundTrsfReq.clSeqNo = ++m_OESTraderAPI->m_DefaultClSeqNo;
    FundTrsfReq.direct = eOesFundTrsfDirectT::OES_FUND_TRSF_DIRECT_IN;
    FundTrsfReq.occurAmt = value * 10000; 
    FundTrsfReq.fundTrsfType = eOesFundTrsfTypeT::OES_FUND_TRSF_TYPE_OES_COUNTER;
    int ret = m_OESTraderAPI->SendFundTrsf(&FundTrsfReq);
    std::string stringBuffer = fmt::format("OESTrader:TransferFundIn SendFundTrsf Account:{} money:{:.2f}", m_XTraderConfig.Account, value);
    HandleRetCode(ret, stringBuffer);
    if(ret == 0)
    {
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EWARNING;
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
        strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
        strncpy(message.EventLog.Event, stringBuffer.c_str(), sizeof(message.EventLog.Event));
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));
    }
}

void OESTradeGateWay::TransferFundOut(double value)
{
    OesFundTrsfReqT FundTrsfReq = {NULLOBJ_OES_FUND_TRSF_REQ};
    FundTrsfReq.clSeqNo = ++m_OESTraderAPI->m_DefaultClSeqNo;
    FundTrsfReq.direct = eOesFundTrsfDirectT::OES_FUND_TRSF_DIRECT_OUT;
    FundTrsfReq.occurAmt = value * 10000;
    FundTrsfReq.fundTrsfType = eOesFundTrsfTypeT::OES_FUND_TRSF_TYPE_OES_COUNTER;
    int ret = m_OESTraderAPI->SendFundTrsf(&FundTrsfReq);
    std::string stringBuffer = fmt::format("OESTrader:TransferFundOut SendFundTrsf Account:{} money:{:.2f}", m_XTraderConfig.Account, value);
    HandleRetCode(ret, stringBuffer);
    if(ret == 0)
    {
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EWARNING;
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
        strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
        strncpy(message.EventLog.Event, stringBuffer.c_str(), sizeof(message.EventLog.Event));
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));
    }
}

void OESTradeGateWay::HandleRetCode(int code, const std::string& op)
{
    std::string Account = m_XTraderConfig.Account;
    std::string errorString = Account + " ";
    if(code >= 0)
    {
        errorString = op + " successed";
        FMTLOG(fmtlog::INF, "{} successed", op);
    }
    else if(code < 0)
    {
        std::string buffer;
        auto it = m_CodeErrorMap.find(-code);
        if(it != m_CodeErrorMap.end())
        {
            errorString = op + " failed " + it->second;
        }
        else
        {
            errorString = op + " failed, unkown Error.";
        }
        FMTLOG(fmtlog::INF, "{} ret={}", errorString, code);
    }
    // 错误发送监控EventLog
    if(code >= 0)
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

int OESTradeGateWay::OESOrderDirection(const Message::TOrderRequest& request)
{
    int direction = -1;
    if(Message::EOrderDirection::EBUY == request.Direction)
    {
        direction = eOesBuySellTypeT::OES_BS_TYPE_BUY;
    }
    else if(Message::EOrderDirection::ESELL == request.Direction)
    {
        direction = eOesBuySellTypeT::OES_BS_TYPE_SELL;
    }
    else if(Message::EOrderDirection::EREVERSE_REPO == request.Direction)
    {
        direction = eOesBuySellTypeT::OES_BS_TYPE_REVERSE_REPO;
    }
    else if(Message::EOrderDirection::ESUBSCRIPTION == request.Direction)
    {
        direction = eOesBuySellTypeT::OES_BS_TYPE_SUBSCRIPTION;
    }
    else if(Message::EOrderDirection::EALLOTMENT  == request.Direction)
    {
        direction = eOesBuySellTypeT::OES_BS_TYPE_ALLOTMENT;
    }
    if(m_XTraderConfig.BusinessType == Message::EBusinessType::ECREDIT)
    {
        // 担保品买入
        if(Message::EOrderDirection::EBUY == request.Direction)
        {
            direction = eOesBuySellTypeT::OES_BS_TYPE_COLLATERAL_BUY;
        }
        // 担保品卖出
        else if(Message::EOrderDirection::ESELL == request.Direction)
        {
            direction = eOesBuySellTypeT::OES_BS_TYPE_COLLATERAL_SELL;
        }
        // 担保品转入
        else if(Message::EOrderDirection::ECOLLATERAL_TRANSFER_IN == request.Direction)
        {
            direction = eOesBuySellTypeT::OES_BS_TYPE_COLLATERAL_TRANSFER_IN;
        }
        // 担保品转出
        else if(Message::EOrderDirection::ECOLLATERAL_TRANSFER_OUT  == request.Direction)
        {
            direction = eOesBuySellTypeT::OES_BS_TYPE_COLLATERAL_TRANSFER_OUT;
        }
        // 融资买入
        else if(Message::EOrderDirection::EMARGIN_BUY  == request.Direction)
        {
            direction = eOesBuySellTypeT::OES_BS_TYPE_MARGIN_BUY;
        }
        // 卖券还款
        else if(Message::EOrderDirection::EREPAY_MARGIN_BY_SELL  == request.Direction)
        {
            direction = eOesBuySellTypeT::OES_BS_TYPE_REPAY_MARGIN_BY_SELL;
        }
        // 融券卖出
        else if(Message::EOrderDirection::ESHORT_SELL  == request.Direction)
        {
            direction = eOesBuySellTypeT::OES_BS_TYPE_SHORT_SELL;
        }
        // 买券还券
        else if(Message::EOrderDirection::EREPAY_STOCK_BY_BUY  == request.Direction)
        {
            direction = eOesBuySellTypeT::OES_BS_TYPE_REPAY_STOCK_BY_BUY;
        }
        // 现券还券
        else if(Message::EOrderDirection::EREPAY_STOCK_DIRECT  == request.Direction)
        {
            direction = eOesBuySellTypeT::OES_BS_TYPE_REPAY_STOCK_DIRECT;
        }
    }
    return direction;
}

int OESTradeGateWay::OrderSide(int bsType)
{
    int side = -1;
    if(bsType == eOesBuySellTypeT::OES_BS_TYPE_BUY)
    {
        side = Message::EOrderSide::EOPEN_LONG;
    }
    else if(bsType == eOesBuySellTypeT::OES_BS_TYPE_SELL)
    {
        side = Message::EOrderSide::ECLOSE_LONG;
    }
    else if(bsType == eOesBuySellTypeT::OES_BS_TYPE_REVERSE_REPO)
    {
        side = Message::EOrderSide::ESIDE_REVERSE_REPO;
    }
    else if(bsType == eOesBuySellTypeT::OES_BS_TYPE_SUBSCRIPTION)
    {
        side = Message::EOrderSide::ESIDE_SUBSCRIPTION;
    }
    else if(bsType == eOesBuySellTypeT::OES_BS_TYPE_ALLOTMENT)
    {
        side = Message::EOrderSide::ESIDE_ALLOTMENT;
    }
    if(m_XTraderConfig.BusinessType == Message::EBusinessType::ECREDIT)
    {
        // 担保品买入
        if(bsType == eOesBuySellTypeT::OES_BS_TYPE_COLLATERAL_BUY)
        {
            side = Message::EOrderSide::EOPEN_LONG;
        }
        // 担保品卖出
        else if(bsType == eOesBuySellTypeT::OES_BS_TYPE_COLLATERAL_SELL)
        {
            side = Message::EOrderSide::ECLOSE_LONG;
        }
        // 融资买入
        else if(bsType == eOesBuySellTypeT::OES_BS_TYPE_MARGIN_BUY)
        {
            side = Message::EOrderSide::ESIDE_MARGIN_BUY;
        }
        // 卖券还款
        else if(bsType == eOesBuySellTypeT::OES_BS_TYPE_REPAY_MARGIN_BY_SELL)
        {
            side = Message::EOrderSide::ESIDE_REPAY_MARGIN_BY_SELL;
        }
        // 融券卖出
        else if(bsType == eOesBuySellTypeT::OES_BS_TYPE_SHORT_SELL)
        {
            side = Message::EOrderSide::ESIDE_SHORT_SELL;
        }
        // 买券还券
        else if(bsType == eOesBuySellTypeT::OES_BS_TYPE_REPAY_STOCK_BY_BUY)
        {
            side = Message::EOrderSide::ESIDE_REPAY_STOCK_BY_BUY;
        }
        // 现券还券
        else if(bsType == eOesBuySellTypeT::OES_BS_TYPE_REPAY_STOCK_DIRECT)
        {
            side = Message::EOrderSide::ESIDE_REPAY_STOCK_DIRECT;
        }
    }
    return side;
}

int OESTradeGateWay::OrderType(int OrderType)
{
    int ret = -1;
    if(OrderType == eOesOrdTypeT::OES_ORD_TYPE_FAK)
    {
        ret = Message::EOrderType::EFAK;
    }
    else if(OrderType == eOesOrdTypeT::OES_ORD_TYPE_LMT_FOK)
    {
        ret = Message::EOrderType::EFOK;
    }
    else if(OrderType == eOesOrdTypeT::OES_ORD_TYPE_LMT)
    {
        ret = Message::EOrderType::ELIMIT;
    }
    else if(OrderType == eOesOrdTypeT::OES_ORD_TYPE_MTL_SAMEPARTY_BEST)
    {
        ret = Message::EOrderType::EMARKET;
    }
    return ret;
}

int OESTradeGateWay::OESOrderType(int OrderType)
{
    int ret = -1;
    if(Message::EOrderType::EFAK == OrderType)
    {
        ret = eOesOrdTypeT::OES_ORD_TYPE_FAK;
    }
    else if(Message::EOrderType::EFOK == OrderType)
    {
        ret = eOesOrdTypeT::OES_ORD_TYPE_LMT_FOK;
    }
    else if(Message::EOrderType::ELIMIT == OrderType)
    {
        ret = eOesOrdTypeT::OES_ORD_TYPE_LMT;
    }
    else if(Message::EOrderType::EMARKET == OrderType)
    {
        ret = eOesOrdTypeT::OES_ORD_TYPE_MTL_SAMEPARTY_BEST;
    }
    return ret;
}

std::string OESTradeGateWay::ExchangeID(uint8 MarketID)
{
    std::string Exchange;
    if(eOesMarketIdT::OES_MKT_SH_ASHARE == MarketID)
    {
        Exchange = "SH";
    }
    else if(eOesMarketIdT::OES_MKT_SZ_ASHARE == MarketID)
    {
         Exchange = "SZ";
    }
    return Exchange;
}

uint8 OESTradeGateWay::OESExchangeID(const std::string& ExchangeID)
{
    uint8 MarketID = 0;
    if(ExchangeID == "SH")
    {
        MarketID = eOesMarketIdT::OES_MKT_SH_ASHARE;
    }
    else if(ExchangeID == "SZ")
    {
        MarketID = eOesMarketIdT::OES_MKT_SZ_ASHARE;
    }
    return MarketID;
}

void OESTradeGateWay::QueryClientOverview()
{
    OesClientOverviewT  clientOverview = {NULLOBJ_OES_CLIENT_OVERVIEW};
    int32 ret = m_OESTraderAPI->GetClientOverview(&clientOverview);
    std::string stringBuffer = fmt::format("OESTrader:QueryClientOverview Account:{}", m_XTraderConfig.Account);
    HandleRetCode(ret, stringBuffer);
    if(ret >= 0) 
    {
        FMTLOG(fmtlog::INF, "OESTrader::QueryClientOverview Account:{} ClientId:{} ClientType:{} ClientStatus:{} ClientName:{} "
                            "BusinessScope:{} SSEStkPbuId:{} SZSEStkPbuId:{} OrdTrafficLimit:{} QryTrafficLimit:{} IsSupportInternalAllot:{} "
                            "AssociatedCustCnt:{}", 
                m_XTraderConfig.Account, clientOverview.clientId, clientOverview.clientType,
                clientOverview.clientStatus, clientOverview.clientName, clientOverview.businessScope, 
                clientOverview.sseStkPbuId, clientOverview.szseStkPbuId, clientOverview.ordTrafficLimit,
                clientOverview.qryTrafficLimit, clientOverview.isSupportInternalAllot, clientOverview.associatedCustCnt);
        for (int i = 0; i < clientOverview.associatedCustCnt; i++) 
        {
            FMTLOG(fmtlog::INF, "OESTrader::QueryClientOverview Account:{} CustId:{} Status:{} RiskLevel:{} BranchId:{} CustName:{}", 
                    m_XTraderConfig.Account, clientOverview.custItems[i].custId, clientOverview.custItems[i].status,
                    clientOverview.custItems[i].riskLevel, clientOverview.custItems[i].branchId, clientOverview.custItems[i].custName);
            if(clientOverview.custItems[i].spotCashAcct.isValid) 
            {
                FMTLOG(fmtlog::INF, "OESTrader::QueryClientOverview Account:{} CashAcctId:{} CashType:{} CashAcctStatus:{} IsFundTrsfDisabled:{}", 
                        m_XTraderConfig.Account, clientOverview.custItems[i].spotCashAcct.cashAcctId, clientOverview.custItems[i].spotCashAcct.cashType,
                        clientOverview.custItems[i].spotCashAcct.cashAcctStatus, clientOverview.custItems[i].spotCashAcct.isFundTrsfDisabled);
            }
            if(clientOverview.custItems[i].shSpotInvAcct.isValid) 
            {
                FMTLOG(fmtlog::INF, "OESTrader::QueryClientOverview Account:{} InvAcctId:{} SHSE MktId:{} Status:{} IsTradeDisabled:{} "
                                    "PBUId:{} TrdOrdCnt:{} NonTrdOrdCnt:{} CancelOrdCnt:{} OESRejectOrdCnt:{} TrdCnt:{}", 
                        m_XTraderConfig.Account, clientOverview.custItems[i].shSpotInvAcct.invAcctId, clientOverview.custItems[i].shSpotInvAcct.mktId, 
                        clientOverview.custItems[i].shSpotInvAcct.status, clientOverview.custItems[i].shSpotInvAcct.isTradeDisabled,
                        clientOverview.custItems[i].shSpotInvAcct.pbuId, clientOverview.custItems[i].shSpotInvAcct.trdOrdCnt,
                        clientOverview.custItems[i].shSpotInvAcct.nonTrdOrdCnt, clientOverview.custItems[i].shSpotInvAcct.cancelOrdCnt,
                        clientOverview.custItems[i].shSpotInvAcct.oesRejectOrdCnt, clientOverview.custItems[i].shSpotInvAcct.exchRejectOrdCnt,
                        clientOverview.custItems[i].shSpotInvAcct.trdCnt);
            }
            if(clientOverview.custItems[i].szSpotInvAcct.isValid) 
            {
                FMTLOG(fmtlog::INF, "OESTrader::QueryClientOverview Account:{} InvAcctId:{} SZSE MktId:{} Status:{} IsTradeDisabled:{} "
                                    "PBUId:{} TrdOrdCnt:{} NonTrdOrdCnt:{} CancelOrdCnt:{} OESRejectOrdCnt:{} TrdCnt:{}", 
                        m_XTraderConfig.Account, clientOverview.custItems[i].szSpotInvAcct.invAcctId, clientOverview.custItems[i].szSpotInvAcct.mktId, 
                        clientOverview.custItems[i].szSpotInvAcct.status, clientOverview.custItems[i].szSpotInvAcct.isTradeDisabled,
                        clientOverview.custItems[i].szSpotInvAcct.pbuId, clientOverview.custItems[i].szSpotInvAcct.trdOrdCnt,
                        clientOverview.custItems[i].szSpotInvAcct.nonTrdOrdCnt, clientOverview.custItems[i].szSpotInvAcct.cancelOrdCnt,
                        clientOverview.custItems[i].szSpotInvAcct.oesRejectOrdCnt, clientOverview.custItems[i].szSpotInvAcct.exchRejectOrdCnt,
                        clientOverview.custItems[i].szSpotInvAcct.trdCnt);
            }
        }
    }
}

void OESTradeGateWay::QueryIssue()
{
    OesQryIssueFilterT  QryFilter = {NULLOBJ_OES_QRY_ISSUE_FILTER};
    int32 ret = m_OESTraderAPI->QueryIssue(&QryFilter);
    std::string stringBuffer = fmt::format("OESTrader:QueryIssue Account:{}", m_XTraderConfig.Account);
    HandleRetCode(ret, stringBuffer);
}

void OESTradeGateWay::QueryLotWinning()
{
    OesQryLotWinningFilterT  QryFilter = {NULLOBJ_OES_QRY_LOT_WINNING_FILTER};
    int32 ret = m_OESTraderAPI->QueryLotWinning(&QryFilter);
    std::string stringBuffer = fmt::format("OESTrader:QueryLotWinning Account:{}", m_XTraderConfig.Account);
    HandleRetCode(ret, stringBuffer);
}

void OESTradeGateWay::QueryCrdCreditAsset()
{
    OesQryCrdCreditAssetFilterT QryFilter = {NULLOBJ_OES_QRY_CRD_CREDIT_ASSET_FILTER};
    int32 ret = m_OESTraderAPI->QueryCrdCreditAsset(&QryFilter);
    std::string stringBuffer = fmt::format("OESTrader:QueryCrdCreditAsset Account:{}", m_XTraderConfig.Account);
    HandleRetCode(ret, stringBuffer);
}

void OESTradeGateWay::QueryCrdCashPosition()
{
    OesQryCrdCashPositionFilterT QryFilter = {NULLOBJ_OES_QRY_CRD_CASH_POSITION_FILTER};
    int32 ret = m_OESTraderAPI->QueryCrdCashPosition(&QryFilter);
    std::string stringBuffer = fmt::format("OESTrader:QueryCrdCashPosition Account:{}", m_XTraderConfig.Account);
    HandleRetCode(ret, stringBuffer);
}

void OESTradeGateWay::QueryCrdSecurityPosition()
{
    OesQryCrdSecurityPositionFilterT QryFilter = {NULLOBJ_OES_QRY_CRD_SECURITY_POSITION_FILTER};
    int32 ret = m_OESTraderAPI->QueryCrdSecurityPosition(&QryFilter);
    std::string stringBuffer = fmt::format("OESTrader:QueryCrdSecurityPosition Account:{}", m_XTraderConfig.Account);
    HandleRetCode(ret, stringBuffer);
}

int32 OESTradeGateWay::OnConnected(eOesApiChannelTypeT channelType, OesApiSessionInfoT *pSessionInfo, OesApiSubscribeInfoT *pSubscribeInfo)
{
    OesAsyncApiChannelT *pAsyncChannel = (OesAsyncApiChannelT*)pSessionInfo->__contextPtr;
    auto channel = pAsyncChannel->pChannelCfg->channelTag;
    auto lastInMsgSeq = pAsyncChannel->lastInMsgSeq;
    if (pAsyncChannel->pChannelCfg->channelType == OESAPI_CHANNEL_TYPE_REPORT) 
    {
        FMTLOG(fmtlog::INF, "OESTrader::OnConnected Account:{} Channel:{} lastInMsgSeq:{}", 
                m_XTraderConfig.Account, channel, lastInMsgSeq);
        pSessionInfo->lastInMsgSeq = -1;
    }
    else if(pAsyncChannel->pChannelCfg->channelType == OESAPI_CHANNEL_TYPE_ORDER)
    {
        FMTLOG(fmtlog::INF, "OESTrader::OnConnected Account:{} Channel:{} lastInMsgSeq:{}", 
                m_XTraderConfig.Account, channel, lastInMsgSeq);
        pSessionInfo->lastInMsgSeq = -1;
    }
    return EAGAIN;
}

int32 OESTradeGateWay::OnDisconnected(eOesApiChannelTypeT channelType, OesApiSessionInfoT *pSessionInfo)
{
    OesAsyncApiChannelT *pAsyncChannel = (OesAsyncApiChannelT *) pSessionInfo->__contextPtr;
    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EEventLog;
    message.EventLog.Level = Message::EEventLogLevel::EWARNING;

    if (pAsyncChannel->pChannelCfg->channelType == OESAPI_CHANNEL_TYPE_REPORT) 
    {
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event), "OESTrader::OnDisconnected Account:{} Channel:{} lastInMsgSeq:{}",
                        m_XTraderConfig.Account, pAsyncChannel->pChannelCfg->channelTag, pAsyncChannel->lastInMsgSeq);
    }
    else if(pAsyncChannel->pChannelCfg->channelType == OESAPI_CHANNEL_TYPE_ORDER)
    {
        fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event), "OESTrader::OnDisconnected Account:{} Channel:{} lastInMsgSeq:{}",
                        m_XTraderConfig.Account, pAsyncChannel->pChannelCfg->channelTag, pAsyncChannel->lastInMsgSeq);  
    }
    FMTLOG(fmtlog::WRN, "{}", message.EventLog.Event);
    strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
    strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
    strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
    strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
    strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
    while(!m_ReportMessageQueue.Push(message));
    return EAGAIN;
}

void OESTradeGateWay::OnBusinessReject(const OesRptMsgHeadT *pRptMsgHead, const OesOrdRejectT *pOrderReject) 
{
    FMTLOG(fmtlog::WRN, "OESTrader::OnBusinessReject Account:{} invAcctId:{} securityId:{} mktId:{} rptMsgType:{} ordRejReason:{} clSeqNo:{} "
                        "origClSeqNo:{} origClOrdId:{} ordType:{} bsType:{} ordQty:{} ordPrice:{}", 
            m_XTraderConfig.Account, pOrderReject->invAcctId, pOrderReject->securityId, pOrderReject->mktId, pRptMsgHead->rptMsgType, 
            pRptMsgHead->ordRejReason, pOrderReject->clSeqNo, pOrderReject->origClSeqNo, pOrderReject->origClOrdId,
            pOrderReject->ordType, pOrderReject->bsType, pOrderReject->ordQty, pOrderReject->ordPrice);
    // 柜台拒单
    std::string OrderRef;
    if(pOrderReject->origClSeqNo > 0)
    {
        OrderRef = fmt::format("{:012}", pOrderReject->origClSeqNo);
    }
    else
    {
        OrderRef = fmt::format("{:012}", pOrderReject->clSeqNo);
    }
    auto it = m_OrderStatusMap.find(OrderRef);
    if(it == m_OrderStatusMap.end())
    {
        std::string Account = m_XTraderConfig.Account;
        std::string ExchangeID;
        if(eOesMarketIdT::OES_MKT_SH_ASHARE == pOrderReject->mktId)
        {
            ExchangeID = "SH";
        }
        else if(eOesMarketIdT::OES_MKT_SZ_ASHARE == pOrderReject->mktId)
        {
            ExchangeID = "SZ";
        }
        else
        {
            ExchangeID = "Unkown";
        }
        std::string Ticker = ExchangeID + std::string(pOrderReject->securityId);

        Message::TOrderStatus& OrderStatus = m_OrderStatusMap[OrderRef];
        OrderStatus.BusinessType = m_XTraderConfig.BusinessType;
        strncpy(OrderStatus.Product, m_XTraderConfig.Product.c_str(), sizeof(OrderStatus.Product));
        strncpy(OrderStatus.Broker, m_XTraderConfig.Broker.c_str(), sizeof(OrderStatus.Broker));
        strncpy(OrderStatus.Account, Account.c_str(), sizeof(OrderStatus.Account));
        strncpy(OrderStatus.ExchangeID, ExchangeID.c_str(), sizeof(OrderStatus.ExchangeID));
        strncpy(OrderStatus.Ticker, Ticker.c_str(), sizeof(OrderStatus.Ticker));
        strncpy(OrderStatus.OrderRef, OrderRef.c_str(), sizeof(OrderStatus.OrderRef));
        OrderStatus.SendVolume = pOrderReject->ordQty;
        OrderStatus.SendPrice = pOrderReject->ordPrice/10000.0;
        if(pOrderReject->ordType == eOesOrdTypeT::OES_ORD_TYPE_FAK)
        {
            OrderStatus.OrderType = Message::EOrderType::EFAK;
        }
        else if(pOrderReject->ordType == eOesOrdTypeT::OES_ORD_TYPE_FOK)
        {
            OrderStatus.OrderType = Message::EOrderType::EFOK;
        }
        else if(pOrderReject->ordType == eOesOrdTypeT::OES_ORD_TYPE_LMT)
        {
            OrderStatus.OrderType = Message::EOrderType::ELIMIT;
        }
        else
        {
            OrderStatus.OrderType = Message::EOrderType::EMARKET;
        }

        OrderStatus.OrderSide = OrderSide(pOrderReject->bsType);
        strncpy(OrderStatus.SendTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.SendTime));
        strncpy(OrderStatus.InsertTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.InsertTime));
    }
    Message::TOrderStatus& OrderStatus = m_OrderStatusMap[OrderRef];
    // 撤单被OES拒绝
    if(pOrderReject->origClSeqNo > 0)
    {
        OrderStatus.OrderStatus = Message::EOrderStatusType::EACTION_ERROR;
    }
    else // 报单被OES拒绝
    {
        OrderStatus.OrderStatus = Message::EOrderStatusType::EBROKER_ERROR;
    }
    OrderStatus.ErrorID = pOrderReject->ordRejReason;
    std::string errorString = m_CodeErrorMap[pOrderReject->ordRejReason];
    strncpy(OrderStatus.ErrorMsg, errorString.c_str(), sizeof(OrderStatus.ErrorMsg));
    strncpy(OrderStatus.UpdateTime, Utils::getCurrentTimeUs(), sizeof(OrderStatus.UpdateTime));
    UpdateOrderStatus(OrderStatus);
    PrintOrderStatus(OrderStatus, "OESTrader::OnBusinessReject ");
}

void OESTradeGateWay::OnOrderInsert(const OesRptMsgHeadT *pRptMsgHead, const OesOrdCnfmT *pOrderInsert) 
{
    FMTLOG(fmtlog::DBG, "OESTrader::OnOrderInsert Account:{} invAcctId:{} securityId:{} mktId:{} clSeqNo:{} origClSeqNo:{} clOrdId:{} "
                        "origClOrdId:{} ordStatus:{} ordType:{} bsType:{} ordQty:{} ordPrice:{} rptMsgType:{} ordRejReason:{}", 
            m_XTraderConfig.Account, pOrderInsert->invAcctId, pOrderInsert->securityId, pOrderInsert->mktId, pOrderInsert->clSeqNo, 
            pOrderInsert->origClSeqNo, pOrderInsert->clOrdId, pOrderInsert->origClOrdId, pOrderInsert->ordStatus, pOrderInsert->ordType, 
            pOrderInsert->bsType, pOrderInsert->ordQty, pOrderInsert->ordPrice, pRptMsgHead->rptMsgType, pRptMsgHead->ordRejReason);
    // 柜台ACK
    std::string OrderRef = fmt::format("{:012}", pOrderInsert->clSeqNo);
    auto it = m_OrderStatusMap.find(OrderRef);
    if(it != m_OrderStatusMap.end())
    {
        std::string errorString = m_CodeErrorMap[pRptMsgHead->ordRejReason];
        it->second.ErrorID = pRptMsgHead->ordRejReason;
        strncpy(it->second.ErrorMsg, errorString.c_str(), sizeof(it->second.ErrorMsg));
        it->second.OrderStatus = Message::EOrderStatusType::EBROKER_ACK;
        fmt::format_to_n(it->second.OrderLocalID, sizeof(it->second.OrderLocalID), "{}", pOrderInsert->clOrdId);
        strncpy(it->second.BrokerACKTime, Utils::getCurrentTimeUs(), sizeof(it->second.BrokerACKTime));
        UpdateOrderStatus(it->second);
        // PrintOrderStatus(it->second, "OESTrader::OnOrderInsert ");
    }
}

void OESTradeGateWay::OnOrderReport(const OesRptMsgHeadT *pRptMsgHead, const OesOrdCnfmT *pOrderReport) 
{
    FMTLOG(fmtlog::INF, "OESTrader::OnOrderReport Account:{} invAcctId:{} securityId:{} clSeqNo:{} origClSeqNo:{} clOrdId:{} origClOrdId:{} "
                        "exchOrdId:{} mktId:{} ordType:{} bsType:{} ordQty:{} cumQty:{} canceledQty:{} ordPrice:{} ordStatus:{} rptMsgType:{} "
                        "ordRejReason:{} ordRejReason:{} exchErrCode:{}", 
            m_XTraderConfig.Account, pOrderReport->invAcctId, pOrderReport->securityId, pOrderReport->clSeqNo, pOrderReport->origClSeqNo, 
            pOrderReport->clOrdId, pOrderReport->origClOrdId, pOrderReport->exchOrdId, pOrderReport->mktId, pOrderReport->ordType, 
            pOrderReport->bsType, pOrderReport->ordQty,  pOrderReport->cumQty,  pOrderReport->canceledQty, pOrderReport->ordPrice, 
            pOrderReport->ordStatus, pRptMsgHead->rptMsgType, pRptMsgHead->ordRejReason, pOrderReport->ordRejReason, pOrderReport->exchErrCode);
    // 交易所ACK、交易所拒单、
    std::string OrderRef;
    if(pOrderReport->origClSeqNo > 0)
    {
        OrderRef = fmt::format("{:012}", pOrderReport->origClSeqNo);
    }
    else
    {
        OrderRef = fmt::format("{:012}", pOrderReport->clSeqNo);
    }
    auto it = m_OrderStatusMap.find(OrderRef);
    if(it != m_OrderStatusMap.end())
    {
        std::string errorString = m_CodeErrorMap[pRptMsgHead->ordRejReason];
        it->second.ErrorID = pRptMsgHead->ordRejReason;
        strncpy(it->second.ErrorMsg, errorString.c_str(), sizeof(it->second.ErrorMsg));
        if(eOesOrdStatusT::OES_ORD_STATUS_DECLARED == pOrderReport->ordStatus)
        {
            it->second.OrderStatus = Message::EOrderStatusType::EEXCHANGE_ACK;
        }
        else if(eOesOrdStatusT::OES_ORD_STATUS_PARTIALLY_FILLED == pOrderReport->ordStatus)
        {
            it->second.OrderStatus = Message::EOrderStatusType::EPARTTRADED;
        }
        else if(eOesOrdStatusT::OES_ORD_STATUS_PARTIALLY_CANCELED == pOrderReport->ordStatus)
        {
            it->second.OrderStatus = Message::EOrderStatusType::EPARTTRADED_CANCELLED;
            it->second.CanceledVolume = pOrderReport->canceledQty;
        }
        else if(eOesOrdStatusT::OES_ORD_STATUS_CANCELED == pOrderReport->ordStatus)
        {
            it->second.OrderStatus = Message::EOrderStatusType::ECANCELLED;
            it->second.CanceledVolume = pOrderReport->canceledQty;
        }
        else if(eOesOrdStatusT::OES_ORD_STATUS_FILLED == pOrderReport->ordStatus)
        {
            it->second.OrderStatus = Message::EOrderStatusType::EALLTRADED;
        }
        else if(eOesOrdStatusT::__OES_ORD_STATUS_VALID_MAX < pOrderReport->ordStatus)
        {
            // 撤单被交易所拒绝
            if(pOrderReport->origClSeqNo > 0)
            {
                it->second.OrderStatus = Message::EOrderStatusType::EACTION_ERROR;
            }
            else // 报单被交易所拒绝
            {
                it->second.OrderStatus = Message::EOrderStatusType::EEXCHANGE_ERROR;
            }
            it->second.ErrorID = pOrderReport->ordRejReason;
            std::string errorString = m_CodeErrorMap[pOrderReport->ordRejReason];
            strncpy(it->second.ErrorMsg, errorString.c_str(), sizeof(it->second.ErrorMsg));
        }
        if(strnlen(it->second.OrderSysID, sizeof(it->second.OrderSysID)) == 0)
        {
            strncpy(it->second.OrderSysID, pOrderReport->exchOrdId, sizeof(it->second.OrderSysID));
            strncpy(it->second.ExchangeACKTime, Utils::getCurrentTimeUs(), sizeof(it->second.ExchangeACKTime));
            OnExchangeACK(it->second);
        }
        // Update OrderStatus
        if(it->second.OrderType == Message::EOrderType::ELIMIT)
        {
            // 对于LIMIT订单，在成交回报更新时更新订单状态
            if(Message::EOrderStatusType::EALLTRADED != it->second.OrderStatus && Message::EOrderStatusType::EPARTTRADED != it->second.OrderStatus)
            {
                UpdateOrderStatus(it->second);
            }
            if(Message::EOrderStatusType::EPARTTRADED_CANCELLED == it->second.OrderStatus ||
                    Message::EOrderStatusType::ECANCELLED == it->second.OrderStatus ||
                    Message::EOrderStatusType::EEXCHANGE_ERROR == it->second.OrderStatus)
            {
                // remove Order
                m_OrderStatusMap.erase(it);
            }
        }
        else if(it->second.OrderType == Message::EOrderType::EFAK || it->second.OrderType == Message::EOrderType::EFOK)
        {
            if(Message::EOrderStatusType::EALLTRADED != it->second.OrderStatus && 
                    Message::EOrderStatusType::EPARTTRADED_CANCELLED != it->second.OrderStatus)
            {
                UpdateOrderStatus(it->second);
            }
            if(Message::EOrderStatusType::ECANCELLED == it->second.OrderStatus ||
                    Message::EOrderStatusType::EEXCHANGE_ERROR == it->second.OrderStatus)
            {
                // remove Order
                m_OrderStatusMap.erase(it);
            }
        }
    } 
}

void OESTradeGateWay::OnTradeReport(const OesRptMsgHeadT *pRptMsgHead, const OesTrdCnfmT *pTradeReport) 
{
    FMTLOG(fmtlog::INF, "OESTrader::OnTradeReport Account:{} invAcctId:{} securityId:{} mktId:{} clSeqNo:{} exchTrdNum:{} trdSide:{} "
                        "ordStatus:{} ordType:{} ordBuySellType:{} trdQty:{} trdPrice:{} trdAmt:{} trdFee:{}", 
            m_XTraderConfig.Account, pTradeReport->invAcctId, pTradeReport->securityId, pTradeReport->mktId, pTradeReport->clSeqNo, 
            pTradeReport->exchTrdNum, pTradeReport->trdSide, pTradeReport->ordStatus, pTradeReport->ordType, pTradeReport->ordBuySellType, 
            pTradeReport->trdQty, pTradeReport->trdPrice, pTradeReport->trdAmt, pTradeReport->trdFee);
    // 成交回报
    std::string OrderRef = fmt::format("{:012}", pTradeReport->clSeqNo);
    auto it = m_OrderStatusMap.find(OrderRef);
    if(it != m_OrderStatusMap.end())
    {
        std::string errorString = m_CodeErrorMap[pRptMsgHead->ordRejReason];
        it->second.ErrorID = pRptMsgHead->ordRejReason;
        strncpy(it->second.ErrorMsg, errorString.c_str(), sizeof(it->second.ErrorMsg));

        double preTradedAmount = it->second.TotalTradedVolume * it->second.TradedAvgPrice;
        it->second.TradedVolume =  pTradeReport->trdQty;
        it->second.TradedPrice =  pTradeReport->trdPrice / 10000.0;
        it->second.TotalTradedVolume += pTradeReport->trdQty;
        it->second.TradedAvgPrice = (preTradedAmount + pTradeReport->trdQty * pTradeReport->trdPrice / 10000.0) / it->second.TotalTradedVolume;
        std::string Account = m_XTraderConfig.Account;
        if(Message::EOrderType::ELIMIT ==  it->second.OrderType)
        {
            // Upadte OrderStatus
            if(it->second.TotalTradedVolume == it->second.SendVolume)
            {
                it->second.OrderStatus = Message::EOrderStatusType::EALLTRADED;
            }
            else
            {
                it->second.OrderStatus = Message::EOrderStatusType::EPARTTRADED;
            }
            UpdateOrderStatus(it->second);
            // PrintOrderStatus(it->second, "OESTrader::OnRtnTrade ");
            // remove Order When AllTraded
            if(Message::EOrderStatusType::EALLTRADED == it->second.OrderStatus)
            {
                m_OrderStatusMap.erase(it);
            }
        }
        else if(Message::EOrderType::EFAK == it->second.OrderType || Message::EOrderType::EFOK == it->second.OrderType)
        {
            if(it->second.TotalTradedVolume == it->second.SendVolume)
            {
                it->second.OrderStatus = Message::EOrderStatusType::EALLTRADED;
            }
            else if(it->second.TotalTradedVolume == it->second.SendVolume - it->second.CanceledVolume)
            {
                it->second.OrderStatus = Message::EOrderStatusType::EPARTTRADED_CANCELLED;
            }
            else
            {
                it->second.OrderStatus = Message::EOrderStatusType::EPARTTRADED;
            }
            UpdateOrderStatus(it->second);
            // PrintOrderStatus(it->second, "OESTrader::OnRtnTrade ");
            if(Message::EOrderStatusType::EALLTRADED == it->second.OrderStatus ||
                    Message::EOrderStatusType::EPARTTRADED_CANCELLED == it->second.OrderStatus)
            {
                m_OrderStatusMap.erase(it);
            }
        }
    }
}

void OESTradeGateWay::OnCashAssetVariation(const OesCashAssetItemT *pCashAssetItem) 
{
    Message::TAccountFund& AccountFund = m_AccountFundMap[m_XTraderConfig.Account];
    AccountFund.BusinessType = m_XTraderConfig.BusinessType;
    strncpy(AccountFund.Product, m_XTraderConfig.Product.c_str(), sizeof(AccountFund.Product));
    strncpy(AccountFund.Broker, m_XTraderConfig.Broker.c_str(), sizeof(AccountFund.Broker));
    strncpy(AccountFund.Account, m_XTraderConfig.Account.c_str(), sizeof(AccountFund.Account));
    if(m_XTraderConfig.BusinessType == Message::EBusinessType::ESTOCK)
    {
        AccountFund.Deposit = pCashAssetItem->totalDepositAmt / 10000.0;
        AccountFund.Withdraw = pCashAssetItem->totalWithdrawAmt / 10000.0;
        AccountFund.Commission = pCashAssetItem->totalFeeAmt / 10000.0;
        AccountFund.PreBalance = pCashAssetItem->beginningBal / 10000.0;
        AccountFund.CurrMargin = 0;
        AccountFund.CloseProfit = 0;
        AccountFund.PositionProfit = 0;
        AccountFund.Available = pCashAssetItem->currentAvailableBal / 10000.0;
        AccountFund.WithdrawQuota = pCashAssetItem->currentDrawableBal / 10000.0;
        AccountFund.ExchangeMargin = 0;
        AccountFund.Balance = pCashAssetItem->currentTotalBal / 10000.0;
    }
    else if(m_XTraderConfig.BusinessType == Message::EBusinessType::ECREDIT)
    {
        AccountFund.Deposit = pCashAssetItem->totalDepositAmt / 10000.0;
        AccountFund.Withdraw = pCashAssetItem->totalWithdrawAmt / 10000.0;
        AccountFund.Commission = pCashAssetItem->totalFeeAmt / 10000.0;
        AccountFund.PreBalance = pCashAssetItem->beginningBal / 10000.0;
        AccountFund.CurrMargin = 0;
        AccountFund.CloseProfit = 0;
        AccountFund.PositionProfit = 0;
        AccountFund.Available = pCashAssetItem->creditExt.marginAvailableBal / 10000.0;
        AccountFund.WithdrawQuota = pCashAssetItem->creditExt.drawableBal / 10000.0;
        AccountFund.ExchangeMargin = 0;
        AccountFund.Balance = pCashAssetItem->creditExt.totalAssetValue / 10000.0;
    }
    strncpy(AccountFund.UpdateTime, Utils::getCurrentTimeUs(), sizeof(AccountFund.UpdateTime));

    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EAccountFund;
    memcpy(&message.AccountFund, &AccountFund, sizeof(AccountFund));
    while(!m_ReportMessageQueue.Push(message));

    FMTLOG(fmtlog::DBG, "OESTrader::OnCashAssetVariation custId:{} cashAcctId:{} beginningBal:{} currentTotalBal:{} currentAvailableBal:{} "
                        "currentDrawableBal:{} totalDepositAmt:{} totalWithdrawAmt:{} cashAcctId:{} totalAssetValue:{} marginAvailableBal:{} "
                        "availableBal:{} drawableBal:{}", 
            pCashAssetItem->custId, pCashAssetItem->cashAcctId, pCashAssetItem->beginningBal, pCashAssetItem->currentTotalBal,
            pCashAssetItem->currentAvailableBal, pCashAssetItem->currentDrawableBal, pCashAssetItem->totalDepositAmt, pCashAssetItem->totalWithdrawAmt,
            pCashAssetItem->creditExt.cashAcctId, pCashAssetItem->creditExt.totalAssetValue, pCashAssetItem->creditExt.marginAvailableBal, 
            pCashAssetItem->creditExt.availableBal, pCashAssetItem->creditExt.drawableBal);
}

void OESTradeGateWay::OnStockHoldingVariation(const OesStkHoldingItemT *pStkHoldingItem) 
{
    std::string Account = m_XTraderConfig.Account;
    std::string Exchange = ExchangeID(pStkHoldingItem->mktId);
    std::string Ticker = Exchange + std::string(pStkHoldingItem->securityId);
    std::string Key = Account + ":" + Ticker;
    auto it = m_TickerAccountPositionMap.find(Key);
    if(m_TickerAccountPositionMap.end() == it)
    {
        Message::TAccountPosition AccountPosition;
        memset(&AccountPosition, 0, sizeof(AccountPosition));
        AccountPosition.BusinessType = m_XTraderConfig.BusinessType;
        strncpy(AccountPosition.Product,  m_XTraderConfig.Product.c_str(), sizeof(AccountPosition.Product));
        strncpy(AccountPosition.Broker,  m_XTraderConfig.Broker.c_str(), sizeof(AccountPosition.Broker));
        strncpy(AccountPosition.Account, m_XTraderConfig.Account.c_str(), sizeof(AccountPosition.Account));
        strncpy(AccountPosition.Ticker, Ticker.c_str(), sizeof(AccountPosition.Ticker));
        strncpy(AccountPosition.ExchangeID, Exchange.c_str(), sizeof(AccountPosition.ExchangeID));
        m_TickerAccountPositionMap[Key] = AccountPosition;
    }
    Message::TAccountPosition& AccountPosition = m_TickerAccountPositionMap[Key];
     if(m_XTraderConfig.BusinessType == Message::EBusinessType::ESTOCK && 0 == pStkHoldingItem->isCreditHolding)
    {
        // 昨仓可用持仓 
        AccountPosition.StockPosition.LongYdPosition = pStkHoldingItem->originalAvlHld; 
        AccountPosition.StockPosition.LongPosition = pStkHoldingItem->sumHld; // 当前持仓
        AccountPosition.StockPosition.LongTdBuy = pStkHoldingItem->totalBuyHld; // 今日买入量
        AccountPosition.StockPosition.LongTdSell = pStkHoldingItem->totalSellHld; // 今日卖出量
    }
    else if(m_XTraderConfig.BusinessType == Message::EBusinessType::ECREDIT && 1 == pStkHoldingItem->isCreditHolding)
    {
        // 昨仓可用持仓
        AccountPosition.StockPosition.LongYdPosition = pStkHoldingItem->originalAvlHld; 
        AccountPosition.StockPosition.LongPosition = pStkHoldingItem->sumHld; // 当前持仓
        AccountPosition.StockPosition.LongTdBuy = pStkHoldingItem->totalBuyHld; // 今日买入量
        AccountPosition.StockPosition.LongTdSell = pStkHoldingItem->totalSellHld; // 今日卖出量

        //  日初融券负债数量 = 日初融券负债数量 (日初融券余量, 不包括日初已还) - 当日已归还融券数量
        AccountPosition.StockPosition.ShortYdPosition = pStkHoldingItem->creditExt.shortSellOriginDebtQty - pStkHoldingItem->creditExt.shortSellRepaidQty; // 
        AccountPosition.StockPosition.ShortPosition = pStkHoldingItem->creditExt.shortSellDebtQty; 
        AccountPosition.StockPosition.ShortTdSell = pStkHoldingItem->creditExt.shortSellDebtQty + pStkHoldingItem->creditExt.shortSellRepaidQty - AccountPosition.StockPosition.ShortYdPosition ; 
        AccountPosition.StockPosition.ShortTdBuy = pStkHoldingItem->creditExt.shortSellRepaidQty; 
        AccountPosition.StockPosition.ShortDirectRepaid = 0; 
        AccountPosition.StockPosition.SpecialPositionAvl = pStkHoldingItem->creditExt.specialSecurityPositionAvailableQty; // 专项证券头寸可用数量
    }
    strncpy(AccountPosition.UpdateTime, Utils::getCurrentTimeUs(), sizeof(AccountPosition.UpdateTime));

    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EAccountPosition;
    memcpy(&message.AccountPosition, &AccountPosition, sizeof(message.AccountPosition));
    while(!m_ReportMessageQueue.Push(message));

    FMTLOG(fmtlog::DBG, "OESTrader::OnStockHoldingVariation invAcctId:{} securityId:{} mktId:{} securityType:{} subSecurityType:{} productType:{} "
                        "isCreditHolding:{} originalHld:{} totalBuyHld:{} totalSellHld:{} totalTrsfInHld:{} totalTrsfOutHld:{} originalAvlHld:{} "
                        "sellAvlHld:{} specialSecurityPositionAvailableQty:{} collateralHoldingQty:{} marginBuyOriginDebtQty:{} marginBuyDebtQty:{} "
                        "marginBuyRepaidQty:{} shortSellOriginDebtQty:{} shortSellDebtQty:{} shortSellRepaidQty:{} repayStockDirectAvlHld:{} "
                        "collateralTotalRepayDirectQty:{}", 
            pStkHoldingItem->invAcctId, pStkHoldingItem->securityId, pStkHoldingItem->mktId, pStkHoldingItem->securityType,
            pStkHoldingItem->subSecurityType, pStkHoldingItem->productType, pStkHoldingItem->isCreditHolding, pStkHoldingItem->originalHld, 
            pStkHoldingItem->totalBuyHld, pStkHoldingItem->totalSellHld, pStkHoldingItem->totalTrsfInHld, pStkHoldingItem->totalTrsfOutHld,
            pStkHoldingItem->originalAvlHld, pStkHoldingItem->creditExt.sellAvlHld, pStkHoldingItem->creditExt.specialSecurityPositionAvailableQty,
            pStkHoldingItem->creditExt.collateralHoldingQty, pStkHoldingItem->creditExt.marginBuyOriginDebtQty,  pStkHoldingItem->creditExt.marginBuyDebtQty, 
            pStkHoldingItem->creditExt.marginBuyRepaidQty, pStkHoldingItem->creditExt.shortSellOriginDebtQty, 
            pStkHoldingItem->creditExt.shortSellDebtQty, pStkHoldingItem->creditExt.shortSellRepaidQty,
            pStkHoldingItem->creditExt.repayStockDirectAvlHld, pStkHoldingItem->creditExt.collateralTotalRepayDirectQty);
}

void OESTradeGateWay::OnFundTrsfReject(const OesRptMsgHeadT *pRptMsgHead, const OesFundTrsfRejectT *pFundTrsfReject) 
{
    FMTLOG(fmtlog::WRN, "OESTrader::OnFundTrsfReject cashAcctId:{} rptMsgType:{} ordRejReason:{} clSeqNo:{} direct:{} "
                        "occurAmt:{} rejReason:{} errorInfo:{} fundTrsfType:{}", 
            pFundTrsfReject->cashAcctId, pRptMsgHead->rptMsgType, pRptMsgHead->ordRejReason, pFundTrsfReject->clSeqNo, pFundTrsfReject->direct, 
            pFundTrsfReject->occurAmt, pFundTrsfReject->rejReason, pFundTrsfReject->errorInfo, pFundTrsfReject->fundTrsfType);
    if(eOesFundTrsfTypeT::OES_FUND_TRSF_TYPE_OES_COUNTER == pFundTrsfReject->fundTrsfType)
    {
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EWARNING;
        if(eOesFundTrsfDirectT::OES_FUND_TRSF_DIRECT_IN == pFundTrsfReject->direct)
        {
            fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event), 
                            "TransferFundIn Failed, cashAcctId:{} Amount:{} ordRejReason:{} rejReason:{} errorInfo:{}",
                            pFundTrsfReject->cashAcctId, pFundTrsfReject->occurAmt/10000.0, pRptMsgHead->ordRejReason, 
                            pFundTrsfReject->rejReason, pFundTrsfReject->errorInfo);
        }
        else if(eOesFundTrsfDirectT::OES_FUND_TRSF_DIRECT_OUT == pFundTrsfReject->direct)
        {
            fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event),
                            "TransferFundOut Failed, cashAcctId:{} Amount:{} ordRejReason:{} rejReason:{} errorInfo:{}",    
                            pFundTrsfReject->cashAcctId, pFundTrsfReject->occurAmt/10000.0, pRptMsgHead->ordRejReason,
                            pFundTrsfReject->rejReason, pFundTrsfReject->errorInfo);
        }
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
        strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));
    }
}

void OESTradeGateWay::OnFundTrsfReport(const OesRptMsgHeadT *pRptMsgHead, const OesFundTrsfReportT *pFundTrsfReport) 
{
    FMTLOG(fmtlog::DBG, "OESTrader::OnFundTrsfReport cashAcctId:{} rptMsgType:{} ordRejReason:{} clSeqNo:{} direct:{} occurAmt:{} "
                        "rejReason:{} errorInfo:{} fundTrsfType:{}", 
            pFundTrsfReport->cashAcctId, pRptMsgHead->rptMsgType, pRptMsgHead->ordRejReason, pFundTrsfReport->clSeqNo, pFundTrsfReport->direct, 
            pFundTrsfReport->occurAmt, pFundTrsfReport->rejReason, pFundTrsfReport->errorInfo, pFundTrsfReport->fundTrsfType);
    if(eOesFundTrsfTypeT::OES_FUND_TRSF_TYPE_OES_COUNTER == pFundTrsfReport->fundTrsfType)
    {
        Message::PackMessage message;
        memset(&message, 0, sizeof(message));
        message.MessageType = Message::EMessageType::EEventLog;
        message.EventLog.Level = Message::EEventLogLevel::EWARNING;
        if(eOesFundTrsfDirectT::OES_FUND_TRSF_DIRECT_IN == pFundTrsfReport->direct)
        {
            fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event),
                            "TransferFundIn Success, cashAcctId:{} Amount:{} ordRejReason:{} rejReason:{} errorInfo:{}",
                            pFundTrsfReport->cashAcctId, pFundTrsfReport->occurAmt/10000.0,  pRptMsgHead->ordRejReason, 
                            pFundTrsfReport->rejReason, pFundTrsfReport->errorInfo);
        }
        else if(eOesFundTrsfDirectT::OES_FUND_TRSF_DIRECT_OUT == pFundTrsfReport->direct)
        {
            fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event),
                            "TransferFundOut Success, cashAcctId:{} Amount:{} ordRejReason:{} rejReason:{} errorInfo:{}",
                            pFundTrsfReport->cashAcctId, pFundTrsfReport->occurAmt/10000.0, pRptMsgHead->ordRejReason, 
                            pFundTrsfReport->rejReason, pFundTrsfReport->errorInfo);
        }
        strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
        strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
        strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
        strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
        strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
        while(!m_ReportMessageQueue.Push(message));
    }
}

void OESTradeGateWay::OnNotifyReport(const OesNotifyInfoReportT *pNotifyInfoRpt) 
{
    FMTLOG(fmtlog::INF, "OESTrader::OnReportSynchronizationRsp Account:{} notifySeqNo:{} notifySource:{} notifyType:{} notifyLevel:{} "
                        "notifyScope:{} tranTime:{} businessType:{} custId:{} securityId:{} mktId:{} content:{}", 
            m_XTraderConfig.Account, pNotifyInfoRpt->notifySeqNo, pNotifyInfoRpt->notifySource, pNotifyInfoRpt->notifyType, 
            pNotifyInfoRpt->notifyLevel, pNotifyInfoRpt->notifyScope, pNotifyInfoRpt->tranTime,
            pNotifyInfoRpt->businessType, pNotifyInfoRpt->custId, pNotifyInfoRpt->securityId,
            pNotifyInfoRpt->mktId, pNotifyInfoRpt->content);
}

void OESTradeGateWay::OnReportSynchronizationRsp(const OesReportSynchronizationRspT *pReportSynchronization) 
{
    FMTLOG(fmtlog::DBG, "OESTrader::OnReportSynchronizationRsp Account:{} lastRptSeqNum:{}", m_XTraderConfig.Account, pReportSynchronization->lastRptSeqNum);
}

void OESTradeGateWay::OnQueryCashAsset(const OesCashAssetItemT *pCashAsset, const OesQryCursorT *pCursor, int32 requestId) 
{
    Message::TAccountFund& AccountFund = m_AccountFundMap[m_XTraderConfig.Account];
    AccountFund.BusinessType = m_XTraderConfig.BusinessType;
    strncpy(AccountFund.Product, m_XTraderConfig.Product.c_str(), sizeof(AccountFund.Product));
    strncpy(AccountFund.Broker, m_XTraderConfig.Broker.c_str(), sizeof(AccountFund.Broker));
    strncpy(AccountFund.Account, m_XTraderConfig.Account.c_str(), sizeof(AccountFund.Account));
    if(m_XTraderConfig.BusinessType == Message::EBusinessType::ESTOCK)
    {
        AccountFund.Deposit = pCashAsset->totalDepositAmt / 10000.0;
        AccountFund.Withdraw = pCashAsset->totalWithdrawAmt / 10000.0;
        AccountFund.Commission = pCashAsset->totalFeeAmt / 10000.0;
        AccountFund.PreBalance = pCashAsset->beginningBal / 10000.0;
        AccountFund.CurrMargin = 0;
        AccountFund.CloseProfit = 0;
        AccountFund.PositionProfit = 0;
        AccountFund.Available = pCashAsset->currentAvailableBal / 10000.0;
        AccountFund.WithdrawQuota = pCashAsset->currentDrawableBal / 10000.0;
        AccountFund.ExchangeMargin = 0;
        AccountFund.Balance = pCashAsset->currentTotalBal / 10000.0;
    }
    else if(m_XTraderConfig.BusinessType == Message::EBusinessType::ECREDIT)
    {
        AccountFund.Deposit = pCashAsset->totalDepositAmt / 10000.0;
        AccountFund.Withdraw = pCashAsset->totalWithdrawAmt / 10000.0;
        AccountFund.Commission = pCashAsset->totalFeeAmt / 10000.0;
        AccountFund.PreBalance = pCashAsset->beginningBal / 10000.0;
        AccountFund.CurrMargin = 0;
        AccountFund.CloseProfit = 0;
        AccountFund.PositionProfit = 0;
        AccountFund.Available = pCashAsset->creditExt.marginAvailableBal / 10000.0;
        AccountFund.WithdrawQuota = pCashAsset->creditExt.drawableBal / 10000.0;
        AccountFund.ExchangeMargin = 0;
        AccountFund.Balance = pCashAsset->creditExt.totalAssetValue / 10000.0;
    }
    strncpy(AccountFund.UpdateTime, Utils::getCurrentTimeUs(), sizeof(AccountFund.UpdateTime));

    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EAccountFund;
    memcpy(&message.AccountFund, &AccountFund, sizeof(AccountFund));
    while(!m_ReportMessageQueue.Push(message));
    FMTLOG(fmtlog::INF, "OESTrader::OnQueryCashAsset Account:{} FundAccount:{} beginningBal:{} beginningAvailableBal:{} beginningDrawableBal:{} "
                        "totalDepositAmt:{} totalWithdrawAmt:{} totalFeeAmt:{} currentTotalBal:{} currentAvailableBal:{} cashAcctId:{} totalAssetValue:{} "
                        "marginAvailableBal:{} availableBal:{} drawableBal:{} ", 
            pCashAsset->custId, pCashAsset->cashAcctId, pCashAsset->beginningBal, pCashAsset->beginningAvailableBal, pCashAsset->beginningDrawableBal,
            pCashAsset->totalDepositAmt, pCashAsset->totalWithdrawAmt, pCashAsset->totalFeeAmt, pCashAsset->currentTotalBal, pCashAsset->currentAvailableBal,
            pCashAsset->creditExt.cashAcctId, pCashAsset->creditExt.totalAssetValue, pCashAsset->creditExt.marginAvailableBal, 
            pCashAsset->creditExt.availableBal, pCashAsset->creditExt.drawableBal);

}

void OESTradeGateWay::OnQueryStkHolding(const OesStkHoldingItemT *pStkHolding, const OesQryCursorT *pCursor, int32 requestId) 
{
    std::string Account = m_XTraderConfig.Account;
    std::string Exchange = ExchangeID(pStkHolding->mktId);
    std::string Ticker = Exchange + std::string(pStkHolding->securityId);
    std::string Key = Account + ":" + Ticker;
    auto it = m_TickerAccountPositionMap.find(Key);
    if(m_TickerAccountPositionMap.end() == it)
    {
        Message::TAccountPosition AccountPosition;
        memset(&AccountPosition, 0, sizeof(AccountPosition));
        AccountPosition.BusinessType = m_XTraderConfig.BusinessType;
        strncpy(AccountPosition.Product,  m_XTraderConfig.Product.c_str(), sizeof(AccountPosition.Product));
        strncpy(AccountPosition.Broker,  m_XTraderConfig.Broker.c_str(), sizeof(AccountPosition.Broker));
        strncpy(AccountPosition.Account, m_XTraderConfig.Account.c_str(), sizeof(AccountPosition.Account));
        strncpy(AccountPosition.Ticker, Ticker.c_str(), sizeof(AccountPosition.Ticker));
        strncpy(AccountPosition.ExchangeID, Exchange.c_str(), sizeof(AccountPosition.ExchangeID));
        m_TickerAccountPositionMap[Key] = AccountPosition;
    }
    Message::TAccountPosition& AccountPosition = m_TickerAccountPositionMap[Key];
    if(m_XTraderConfig.BusinessType == Message::EBusinessType::ESTOCK && 0 == pStkHolding->isCreditHolding)
    {
        // 昨仓可用持仓 
        AccountPosition.StockPosition.LongYdPosition = pStkHolding->originalAvlHld; 
        AccountPosition.StockPosition.LongPosition = pStkHolding->sumHld; // 当前持仓
        AccountPosition.StockPosition.LongTdBuy = pStkHolding->totalBuyHld; // 今日买入量
        AccountPosition.StockPosition.LongTdSell = pStkHolding->totalSellHld; // 今日卖出量
    }
    else if(m_XTraderConfig.BusinessType == Message::EBusinessType::ECREDIT && 1 == pStkHolding->isCreditHolding)
    {
        // 昨仓可用持仓
        AccountPosition.StockPosition.LongYdPosition = pStkHolding->originalAvlHld; 
        AccountPosition.StockPosition.LongPosition = pStkHolding->sumHld; // 当前持仓
        AccountPosition.StockPosition.LongTdBuy = pStkHolding->totalBuyHld; // 今日买入量
        AccountPosition.StockPosition.LongTdSell = pStkHolding->totalSellHld; // 今日卖出量

        //  日初融券负债数量 = 日初融券负债数量 (日初融券余量, 不包括日初已还) - 当日已归还融券数量
        AccountPosition.StockPosition.ShortYdPosition = pStkHolding->creditExt.shortSellOriginDebtQty - pStkHolding->creditExt.shortSellRepaidQty; // 
        AccountPosition.StockPosition.ShortPosition = pStkHolding->creditExt.shortSellDebtQty; 
        AccountPosition.StockPosition.ShortTdSell = pStkHolding->creditExt.shortSellDebtQty + pStkHolding->creditExt.shortSellRepaidQty - AccountPosition.StockPosition.ShortYdPosition ; 
        AccountPosition.StockPosition.ShortTdBuy = pStkHolding->creditExt.shortSellRepaidQty; 
        AccountPosition.StockPosition.ShortDirectRepaid = 0; 
        AccountPosition.StockPosition.SpecialPositionAvl = pStkHolding->creditExt.specialSecurityPositionAvailableQty; // 专项证券头寸可用数量
    }
    strncpy(AccountPosition.UpdateTime, Utils::getCurrentTimeUs(), sizeof(AccountPosition.UpdateTime));

    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EAccountPosition;
    memcpy(&message.AccountPosition, &AccountPosition, sizeof(message.AccountPosition));
    while(!m_ReportMessageQueue.Push(message));
    FMTLOG(fmtlog::INF, "OESTrader::OnQueryStkHolding Account:{} invAcctId:{} securityId:{} mktId:{} securityType:{} subSecurityType:{} isCreditHolding:{} "
                        "originalHld:{} originalAvlHld:{} sumHld:{} totalBuyHld:{} totalSellHld:{} sellAvlHld:{} sellAvlHld:{} specialSecurityPositionAvailableQty:{} "
                        "collateralHoldingQty:{} marginBuyOriginDebtQty:{} marginBuyDebtQty:{} marginBuyRepaidQty:{} shortSellOriginDebtQty:{} shortSellDebtQty:{} "
                        "shortSellRepaidQty:{} repayStockDirectAvlHld:{} collateralTotalRepayDirectQty:{}", 
            m_XTraderConfig.Account, pStkHolding->invAcctId, pStkHolding->securityId, pStkHolding->mktId, pStkHolding->securityType, 
            pStkHolding->subSecurityType, pStkHolding->isCreditHolding, pStkHolding->originalHld, pStkHolding->originalAvlHld, 
            pStkHolding->sumHld, pStkHolding->totalBuyHld, pStkHolding->totalSellHld, pStkHolding->sellAvlHld, pStkHolding->creditExt.sellAvlHld, 
            pStkHolding->creditExt.specialSecurityPositionAvailableQty, pStkHolding->creditExt.collateralHoldingQty, 
            pStkHolding->creditExt.marginBuyOriginDebtQty, pStkHolding->creditExt.marginBuyDebtQty, pStkHolding->creditExt.marginBuyRepaidQty, 
            pStkHolding->creditExt.shortSellOriginDebtQty, pStkHolding->creditExt.shortSellDebtQty, pStkHolding->creditExt.shortSellRepaidQty, 
            pStkHolding->creditExt.repayStockDirectAvlHld, pStkHolding->creditExt.collateralTotalRepayDirectQty);
}

void OESTradeGateWay::OnQueryOrder(const OesOrdItemT *pOrder, const OesQryCursorT *pCursor, int32 requestId) 
{
    std::string OrderRef = fmt::format("{:012}", pOrder->clSeqNo);
    auto it = m_OrderStatusMap.find(OrderRef);
    if(it == m_OrderStatusMap.end())
    {
        if(pOrder->ordStatus < eOesOrdStatusT::__OES_ORD_STATUS_FINAL_MIN)
        {
            Message::TOrderStatus& OrderStatus = m_OrderStatusMap[OrderRef];
            OrderStatus.BusinessType = m_XTraderConfig.BusinessType;
            // Update OrderStatus
            strncpy(OrderStatus.Product, m_XTraderConfig.Product.c_str(), sizeof(OrderStatus.Product));
            strncpy(OrderStatus.Broker, m_XTraderConfig.Broker.c_str(), sizeof(OrderStatus.Broker));
            strncpy(OrderStatus.Account, m_XTraderConfig.Account.c_str(), sizeof(OrderStatus.Account));
            strncpy(OrderStatus.ExchangeID, ExchangeID(pOrder->mktId).c_str(), sizeof(OrderStatus.ExchangeID));
            std::string Ticker = std::string(pOrder->securityId) + "." + ExchangeID(pOrder->mktId);
            strncpy(OrderStatus.Ticker, Ticker.c_str(), sizeof(OrderStatus.Ticker));
            strncpy(OrderStatus.OrderRef, OrderRef.c_str(), sizeof(OrderStatus.OrderRef));
            strncpy(OrderStatus.OrderSysID, pOrder->exchOrdId, sizeof(OrderStatus.OrderSysID));
            fmt::format_to_n(OrderStatus.OrderLocalID, sizeof(OrderStatus.OrderLocalID), "{}", pOrder->clOrdId);
            OrderStatus.SendPrice = pOrder->ordPrice/10000.0;
            OrderStatus.SendVolume = pOrder->ordQty;
            OrderStatus.OrderType = OrderType(pOrder->ordType);
            OrderStatus.OrderSide = OrderSide(pOrder->bsType);
            int32 ordDate = pOrder->ordDate;
            int32 ordTime = pOrder->ordTime; // 141206000
            int32 ordCnfmTime = pOrder->ordCnfmTime;
            fmt::format_to_n(OrderStatus.InsertTime, sizeof(OrderStatus.InsertTime), 
                            "{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}000",
                            ordDate/10000, (ordDate/100)%100, ordDate%100, 
                            ordTime/10000000, (ordTime/100000)%100, (ordTime/1000)%100, ordTime%1000);
            fmt::format_to_n(OrderStatus.BrokerACKTime, sizeof(OrderStatus.BrokerACKTime), 
                            "{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}000", 
                            ordDate/10000, (ordDate/100)%100, ordDate%100, 
                            ordCnfmTime/10000000, (ordCnfmTime/100000)%100, (ordCnfmTime/1000)%100, ordCnfmTime%1000);
            strncpy(OrderStatus.ExchangeACKTime, OrderStatus.BrokerACKTime, sizeof(OrderStatus.ExchangeACKTime));
            if(eOesOrdStatusT::OES_ORD_STATUS_DECLARED == pOrder->ordStatus)
            {
                OrderStatus.OrderStatus = Message::EOrderStatusType::EEXCHANGE_ACK;
            }
            else if(eOesOrdStatusT::OES_ORD_STATUS_PARTIALLY_FILLED == pOrder->ordStatus)
            {
                OrderStatus.OrderStatus = Message::EOrderStatusType::EPARTTRADED;
                OrderStatus.TradedVolume =  pOrder->cumQty;
                OrderStatus.TotalTradedVolume =  pOrder->cumQty;
                OrderStatus.TradedPrice = pOrder->ordPrice / 10000.0;
                OrderStatus.TradedAvgPrice = pOrder->ordPrice / 10000.0;
            }
            UpdateOrderStatus(OrderStatus);
            PrintOrderStatus(OrderStatus, "OESTrader::OnQueryOrder ");
        }
    }
    FMTLOG(fmtlog::INF, "OESTrader::OnQueryOrder Account:{} invAcctId:{} securityId:{} clSeqNo:{} clOrdId:{} exchOrdId:{} mktId:{} ordType:{} bsType:{} "
                        "ordQty:{} cumQty:{} canceledQty:{} ordPrice:{} ordStatus:{} origClOrdId:{} ordRejReason:{} exchErrCode:{} securityType:{} "
                        "subSecurityType:{}", 
            m_XTraderConfig.Account, pOrder->invAcctId, pOrder->securityId, pOrder->clSeqNo, pOrder->clOrdId, pOrder->exchOrdId, pOrder->mktId, pOrder->ordType, 
            pOrder->bsType, pOrder->ordQty,  pOrder->cumQty,  pOrder->canceledQty, pOrder->ordPrice, pOrder->ordStatus, 
            pOrder->origClOrdId, pOrder->ordRejReason, pOrder->exchErrCode, pOrder->securityType, pOrder->subSecurityType);
    // 查询结果推送完成时撤销挂单
    if(pCursor->isEnd)
    {
        if(m_XTraderConfig.CancelAll)
        {
            for(auto it = m_OrderStatusMap.begin(); it != m_OrderStatusMap.end(); it++)
            {
                PrintOrderStatus(it->second, "OESTrader::OnQueryOrder Hangling Order ");
                CancelOrder(it->second);
            }
        }
    }
}

void OESTradeGateWay::OnQueryTrade(const OesTrdItemT *pTrade, const OesQryCursorT *pCursor, int32 requestId) 
{
   FMTLOG(fmtlog::INF, "OESTrader::OnQueryTrade Account:{} invAcctId:{} securityId:{} mktId:{} trdSide:{} ordStatus:{} ordType:{} ordBuySellType:{} "
                       "trdQty:{} trdPrice:{} trdAmt:{} trdFee:{}", 
            m_XTraderConfig.Account, pTrade->invAcctId, pTrade->securityId, pTrade->mktId, pTrade->trdSide, pTrade->ordStatus, 
            pTrade->ordType, pTrade->ordBuySellType, pTrade->trdQty, pTrade->trdPrice, pTrade->trdAmt, pTrade->trdFee);
}

void OESTradeGateWay::OnQueryIssue(const OesIssueItemT *pIssue, const OesQryCursorT *pCursor, int32 requestId)
{
    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EEventLog;
    message.EventLog.Level = Message::EEventLogLevel::EINFO;
    
    char buffer[512] = {0};
    fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event), 
                    "OESTrader::OnQueryIssue Account:{} Ticker:{}.{} IssuePrice:{:.2f} IssueQty:{} UnderlyingTicker:{} SecurityName:{}",
                    m_XTraderConfig.Account, pIssue->securityId, ExchangeID(pIssue->mktId),
                    pIssue->issuePrice / 10000.0, pIssue->issueQty, pIssue->underlyingSecurityId, pIssue->securityName);
    strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
    strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
    strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
    strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
    strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
    while(!m_ReportMessageQueue.Push(message));
    FMTLOG(fmtlog::INF, "OESTrader::OnQueryIssue Account:{} securityId:{} mktId:{} issueType:{} isCancelAble:{} issuePrice:{} issueQty:{} "
                        "underlyingSecurityId:{} securityName:{} securityType:{} subSecurityType:{}", 
            m_XTraderConfig.Account.c_str(), pIssue->securityId, pIssue->mktId, pIssue->issueType, pIssue->isCancelAble, 
            pIssue->issuePrice / 10000.0, pIssue->issueQty, pIssue->underlyingSecurityId, pIssue->securityName,
            pIssue->securityType, pIssue->subSecurityType);
}

void OESTradeGateWay::OnQueryLotWinning(const OesLotWinningItemT *pLotWinning, const OesQryCursorT *pCursor, int32 requestId) 
{
    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EEventLog;
    message.EventLog.Level = Message::EEventLogLevel::EINFO;
    fmt::format_to_n(message.EventLog.Event, sizeof(message.EventLog.Event),
                    "OESTrader::OnQueryLotWinning Account:{} Ticker:{}.{} LotPrice:{:.2f} LotQty:{} SecurityName:{}",
                    m_XTraderConfig.Account, pLotWinning->securityId, ExchangeID(pLotWinning->mktId),
                    pLotWinning->lotPrice / 10000.0, pLotWinning->lotQty, pLotWinning->securityName);
    strncpy(message.EventLog.Product, m_XTraderConfig.Product.c_str(), sizeof(message.EventLog.Product));
    strncpy(message.EventLog.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.EventLog.Broker));
    strncpy(message.EventLog.App, APP_NAME, sizeof(message.EventLog.App));
    strncpy(message.EventLog.Account, m_XTraderConfig.Account.c_str(), sizeof(message.EventLog.Account));
    strncpy(message.EventLog.UpdateTime, Utils::getCurrentTimeUs(), sizeof(message.EventLog.UpdateTime));
    while(!m_ReportMessageQueue.Push(message));
    FMTLOG(fmtlog::INF, "OESTrader::OnQueryLotWinning invAcctId:{} securityId:{} mktId:{} lotType:{} rejReason:{} lotDate:{} assignNum:{} "
                        "lotQty:{} lotPrice:{} lotAmt:{}", 
            pLotWinning->invAcctId, pLotWinning->securityId, pLotWinning->mktId, pLotWinning->lotType, pLotWinning->rejReason, 
            pLotWinning->lotDate, pLotWinning->assignNum, pLotWinning->lotQty, pLotWinning->lotPrice, pLotWinning->lotAmt);
}

void OESTradeGateWay::OnQueryInvAcct(const OesInvAcctItemT *pInvAcct, const OesQryCursorT *pCursor, int32 requestId) 
{
    FMTLOG(fmtlog::INF, "OESTrader::OnQueryInvAcct Account:{} invAcctId:{} mktId:{} acctType:{} custId:{} status:{} pbuId:{} "
                        "subscriptionQuota:{} kcSubscriptionQuota:{}", 
            m_XTraderConfig.Account, pInvAcct->custId, pInvAcct->invAcctId, pInvAcct->mktId, pInvAcct->acctType, pInvAcct->custId, pInvAcct->status,
            pInvAcct->pbuId, pInvAcct->subscriptionQuota, pInvAcct->kcSubscriptionQuota);
}

void OESTradeGateWay::OnQueryCrdCreditAsset(const OesCrdCreditAssetItemT *pCreditAsset, const OesQryCursorT *pCursor, int32 requestId)
{
    FMTLOG(fmtlog::INF, "OESTrader::OnQueryCrdCreditAsset Account:{} cashAcctId:{} custId:{} totalAssetValue:{} totalDebtValue:{} maintenaceRatio:{} "
                        "marginAvailableBal:{} cashBalance:{} availableBal:{} drawableBal:{} marginBuyMaxQuota:{} shortSellMaxQuota:{} "
                        "specialCashPositionAmt:{} specialCashPositionAvailableBal:{} publicCashPositionAmt:{} publicCashPositionAvailableBal:{}", 
            m_XTraderConfig.Account, pCreditAsset->cashAcctId, pCreditAsset->custId, pCreditAsset->totalAssetValue, 
            pCreditAsset->totalDebtValue, pCreditAsset->maintenaceRatio, pCreditAsset->marginAvailableBal,
            pCreditAsset->cashBalance, pCreditAsset->availableBal, pCreditAsset->drawableBal, 
            pCreditAsset->marginBuyMaxQuota, pCreditAsset->shortSellMaxQuota, pCreditAsset->specialCashPositionAmt,
            pCreditAsset->specialCashPositionAvailableBal, pCreditAsset->publicCashPositionAmt, pCreditAsset->publicCashPositionAvailableBal);
}

void OESTradeGateWay::OnQueryCrdCashPosition(const OesCrdCashPositionItemT *pCashPosition, const OesQryCursorT *pCursor, int32 requestId)
{
    FMTLOG(fmtlog::INF, "OESTrader::OnQueryCrdCashPosition Account:{} custId:{} cashAcctId:{} cashGroupNo:{} cashGroupProperty:{} positionAmt:{} "
                        "repaidPositionAmt:{} usedPositionAmt:{} originalBalance:{} originalAvailable:{} originalUsed:{}", 
            m_XTraderConfig.Account, pCashPosition->custId, pCashPosition->cashAcctId, pCashPosition->cashGroupNo, pCashPosition->cashGroupProperty, 
            pCashPosition->positionAmt, pCashPosition->repaidPositionAmt, pCashPosition->usedPositionAmt, pCashPosition->originalBalance, 
            pCashPosition->originalAvailable, pCashPosition->originalUsed);
}

void OESTradeGateWay::OnQueryCrdSecurityPosition(const OesCrdSecurityPositionItemT *pSecurityPosition, const OesQryCursorT *pCursor, int32 requestId)
{
    FMTLOG(fmtlog::INF, "OESTrader::OnQueryCrdSecurityPosition Account:{} custId:{} invAcctId:{} securityId:{} mktId:{} cashGroupProperty:{} "
                        "cashGroupNo:{} positionQty:{} repaidPositionQty:{} usedPositionQty:{} originalBalanceQty:{} originalAvailableQty:{} "
                        "originalUsedQty:{} availablePositionQty:{}", 
            m_XTraderConfig.Account, pSecurityPosition->custId, pSecurityPosition->invAcctId, pSecurityPosition->securityId, 
            pSecurityPosition->mktId, pSecurityPosition->cashGroupProperty, pSecurityPosition->cashGroupNo, pSecurityPosition->positionQty,
            pSecurityPosition->repaidPositionQty, pSecurityPosition->usedPositionQty, pSecurityPosition->originalBalanceQty,
            pSecurityPosition->originalAvailableQty, pSecurityPosition->originalUsedQty, pSecurityPosition->availablePositionQty);
}