#include <config.h>
#include <iostream>
#include <string_view>
#include <ranges>
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
std::vector<std::string> StringSplit(const std::string& s, const std::string& delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }

    res.push_back(s.substr(pos_start));
    return res;
}

Config LoadConfig(const std::filesystem::path& path)
{
    std::cout << "Loading config file at " << path << std::endl;
    Config config;


    YAML::Node y = YAML::LoadFile(path.string());
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

std::optional<std::filesystem::path> FindExistingConfigFile(const std::filesystem::path& pwd) {
    std::vector<std::filesystem::path> dirs = {
        pwd
    };
#if WIN32
    dirs.push_back(std::filesystem::path(std::getenv("LOCALAPPDATA")) / "snfm");
#else
    const char* xdg_config_dirs = std::getenv("XDG_CONFIG_DIRS");

    if (xdg_config_dirs)
    {
     /*   auto splits =
            std::string_view(xdg_config_dirs) | std::views::split(':')
            | std::views::transform([](auto v) {
            return std::string_view(v.begin(), v.end());
                });*/
        auto splits = StringSplit(xdg_config_dirs, ":");
        for (const auto& d : splits) {
            dirs.push_back(std::filesystem::path(d) / "snfm");
        }
    }
    else
    {
        const char* home = std::getenv("HOME");
        dirs.push_back(std::filesystem::path(home) / ".config" / "snfm");
    }

#endif
    const std::filesystem::path config_name{ "snfm_config.yaml" };
    for (const auto& dir : dirs)
    {
        const std::filesystem::path path = dir / config_name;
        std::cout << "Looking for config at " << path << std::endl;
        if (std::filesystem::exists(path))
        {
            std::cout << "Found" << std::endl;
            return path;
        }
    }
    std::cout << "No config file found, using defaults." << std::endl;
    return {};
}