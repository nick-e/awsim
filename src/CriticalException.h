#ifndef AWSIM_CRITICALEXCEPTION_H
#define AWSIM_CRITICALEXCEPTION_H

#include <stdexcept>
#include <string>

namespace awsim
{
    class CriticalException : public std::runtime_error
    {
    public:
        CriticalException(const std::string &message);
    };
}

#endif
