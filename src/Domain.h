#ifndef AWSIM_DOMAIN_H
#define AWSIM_DOMAIN_H

#include <dlfcn.h>
#include <string>
#include <unordered_map>

#include "Client.h"
#include "Config.h"
#include "DynamicPage.h"
#include "HttpRequest.h"

namespace awsim
{

    class Domain
    {
    public:
        const std::string name;
        const std::string rootDirectory;

        Domain(const Config::Domain &domain);

        DynamicPage get_dynamic_page(const std::string &url) const;

    private:
        struct Triple;
        typedef std::unordered_map<std::string, Triple> DynamicPageMap;
        struct Triple
        {
            DynamicPage dynamicPage;
            void *lib;
            DynamicPageMap *nextMap;

            Triple(DynamicPage dynamicPage, void *lib);
            ~Triple();
        };

        DynamicPageMap dynamicPages;

        void insert_dynamic_page(void *lib);
    };
}

#endif
