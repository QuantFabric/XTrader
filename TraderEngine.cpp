#include "TraderEngine.h"


TraderEngine::TraderEngine() : m_RequestMessageQueue(1 << 12), m_ReportMessageQueue(1 << 12)
{
    m_HPPackClient = NULL;
    m_RiskClient = NULL;
    m_pOrderServer = NULL;
    m_TradeGateWay = NULL;
}

void TraderEngine::LoadConfig(const std::string& path)
{
    std::string errorString;
    if(!Utils::LoadXTraderConfig(path.c_str(), m_XTraderConfig, errorString))
    {
        FMTLOG(fmtlog::WRN, "TraderEngine::LoadXTraderConfig {} failed, {}",  path, errorString);
    }
    else
    {
        FMTLOG(fmtlog::INF, "TraderEngine::LoadXTraderConfig {} successed", path);
        FMTLOG(fmtlog::INF, "LoadXTraderConfig Product:{} BrokerID:{} Account:{} AppID:{} AuthCode:{}", 
                m_XTraderConfig.Product, m_XTraderConfig.BrokerID, m_XTraderConfig.Account, m_XTraderConfig.AppID, m_XTraderConfig.AuthCode);
        FMTLOG(fmtlog::INF, "LoadXTraderConfig ServerIP:{} Port:{} OpenTime:{} CloseTime:{}", 
                m_XTraderConfig.ServerIP, m_XTraderConfig.Port, m_XTraderConfig.OpenTime, m_XTraderConfig.CloseTime);
        FMTLOG(fmtlog::INF, "LoadXTraderConfig TickerListPath:{} ErrorPath:{}",  m_XTraderConfig.TickerListPath, m_XTraderConfig.ErrorPath);    
        FMTLOG(fmtlog::INF, "LoadXTraderConfig RiskServerName:{} OrderServerName:{} TraderAPI:{}",
                m_XTraderConfig.RiskServerName, m_XTraderConfig.OrderServerName, m_XTraderConfig.TraderAPI);
    }
    m_OpenTime = Utils::getTimeStampMs(m_XTraderConfig.OpenTime.c_str());
    m_CloseTime = Utils::getTimeStampMs(m_XTraderConfig.CloseTime.c_str());
}

void TraderEngine::SetCommand(const std::string& cmd)
{
    m_Command = cmd;
    FMTLOG(fmtlog::INF, "TraderEngine::SetCommand cmd:{}", m_Command);
}

void TraderEngine::LoadTradeGateWay(const std::string& soPath)
{
    std::string errorString;
    m_TradeGateWay = XPluginEngine<TradeGateWay>::LoadPlugin(soPath, errorString);
    if(NULL == m_TradeGateWay)
    {
        FMTLOG(fmtlog::ERR, "TraderEngine::LoadTradeGateWay {} failed, {}", soPath, errorString);
        sleep(1);
        exit(-1);
    }
    else
    {
        FMTLOG(fmtlog::INF, "TraderEngine::LoadTradeGateWay {} successed", soPath);
        m_TradeGateWay->SetTraderConfig(m_XTraderConfig);
    }
}

void TraderEngine::Run()
{
    FMTLOG(fmtlog::INF, "TraderEngine::Run RiskServer:{} OrderServer:{}", m_XTraderConfig.RiskServerName, m_XTraderConfig.OrderServerName);
    // Connect to XWatcher
    RegisterClient(m_XTraderConfig.ServerIP.c_str(), m_XTraderConfig.Port);
    // Connect to RiskServer
    FMTLOG(fmtlog::INF, "TraderEngine::Run connect to RiskServer:{}", m_XTraderConfig.RiskServerName);
    m_RiskClient = new SHMIPC::SHMConnection<Message::PackMessage, ClientConf>(m_XTraderConfig.Account);
    m_RiskClient->Start(m_XTraderConfig.RiskServerName);
    // Start OrderServer
    FMTLOG(fmtlog::INF, "TraderEngine::Run Start OrderServer:{}", m_XTraderConfig.OrderServerName + m_XTraderConfig.Account);
    m_pOrderServer = new OrderServer();
    m_pOrderServer->Start(m_XTraderConfig.OrderServerName + m_XTraderConfig.Account);
    sleep(1);
    // Update App Status
    InitAppStatus();
    // 创建交易API实例
    m_TradeGateWay->CreateTraderAPI();
    // 登录交易网关
    m_TradeGateWay->LoadTrader();
    // 查询Order、Trade、Fund、Position信息
    m_TradeGateWay->Qry();

    usleep(1000000);
    m_pWorkThread = new std::thread(&TraderEngine::WorkFunc, this);

    m_RiskClient->Join();
    m_pOrderServer->Join();
    m_pWorkThread->join();
}

void TraderEngine::WorkFunc()
{
    FMTLOG(fmtlog::INF, "TraderEngine::WorkFunc WorkThread start");
    // 风控初始化检查
    InitRiskCheck();
    while(true)
    {
        // 从OrderServer内存通道读取报单、撤单请求并写入请求队列
        HandleOrderFromQuant();
        // 从客户端读取报单、撤单请求并写入请求队列
        ReadRequestFromClient();
        // 处理请求
        HandleRequestMessage();
        // 处理风控检查结果
        HandleRiskResponse();
        // 处理回报
        HandleExecuteReport();
        // 写入回报到QuantServer内存通道
        SendReportToQuant();

        m_RiskClient->HandleMsg();
        unsigned long CurrentTimeStamp = Utils::getTimeMs();
        static unsigned long PreTimeStamp = CurrentTimeStamp / 1000;
        if(PreTimeStamp < CurrentTimeStamp / 1000)
        {
            PreTimeStamp = CurrentTimeStamp / 1000;
        }
        if(PreTimeStamp % 5 == 0)
        {
            m_HPPackClient->ReConnect();
            if(m_XTraderConfig.QryFund)
            {
                m_TradeGateWay->ReqQryFund();
            }
            PreTimeStamp += 1;
        }
    }
}


void TraderEngine::RegisterClient(const char *ip, unsigned int port)
{
    m_HPPackClient = new HPPackClient(ip, port);
    m_HPPackClient->Start();
    Message::TLoginRequest login;
    login.ClientType = Message::EClientType::EXTRADER;
    strncpy(login.Account, m_XTraderConfig.Account.c_str(), sizeof(login.Account));
    m_HPPackClient->Login(login);
}


void TraderEngine::HandleOrderFromQuant()
{
    static Message::PackMessage message;
    bool ok = m_pOrderServer->Pop(message);
    if(ok)
    {
        FMTLOG(fmtlog::INF, "TraderEngine::HandleOrderFromQuant recv msg from ChannelID:{}", message.ChannelID);
        FMTLOG(fmtlog::DBG, "TraderEngine::HandleOrderFromQuant Account:{} Ticker:{} RiskStatus:{} OrderToken:{} RiskID:{} Price:{} Volume:{} ErrorMsg:{} {:#X}", 
                message.OrderRequest.Account, message.OrderRequest.Ticker, message.OrderRequest.RiskStatus, 
                message.OrderRequest.OrderToken, message.OrderRequest.RiskID, message.OrderRequest.Price, 
                message.OrderRequest.Volume, message.OrderRequest.ErrorMsg, message.MessageType);
        while(!m_RequestMessageQueue.Push(message));
    }
}

void TraderEngine::ReadRequestFromClient()
{
    while(true)
    {
        Message::PackMessage message;
        bool ok = m_HPPackClient->m_PackMessageQueue.Pop(message);
        if(ok)
        {
            switch(message.MessageType)
            {
                case Message::EMessageType::EOrderRequest:
                {
                    while(!m_RequestMessageQueue.Push(message));
                    break;
                }
                case Message::EMessageType::EActionRequest:
                {
                    while(!m_RequestMessageQueue.Push(message));
                    break;
                }
                case Message::EMessageType::ECommand:
                    HandleCommand(message);
                    break;
                default:
                {
                    FMTLOG(fmtlog::WRN, "TraderEngine::ReadRequestFromClient Unkown Message Type:{:#X}", message.MessageType);
                    break;
                }
            }
        }
        else
        {
            break;
        }
    }
}

void TraderEngine::HandleRequestMessage()
{
    // 处理报单、撤单请求，发送至风控系统或直接通过API报单、撤单
    while(true)
    {
        Message::PackMessage request;
        bool ok = m_RequestMessageQueue.Pop(request);
        if(ok)
        {
            switch(request.MessageType)
            {
                case Message::EMessageType::EOrderRequest:
                {
                    request.OrderRequest.BusinessType = m_XTraderConfig.BusinessType;
                    if(request.OrderRequest.RiskStatus == Message::ERiskStatusType::ENOCHECKED)
                    {
                        SendRequest(request);
                    }
                    else if(request.OrderRequest.RiskStatus == Message::ERiskStatusType::EPREPARE_CHECKED)
                    {
                        SendRiskCheckReqeust(request);
                    }
                    else if(request.OrderRequest.RiskStatus == Message::ERiskStatusType::ECHECK_INIT)
                    {
                        SendRiskCheckReqeust(request);
                    }
                    break;
                }
                case Message::EMessageType::EActionRequest:
                {
                    if(request.ActionRequest.RiskStatus == Message::ERiskStatusType::ENOCHECKED)
                    {
                        SendRequest(request);
                    }
                    else if(request.ActionRequest.RiskStatus == Message::ERiskStatusType::EPREPARE_CHECKED)
                    {
                        SendRiskCheckReqeust(request);
                    }
                    break;
                }
                default:
                {
                    FMTLOG(fmtlog::WRN, "TraderEngine::HandleRequestMessage Unkown Message Type:{:#X}", request.MessageType);
                    break;
                }
            }
        }
        else
        {
            break;
        }
    }
}

void TraderEngine::HandleRiskResponse()
{
    // 处理风控检查结果
    while(true)
    {
        m_RiskClient->HandleMsg();
        Message::PackMessage message;
        bool ok = m_RiskClient->Pop(message);
        if(ok)
        {
            FMTLOG(fmtlog::INF, "TraderEngine::HandleRiskResponse recv msg from ChannelID:{}", message.ChannelID);
            switch(message.MessageType)
            {
                case Message::EMessageType::EOrderRequest:
                {
                    SendRequest(message);
                    break;
                }
                case Message::EMessageType::EActionRequest:
                {
                    SendRequest(message);
                    break;
                }
                case Message::EMessageType::ERiskReport:
                {
                    SendMonitorMessage(message);
                    break;
                }
                default:
                {
                    FMTLOG(fmtlog::WRN, "TraderEngine::HandleRiskResponse Unkown Message Type:{:#X}", message.MessageType);
                    break;
                }
            }
        }
        else
        {
            break;
        }
    }
}

void TraderEngine::HandleExecuteReport()
{
    // OrderStatus,AccountFund,AccountPosition写入内存队列，所有监控消息回传
    while(true)
    {
        Message::PackMessage report;
        // 从交易网关回报队列取出消息
        bool ok = m_TradeGateWay->m_ReportMessageQueue.Pop(report);
        if(ok)
        {
            switch(report.MessageType)
            {
                case Message::EMessageType::EOrderStatus:
                case Message::EMessageType::EAccountFund:
                case Message::EMessageType::EAccountPosition:
                {
                    m_ReportMessageQueue.Push(report);
                    SendMonitorMessage(report);
                    break;
                }
                case Message::EMessageType::EEventLog:
                {
                    SendMonitorMessage(report);
                    break;
                }
                default:
                {
                    FMTLOG(fmtlog::WRN, "TraderEngine::HandleExcuteReport Unkown Message Type:{:#X}", report.MessageType);
                    break;
                }
            }
        }
        else
        {
            break;
        }
    }
}

void TraderEngine::HandleCommand(const Message::PackMessage& msg)
{
    if(Message::EBusinessType::ESTOCK ==  m_XTraderConfig.BusinessType)
    {
        if(Message::ECommandType::ETRANSFER_FUND_IN == msg.Command.CmdType)
        {
            std::string Command = msg.Command.Command;
            std::vector<std::string> vec;
            Utils::Split(Command, ":", vec);
            if(vec.size() >= 2)
            {
                double Amount = atof(vec.at(1).c_str());
                m_TradeGateWay->TransferFundIn(Amount);
            }
            else
            {
                FMTLOG(fmtlog::WRN, "TraderEngine::HandleCommand Account:{} invalid TransferFundIn Command:{}", m_XTraderConfig.Account, msg.Command.Command);
            }
        }
        else if(Message::ECommandType::ETRANSFER_FUND_OUT == msg.Command.CmdType)
        {
            std::string Command = msg.Command.Command;
            std::vector<std::string> vec;
            Utils::Split(Command, ":", vec);
            if(vec.size() >= 2)
            {
                double Amount = atof(vec.at(1).c_str());
                m_TradeGateWay->TransferFundOut(Amount);
            }
            else
            {
                FMTLOG(fmtlog::WRN, "TraderEngine::HandleCommand Account:{} invalid TransferFundOut Command:{}", m_XTraderConfig.Account, msg.Command.Command);
            }
        }
        else 
        {
            FMTLOG(fmtlog::WRN, "TraderEngine::HandleCommand Account:{} invalid Command:{}", m_XTraderConfig.Account, msg.Command.Command);
        }
    }
    else if(Message::EBusinessType::ECREDIT ==  m_XTraderConfig.BusinessType)
    {
        if(Message::ECommandType::ETRANSFER_FUND_IN == msg.Command.CmdType)
        {
            std::string Command = msg.Command.Command;
            std::vector<std::string> vec;
            Utils::Split(Command, ":", vec);
            if(vec.size() >= 2)
            {
                double Amount = atof(vec.at(1).c_str());
                m_TradeGateWay->TransferFundIn(Amount);
            }
            else
            {
                FMTLOG(fmtlog::WRN, "TraderEngine::HandleCommand Account:{} invalid TransferFundIn Command:{}", m_XTraderConfig.Account, msg.Command.Command);
            }
        }
        else if(Message::ECommandType::ETRANSFER_FUND_OUT == msg.Command.CmdType)
        {
            std::string Command = msg.Command.Command;
            std::vector<std::string> vec;
            Utils::Split(Command, ":", vec);
            if(vec.size() >= 2)
            {
                double Amount = atof(vec.at(1).c_str());
                m_TradeGateWay->TransferFundOut(Amount);
            }
            else
            {
                FMTLOG(fmtlog::WRN, "TraderEngine::HandleCommand Account:{} invalid TransferFundOut Command:{}", m_XTraderConfig.Account, msg.Command.Command);
            }
        }
        else if(Message::ECommandType::EREPAY_MARGIN_DIRECT == msg.Command.CmdType)
        {
            std::string Command = msg.Command.Command;
            std::vector<std::string> vec;
            Utils::Split(Command, ":", vec);
            if(vec.size() >= 2)
            {
                double Amount = atof(vec.at(1).c_str());
                m_TradeGateWay->RepayMarginDirect(Amount);
            }
            else
            {
                FMTLOG(fmtlog::WRN, "TraderEngine::HandleCommand Account:{} invalid RepayMarginDirect Command:{}", m_XTraderConfig.Account, msg.Command.Command);
            }
        }
        else 
        {
            FMTLOG(fmtlog::WRN, "TraderEngine::HandleCommand Account:{} invalid Command:{}", m_XTraderConfig.Account, msg.Command.Command);
        }
    }
    else 
    {
        FMTLOG(fmtlog::WRN, "TraderEngine::HandleCommand Account:{} invalid Command:{}", m_XTraderConfig.Account, msg.Command.Command);
    }
}

void TraderEngine::SendReportToQuant()
{
    while(true)
    {
        Message::PackMessage report;
        bool ok = m_ReportMessageQueue.Pop(report);
        if(ok)
        {
            m_pOrderServer->Push(report);
        }
        else
        {
            break;
        }
    }
}

void TraderEngine::SendRequest(const Message::PackMessage& request)
{
    m_TradeGateWay->SendRequest(request);
}

void TraderEngine::SendRiskCheckReqeust(const Message::PackMessage& request)
{
    m_RiskClient->Push(request);
    m_RiskClient->HandleMsg();
}

void TraderEngine::SendMonitorMessage(const Message::PackMessage& message)
{
    switch(message.MessageType)
    {
        case Message::EMessageType::EOrderStatus:
        {
            m_HPPackClient->SendData((const unsigned char*)&message, sizeof(message));
            m_RiskClient->Push(message);
            m_RiskClient->HandleMsg();
            break;
        }
        case Message::EMessageType::EEventLog:
        {
            m_HPPackClient->SendData((const unsigned char*)&message, sizeof(message));
            break;
        }
        case Message::EMessageType::ERiskReport:
        {
            m_HPPackClient->SendData((const unsigned char*)&message, sizeof(message));
            break;
        }
        case Message::EMessageType::EAccountFund:
        {
            m_HPPackClient->SendData((const unsigned char*)&message, sizeof(message));
            break;
        }
        case Message::EMessageType::EAccountPosition:
        {
            m_HPPackClient->SendData((const unsigned char*)&message, sizeof(message));
            break;
        }
        default:
        {
            FMTLOG(fmtlog::WRN, "TraderEngine::SendMonitorMessage Unkown Message Type:{:#X}", message.MessageType);
            break;
        }
    }
}

void TraderEngine::InitRiskCheck()
{
    Message::PackMessage message;
    memset(&message, 0, sizeof(message));
    message.MessageType = Message::EMessageType::EOrderRequest;
    message.OrderRequest.Price = 0.0;
    message.OrderRequest.Volume = 0;
    message.OrderRequest.RiskStatus = Message::ERiskStatusType::ECHECK_INIT;
    strncpy(message.OrderRequest.Broker, m_XTraderConfig.Broker.c_str(), sizeof(message.OrderRequest.Broker));
    strncpy(message.OrderRequest.Account, m_XTraderConfig.Account.c_str(), sizeof(message.OrderRequest.Account));
    message.OrderRequest.OrderType = Message::EOrderType::ELIMIT;
    message.OrderRequest.Direction = Message::EOrderDirection::EBUY;
    message.OrderRequest.Offset = Message::EOrderOffset::EOPEN;
    m_RequestMessageQueue.Push(message);
}

bool TraderEngine::IsTrading()const
{
    return m_Trading;
}

void TraderEngine::CheckTrading()
{
    m_CurrentTimeStamp = Utils::getTimeStampMs(Utils::getCurrentTimeMs() + 11);
    m_Trading  = (m_CurrentTimeStamp >= m_OpenTime && m_CurrentTimeStamp <= m_CloseTime);
}

void TraderEngine::InitAppStatus()
{
    Message::PackMessage message;
    message.MessageType = Message::EMessageType::EAppStatus;
    UpdateAppStatus(m_Command, message.AppStatus);
    m_HPPackClient->SendData((const unsigned char*)&message, sizeof(message));
}

void TraderEngine::UpdateAppStatus(const std::string& cmd, Message::TAppStatus& AppStatus)
{
    FMTLOG(fmtlog::INF, "TraderEngine::UpdateAppStatus cmd:{}", cmd);
    std::vector<std::string> ItemVec;
    Utils::Split(cmd, " ", ItemVec);
    std::string Account;
    for(int i = 0; i < ItemVec.size(); i++)
    {
        if(Utils::equalWith(ItemVec.at(i), "-a"))
        {
            Account = ItemVec.at(i + 1);
            break;
        }
    }
    strncpy(AppStatus.Account, Account.c_str(), sizeof(AppStatus.Account));

    std::vector<std::string> Vec;
    Utils::Split(ItemVec.at(0), "/", Vec);
    FMTLOG(fmtlog::INF, "TraderEngine::UpdateAppStatus {}", Vec.size());
    std::string AppName = Vec.at(Vec.size() - 1);
    strncpy(AppStatus.AppName, AppName.c_str(), sizeof(AppStatus.AppName));
    AppStatus.PID = getpid();
    strncpy(AppStatus.Status, "Start", sizeof(AppStatus.Status));

    std::string AppLogPath;
    char* p = getenv("APP_LOG_PATH");
    if(p == NULL)
    {
        AppLogPath = "./log/";
    }
    else
    {
        AppLogPath = p;
    }
    fmt::format_to_n(AppStatus.StartScript, sizeof(AppStatus.StartScript), "nohup {} > {}/{}_{}_run.log 2>&1 &", cmd, AppLogPath, AppName, AppStatus.Account);
    std::string SoCommitID;
    std::string SoUtilsCommitID;
    m_TradeGateWay->GetCommitID(SoCommitID, SoUtilsCommitID);
    std::string CommitID = std::string(APP_COMMITID) + ":" + SoCommitID + ":" + SHMSERVER_COMMITID;
    strncpy(AppStatus.CommitID, CommitID.c_str(), sizeof(AppStatus.CommitID));
    std::string UtilsCommitID = std::string(UTILS_COMMITID) + ":" + SoUtilsCommitID;
    strncpy(AppStatus.UtilsCommitID, UtilsCommitID.c_str(), sizeof(AppStatus.UtilsCommitID));
    std::string APIVersion;
    m_TradeGateWay->GetAPIVersion(APIVersion);
    strncpy(AppStatus.APIVersion, APIVersion.c_str(), sizeof(AppStatus.APIVersion));
    strncpy(AppStatus.StartTime, Utils::getCurrentTimeUs(), sizeof(AppStatus.StartTime));
    strncpy(AppStatus.LastStartTime, Utils::getCurrentTimeUs(), sizeof(AppStatus.LastStartTime));
    strncpy(AppStatus.UpdateTime, Utils::getCurrentTimeUs(), sizeof(AppStatus.UpdateTime));
    FMTLOG(fmtlog::INF, "TraderEngine::UpdateAppStatus AppCommitID:{} AppUtilsCommitID:{} SoCommitID:{} SoUtilsCommitID:{} SHMServerCommitID:{} CMD:{}",
            APP_COMMITID, UTILS_COMMITID, SoCommitID, SoUtilsCommitID, SHMSERVER_COMMITID, AppStatus.StartScript);
}
