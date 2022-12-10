#include <config.h>
#include <iostream>

#ifdef WIN32
#include <shlwapi.h>
bool globMatch(const char* pattern, const char* s) {
    return PathMatchSpecA(s, pattern);
}
#else
#include <fnmatch.h>
bool globMatch(const char* pattern, const char* s) {
    return fnmatch(pattern, s, 0) == 0;
}
#endif

std::optional<std::reference_wrapper<const RomDestinationRule>> Config::FindRuleForRom(const std::string& rom_name)
{
    for (const auto& rule : rom_destination_rules)
    {
        for (const std::string& pattern : rule.name_patterns)
        {
            if (globMatch(pattern.c_str(), rom_name.c_str()))
            {
                return rule;
            }
        }
    }
    return {};
}
std::optional<Config> LoadConfig(const std::string& path)
{
    std::cout << "Loading config file at " << path << std::endl;
    Config config;
    try {

        YAML::Node y = YAML::LoadFile(path);
        auto default_dir = y["default_rom_directory"];
        if (default_dir) {
            config.default_rom_directory = default_dir.as<std::string>();
        }
        auto r = y["rom_destination_rules"];
        
        for (auto rule_yaml : y["rom_destination_rules"]) {
            config.rom_destination_rules.push_back({});
            RomDestinationRule& rule = config.rom_destination_rules.back();
            rule.name = rule_yaml.first.as<std::string>();
            for (auto name_pattern_yaml : rule_yaml.second["name_patterns"]) {
                rule.name_patterns.push_back(name_pattern_yaml.as<std::string>());
            }
            for (auto name_pattern_yaml : rule_yaml.second["destinations"]) {
                if (!name_pattern_yaml["name"])
                {
                    throw YAML::Exception(name_pattern_yaml.Mark(), "Missing 'name' for destination");
                }
                if (!name_pattern_yaml["name"])
                {
                    throw YAML::Exception(name_pattern_yaml.Mark(), "Missing 'path' for destination");
                }
                rule.destinations.push_back({
                    .config_name = name_pattern_yaml["name"].as<std::string>(),
                    .path = name_pattern_yaml["path"].as<std::string>(),
                    .rom_name = name_pattern_yaml["rom_name"].as<std::string>(""),
                });
            }
        }
        return config;
    }
    catch (YAML::BadFile e)
    {
        std::cout << "No file found, continuing with defaults" << std::endl;
    }

    return {};
}