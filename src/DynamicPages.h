#ifndef AWSIM_DYNAMICPAGES_H
#define AWSIM_DYNAMICPAGES_H

#include <dlfcn.h>
#include <errno.h>
#include <stdexcept>
#include <string>
#include <string.h>
#include <unordered_map>

#include "DynamicPage.h"

namespace awsim
{
    class DynamicPages
    {
    public:
        DynamicPage get_dynamic_page(const std::string &url) const;
        void insert_dynamic_page(const std::string &url,
            const std::string &sharedLibraryFilePath);
    private:
        struct Triple
        {
            DynamicPage dynamicPage;
            void *lib;
            std::unordered_map<std::string, Triple> *nextMap;

            Triple(DynamicPage dynamicPage, void *lib);
            ~Triple();
        };

        std::unordered_map<std::string, Triple> root;
    };
}

#endif
