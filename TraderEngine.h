#ifndef TRADERENGINE_HPP
#define TRADERENGINE_HPP

#include <vector>
#include <stdlib.h>
#include <fmt/core.h>
#include "HPPackClient.h"
#include "YMLConfig.hpp"
#include "XPluginEngine.hpp"
#include "LockFreeQueue.hpp"
#include "TraderAPI/TradeGateWay.hpp"
#include "SHMConnection.hpp"
#include "FMTLogger.hpp"
#include "OrderServer.hpp"

struct ClientConf : public SHMIPC::CommonConf
{
    static const bool Performance = false;
};

class TraderEngine
{
public:
    TraderEngine();
    void LoadConfig(const std::string& path);
    void SetCommand(const std::string& cmd);
    void LoadTradeGateWay(const std::string& soPath);
    void Run();

protected:
    void WorkFunc();
    void RegisterClient(const char *ip, unsigned int port);
    void HandleOrderFromQuant();
    void ReadRequestFromClient();
    void HandleRequestMessage();
    void HandleRiskResponse();
    void HandleExecuteReport();
    void HandleCommand(const Message::PackMessage& msg);
    void SendReportToQuant();
    void SendRequest(const Message::PackMessage& request);
    void SendRiskCheckReqeust(const Message::PackMessage& request);
    void SendMonitorMessage(const Message::PackMessage& msg);
    void InitRiskCheck();

    bool IsTrading()const;
    void CheckTrading();

    void InitAppStatus();
    void UpdateAppStatus(const std::string& cmd, Message::TAppStatus& AppStatus);
private:
    HPPackClient* m_HPPackClient;
    SHMIPC::SHMConnection<Message::PackMessage, ClientConf>* m_RiskClient;
    OrderServer* m_pOrderServer;
    Utils::XTraderConfig m_XTraderConfig;
    bool m_Trading;
    unsigned long m_CurrentTimeStamp;
    int m_OpenTime;
    int m_CloseTime;
    TradeGateWay* m_TradeGateWay;
    std::string m_Command;
    Utils::LockFreeQueue<Message::PackMessage> m_RequestMessageQueue;
    Utils::LockFreeQueue<Message::PackMessage> m_ReportMessageQueue;
    std::thread* m_pWorkThread;
};

#endif // TRADERENGINE_HPP