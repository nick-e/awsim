#include "Domain.h"

static void* load_symbol(void *lib, const std::string &name);
static std::string next_token(const std::string &str, size_t start,
    char delimiter, size_t *end);
static void* open_dynamic_library(const std::string &name);

awsim::Domain::Domain(const Config::Domain &domain) :
    name(domain.name),
    rootDirectory(domain.rootDirectory)
{
    for (const std::string &dynamicPage : domain.dynamicPages)
    {
        void *lib;

        try
        {
            lib = open_dynamic_library(dynamicPage);
        }
        catch (const std::exception &ex)
        {
            throw std::runtime_error("Failed to load dynamic library \""
                + dynamicPage + "\" -> " + ex.what());
        }

        try
        {
            insert_dynamic_page(lib);
        }
        catch (const std::exception &ex)
        {
            throw std::runtime_error("Failed to insert dynamic library \""
                + dynamicPage + "\" -> " + ex.what());
        }
    }
}

void awsim::Domain::insert_dynamic_page(void *lib)
{
    std::string (*get_url)();
    std::string url;
    DynamicPageMap *map;
    size_t start = 0;
    DynamicPage dynamicPage;

    try
    {
        get_url = (std::string (*)())load_symbol(lib, "get_url");
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(
            std::string("Could not load function \"get_url\" -> ") + ex.what());
    }

    try
    {
        dynamicPage = (DynamicPage)dlsym(lib, "process");
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(
            std::string("Could not load function \"process\" -> ") + ex.what());
    }

    url = get_url();
    map = &dynamicPages;
    while (start < url.length())
    {
        std::string token = next_token(url, start, '/', &start);
        DynamicPageMap::iterator it = map->find(token);
        if (it == map->end())
        {
            std::pair<DynamicPageMap::iterator, bool> pair = map->emplace(
                std::piecewise_construct, std::forward_as_tuple(token),
                std::forward_as_tuple(nullptr, nullptr));
            it = pair.first;
        }

        if (start < url.length())
        {
            if (it->second.nextMap == nullptr)
            {
                it->second.nextMap = new DynamicPageMap();
            }
            map = it->second.nextMap;
        }
        else
        {
            it->second.dynamicPage = dynamicPage;
            it->second.lib = lib;
            break;
        }
    }
}

awsim::DynamicPage awsim::Domain::get_dynamic_page(const std::string &url) const
{
    const DynamicPageMap *map = &dynamicPages;
    size_t start = 0;
    while (start < url.length())
    {
        std::string token = next_token(url, start, '/', &start);
        const auto it = map->find(token);
        if (it == map->end())
        {
            break;
        }
        const Triple &triple = it->second;
        if (triple.nextMap == nullptr)
        {
            break;
        }
        if (triple.dynamicPage != nullptr)
        {
            return triple.dynamicPage;
        }
        map = triple.nextMap;
    }
    return nullptr;
}

static void* load_symbol(void *lib, const std::string &name)
{
    void *sym;

    sym = dlsym(lib, name.c_str());
    if (sym == nullptr)
    {
        throw std::runtime_error("dlsym(lib, \"" + name + "\") failed -> "
            + strerror(errno));
    }

    return sym;
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

static void* open_dynamic_library(const std::string &name)
{
    void *lib;

    lib = dlopen(name.c_str(), RTLD_NOW);
    if (lib == nullptr)
    {
        throw std::runtime_error(std::string("dlopen(\"" + name
            + "\", RTLD_NOW) failed -> ") + strerror(errno));
    }

    return lib;
}

awsim::Domain::Triple::Triple(DynamicPage dynamicPage, void *lib) :
    dynamicPage(dynamicPage),
    lib(lib),
    nextMap(nullptr)
{

}

awsim::Domain::Triple::~Triple()
{
    dlclose(lib);
    if (nextMap != nullptr)
    {
        delete nextMap;
    }
}
