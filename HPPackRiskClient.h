#ifndef HPPACKRISKCLIENT_H
#define HPPACKRISKCLIENT_H

#include "HPSocket4C.h"
#include "Logger.h"
#include "PackMessage.hpp"
#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <sys/types.h>         
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <vector>
#include "LockFreeQueue.hpp"

class HPPackRiskClient
{
public:
    HPPackRiskClient(const char* ip, unsigned int port);
    virtual ~HPPackRiskClient();
    void Start();
    void Login(const Message::TLoginRequest& login);
    void ReConnect();
    void Stop();
    void SendData(const unsigned char* pBuffer, int iLength);
public:
    static Utils::LockFreeQueue<Message::PackMessage> m_PackMessageQueue;
protected:
    static En_HP_HandleResult __stdcall OnPrepareConnect(HP_Client pSender, CONNID dwConnID, UINT_PTR socket);
    static En_HP_HandleResult __stdcall OnConnect(HP_Client pSender, HP_CONNID dwConnID);
    static En_HP_HandleResult __stdcall OnSend(HP_Server pSender, HP_CONNID dwConnID, const BYTE* pData, int iLength);
    static En_HP_HandleResult __stdcall OnReceive(HP_Server pSender, HP_CONNID dwConnID, const BYTE* pData, int iLength);
    static En_HP_HandleResult __stdcall OnClose(HP_Server pSender, HP_CONNID dwConnID, En_HP_SocketOperation enOperation, int iErrorCode);
private:
    std::string m_ServerIP;
    unsigned int m_ServerPort;
    HP_TcpPackClient m_pClient;
    HP_TcpPackClientListener m_pListener;
    static bool m_Connected;
    Message::TLoginRequest m_LoginRequest;
};

#endif // HPPACKRISKCLIENT_H