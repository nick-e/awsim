#ifndef AWSIM_DOMAIN_H
#define AWSIM_DOMAIN_H

#include <dlfcn.h>
#include <string>
#include <unordered_map>

#include "HttpRequest.h"
#include "Client.h"
#include "DynamicPage.h"
#include "Config.h"

namespace awsim
{

    class Domain
    {
    public:
        const std::string name;
        const std::string rootDirectory;

        Domain(const std::string &name, const std::string &rootDirectory,
            const Config::Domain &domain);

        DynamicPage get_dynamic_page(const std::string &url);

    private:
        struct Triple;
        typedef std::unordered_map<std::string, Triple*> DynamicPageMap;
        struct Triple
        {
            DynamicPage dynamicPage;
            void *lib;
            DynamicPageMap nextMap;

            Triple(DynamicPage dynamicPage, void *lib);
            ~Triple();
        };

        DynamicPageMap dynamicPages;
    };
}

#endif
