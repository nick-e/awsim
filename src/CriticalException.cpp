#include "Server.h"

awsim::Server::Worker::CriticalException::CriticalException(
    const std::string &message) :
    std::runtime_error(message)
{

}
