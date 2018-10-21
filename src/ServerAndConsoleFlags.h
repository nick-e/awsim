#ifndef AWSIM_SERVERANDCONSOLEFLAGS_H
#define AWSIM_SERVERANDCONSOLEFLAGS_H

#include <string>

namespace awsim
{
    class ServerAndConsoleFlags
    {
    public:
        enum class ToServerFlags
        {
            EndServerRequest = 0x01,
            ChangeNumberOfWorkersRequest = 0x02,
            ChangeRootDirectoryRequest = 0x03,
            RestartServerRequest = 0x04
        };

        enum class ToConsoleFlags
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

        static std::string to_string(ToConsoleFlags flag);

        static std::string to_string(ToServerFlags flag);
    };
}

#endif
