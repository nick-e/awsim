#include "Domain.h"

static std::string next_token(const std::string &str, size_t start,
    char delimiter, size_t *end);

awsim::Domain::Domain(const std::string &name,
    const std::string &rootDirectory, const Config::Domain &domain) :
    name(name),
    rootDirectory(rootDirectory)
{
    for (const std::string &dynamicPage : domain.dynamicPages)
    {
        void *lib;

        lib = dlopen(dynamicPage.c_str(), RTLD_NOW);
    }
}

awsim::DynamicPage awsim::Domain::get_dynamic_page(const std::string &url)
{
    DynamicPageMap *map = &dynamicPages;
    size_t start = 0;
    while (start < url.length())
    {
        std::string token = next_token(url, start, '/', &start);
        const auto it = map->find(token);
        if (it == map->end())
        {
            break;
        }
        Triple *triple = it->second;
        if (triple == nullptr)
        {
            break;
        }
        if (triple->dynamicPage != nullptr)
        {
            return triple->dynamicPage;
        }
        map = &triple->nextMap;
    }
    return nullptr;
}

static std::string next_token(const std::string &str, size_t start,
    char delimiter, size_t *end)
{
    size_t tmp;

    while (str[start] == delimiter)
    {
        ++start;
    }

    tmp = start + 1;
    while (tmp < str.length() && str[tmp] != delimiter)
    {
        ++tmp;
    }
    *end = tmp;

    return str.substr(start, tmp - start);
}

awsim::Domain::Triple::Triple(DynamicPage dynamicPage, void *lib) :
    dynamicPage(dynamicPage),
    lib(lib)
{

}

awsim::Domain::Triple::~Triple()
{
    dlclose(lib);
}
