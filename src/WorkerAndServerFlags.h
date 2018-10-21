#ifndef AWSIM_WORKERANDSERVERFLAGS_H
#define AWSIM_WORKERANDSERVERFLAGS_H

#include <string>

namespace awsim
{
    class WorkerAndServerFlags
    {
    public:
        enum class ConsoleToServerFlags
        {
            EndServerRequest = 0x01,
            ChangeNumberOfWorkersRequest = 0x02,
            ChangeRootDirectoryRequest = 0x03,
            RestartServerRequest = 0x04
        };

        enum class ServerToConsoleFlags
        {
            TooManyConsoles = 0x01,
            EndServerSuccess = 0x02,
            EndServerFailed = 0x03,
            ChangeNumberOfWorkersSuccess = 0x04,
            ChangeNumberOfWorkersFailed = 0x05,
            ChangeRootDirectorySuccess = 0x06,
            ChangeRootDirectoryFailed = 0x07,
            RestartServerSuccess = 0x08,
            RestartServerFailed = 0x09
        };

        enum class ToWorkerFlags
        {
            WeakStopRequest = 0x01,
            StopRequest = 0x02,
            NumberOfClientsRequest = 0x03
        };

        enum class ToServerFlags
        {
            NumberOfClients = 0x01,
            WeakStopCompleted = 0x02,
            Crashed = 0x3
        };

        static std::string to_string(ToWorkerFlags flag);

        static std::string to_string(ToServerFlags flag);
    };
}

#endif
