#include "Server.h"

#define WORKERS_PIPE_BUFFER_LENGTH 1024

const std::string awsim::Server::CONFIG_FILE_PATH = "/etc/awsimd.conf";

sockaddr_storage awsim::Server::address;
int awsim::Server::epollfd = -1;
int awsim::Server::consoleSocket = -1;
bool awsim::Server::dynamicNumberOfWorkers;
std::string awsim::Server::consoleSocketPath = "";
int awsim::Server::httpSocket = -1;
uint64_t awsim::Server::nextWorkerID = 0;
uint64_t awsim::Server::numberOfConnectedConsoles;
uint64_t awsim::Server::numberOfWorkers;
double awsim::Server::percentOfCoresForWorkers;
bool awsim::Server::quit = false;
int awsim::Server::signalsfd = -1;
uint64_t awsim::Server::staticNumberOfWorkers;
//awsim::Worker awsim::Server::workers[awsim::Server::MAX_NUMBER_OF_WORKERS];
std::unordered_map<uint64_t, awsim::Server::Worker> awsim::Server::workers;
int awsim::Server::workersReadfd = -1;
int awsim::Server::workersWritefd = -1;

static void change_directory(const std::string &directory);
static void create_config_file(const std::string &configFilePath);
static int create_epoll(int consoleSocket, int signalsfd,
    int workersReadfd);
static int create_remote_host_internet_socket(sockaddr_storage *address,
    uint16_t port);
static int create_remote_host_unix_socket(const std::string &path);
static int create_signal_fd();
static void create_worker_pipe(int *readfd, int *writefd);
static void delete_fd_from_epoll(int epollfd, int fd);
static FILE* get_config_file(const std::string &filePath);
static ssize_t get_message_from_console(int epollfd, int consoleSocket,
    char *buffer);
static void json_check_existance(rapidjson::Document &document,
    const std::string name);
static bool json_get_bool(rapidjson::Value &value);
static double json_get_double(rapidjson::Value &value);
static const char* json_get_string(rapidjson::Value &value);
static uint16_t json_get_uint16(rapidjson::Value &value);
static uint64_t json_get_uint64(rapidjson::Value &value);
//static std::string json_value_to_string(const rapidjson::Value &value);
static void parse_json_file(FILE *fp, rapidjson::Document &document, char *buf,
    size_t buflen);
static ssize_t read_fd(int fd, void *buffer, size_t length);
static std::string signo_to_string(uint32_t signal);

void awsim::Server::accept_console()
{
    int sock = accept(consoleSocket, nullptr, nullptr);
    if (sock == -1)
    {
        throw std::runtime_error("accept(" + std::to_string(consoleSocket)
            + ", nullptr, nullptr) failed. " + strerror(errno) + ")");
    }
    epoll_event event;
    event.data.fd = sock;
    event.events = EPOLLIN;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &event) == -1)
    {
        throw std::runtime_error("epoll_ctl(" + std::to_string(epollfd)
            + ", EPOLL_CTL_ADD, " + std::to_string(sock) + ", &event) failed. "
            + strerror(errno) + ".");
    }
    numberOfConnectedConsoles += 1;
}

void awsim::Server::add_worker()
{
    workers.emplace(nextWorkerID, nextWorkerID);
    nextWorkerID++;
}

static void change_directory(const std::string &directory)
{
    if (access(directory.c_str(), F_OK) == -1)
    {
        syslog(LOG_NOTICE,
            "Directory \"%s\" does not exist, creating directory \"%s\".",
        directory.c_str(), directory.c_str());
        if (mkdir(directory.c_str(),
            S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) == -1)
        {
            throw std::runtime_error(
                "mkdir(\"" + directory + "\", 0) failed. " + strerror(errno)
                + ".");
        }
    }
    if (chdir(directory.c_str()) == -1)
    {
        throw std::runtime_error("chdir(\"" + directory
            + "\") failed. " + strerror(errno) + ".");
    }
}

awsim::Server::Config::Config(const std::string &filePath)
{
    FILE *fp;;
    char buf[2048];
    rapidjson::Document document;
    const char *tmp;

    try
    {
        fp = get_config_file(filePath);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error("Failed to get config file \"" + filePath
            + "\". " + ex.what());
    }

    try
    {
        parse_json_file(fp, document, buf, sizeof(buf));
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error("Failed to parse config file at \""
            + filePath + "\". "
            + ex.what());
    }

    try
    {
        json_check_existance(document, "console socket path");
        consoleSocketPath = json_get_string(document["console socket path"]);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(std::string("Failed to get \"console socket "
            "path\" from config file. ")
            + ex.what());
    }

    try
    {
        json_check_existance(document, "http port");
        httpPort = json_get_uint16(document["http port"]);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(
            std::string("Failed to get \"http port\" from config file. ")
            + ex.what());
    }

    try
    {
        json_check_existance(document, "https port");
        httpsPort = json_get_uint16(document["https port"]);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(
            std::string("Failed to get \"https port\" from config file. ")
            + ex.what());
    }

    try
    {
        json_check_existance(document, "root directory");
        tmp = json_get_string(document["root directory"]);
        strcpy(rootDirectory, tmp);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(
            std::string("Failed to get \"root directory\" from config file. ")
            + ex.what());
    }

    try
    {
        json_check_existance(document, "dynamic number of workers");
        dynamicNumberOfWorkers = json_get_bool(
            document["dynamic number of workers"]);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(std::string("Failed to get \"dynamic number "
            "of workers\" from config file. ") + ex.what());
    }

    try
    {
        json_check_existance(document, "percent of cores as workers");
        percentOfCoresForWorkers = json_get_double(
            document["percent of cores as workers"]);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(std::string("Failed to get \"percent of cores "
            "as workers\" from config file. ") + ex.what());
    }
    if (percentOfCoresForWorkers <= 0)
    {
        throw std::runtime_error("\"static number of workers\" must be a "
            "number greater than 0, currently set to \""
            + std::to_string(numberOfWorkers) + "\"");
    }

    try
    {
        json_check_existance(document, "static number of workers");
        staticNumberOfWorkers = json_get_uint64(
            document["static number of workers"]);
    }
        catch (const std::exception &ex)
    {
        throw std::runtime_error(std::string("Failed to get \"static number of "
            "workers\" from config file. ") + ex.what());
    }
    if (staticNumberOfWorkers < 1)
    {
        throw std::runtime_error("\"static number of workers\" must be an "
            "integer greater than or equal to 1, currently set to \""
            + std::to_string(numberOfWorkers) + "\"");
    }

    fclose(fp);
}

static void create_config_file(const std::string &configFilePath)
{
    std::ofstream stream(configFilePath);

    if (!stream)
    {
        throw std::runtime_error(std::string("Failed to create ofstream. ")
            + strerror(errno));
    }

    stream << "{" << std::endl
        << "    \"console socket path\": \""
            << AWSIM_DEFAULT_CONSOLE_SOCKET_PATH << "\", " << std::endl
        << "    \"http port\": " << AWSIM_DEFAULT_HTTP_PORT_NUMBER << ", "
            << std::endl
        << "    \"https port\": " << AWSIM_DEFAULT_HTTPS_PORT_NUMBER << ", "
            << std::endl
        << "    \"root directory\": \"" << AWSIM_DEFAULT_ROOT_DIRECTORY
            << "\"," << std::endl
        << "    \"dynamic number of workers\": "
            << (AWSIM_DEFAULT_DYNAMIC_NUMBER_OF_WORKERS ? "true" : "false")
            << "," << std::endl
        << "    \"percent of cores as workers\": "
            << AWSIM_DEFAULT_PERCENT_OF_CORES_AS_WORKERS << "," << std::endl
        << "    \"static number of workers\": "
            << AWSIM_DEFAULT_STATIC_NUMBER_OF_WORKERS << std::endl
        << "}" << std::endl;
    stream.close();
}

static int create_epoll(int consoleSocket, int signalsfd,
    int workersReadfd)
{
    int epollfd;
    epoll_event event;

    epollfd = epoll_create1(0);
    if (epollfd == -1)
    {
        throw std::runtime_error(std::string("epoll_create1(0) failed. ")
            + strerror(errno) + ".");
    }

    event.events = EPOLLIN;
    event.data.fd = consoleSocket;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, consoleSocket, &event) == -1)
    {
        throw std::runtime_error("epoll_ctl(" + std::to_string(epollfd)
            + ", EPOLL_CTL_ADD, " + std::to_string(consoleSocket)
            + ", &event) failed. " + strerror(errno) + ". (1)");
    }

    event.events = EPOLLIN;
    event.data.fd = signalsfd;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, signalsfd, &event) == -1)
    {
        throw std::runtime_error("epoll_ctl(" + std::to_string(epollfd)
            + ", EPOLL_CTL_ADD, " + std::to_string(signalsfd)
            + ", &event) failed. " + strerror(errno) + ". (2)");
    }

    event.events = EPOLLIN;
    event.data.fd = workersReadfd;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, workersReadfd, &event) == -1)
    {
        throw std::runtime_error("epoll_ctl(" + std::to_string(epollfd)
            + ", EPOLL_CTL_ADD, " + std::to_string(workersReadfd)
            + ", &event) failed. " + strerror(errno) + ". (3)");
    }

    return epollfd;
}

static int create_remote_host_internet_socket(sockaddr_storage *address,
    uint16_t port)
{
    addrinfo hints;
    addrinfo *results;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int fd = -1;
    int ret;

    ret = getaddrinfo(nullptr, std::to_string(port).c_str(), &hints,
        &results);
    if (ret)
    {
        throw std::runtime_error("getaddrinfo failed ("
            + std::string(gai_strerror(ret)) + ")");
    }

    for (addrinfo *ai = results; ai != nullptr; ai = ai->ai_next)
    {
        size_t size;

        fd = socket(results->ai_family, results->ai_socktype,
            results->ai_protocol);
        size = results->ai_family == AF_INET ? sizeof(sockaddr_in)
            : sizeof(sockaddr_in6);

        if (fd == -1)
        {
            throw std::runtime_error(std::string("socket(")
                + AF_FAMILY_TO_STR(results->ai_family) + ", "
                + SOCKTYPE_TO_STR(results->ai_socktype) + ", "
                + std::to_string(results->ai_protocol) + " failed. "
                + std::string(strerror(errno)) + ".");
        }
        if (bind(fd, results->ai_addr, size) == -1)
        {
            char addrstr[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];

            inet_ntop(ai->ai_family, ai->ai_addr, addrstr, ai->ai_addrlen);
            if (ai->ai_family == AF_INET)
            {
                syslog(LOG_INFO, "Failed to bind socket to %s:%" PRIu16 ". %s.",
                    addrstr, port, strerror(errno));
            }
            else
            {
                syslog(LOG_INFO, "Failed to bind socket to [%s]:%" PRIu16
                    ". %s.", addrstr, port, strerror(errno));
            }

            close(fd);
            fd = -1;
            continue;
        }
        if (listen(fd, awsim::Server::INTERNET_BACKLOG) == -1)
        {
            syslog(LOG_ERR, "listen(%d, %d) failed. %s.", fd,
                (int)awsim::Server::INTERNET_BACKLOG, strerror(errno));
            close(fd);
            fd = -1;
        }
        else
        {
            break;
        }
    }
    if (fd == -1)
    {
        throw std::runtime_error("Could not set up a remote host for port "
            + std::to_string(port) + ".");
    }

    memset(address, 0, sizeof(sockaddr_storage));
    memcpy(address, results->ai_addr, results->ai_addrlen);
    freeaddrinfo(results);

    return fd;
}

static int create_remote_host_unix_socket(const std::string &path)
{
    int fd;
    sockaddr_un address;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1)
    {
        throw std::runtime_error("socket(AF_UNIX, SOCK_STREAM, 0) failed. "
            + std::string(strerror(errno)) + ".");
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, path.c_str(), path.length());

    unlink(path.c_str());
    if (bind(fd, (sockaddr*)&address, sizeof(address)) == -1)
    {
        throw std::runtime_error("Failed to bind unix socket to \"" + path
            + "\". " + strerror(errno) + ".");
    }
    if (listen(fd, awsim::Server::CONSOLE_BACKLOG) == -1)
    {
        throw std::runtime_error("listen(" + std::to_string(fd) + ", "
            + std::to_string(awsim::Server::CONSOLE_BACKLOG) + ") failed. "
            + strerror(errno) + ".");
    }
    return fd;
}

static int create_signal_fd()
{
    int fd;
    sigset_t sigset;

    if (sigemptyset(&sigset) == -1)
    {
        throw std::runtime_error(
            std::string("sigemptyset(&sigset) failed. ") + strerror(errno)
            + ".");
    }

    if (sigaddset(&sigset, SIGINT) == -1)
    {
        throw std::runtime_error(
            std::string("sigaddset(&sigset, SIGINT) failed. ")
            + strerror(errno) + ".");
    }

    if (sigaddset(&sigset, SIGQUIT) == -1)
    {
        throw std::runtime_error(
            std::string("sigaddset(&sigset, SIGQUIT) failed. ")
            + strerror(errno) + ".");
    }

    if (sigaddset(&sigset, SIGTERM) == -1)
    {
        throw std::runtime_error(
            std::string("sigaddset(&sigset, SIGTERM) failed. ")
            + strerror(errno) + ".");
    }

    if (sigprocmask(SIG_BLOCK, &sigset, nullptr) == -1)
    {
        throw std::runtime_error(
            std::string("sigprocmask(SIG_BLOCK, &sigset, nullptr) failed. ")
            + strerror(errno) + ".");
    }

    fd = signalfd(-1, &sigset, 0);
    if (fd == -1)
    {
        throw std::runtime_error(std::string("signalfd(-1, &mask,0) failed. ")
            + strerror(errno) + ".");
    }

    return fd;
}

static void create_worker_pipe(int *readfd, int *writefd)
{
    int fds[2];
    if (pipe(fds) == -1)
    {
        throw std::runtime_error(std::string("pipe(fds) failed. ")
        + strerror(errno) + ".");
    }
    *readfd = fds[0];
    *writefd = fds[1];
}

static void delete_fd_from_epoll(int epollfd, int fd)
{
    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr))
    {
        throw std::runtime_error("epoll_ctl("
            + std::to_string(epollfd) + ", EPOLL_CTL_DEL, " + std::to_string(fd)
            + ", nullptr) failed. " + strerror(errno) + ".");
    }
}


void awsim::Server::end()
{
    syslog(LOG_INFO, "Ending.");
    end_workers();
    close(httpSocket);
    close(consoleSocket);
    close(epollfd);
    unlink(consoleSocketPath.c_str());
    syslog(LOG_INFO, "Ended.");
}

void awsim::Server::end_workers()
{
    workers.clear();
}

static FILE* get_config_file(const std::string &filePath)
{
    FILE *fp;
    if (access(filePath.c_str(), F_OK) == -1)
    {
        syslog(LOG_NOTICE,
            "No config file exists at \"%s\", creating a new one at \"%s\".",
            filePath.c_str(), filePath.c_str());
        try
        {
            create_config_file(filePath);
        }
        catch (const std::exception &ex)
        {
            throw std::runtime_error("Failed to create a config file at \""
                + filePath + "\". " + ex.what() + ".");
        }
    }
    fp = fopen(filePath.c_str(), "r");
    if (fp == nullptr)
    {
        throw std::runtime_error("fopen(\"" + filePath + "\", \"r\") failed. "
            + strerror(errno) + ".");
    }

    return fp;
}

static ssize_t get_message_from_console(int epollfd, int consoleSocket,
    char *buffer)
{
    ssize_t length;

    length = recv(consoleSocket, buffer, sizeof(buffer), 0);
    if (length == -1)
    {
        try
        {
            delete_fd_from_epoll(epollfd, consoleSocket);
        }
        catch (std::exception &ex)
        {
            throw std::runtime_error(
                std::string("Failed to delete connected console socket. ")
                + ex.what());
        }
        throw std::runtime_error("recv(" + std::to_string(consoleSocket)
            + ", buffer, " + std::to_string(sizeof(buffer)) + ", 0) failed. "
            + strerror(errno) + ".");
    }

    return length;
}

bool awsim::Server::handle_console(int consoleSocket)
{
    char buffer[CONSOLE_BUFFER_SIZE];
    ssize_t length;
    uint16_t flag;

    length = get_message_from_console(epollfd, consoleSocket, buffer);
    if (length == 0)
    {
        try
        {
            delete_fd_from_epoll(epollfd, consoleSocket);
            numberOfConnectedConsoles -= 1;
        }
        catch (std::exception &ex)
        {
            throw std::runtime_error(
                std::string("Failed to delete connected console socket. ")
                + ex.what());
        }
    }
    flag = ntohs(((uint16_t*)buffer)[0]);

    switch (flag)
    {
        case (uint16_t)ConsoleToServerFlags::EndServerRequest:
            flag = htons((uint16_t)ServerToConsoleFlags::EndServerSuccess);
            send(consoleSocket, &flag, sizeof(flag), 0);
            end();
            return true;
        case (uint16_t)ConsoleToServerFlags::RestartServerRequest:
            flag = htons((uint16_t)ServerToConsoleFlags::EndServerSuccess);
            send(consoleSocket, &flag, sizeof(flag), 0);
            restart();
            break;
        default:
            syslog(LOG_WARNING,
                "Received unkown console command. Command flag is 0x%02x.",
                    flag);
            break;
    }

    return false;
}

bool awsim::Server::handle_signal()
{
    signalfd_siginfo siginfo;
    passwd *user;
    const char *name = "UNKOWN";

    read_fd(signalsfd, &siginfo, sizeof(signalfd_siginfo));

    user = getpwuid(siginfo.ssi_uid);
    if (user != nullptr)
    {
        name = user->pw_name;
    }
    syslog(LOG_NOTICE,
            "Received %s from pid=%" PRIu32 ", uid=%" PRIu32 ", user=\"%s\"",
            signo_to_string(siginfo.ssi_signo).c_str(), siginfo.ssi_pid,
            siginfo.ssi_uid, name);

    switch (siginfo.ssi_signo)
    {
        case SIGINT:
            end();
            return true;
        case SIGQUIT:
            end();
            return true;
        case SIGTERM:
            end();
            return true;
    }
    return false;
}

void awsim::Server::handle_workers()
{
    char buffer[WORKERS_PIPE_BUFFER_LENGTH];
    char *offset;
    ssize_t length;
    WorkersPipeHeader header;

    length = read_fd(workersReadfd, buffer, sizeof(buffer));
    offset = buffer;
    while ((size_t)length >= sizeof(WorkersPipeHeader))
    {
        header.workerID = ((uint64_t*)buffer)[0];
        header.flag = (WorkerToServerFlags)((uint16_t*)(buffer
            + sizeof(uint64_t)))[0];

        #ifdef AWSIM_DEBUG
            syslog(LOG_DEBUG, "Received %s from worker %" PRIu64 ".",
                WorkerToServerFlagToString(header.flag).c_str(),
                header.workerID);
        #endif

        auto worker = workers.find(header.workerID);
        if (worker == workers.end())
        {
            throw std::runtime_error(
                "Received message from worker with invalid ID.");
        }
        length -= sizeof(WorkersPipeHeader);
        offset += sizeof(WorkersPipeHeader);

        switch(header.flag)
        {
            case WorkerToServerFlags::WeakStopCompleted:
                #ifdef AWSIM_DEBUG
                    syslog(LOG_DEBUG, "Removing worker %" PRIu64 ".",
                        header.workerID);
                #endif
                workers.erase(header.workerID);
                break;
            case WorkerToServerFlags::NumberOfClients:
                worker->second.numberOfClients = ((uint64_t*)buffer)[0];
                length -= sizeof(uint64_t);
                offset += sizeof(uint64_t);
                break;
            default:
                throw std::runtime_error("Received unknown flag form worker, "
                    + std::to_string((uint16_t)header.flag) + ".");
        }
    }
}

void awsim::Server::init()
{
    start();

    try
    {
        create_worker_pipe(&workersReadfd, &workersWritefd);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(std::string("Failed to create workers pipe. ")
            + ex.what());
    }

    try
    {
        signalsfd = create_signal_fd();
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(
            std::string("Failed to create a signal file descriptor. ")
            + ex.what());
    }

    try
    {
        epollfd = create_epoll(consoleSocket, signalsfd, workersReadfd);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(
            std::string("Failed to create epoll for connected consoles. ")
            + ex.what());
    }

    loop();
}

static void json_check_existance(rapidjson::Document &document,
    const std::string name)
{
    if (!document.HasMember(name.c_str()))
    {
        throw std::runtime_error("\"" + name + "\" does not exist.");
    }
}

static bool json_get_bool(rapidjson::Value &value)
{
    if (value.IsNull())
    {
        throw std::runtime_error("Value not found.");
    }

    if (!value.IsBool())
    {
        throw std::runtime_error("Value not a bool.");
    }

    return value.GetBool();
}

static double json_get_double(rapidjson::Value &value)
{
    if (value.IsNull())
    {
        throw std::runtime_error("Value not found.");
    }

    if (!value.IsNumber())
    {
        throw std::runtime_error("Value not a number.");
    }

    return value.GetDouble();
}

static const char* json_get_string(rapidjson::Value &value)
{
    if (value.IsNull())
    {
        throw std::runtime_error("Value not found.");
    }

    if (!value.IsString())
    {
        throw std::runtime_error("Value not a string.");
    }

    return value.GetString();
}

static uint16_t json_get_uint16(rapidjson::Value &value)
{
    uint64_t tmp;

    if (value.IsNull())
    {
        throw std::runtime_error("Value not found.");
    }

    if (!value.IsUint())
    {
        throw std::runtime_error("Value not an unsigned 16 bit integer.");
    }

    tmp = value.GetUint();
    if (tmp > 65535)
    {
        throw std::runtime_error("Value not an unsigned 16 bit integer.");
    }

    return (uint16_t)tmp;
}

static uint64_t json_get_uint64(rapidjson::Value &value)
{

    if (value.IsNull())
    {
        throw std::runtime_error("Value not found.");
    }

    if (!value.IsUint())
    {
        throw std::runtime_error("Value not an unsigned 64 bit integer.");
    }

    return value.GetUint();
}

/*static std::string json_value_to_string(const rapidjson::Value &value)
{
    if (value.IsBool())
    {
        return std::to_string(value.GetBool());
    }
    if (value.IsDouble())
    {
        return std::to_string(value.GetDouble());
    }
    if (value.IsFloat())
    {
        return std::to_string(value.GetFloat());
    }
    if (value.IsInt())
    {
        return std::to_string(value.GetInt());
    }
    if (value.IsInt64())
    {
        return std::to_string(value.GetInt64());
    }
    if (value.IsNull())
    {
        return "NULL";
    }
    if (value.IsString())
    {
        return value.GetString();
    }
    if (value.IsUint())
    {
        return std::to_string(value.GetUint());
    }
    if (value.IsUint64())
    {
        return std::to_string(value.GetUint64());
    }
    return "empty";
}*/

void awsim::Server::loop()
{
    epoll_event events[NUMBER_OF_EPOLL_EVENTS];

    quit = false;
    numberOfConnectedConsoles = 0;
    while (!quit)
    {
        int nfds = epoll_wait(epollfd, events, NUMBER_OF_EPOLL_EVENTS, -1);

        if (nfds == -1 && errno != EINTR)
        {
            throw std::runtime_error("epoll_wait("
                + std::to_string(epollfd) + ", events, "
                + std::to_string(NUMBER_OF_EPOLL_EVENTS) + ", -1) failed. "
                + strerror(errno) + ".");
        }
        for (int i = 0; !quit && i < nfds; ++i)
        {
            int fd = events[i].data.fd;

            if (fd == consoleSocket)
            {
                try
                {
                    accept_console();
                }
                catch (const std::exception &ex)
                {
                    syslog(LOG_ERR,
                        "Failed to accept a pending console connection. %s.",
                        ex.what());
                }
            }
            else if (fd == workersReadfd)
            {
                try
                {
                    handle_workers();
                }
                catch (const std::exception &ex)
                {
                    throw std::runtime_error(
                        std::string("Failed to handle workers pipe. ")
                        + ex.what());
                }
            }
            else if (fd == signalsfd)
            {
                try
                {

                    quit = handle_signal();
                }
                catch (const std::exception &ex)
                {
                    throw std::runtime_error(
                        std::string("Failed to process signal. ") + ex.what());
                }
            }
            else
            {
                try
                {
                    quit = handle_console(fd);
                }
                catch (const std::exception &ex)
                {
                    syslog(LOG_ERR, "Failed to handle console. %s.", ex.what());
                }
            }
        }
        if (!quit)
        {
            start_workers();
        }
    }
}

static void parse_json_file(FILE *fp, rapidjson::Document &document, char *buf,
    size_t buflen)
{
    rapidjson::ParseResult parseResult;

    rapidjson::FileReadStream stream(fp, buf, buflen);
    parseResult = document.ParseStream(stream);
    if (!parseResult)
    {
        throw std::runtime_error(std::string("ParseStream(stream) failed. ")
            + rapidjson::GetParseError_En(parseResult.Code()));
    }
}

static ssize_t read_fd(int fd, void *buffer, size_t length)
{
    ssize_t result;

    result = read(fd, buffer, length);
    if (result == -1)
    {
        throw std::runtime_error("read(" + std::to_string(fd) + ", buffer, "
            + std::to_string(length) + ") failed. " + strerror(errno) + ".");
    }
    return result;
}

void awsim::Server::restart()
{
    syslog(LOG_INFO, "Restarting.");
    stop();
    start();
}

static std::string signo_to_string(uint32_t signal)
{
    switch(signal)
    {
        case SIGINT:
            return "SIGINT";
        case SIGQUIT:
            return "SIGQUIT";
        case SIGTERM:
            return "SIGTERM";
        default:
            return "unknown (" + std::to_string(signal) + ")";
    }
}

void awsim::Server::start()
{
    char addrstr[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];

    #ifdef AWSIM_DEBUG
        syslog(LOG_INFO, "Starting in debug mode.");
    #else
        syslog(LOG_INFO, "Starting.");
    #endif
    Config config(CONFIG_FILE_PATH);
    numberOfWorkers = 0;
    staticNumberOfWorkers = config.staticNumberOfWorkers;
    dynamicNumberOfWorkers = config.dynamicNumberOfWorkers;
    percentOfCoresForWorkers = config.percentOfCoresForWorkers;

    try
    {
        change_directory(config.rootDirectory);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(std::string("Failed to change directories. ")
            + ex.what());
    }

    try
    {
        httpSocket = create_remote_host_internet_socket(&address,
            config.httpPort);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(std::string("Failed to create http socket. ")
            + ex.what());
    }

    try
    {
        consoleSocket = create_remote_host_unix_socket(
            consoleSocketPath);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(
            std::string("Failed to create console listening socket. ")
            + ex.what());
    }

    start_workers();

    inet_ntop(address.ss_family, &address, addrstr, sizeof(address));
    syslog(LOG_INFO, "HTTP listening on %s:%" PRIu16 ".", addrstr,
        ntohs(((sockaddr_in*)&address)->sin_port));
}

void awsim::Server::start_workers()
{
    uint64_t count = staticNumberOfWorkers;
    auto size = workers.size();

    if (dynamicNumberOfWorkers)
    {
        count = (uint64_t)ceil(get_nprocs_conf() * percentOfCoresForWorkers);
    }
    if (count > size)
    {
        #ifdef AWSIM_DEBUG
            syslog(LOG_DEBUG, "Adding %lu workers.", count - size);
        #endif
        for (; count > size; count--)
        {
            add_worker();
        }
    }
    else if (count < size)
    {
        auto it = workers.begin();

        #ifdef AWSIM_DEBUG
            syslog(LOG_DEBUG, "Removing %lu workers.", size - count);
        #endif
        for (uint64_t i = size - count; i > 0; --i)
        {
            it->second.request_weak_stop();
            ++it;
        }
    }
}

void awsim::Server::stop()
{
    syslog(LOG_INFO, "Stopping.");
    end_workers();
    close(httpSocket);
}

void awsim::Server::write_to_pipe(void *buffer, size_t length)
{
    if (write(workersWritefd, buffer, length) == -1)
    {
        throw std::runtime_error("write(" + std::to_string(workersWritefd)
            + ", buffer, " + std::to_string(length) + ") failed. "
            + strerror(errno) + ".");
    }
}
