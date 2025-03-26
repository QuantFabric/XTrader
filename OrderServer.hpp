#ifndef ORDERSERVER_HPP
#define ORDERSERVER_HPP

#include <string>
#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <mutex>
#include <unordered_map>
#include <unistd.h>
#include "PackMessage.hpp"
#include "FMTLogger.hpp"
#include "LockFreeQueue.hpp"
#include "phmap.h"
#include <shared_mutex>

#include "SHMServer.hpp"

struct ServerConf : public SHMIPC::CommonConf
{
    static const bool Publish = false;
    static const bool Performance = true;
};

class OrderServer : public SHMIPC::SHMServer<Message::PackMessage, ServerConf>
{
public:
    OrderServer():SHMServer<Message::PackMessage, ServerConf>()
    {

    }

    virtual ~OrderServer()
    {

    }

    void HandleMsg()
    {

    }
};


#endif // ORDERSERVER_HPP