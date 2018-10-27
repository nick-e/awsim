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


        Domain(const Config::Domain &domain);
        ~Domain();

        DynamicPage get_dynamic_page(const std::string &url) const;
        void get_resource(std::string url, Resource &destination) const;
        void send_403(HttpRequest *request, Client *client) const;
        void send_404(HttpRequest *request, Client *client) const;

    private:
        DynamicPages dynamicPages;
        int directoryFd;
        Resource statusCode403Resource;
        Resource statusCode404Resource;
    };
}

#endif
