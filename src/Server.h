#ifndef AWSIM_SERVER_H
#define AWSIM_SERVER_H

#include <atomic>
#include <arpa/inet.h>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <list>
#include <math.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/un.h>
#include <syslog.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/filereadstream.h"

#include "Client.h"
#include "Config.h"
#include "CriticalException.h"
#include "defaults.h"
#include "Domain.h"
#include "DynamicPage.h"
#include "WorkerAndServerFlags.h"
#include "ServerAndConsoleFlags.h"
#include "HttpParser.h"
#include "HttpRequest.h"
#include "ServerInfo.h"
#include "Worker.h"

#define AF_FAMILY_TO_STR(a) ((a) == AF_INET ? "AF_INET" : (a) == AF_INET6 \
    ? "AF_INET6" : (a) == AF_UNIX ? "AF_UNIX" : "UNKNOWN")
#define SOCKTYPE_TO_STR(a) ((a) == SOCK_STREAM ? "SOCK_STREAM" \
    : (a) == SOCK_DGRAM ? "SOCK_DGRAM" : "UNKNOWN")
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define INSERT_FLAG(flag, buffer) (((uint16_t*)(buffer))[0] = (uint16_t)(flag))

namespace awsim
{
    class Server
    {
    public:
        static const std::string CONFIG_FILE_PATH;
        static const uint64_t CONSOLE_BACKLOG = 32;
        static const uint64_t CONSOLE_BUFFER_SIZE = 1024;
        static const uint64_t NUMBER_OF_EPOLL_EVENTS = 64;
        static const uint64_t INTERNET_BACKLOG = 1024;

        Server();

    private:
        struct WorkersPipeHeader
        {
            uint64_t workerID;
            WorkerAndServerFlags::ToServerFlags flag;
        };

        int consoleSocket;
        std::string consoleSocketPath;
        bool dynamicNumberOfWorkers;
        int epollfd;
        ServerInfo info;
        uint64_t nextWorkerID;
        uint64_t numberOfConnectedConsoles;
        uint64_t numberOfWorkers;
        double percentOfCoresForWorkers;
        bool quit;
        int signalsfd;
        uint64_t staticNumberOfWorkers;
        int workersReadfd;
        std::unordered_map<uint64_t, Worker> workers;

        void accept_console();
        void add_worker();
        void end();
        void end_workers();
        bool handle_console(int consoleSocket);
        bool handle_epoll_events(int count, epoll_event *events);
        bool handle_signal();
        void handle_workers();
        void loop();
        void restart();
        void start();
        void start_workers();
        void stop();
    };
};

#endif
