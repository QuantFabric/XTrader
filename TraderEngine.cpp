#include "TraderEngine.h"

extern Utils::Logger *gLogger;

TraderEngine::TraderEngine()
{
    m_HPPackClient = NULL;
    m_RiskClient = NULL;
    m_TradeGateWay = NULL;
}

void TraderEngine::LoadConfig(const std::string& path)
{
    std::string errorString;
    if(!Utils::LoadXTraderConfig(path.c_str(), m_XTraderConfig, errorString))
    {
        Utils::gLogger->Log->error("TraderEngine::LoadXTraderConfig {} failed, {}", path, errorString);
    }
    else
    {
        Utils::gLogger->Log->info("TraderEngine::LoadXTraderConfig {} successed", path);
        Utils::gLogger->Log->info("LoadXTraderConfig Product:{} BrokerID:{} Account:{} AppID:{} AuthCode:{}", 
                                m_XTraderConfig.Product, m_XTraderConfig.BrokerID, m_XTraderConfig.Account, m_XTraderConfig.AppID, m_XTraderConfig.AuthCode);
        Utils::gLogger->Log->info("LoadXTraderConfig ServerIP:{} Port:{} OpenTime:{} CloseTime:{}", 
                                    m_XTraderConfig.ServerIP, m_XTraderConfig.Port, m_XTraderConfig.OpenTime, m_XTraderConfig.CloseTime);
        Utils::gLogger->Log->info("LoadXTraderConfig TickerListPath:{} ErrorPath:{}",  m_XTraderConfig.TickerListPath, m_XTraderConfig.ErrorPath);
        Utils::gLogger->Log->info("LoadXTraderConfig RiskServerIP:{} RiskServerPort:{} TraderAPI:{}", m_XTraderConfig.RiskServerIP, m_XTraderConfig.RiskServerPort,m_XTraderConfig.TraderAPI);
        char buffer[256] = {0};
        sprintf(buffer, "LoadXTraderConfig TraderAPIConfig:%s OrderChannelKey:0X%X ReportChannelKey:0X%X",
                        m_XTraderConfig.TraderAPIConfig.c_str(), m_XTraderConfig.OrderChannelKey, m_XTraderConfig.ReportChannelKey);
        Utils::gLogger->Log->info(buffer);
    }
    m_OpenTime = Utils::getTimeStampMs(m_XTraderConfig.OpenTime.c_str());
    m_CloseTime = Utils::getTimeStampMs(m_XTraderConfig.CloseTime.c_str());
}

void TraderEngine::SetCommand(const std::string& cmd)
{
    m_Command = cmd;
    Utils::gLogger->Log->info("TraderEngine::SetCommand cmd:{}", m_Command);
}

void TraderEngine::LoadTradeGateWay(const std::string& soPath)
{
    std::string errorString;
    m_TradeGateWay = XPluginEngine<TradeGateWay>::LoadPlugin(soPath, errorString);
    if(NULL == m_TradeGateWay)
    {
        Utils::gLogger->Log->error("TraderEngine::LoadTradeGateWay {} failed, {}", soPath, errorString);
        sleep(1);
        exit(-1);
    }
    else
    {
        Utils::gLogger->Log->info("TraderEngine::LoadTradeGateWay {} successed", soPath);
        m_TradeGateWay->SetLogger(Utils::Singleton<Utils::Logger>::GetInstance());
        m_TradeGateWay->SetTraderConfig(m_XTraderConfig);
    }
}

void TraderEngine::Run()
{
    char buffer[256] = {0};
    sprintf(buffer, "OrderChnnel:0X%X ReportChannel:0X%X", m_XTraderConfig.OrderChannelKey, m_XTraderConfig.ReportChannelKey);
    Utils::gLogger->Log->info("TraderEngine::Run {}", buffer);

    // Connect to XWatcher
    RegisterClient(m_XTraderConfig.ServerIP.c_str(), m_XTraderConfig.Port);
    // Connect to Risk Server
    RegisterRiskClient(m_XTraderConfig.RiskServerIP.c_str(), m_XTraderConfig.RiskServerPort);

    sleep(1);
    // Update App Status
    InitAppStatus();
    // 创建交易API实例
    m_TradeGateWay->CreateTraderAPI();
    // 登录交易网关
    m_TradeGateWay->LoadTrader();
    // 查询Order、Trade、Fund、Position信息
    m_TradeGateWay->Qry();

    // start 
    while(true)
    {
        CheckTrading();
        if(m_Trading)
        {
            // Handle Message
        }
        static unsigned long currentTimeStamp = m_CurrentTimeStamp / 1000;
        if(currentTimeStamp < m_CurrentTimeStamp / 1000)
        {
            currentTimeStamp = m_CurrentTimeStamp / 1000;
        }
        if(currentTimeStamp % 5 == 0)
        {
            // ReConnect 
            m_HPPackClient->ReConnect();
            m_RiskClient->ReConnect();
            m_TradeGateWay->ReLoadTrader();
            if(m_XTraderConfig.QryFund)
            {
                m_TradeGateWay->ReqQryFund();
            }
            currentTimeStamp += 1;
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

void TraderEngine::RegisterRiskClient(const char *ip, unsigned int port)
{
    m_RiskClient = new HPPackRiskClient(ip, port);
    m_RiskClient->Start();
    Message::TLoginRequest login;
    login.ClientType = Message::EClientType::EXTRADER;
    strncpy(login.Account, m_XTraderConfig.Account.c_str(), sizeof(login.Account));
    m_RiskClient->Login(login);
}

bool TraderEngine::IsTrading()const
{
    return m_Trading;
}

void TraderEngine::CheckTrading()
{
    std::string buffer = Utils::getCurrentTimeMs() + 11;
    m_CurrentTimeStamp = Utils::getTimeStampMs(buffer.c_str());
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
    Utils::gLogger->Log->info("TraderEngine::UpdateAppStatus cmd:{}", cmd);
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
    Utils::gLogger->Log->info("TraderEngine::UpdateAppStatus {}", Vec.size());
    std::string AppName = Vec.at(Vec.size() - 1);
    strncpy(AppStatus.AppName, AppName.c_str(), sizeof(AppStatus.AppName));
    AppStatus.PID = getpid();
    strncpy(AppStatus.Status, "Start", sizeof(AppStatus.Status));

    char command[512] = {0};
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
    sprintf(command, "nohup %s > %s/%s_%s_run.log 2>&1 &", cmd.c_str(), AppLogPath.c_str(), 
	    AppName.c_str(), AppStatus.Account);
    strncpy(AppStatus.StartScript, command, sizeof(AppStatus.StartScript));
    std::string SoCommitID;
    std::string SoUtilsCommitID;
    m_TradeGateWay->GetCommitID(SoCommitID, SoUtilsCommitID);
    std::string CommitID = std::string(APP_COMMITID) + ":" + SoCommitID;
    strncpy(AppStatus.CommitID, CommitID.c_str(), sizeof(AppStatus.CommitID));
    std::string UtilsCommitID = std::string(UTILS_COMMITID) + ":" + SoUtilsCommitID;
    strncpy(AppStatus.UtilsCommitID, UtilsCommitID.c_str(), sizeof(AppStatus.UtilsCommitID));
    std::string APIVersion;
    m_TradeGateWay->GetAPIVersion(APIVersion);
    strncpy(AppStatus.APIVersion, APIVersion.c_str(), sizeof(AppStatus.APIVersion));
    strncpy(AppStatus.StartTime, Utils::getCurrentTimeUs(), sizeof(AppStatus.StartTime));
    strncpy(AppStatus.LastStartTime, Utils::getCurrentTimeUs(), sizeof(AppStatus.LastStartTime));
    strncpy(AppStatus.UpdateTime, Utils::getCurrentTimeUs(), sizeof(AppStatus.UpdateTime));

    Utils::gLogger->Log->info("TraderEngine::UpdateAppStatus AppCommitID:{} AppUtilsCommitID:{} SoCommitID:{} SoUtilsCommitID:{} CMD:{}",
                                APP_COMMITID, UTILS_COMMITID, SoCommitID, SoUtilsCommitID, command);
}
