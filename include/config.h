#include <filesystem>
#include <optional>
#include <vector>
#include <string>
#include <yaml-cpp/yaml.h>

struct RomDestination
{
    std::string config_name;
    std::string path;
    std::string rom_name;
};
struct RomDestinationRule
{
    std::string name;
    std::vector<std::string> name_patterns;
    std::vector<RomDestination> destinations;
};
struct Config
{
    std::string default_rom_directory = ".sni";
    std::vector<RomDestinationRule> rom_destination_rules;

    std::optional<std::reference_wrapper<const RomDestinationRule>> FindRuleForRom(const std::string& rom_name);
};

Config LoadConfig(const std::filesystem::path& path);


std::optional<std::filesystem::path> FindExistingConfigFile(const std::filesystem::path& pwd);