#ifndef AWSIM_SERVERINFO_H
#define AWSIM_SERVERINFO_H

#include <sys/socket.h>
#include <string>
#include <stdint.h>
#include <unordered_map>

#include "Domain.h"

namespace awsim
{
    struct ServerInfo
    {
        sockaddr_storage address;
        std::unordered_map<std::string, Domain> domains;
        int httpSocket;
        uint64_t minimumSizeOfLargeFiles;
        int workersWritefd;
    };
}

#endif
