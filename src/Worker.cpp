#include "Server.h"

#define IS_IN_ACTIVE_STATE(state) ((state) == State::Running \
    || (state) == State::WeakStopPending)
#define SERVER_PIPE_PTR (void*)0x0
#define HTTP_REMOTE_HOST_PTR (void*)0x1
#define HTTPS_REMOTE_HOST_PTR (void*)0x2
#define GET_FLAG(buffer) ((ServerToWorkerFlags)(((uint16_t*)(buffer))[0]))
#define HTTP_HEADER_BUFFER_SIZE 8192
#define UNUSED(x) (void)(x)

static int on_message_begin(awsim::HttpRequest *request,
    awsim::HttpParser *parser);
static int on_url(awsim::HttpRequest *request, awsim::HttpParser *parser,
    const char *at, size_t length);
static int on_header_field(awsim::HttpRequest *request,
    awsim::HttpParser *parser, const char *at, size_t length);
static int on_header_value(awsim::HttpRequest *request,
    awsim::HttpParser *parser, const char *at, size_t length);
static int on_headers_complete(awsim::HttpRequest *request,
    awsim::HttpParser *parser, const char *at, size_t length);
static int on_body(awsim::HttpRequest *request, awsim::HttpParser *parser,
    const char *at, size_t length);
static int on_message_complete(awsim::HttpRequest *request,
    awsim::HttpParser *parser);
static int on_reason(awsim::HttpRequest *request, awsim::HttpParser *parser,
    const char *at, size_t length);
static int on_chunk_header(awsim::HttpRequest *request,
    awsim::HttpParser *parser);
static int on_chunk_complete(awsim::HttpRequest *request,
    awsim::HttpParser *parser);
awsim::HttpParserSettings awsim::Server::Worker::httpParserSettings =
{
    .on_message_begin = on_message_begin,
    .on_url = on_url,
    .on_header_field = on_header_field,
    .on_header_value = on_header_value,
    .on_headers_complete = on_headers_complete,
    .on_body = on_body,
    .on_message_complete = on_message_complete,
    .on_reason = on_reason,
    .on_chunk_header = on_chunk_header,
    .on_chunk_complete = on_chunk_complete
};

static int create_epoll(int httpSocket, int serverReadfd);
static void create_pipe(int *readfd, int *writefd);
static ssize_t read_fd(int fd, void *buffer, size_t length);
static void remove_fd_from_epoll(int fd, int epollfd);

awsim::HttpRequest::Field httpRequestField;

static int on_message_begin(awsim::HttpRequest *request,
    awsim::HttpParser *parser)
{
    UNUSED(request);
    UNUSED(parser);
    return 0;
}

static int on_url(awsim::HttpRequest *request, awsim::HttpParser *parser,
    const char *at, size_t length)
{
    UNUSED(parser);
    bool foundPeriod = false;

    request->url.buffer = at;
    request->url.length = length;

    for (size_t i = 0; i < length; ++i)
    {
        if (at[i] == '.')
        {
            if (foundPeriod)
            {
                return -1;
            }
            foundPeriod = true;
        }
        else
        {
            foundPeriod = false;
        }
    }

    return 0;
}

static int on_header_field(awsim::HttpRequest *request,
    awsim::HttpParser *parser, const char *at, size_t length)
{
    const char *fieldString;

    UNUSED(request);
    UNUSED(parser);

    httpRequestField = awsim::HttpRequest::Field::Unknown;
    if (length > 0)
    {
        switch (at[0])
        {
            case 'H':
                httpRequestField = awsim::HttpRequest::Field::Host;
                break;
            case 'A':
                httpRequestField = awsim::HttpRequest::Field::Accept; // or maybe Accept-Language or Accept-Encoding
                break;
            case 'C':
                httpRequestField = awsim::HttpRequest::Field::Connection;
                break;
            case 'U':
                httpRequestField = awsim::HttpRequest::Field::UserAgent; // or maybe Upgrade-Insecure-Requests
                break;
        }
    }

    if (httpRequestField != awsim::HttpRequest::Field::Unknown)
    {
        fieldString = awsim::HttpRequest::fieldStrings[
            (int)httpRequestField];
        for (size_t i = 0; i < length; ++i)
        {
            char actual = at[i];
            char desired = fieldString[i];

            if (actual != desired)
            {
                if (httpRequestField == awsim::HttpRequest::Field::Accept)
                {
                    httpRequestField =
                        awsim::HttpRequest::Field::AcceptEncoding;
                    fieldString = awsim::HttpRequest::fieldStrings[
                        (int)httpRequestField];
                    --i;
                }
                else if (httpRequestField ==
                    awsim::HttpRequest::Field::AcceptEncoding)
                {
                    httpRequestField =
                        awsim::HttpRequest::Field::AcceptLanguage;
                    fieldString = awsim::HttpRequest::fieldStrings[
                        (int)httpRequestField];
                    --i;
                }
                else if (httpRequestField ==
                    awsim::HttpRequest::Field::UserAgent)
                {
                    httpRequestField =
                        awsim::HttpRequest::Field::UpgradeInsecureRequests;
                    fieldString = awsim::HttpRequest::fieldStrings[
                        (int)httpRequestField];
                    --i;
                }
                else
                {
                    httpRequestField = awsim::HttpRequest::Field::Unknown;
                    break;
                }
            }
            else if (i == length - 1 && fieldString[i + 1] != '\0')
            {
                httpRequestField = awsim::HttpRequest::Field::Unknown;
                break;
            }
        }
    }

    #ifdef AWSIM_DEBUG
        if (httpRequestField == awsim::HttpRequest::Field::Unknown)
        {
            syslog(LOG_DEBUG, "Unknown header field \"%.*s\"", (int)length, at);
        }
    #endif

    return 0;
}

static int on_header_value(awsim::HttpRequest *request,
    awsim::HttpParser *parser, const char *at, size_t length)
{
    UNUSED(parser);

    switch (httpRequestField)
    {
        case awsim::HttpRequest::Field::Host:
            request->host.buffer = at;
            request->host.length = length;
            request->host.set = true;
            break;
        case awsim::HttpRequest::Field::UserAgent:
            request->userAgent.buffer = at;
            request->userAgent.length = length;
            request->userAgent.set = true;
            break;
        case awsim::HttpRequest::Field::Accept:
            request->accept.buffer = at;
            request->accept.length = length;
            request->accept.set = true;
            break;
        case awsim::HttpRequest::Field::AcceptLanguage:
            request->acceptLanguage.buffer = at;
            request->acceptLanguage.length = length;
            request->acceptLanguage.set = true;
            break;
        case awsim::HttpRequest::Field::AcceptEncoding:
            request->acceptEncoding.buffer = at;
            request->acceptEncoding.length = length;
            request->acceptEncoding.set = true;
            break;
        case awsim::HttpRequest::Field::Connection:
            request->connection.buffer = at;
            request->connection.length = length;
            request->connection.set = true;
            break;
        case awsim::HttpRequest::Field::UpgradeInsecureRequests:
            request->upgradeInsecureRequests.buffer = at;
            request->upgradeInsecureRequests.length = length;
            request->upgradeInsecureRequests.set = true;
            break;
        case awsim::HttpRequest::Field::Unknown:
        default:
            syslog(LOG_WARNING, "Unknown header value \"%.*s\"", (int)length,
                at);
            break;
    }

    return 0;
}

static int on_headers_complete(awsim::HttpRequest *request,
    awsim::HttpParser *parser, const char *at, size_t length)
{
    UNUSED(at);
    UNUSED(length);

    switch(parser->method)
    {
        case awsim::HTTPRequestMethod::GET:
            request->method = awsim::HttpRequest::Method::GET;
            break;
        case awsim::HTTPRequestMethod::POST:
            request->method = awsim::HttpRequest::Method::POST;
            break;
        case awsim::HTTPRequestMethod::HEAD:
            request->method = awsim::HttpRequest::Method::HEAD;
            break;
        case awsim::HTTPRequestMethod::PUT:
            request->method = awsim::HttpRequest::Method::PUT;
            break;
        case awsim::HTTPRequestMethod::CONNECT:
            request->method = awsim::HttpRequest::Method::CONNECT;
            break;
        case awsim::HTTPRequestMethod::OPTIONS:
            request->method = awsim::HttpRequest::Method::OPTIONS;
            break;
        case awsim::HTTPRequestMethod::TRACE:
            request->method = awsim::HttpRequest::Method::TRACE;
            break;
        case awsim::HTTPRequestMethod::COPY:
            request->method = awsim::HttpRequest::Method::COPY;
            break;
        case awsim::HTTPRequestMethod::LOCK:
            request->method = awsim::HttpRequest::Method::LOCK;
            break;
        case awsim::HTTPRequestMethod::MKCOL:
            request->method = awsim::HttpRequest::Method::MKCOL;
            break;
        case awsim::HTTPRequestMethod::MOVE:
            request->method = awsim::HttpRequest::Method::MOVE;
            break;
        case awsim::HTTPRequestMethod::PROPFIND:
            request->method = awsim::HttpRequest::Method::PROPFIND;
            break;
        case awsim::HTTPRequestMethod::PROPPATCH:
            request->method = awsim::HttpRequest::Method::PROPPATCH;
            break;
        case awsim::HTTPRequestMethod::UNLOCK:
            request->method = awsim::HttpRequest::Method::UNLOCK;
            break;
        case awsim::HTTPRequestMethod::REPORT:
            request->method = awsim::HttpRequest::Method::REPORT;
            break;
        case awsim::HTTPRequestMethod::MKACTIVITY:
            request->method = awsim::HttpRequest::Method::MKACTIVITY;
            break;
        case awsim::HTTPRequestMethod::CHECKOUT:
            request->method = awsim::HttpRequest::Method::CHECKOUT;
            break;
        case awsim::HTTPRequestMethod::MERGE:
            request->method = awsim::HttpRequest::Method::MERGE;
            break;
        case awsim::HTTPRequestMethod::MSEARCH:
            request->method = awsim::HttpRequest::Method::MSEARCH;
            break;
        case awsim::HTTPRequestMethod::NOTIFY:
            request->method = awsim::HttpRequest::Method::NOTIFY;
            break;
        case awsim::HTTPRequestMethod::SUBSCRIBE:
            request->method = awsim::HttpRequest::Method::SUBSCRIBE;
            break;
        case awsim::HTTPRequestMethod::UNSUBSCRIBE:
            request->method = awsim::HttpRequest::Method::UNSUBSCRIBE;
            break;
        case awsim::HTTPRequestMethod::PATCH:
            request->method = awsim::HttpRequest::Method::PATCH;
            break;
        case awsim::HTTPRequestMethod::DELETE:
            request->method = awsim::HttpRequest::Method::DELETE;
            break;
    }
    return 0;
}

static int on_body(awsim::HttpRequest *request, awsim::HttpParser *parser,
    const char *at, size_t length)
{
    UNUSED(request);
    UNUSED(parser);
    UNUSED(at);
    UNUSED(length);
    return 0;
}

static int on_message_complete(awsim::HttpRequest *request,
    awsim::HttpParser *parser)
{
    UNUSED(request);
    UNUSED(parser);
    return 0;
}

static int on_reason(awsim::HttpRequest *request, awsim::HttpParser *parser,
    const char *at, size_t length)
{
    UNUSED(request);
    UNUSED(parser);
    UNUSED(at);
    UNUSED(length);
    return 0;
}

static int on_chunk_header(awsim::HttpRequest *request,
    awsim::HttpParser *parser)
{
    UNUSED(request);
    UNUSED(parser);
    return 0;
}

static int on_chunk_complete(awsim::HttpRequest *request,
    awsim::HttpParser *parser)
{
    UNUSED(request);
    UNUSED(parser);
    return 0;
}

void awsim::Server::Worker::accept_http_client()
{
    int clientSocket;

    #ifdef AWSIM_DEBUG
        syslog(LOG_DEBUG, "(Worker %" PRIu64 ") Accepting HTTP client.", id);
    #endif
    clientSocket = accept(Server::httpSocket, nullptr, nullptr);
    if (clientSocket == -1)
    {
        throw std::runtime_error("accept(" + std::to_string(Server::httpSocket)
            + ", nullptr, nullptr) failed. " + strerror(errno) + ")");
    }

    try
    {
        add_http_client(clientSocket);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(
            std::string("Could not add HTTP client. ") + ex.what());
    }
}

void awsim::Server::Worker::add_http_client(int clientSocket)
{
    epoll_event event;
    Client *client;

    #ifdef AWSIM_DEBUG
        syslog(LOG_DEBUG, "(Worker %" PRIu64 ") Adding HTTP client.", id);
    #endif
    if (_numberOfClients == maxNumberOfClients)
    {
        try
        {
            increase_max_number_of_clients(maxNumberOfClients * 2);
        }
        catch (const CriticalException &ex)
        {
            throw CriticalException(
                std::string("Failed to increase max number of clients. ")
                + ex.what());
        }
        catch (const std::exception &ex)
        {
            throw std::runtime_error(
                std::string("Failed to increase max number of clients. ")
                + ex.what());
        }
    }
    client = unallocatedList;
    event.events = EPOLLIN;
    event.data.ptr = client;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, clientSocket, &event) == -1)
    {
        throw std::runtime_error("epoll_ctl(" + std::to_string(epollfd)
            + ", EPOLL_CTL_ADD, " + std::to_string(clientSocket)
            + ", &event) failed. " + strerror(errno) + ".");
    }

    unallocatedList = client->next;
    if (unallocatedList != nullptr)
    {
        unallocatedList->prev = nullptr;
    }
    client->next = allocatedList;
    client->prev = nullptr;
    allocatedList = client;
    if (client->next != nullptr)
    {
        client->next->prev = client;
    }
    _numberOfClients++;
    client->init(clientSocket, false);
}

static int create_epoll(int httpSocket, int serverReadfd)
{
    int epollfd;
    epoll_event event;

    if ((epollfd = epoll_create1(0)) == -1)
    {
        throw std::runtime_error("epoll_create1() failed ("
            + std::string(strerror(errno)) + ")");
    }

    event.events = EPOLLIN | EPOLLEXCLUSIVE;
    event.data.ptr = HTTP_REMOTE_HOST_PTR;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, httpSocket, &event))
    {
        throw std::runtime_error("epoll_ctl(" + std::to_string(epollfd)
            + ", EPOLL_CTL_ADD, " + std::to_string(httpSocket)
            + ", &event) failed. " + strerror(errno) + ".");
    }

    event.events = EPOLLIN;
    event.data.ptr = SERVER_PIPE_PTR;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, serverReadfd, &event))
    {
        throw std::runtime_error("epoll_ctl(" + std::to_string(epollfd)
            + ", EPOLL_CTL_ADD, " + std::to_string(serverReadfd)
            + ", &event) failed. " + strerror(errno) + ".");
    }

    return epollfd;
}

static void create_pipe(int *readfd, int *writefd)
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

void awsim::Server::Worker::handle_client(Client *client)
{
    char buffer[HTTP_HEADER_BUFFER_SIZE];
    ssize_t length;
    size_t nparsed;
    HttpRequest request;

    #ifdef AWSIM_DEBUG
        syslog(LOG_DEBUG, "(Worker %" PRIu64 ") Handling a client.", id);
    #endif
    length = recv(client->sock, buffer, 1024, 0);
    if (length == -1)
    {
        throw std::runtime_error("recv(" + std::to_string(client->sock)
            + ", buffer, " + std::to_string(sizeof(buffer)) + ", "
            + std::to_string(NO_FLAGS) + ") failed. " + strerror(errno));
    }
    if (length == 0)
    {
        #ifdef AWSIM_DEBUG
            syslog(LOG_DEBUG, "(Worker %" PRIu64 ") Client disconnected.", id);
        #endif
        remove_client(client);
        return;
    }

    httpParser.data = client;
    nparsed = http_parser_execute(&request, &httpParser,
        &httpParserSettings, buffer, length);
    if (nparsed != (size_t)length)
    {
        remove_client(client);
        throw std::runtime_error("Failed to parse HTTP request");
    }

    
}

void awsim::Server::Worker::handle_server_pipe()
{
    char buffer[SERVER_PIPE_BUFFER_SIZE];
    char *offset;
    ssize_t length;

    length = read_fd(serverReadfd, buffer, sizeof(buffer));
    offset = buffer;
    while ((size_t)length >= sizeof(uint16_t))
    {
        ServerToWorkerFlags flag = GET_FLAG(offset);

        #ifdef AWSIM_DEBUG
            syslog(LOG_DEBUG, "(Worker %" PRIu64 ") Received %s.", id,
                ServerToWorkerFlagToString(flag).c_str());
        #endif
        length -= sizeof(uint16_t);
        offset += sizeof(uint16_t);
        switch(flag)
        {
            case ServerToWorkerFlags::WeakStopRequest:
                weak_stop();
                break;
            case ServerToWorkerFlags::StopRequest:
                stop();
                break;
            case ServerToWorkerFlags::NumberOfClientsRequest:
                INSERT_FLAG(WorkerToServerFlags::NumberOfClients, buffer);
                ((uint64_t*)(buffer + sizeof(uint16_t)))[0] = _numberOfClients;
                break;
            default:
                throw std::runtime_error("Received unknown flag form worker, "
                    + std::to_string((uint16_t)flag) + ".");
        }
    }
}

void awsim::Server::Worker::increase_max_number_of_clients(
    uint64_t maxNumberOfClients)
{
    #ifdef AWSIM_DEBUG
        syslog(LOG_DEBUG, "(Worker %" PRIu64 ") Increasing max number of "
            "clients from %" PRIu64 " to %" PRIu64 ".", id,
            this->maxNumberOfClients, maxNumberOfClients);
    #endif
    if (maxNumberOfClients < this->maxNumberOfClients)
    {
        throw std::runtime_error("The new max number of clients must be greater than "
            "the current value. Current value is "
            + std::to_string(this->maxNumberOfClients)
            + " and the new value is "
            + std::to_string(this->maxNumberOfClients) + ".");
    }

    if (maxNumberOfClients == this->maxNumberOfClients)
    {
        return;
    }
    if (this->maxNumberOfClients == 0)
    {
        clientHeap = (Client*)malloc(sizeof(Client) * maxNumberOfClients);
        if (clientHeap == nullptr)
        {
            throw CriticalException("malloc("
               + std::to_string(sizeof(Client) * maxNumberOfClients)
                + ") failed. " + strerror(errno) + ".");
        }
    }
    else
    {
        clientHeap = (Client*)realloc(clientHeap,
            sizeof(Client) * maxNumberOfClients);
        if (clientHeap == nullptr)
        {
            throw CriticalException("realloc(clientHeap, "
               + std::to_string(sizeof(Client) * maxNumberOfClients)
                + ") failed. " + strerror(errno) + ".");
        }

    }

    for (uint64_t i = this->maxNumberOfClients; i < maxNumberOfClients; ++i)
    {
        clientHeap[i].allocated = false;
    }
    allocatedList = nullptr;
    unallocatedList = nullptr;
    for (uint64_t i = 0; i < maxNumberOfClients; ++i)
    {
        Client &client = clientHeap[i];
        if (client.allocated)
        {
            epoll_event event;

            event.events = EPOLLIN;
            event.data.ptr = &client;
            if (epoll_ctl(epollfd, EPOLL_CTL_MOD, client.sock, &event) == -1)
            {
                throw std::runtime_error("epoll_ctl(" + std::to_string(epollfd)
                    + ", EPOLL_CTL_MOD, " + std::to_string(client.sock)
                    + "&event) failed. " + strerror(errno) + ".");
            }

            client.next = allocatedList;
            allocatedList = &client;
            if (client.next != nullptr)
            {
                client.next->prev = &client;
            }
        }
        else
        {
            client.next = unallocatedList;
            unallocatedList = &client;
            if (client.next != nullptr)
            {
                client.next->prev = &client;
            }
        }
    }

    if (unallocatedList != nullptr)
    {
        unallocatedList->prev = nullptr;
    }
    if (allocatedList != nullptr)
    {
        allocatedList->prev = nullptr;
    }
    this->maxNumberOfClients = maxNumberOfClients;
}

void awsim::Server::Worker::remove_client(Client *client)
{
    #ifdef AWSIM_DEBUG
        syslog(LOG_DEBUG, "(Worker %" PRIu64 ") Removing client.", id);
    #endif

    if (client->prev != nullptr)
    {
        client->prev->next = client->next;
    }
    if (client->next != nullptr)
    {
        client->next->prev = client->prev;
    }
    client->prev = nullptr;
    client->next = unallocatedList;
    unallocatedList = client;
    client->allocated = false;
    _numberOfClients--;
    close(client->sock);
}

static void remove_fd_from_epoll(int fd, int epollfd)
{
    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr) == -1)
    {
        throw std::runtime_error("epoll_ctl(" + std::to_string(epollfd)
            + ", EPOLL_CTL_DEL, " + std::to_string(fd) + ", nullptr) failed. "
            + strerror(errno) + ".");
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

void awsim::Server::Worker::remove_remote_host_sockets()
{
    try
    {
        remove_fd_from_epoll(Server::httpSocket, epollfd);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(
            std::string("Failed to remove HTTP socket from epoll. ")
            + ex.what());
    }
}

void awsim::Server::Worker::request_stop()
{
    uint16_t flag = (uint16_t)ServerToWorkerFlags::StopRequest;
    try
    {
        write_to_pipe(&flag, sizeof(flag));
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(
            std::string("Failed to request worker to stop. ") + ex.what());
    }
}

void awsim::Server::Worker::request_weak_stop()
{
    uint16_t flag = (uint16_t)ServerToWorkerFlags::WeakStopRequest;
    try
    {
        write_to_pipe(&flag, sizeof(flag));
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(
            std::string("Failed to request worker to weak stop. ")
            + ex.what());
    }
}

void awsim::Server::Worker::routine_loop()
{
    epoll_event events[NUMBER_OF_EPOLL_EVENTS];

    state = State::Running;
    while (IS_IN_ACTIVE_STATE(state))
    {
        int nfds = epoll_wait(epollfd, events, NUMBER_OF_EPOLL_EVENTS, -1);

        if (nfds == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            throw std::runtime_error("epoll_wait(" + std::to_string(epollfd)
                + ", events, "
                + std::to_string(NUMBER_OF_EPOLL_EVENTS) + ", -1) failed. "
                + strerror(errno) + "." + std::to_string(errno));
        }
        for (int i = 0; IS_IN_ACTIVE_STATE(state) && i < nfds; ++i)
        {
            epoll_event &event = events[i];

            if (event.data.ptr == HTTP_REMOTE_HOST_PTR)
            {
                try
                {
                    accept_http_client();
                }
                catch (const CriticalException &ex)
                {
                    throw std::runtime_error(
                        std::string("Failed to accept HTTP client. ")
                        + ex.what());
                }
                catch (const std::exception &ex)
                {
                    syslog(LOG_ERR,
                        "(Worker %" PRIu64 ") Failed to accept HTTP client. %s",
                        id, ex.what());
                }
            }
            else if (event.data.ptr == SERVER_PIPE_PTR)
            {
                handle_server_pipe();
            }
            else
            {
                Client *client = (Client*)event.data.ptr;
                try
                {
                    handle_client(client);
                }
                catch (const std::exception &ex)
                {
                    remove_client(client);
                    syslog(LOG_ERR,
                        "(Worker %" PRIu64 ") Failed to handle client. %s", id,
                        ex.what());
                }
            }
        }
    }
    if (state == State::WeakStopPending)
    {
        uint16_t flag = (uint16_t)WorkerToServerFlags::WeakStopCompleted;

        #ifdef AWSIM_DEBUG
            syslog(LOG_DEBUG, "(Worker %" PRIu64 ") Weak stop completed.", id);
        #endif
        try
        {
            Server::write_to_pipe(&flag, sizeof(flag));
        }
        catch (const std::exception &ex)
        {
            throw std::runtime_error(
                std::string("Failed to notify main thread of weak stop. ")
                + ex.what());
        }
    }
}

void awsim::Server::Worker::routine_start(Worker &worker)
{
    try
    {
        create_pipe(&worker.serverReadfd, &worker.serverWritefd);
    }
    catch (const std::exception &ex)
    {
        worker.state = State::Crashed;
        syslog(LOG_ALERT,
            "(Worker %" PRIu64 ") Crashed. Failed to create a pipe. %s",
            worker.id, ex.what());
        return;
    }

    try
    {
        worker.epollfd = ::create_epoll(Server::httpSocket,
            worker.serverReadfd);
    }
    catch (const std::exception &ex)
    {
        worker.state = State::Crashed;
        syslog(LOG_ALERT,
            "(Worker %" PRIu64 ") Crashed. Failed to setup an epoll. %s",
            worker.id, ex.what());
        return;
    }

    worker.allocatedList = nullptr;
    worker.unallocatedList = nullptr;
    worker.maxNumberOfClients = 0;
    worker.clientHeap = nullptr;

    try
    {
        worker.increase_max_number_of_clients(INITIAL_MAX_NUMBER_OF_CLIENTS);
    }
    catch (const std::exception &ex)
    {
        worker.state = State::Crashed;
        syslog(LOG_ALERT, "(Worker %" PRIu64 ") Crashed. Failed increase max "
            "number of clients. %s", worker.id, ex.what());
        return;
    }

    http_parser_init(&worker.httpParser, HTTP_REQUEST);

    try
    {
        worker.routine_loop();
    }
    catch (const std::exception &ex)
    {
        worker.state = State::Crashed;
        syslog(LOG_ALERT, "(Worker %" PRIu64 ") Crashed. %s", worker.id,
            ex.what());
    }
}

void awsim::Server::Worker::stop()
{
    #ifdef AWSIM_DEBUG
        syslog(LOG_DEBUG, "(Worker %" PRIu64 ") Stopping.", id);
    #endif
    Client *client;

    client = allocatedList;
    while (client != nullptr)
    {
        client->~Client();
        client = client->next;
    }
    if (clientHeap != nullptr)
    {
        free(clientHeap);
    }
    allocatedList = nullptr;
    unallocatedList = nullptr;
    _numberOfClients = 0;

    close(epollfd);
    close(serverWritefd);
    close(serverReadfd);

    state = State::Stopped;
}

void awsim::Server::Worker::weak_stop()
{
    #ifdef AWSIM_DEBUG
        syslog(LOG_DEBUG, "(Worker %" PRIu64 ") Weak stopping.", id);
    #endif
    remove_remote_host_sockets();
    if (_numberOfClients == 0)
    {
        state = State::WeakStopped;
    }
    else
    {
        state = State::WeakStopPending;
    }
}

awsim::Server::Worker::Worker(uint64_t id) :
    numberOfClients(0),
    id(id), // order matters, this should be set before the thread begins
    thread(&routine_start, std::ref(*this))
{
    #ifdef AWSIM_DEBUG
        syslog(LOG_DEBUG, "(Worker %" PRIu64 ") Starting.", id);
    #endif
}

awsim::Server::Worker::~Worker()
{
    #ifdef AWSIM_DEBUG
        syslog(LOG_DEBUG, "(Worker %" PRIu64 ") Ending.", id);
    #endif
    request_stop();
    thread.join();
}

void awsim::Server::Worker::write_to_pipe(void *buffer, size_t length)
{
    if (write(serverWritefd, buffer, length) == -1)
    {
        throw std::runtime_error("write(" + std::to_string(serverWritefd)
            + ", buffer, " + std::to_string(length) + ") failed. "
            + strerror(errno) + ".");
    }
}
