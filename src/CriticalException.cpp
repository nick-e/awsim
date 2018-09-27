#include "CriticalException.h"

awsim::CriticalException::CriticalException(
    const std::string &message) :
    std::runtime_error(message)
{

}
