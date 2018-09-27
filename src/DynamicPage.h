#ifndef AWSIM_DYNAMICPAGE_H
#define AWSIM_DYNAMICPAGE_H

#include "HttpRequest.h"
#include "Client.h"

namespace awsim
{
    typedef void (*DynamicPage) (const HttpRequest*, const Client*);
}

#endif
