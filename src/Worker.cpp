#include "Server.h"

#define IS_IN_ACTIVE_STATE(state) ((state) == State::Running \
    || (state) == State::WeakStopPending)
#define SERVER_PIPE_PTR (void*)0x0
#define HTTP_REMOTE_HOST_PTR (void*)0x1
#define HTTPS_REMOTE_HOST_PTR (void*)0x2
#define GET_FLAG(buffer) ((ServerToWorkerFlags)(((uint16_t*)(buffer))[0]))
#define HTTP_HEADER_BUFFER_SIZE 8192

static int create_epoll(int httpSocket, int serverReadfd);
static void create_pipe(int *readfd, int *writefd);
static ssize_t read_fd(int fd, void *buffer, size_t length);
static void remove_fd_from_epoll(int fd, int epollfd);

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
    client->allocated = true;
    client->sock = clientSocket;
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
    ssize_t size;

    #ifdef AWSIM_DEBUG
        syslog(LOG_DEBUG, "(Worker %" PRIu64 ") Handling a client.", id);
    #endif
    size = recv(client->sock, buffer, 1024, 0);
    if (size == -1)
    {
        throw std::runtime_error("recv(" + std::to_string(client->sock)
            + ", buffer, " + std::to_string(sizeof(buffer)) + ", "
            + std::to_string(NO_FLAGS) + ") failed. " + strerror(errno));
    }
    if (size == 0)
    {
        #ifdef AWSIM_DEBUG
            syslog(LOG_DEBUG, "(Worker %" PRIu64 ") Client disconnected.", id);
        #endif
        remove_client(client);
        return;
    }

    syslog(LOG_DEBUG, "%s", buffer);
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
