#include "DynamicPages.h"

static std::string next_token(const std::string &str, size_t start,
    char delimiter, size_t *end);
static void* load_symbol(void *lib, const std::string &name);
static void* open_dynamic_library(const std::string &filePath);

awsim::DynamicPage awsim::DynamicPages::get_dynamic_page(const std::string &url)
    const
{
    const std::unordered_map<std::string, Triple> *map = &root;
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

void awsim::DynamicPages::insert_dynamic_page(const std::string &url,
    const std::string &sharedLibraryFilePath)
{
    std::unordered_map<std::string, Triple> *map;
    size_t start = 0;
    DynamicPage dynamicPage;
    map = &root;
    void *lib;

    try
    {
        lib = open_dynamic_library(sharedLibraryFilePath);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error("Failed to open dynamic library \""
            + sharedLibraryFilePath + "\" -> " + ex.what());
    }

    try
    {
        dynamicPage = (DynamicPage)load_symbol(lib, "process");
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(
            "Failed to open load dynamic page from library \""
            + sharedLibraryFilePath + "\" -> " + ex.what());
    }

    while (start < url.length())
    {
        std::string token = next_token(url, start, '/', &start);
        std::unordered_map<std::string, Triple>::iterator it =
            map->find(token);
        if (it == map->end())
        {
            std::pair<std::unordered_map<std::string, Triple>::iterator, bool>
                pair = map->emplace(std::piecewise_construct,
                std::forward_as_tuple(token),
                std::forward_as_tuple(nullptr, nullptr));
            it = pair.first;
        }

        if (start < url.length())
        {
            if (it->second.nextMap == nullptr)
            {
                it->second.nextMap =
                    new std::unordered_map<std::string, Triple>();
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

static void* open_dynamic_library(const std::string &filePath)
{
    void *lib;

    lib = dlopen(filePath.c_str(), RTLD_NOW);
    if (lib == nullptr)
    {
        throw std::runtime_error(std::string("dlopen(\"" + filePath
            + "\", RTLD_NOW) failed -> ") + strerror(errno));
    }

    return lib;
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

awsim::DynamicPages::Triple::Triple(DynamicPage dynamicPage, void *lib) :
    dynamicPage(dynamicPage),
    lib(lib),
    nextMap(nullptr)
{

}

awsim::DynamicPages::Triple::~Triple()
{
    dlclose(lib);
    if (nextMap != nullptr)
    {
        delete nextMap;
    }
}
