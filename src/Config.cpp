#include "Config.h"

static void create_config_file(const std::string &configFilePath);
static FILE* get_config_file(const std::string &filePath);
static void json_check_existance(rapidjson::Document &document,
    const std::string name);
static void json_check_existance(rapidjson::Value &value,
    const std::string name);
static bool json_get_bool(rapidjson::Value &value);
static double json_get_double(rapidjson::Value &value);
static const char* json_get_string(rapidjson::Value &value);
static void json_get_string_array(rapidjson::Value &parent,
    const std::string &name, std::vector<std::string> &dst);
static uint16_t json_get_uint16(rapidjson::Value &value);
static uint64_t json_get_uint64(rapidjson::Value &value);
//static std::string json_value_to_string(const rapidjson::Value &value);
static void parse_domains(rapidjson::Document &document,
    std::vector<awsim::Config::Domain> &domains);
static void parse_json_file(FILE *fp, rapidjson::Document &document, char *buf,
    size_t buflen);

awsim::Config::Config(const std::string &filePath)
{
    FILE *fp;;
    char buf[2048];
    rapidjson::Document document;

    try
    {
        fp = get_config_file(filePath);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error("Failed to get config file \"" + filePath
            + "\"-> " + ex.what());
    }

    try
    {
        parse_json_file(fp, document, buf, sizeof(buf));
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error("Failed to parse config file at \""
            + filePath + "\"-> " + ex.what());
    }

    try
    {
        json_check_existance(document, "console socket path");
        consoleSocketPath = json_get_string(document["console socket path"]);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(std::string("Failed to get \"console socket "
            "path\" from config file -> ") + ex.what());
    }

    try
    {
        parse_domains(document, this->domains);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(
            std::string("Failed to parse domains from config file -> ")
            + ex.what());
    }

    try
    {
        json_check_existance(document, "http port");
        httpPort = json_get_uint16(document["http port"]);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(
            std::string("Failed to get \"http port\" from config file -> ")
            + ex.what());
    }

    try
    {
        json_check_existance(document, "https port");
        httpsPort = json_get_uint16(document["https port"]);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(
            std::string("Failed to get \"https port\" from config file -> ")
            + ex.what());
    }

    try
    {
        json_check_existance(document, "localhost domain name");
        localhostDomainName = json_get_string(
            document["localhost domain name"]);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(std::string("Failed to get \"localhost "
            "domain name\" from config file -> ") + ex.what());
    }

    try
    {
        json_check_existance(document, "dynamic number of workers");
        dynamicNumberOfWorkers = json_get_bool(
            document["dynamic number of workers"]);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(std::string("Failed to get \"dynamic number "
            "of workers\" from config file -> ") + ex.what());
    }

    try
    {
        json_check_existance(document, "minimum size of large files");
        minimumSizeOfLargeFiles = json_get_uint64(
            document["minimum size of large files"]);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(std::string("Failed to get \"minimum size of "
            "large files\" from config file -> ") + ex.what());
    }

    try
    {
        json_check_existance(document, "percent of cores as workers");
        percentOfCoresForWorkers = json_get_double(
            document["percent of cores as workers"]);
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error(std::string("Failed to get \"percent of cores "
            "as workers\" from config file -> ") + ex.what());
    }
    if (percentOfCoresForWorkers <= 0)
    {
        throw std::runtime_error("\"static number of workers\" must be a "
            "number greater than 0, currently set to \""
            + std::to_string(numberOfWorkers) + "\"");
    }

    try
    {
        json_check_existance(document, "static number of workers");
        staticNumberOfWorkers = json_get_uint64(
            document["static number of workers"]);
    }
        catch (const std::exception &ex)
    {
        throw std::runtime_error(std::string("Failed to get \"static number of "
            "workers\" from config file -> ") + ex.what());
    }
    if (staticNumberOfWorkers < 1)
    {
        throw std::runtime_error("\"static number of workers\" must be an "
            "integer greater than or equal to 1, currently set to \""
            + std::to_string(numberOfWorkers) + "\"");
    }

    fclose(fp);
}

static void create_config_file(const std::string &configFilePath)
{
    std::ofstream stream(configFilePath);

    if (!stream)
    {
        throw std::runtime_error(std::string("Failed to create ofstream. ")
            + strerror(errno));
    }

    stream << "{" << std::endl
        << "   \"console socket path\": \""
            << AWSIM_DEFAULT_CONSOLE_SOCKET_PATH << "\", " << std::endl
        << "   \"domains\": [" << std::endl
        << "      {" << std::endl
        << "         \"dynamic pages\": []," << std::endl
        << "         \"name\": \"\"," << std::endl
        << "         \"root directory\": \"\"" << std::endl
        << "      }" << std::endl
        << "   ]," << std::endl
        << "   \"http port\": " << AWSIM_DEFAULT_HTTP_PORT_NUMBER << ", "
            << std::endl
        << "   \"https port\": " << AWSIM_DEFAULT_HTTPS_PORT_NUMBER << ", "
            << std::endl
        << "   \"dynamic number of workers\": "
            << (AWSIM_DEFAULT_DYNAMIC_NUMBER_OF_WORKERS ? "true" : "false")
            << "," << std::endl
        << "   \"localhost domain name\": ," << std::endl
        << "   \"minimum size of large files\": "
            << AWSIM_DEFAULT_MINIMUM_SIZE_OF_LARGE_FILES << ", " << std::endl
        << "   \"percent of cores as workers\": "
            << AWSIM_DEFAULT_PERCENT_OF_CORES_AS_WORKERS << "," << std::endl
        << "   \"static number of workers\": "
            << AWSIM_DEFAULT_STATIC_NUMBER_OF_WORKERS << std::endl
        << "}" << std::endl;
    stream.close();
}

static FILE* get_config_file(const std::string &filePath)
{
    FILE *fp;
    if (access(filePath.c_str(), F_OK) == -1)
    {
        syslog(LOG_NOTICE,
            "No config file exists at \"%s\", creating a new one at \"%s\"",
            filePath.c_str(), filePath.c_str());
        try
        {
            create_config_file(filePath);
        }
        catch (const std::exception &ex)
        {
            throw std::runtime_error("Failed to create a config file at \""
                + filePath + "\" -> " + ex.what() + ".");
        }
    }
    fp = fopen(filePath.c_str(), "r");
    if (fp == nullptr)
    {
        throw std::runtime_error("fopen(\"" + filePath
            + "\", \"r\") failed -> " + strerror(errno));
    }

    return fp;
}

awsim::Config::Domain::Domain(const std::vector<std::string> &dynamicPages,
    const std::string &name, const std::string &rootDirectory) :
    dynamicPages(dynamicPages),
    name(name),
    rootDirectory(rootDirectory)
{

}

static void json_check_existance(rapidjson::Document &document,
    const std::string name)
{
    if (!document.HasMember(name.c_str()))
    {
        throw std::runtime_error("\"" + name + "\" does not exist");
    }
}

static void json_check_existance(rapidjson::Value &value,
    const std::string name)
{
    if (!value.HasMember(name.c_str()))
    {
        throw std::runtime_error("\"" + name + "\" does not exist");
    }
}

static bool json_get_bool(rapidjson::Value &value)
{
    if (value.IsNull())
    {
        throw std::runtime_error("Value not found");
    }

    if (!value.IsBool())
    {
        throw std::runtime_error("Value not a bool");
    }

    return value.GetBool();
}

static double json_get_double(rapidjson::Value &value)
{
    if (value.IsNull())
    {
        throw std::runtime_error("Value not found");
    }

    if (!value.IsNumber())
    {
        throw std::runtime_error("Value not a number");
    }

    return value.GetDouble();
}

static const char* json_get_string(rapidjson::Value &value)
{
    if (value.IsNull())
    {
        throw std::runtime_error("Value not found");
    }

    if (!value.IsString())
    {
        throw std::runtime_error("Value not a string");
    }

    return value.GetString();
}

static void json_get_string_array(rapidjson::Value &parent,
    const std::string &name, std::vector<std::string> &dst)
{
    json_check_existance(parent, name);

    rapidjson::Value &tmp = parent[name.c_str()];
    if (!tmp.IsArray())
    {
        throw std::runtime_error("\"" + name + "\" is not an array");
    }
    size_t index = 0;
    for (auto it = tmp.Begin(); it != tmp.End(); ++it, ++index)
    {
        try
        {
            dst.push_back(json_get_string(*it));
        }
        catch (const std::exception &ex)
        {
            throw std::runtime_error("Failed to get string at index "
                + std::to_string(index) + " -> " + ex.what());
        }
    }
}

static uint16_t json_get_uint16(rapidjson::Value &value)
{
    uint64_t tmp;

    if (value.IsNull())
    {
        throw std::runtime_error("Value not found");
    }

    if (!value.IsUint())
    {
        throw std::runtime_error("Value not an unsigned 16 bit integer");
    }

    tmp = value.GetUint();
    if (tmp > 65535)
    {
        throw std::runtime_error("Value not an unsigned 16 bit integer");
    }

    return (uint16_t)tmp;
}

static uint64_t json_get_uint64(rapidjson::Value &value)
{

    if (value.IsNull())
    {
        throw std::runtime_error("Value not found");
    }

    if (!value.IsUint())
    {
        throw std::runtime_error("Value not an unsigned 64 bit integer");
    }

    return value.GetUint();
}

/*static std::string json_value_to_string(const rapidjson::Value &value)
{
    if (value.IsBool())
    {
        return std::to_string(value.GetBool());
    }
    if (value.IsDouble())
    {
        return std::to_string(value.GetDouble());
    }
    if (value.IsFloat())
    {
        return std::to_string(value.GetFloat());
    }
    if (value.IsInt())
    {
        return std::to_string(value.GetInt());
    }
    if (value.IsInt64())
    {
        return std::to_string(value.GetInt64());
    }
    if (value.IsNull())
    {
        return "NULL";
    }
    if (value.IsString())
    {
        return value.GetString();
    }
    if (value.IsUint())
    {
        return std::to_string(value.GetUint());
    }
    if (value.IsUint64())
    {
        return std::to_string(value.GetUint64());
    }
    if (value.IsArray())
    {
        return "array";
    }
    return "empty";
}*/

static void parse_domains(rapidjson::Document &document,
    std::vector<awsim::Config::Domain> &domains)
{
    json_check_existance(document, "domains");

    rapidjson::Value &tmp = document["domains"];
    if (!tmp.IsArray())
    {
        throw std::runtime_error("\"domains\" is not an array");
    }
    size_t index = 0;
    for (auto it = tmp.Begin(); it != tmp.End(); ++it, ++index)
    {
        std::string name;
        std::string rootDirectory;
        std::vector<std::string> dynamicPages;
        rapidjson::Value &domain = *it;

        try
        {
            json_check_existance(domain, "name");
            name = json_get_string(domain["name"]);
        }
        catch (const std::exception &ex)
        {
            throw std::runtime_error("Failed to get \"name\" for domain "
                + std::to_string(index + 1) + " -> " + ex.what());
        }

        try
        {
            json_check_existance(domain, "root directory");
            rootDirectory = json_get_string(domain["root directory"]);
        }
        catch (const std::exception &ex)
        {
            throw std::runtime_error(
                "Failed to get \"root directory\" for domain "
                + std::to_string(index + 1) + " -> " + ex.what());
        }

        try
        {
            json_get_string_array(domain, "dynamic pages", dynamicPages);
        }
        catch (const std::exception &ex)
        {
            throw std::runtime_error(
                "Failed to get \"dynamic pages\" for domain "
                + std::to_string(index + 1) + " -> " + ex.what());
        }

        domains.emplace_back(dynamicPages, name, rootDirectory);
    }
}

static void parse_json_file(FILE *fp, rapidjson::Document &document, char *buf,
    size_t buflen)
{
    rapidjson::ParseResult parseResult;

    rapidjson::FileReadStream stream(fp, buf, buflen);
    parseResult = document.ParseStream(stream);
    if (!parseResult)
    {
        throw std::runtime_error(std::string("ParseStream(stream) failed -> ")
            + rapidjson::GetParseError_En(parseResult.Code()));
    }
}
