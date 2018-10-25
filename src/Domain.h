#ifndef AWSIM_DOMAIN_H
#define AWSIM_DOMAIN_H

#include <dlfcn.h>
#include <string>
#include <unordered_map>

#include "Client.h"
#include "Config.h"
#include "DynamicPage.h"
#include "DynamicPages.h"
#include "HttpRequest.h"
#include "Resource.h"

namespace awsim
{
    class Domain
    {
    public:
        const std::string name;
        const std::string rootDirectory;
        const Resource *statusCode403Resource;
        const Resource *statusCode404Resource;

        Domain(const Config::Domain &domain);
        ~Domain();

        DynamicPage get_dynamic_page(const std::string &url) const;

    private:
        DynamicPages dynamicPages;
        int directoryFd;
    };
}

#endif
