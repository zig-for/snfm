#include "snfm.h"
#include "config.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#ifndef _WIN32
#include <sys/wait.h>
#endif

void do_error(const std::string& e)
{
    std::cout << e << std::endl;
    std::cout << "(PRESS ENTER TO EXIT)" << std::endl;
    std::string s;
    std::getline(std::cin, s);
}


// The values we substitute in come from ROM file names and the config file, so they
// can contain characters the shell treats as syntax.  Escape them so that whatever is
// in them ends up as literal text rather than as extra commands.  This only works if
// the placeholder is left unquoted in the config - see snfm_config_example.yaml.
std::string EscapeForShell(const std::string& value)
{
#ifdef _WIN32
    // cmd.exe has no way to escape a quote once it's inside one, so drop quotes (and
    // newlines) outright - Windows file names can't contain them anyway - and escape
    // everything else cmd would act on with a caret.
    std::string escaped;
    for (const char c : value)
    {
        if (c == '"' || c == '\r' || c == '\n')
        {
            continue;
        }
        if (std::strchr("^&|<>()%", c) != nullptr)
        {
            escaped.push_back('^');
        }
        escaped.push_back(c);
    }
    return escaped;
#else
    // Single quotes protect everything except a single quote itself, which has to be
    // closed, escaped, and reopened.
    std::string escaped = "'";
    for (const char c : value)
    {
        if (c == '\'')
        {
            escaped += "'\\''";
        }
        else
        {
            escaped.push_back(c);
        }
    }
    escaped.push_back('\'');
    return escaped;
#endif
}

void run_post_upload_script(const std::string& script_template,
    const std::vector<std::pair<std::string, std::string>>& variables)
{
    std::vector<std::pair<std::string, std::string>> escaped_variables;
    escaped_variables.reserve(variables.size());
    for (const auto& variable : variables)
    {
        escaped_variables.emplace_back(variable.first, EscapeForShell(variable.second));
    }

    const std::string command = ExpandTemplate(script_template, escaped_variables);
    std::cout << "Running post upload script: " << command << std::endl;

    std::string to_run = command;
#ifdef _WIN32
    // cmd.exe strips the outer quotes off a command that starts with one, which
    // breaks quoted script paths - give it a spare pair to chew on.
    if (!to_run.empty() && to_run.front() == '"')
    {
        to_run = "\"" + to_run + "\"";
    }
#endif
    const int result = std::system(to_run.c_str());
    if (result == -1)
    {
        std::cout << "Couldn't run the post upload script." << std::endl;
    }
    else if (result != 0)
    {
#ifdef _WIN32
        std::cout << "Post upload script exited with code " << result << std::endl;
#else
        // system() hands back a wait status, not the exit code.
        if (WIFEXITED(result))
        {
            std::cout << "Post upload script exited with code " << WEXITSTATUS(result) << std::endl;
        }
        else if (WIFSIGNALED(result))
        {
            std::cout << "Post upload script was killed by signal " << WTERMSIG(result) << std::endl;
        }
        else
        {
            std::cout << "Post upload script failed with status " << result << std::endl;
        }
#endif
    }
}

int main(int argc, char* argv[])
{
    auto maybePath = FindExistingConfigFile(std::filesystem::path(argv[0]).parent_path());
    Config config;
    if (maybePath) {
        try {
            config = LoadConfig(*maybePath);
        }
        catch (YAML::Exception e)
        {
            std::cout << "Error loading config: " << e.what() << std::endl;
            return 1;
        }
    }

    if (argc != 2)
    {
        do_error("A single argument - the name of the file you're trying to put on your SNES - is required");
        return 1;
    }

    std::filesystem::path rom_path = argv[1];
    std::cout << rom_path.string() << std::endl;

    std::string device_directory = config.default_rom_directory;
    std::string destination_rom_name = rom_path.filename().string();
    std::string rule_name;
    std::string destination_name;

    auto maybe_rule = config.FindRuleForRom(rom_path.filename().string());
    if (maybe_rule)
    {
        auto rule = maybe_rule->get();
        rule_name = rule.name;
        if (!rule.destinations.empty())
        {
            RomDestination& destination = rule.destinations[0];
            if (rule.destinations.size() > 1)
            {
                std::cout << "Rule " << rule.name << " has multiple destinations possible, please choose the destination:" << std::endl;

                size_t longest_name = 0;
                for (const auto& destination : rule.destinations)
                {
                    longest_name = std::max(destination.config_name.size(), longest_name);
                }

                const int space_count = std::to_string(rule.destinations.size() + 1).size() + 1;
                for (int i = 0; i < rule.destinations.size(); i++)
                {
                    std::cout << " " << std::right << std::setw(space_count) << std::to_string(i);
                    std::cout << " - " << std::left << std::setw(longest_name) << rule.destinations[i].config_name;
                    std::cout << " - " << rule.destinations[i].path << "/" << rule.destinations[i].rom_name << std::endl;
                }
                while (true) {
                    std::cout << "Please choose your destination (defaults to 0):" << std::endl << "> ";
                    std::string input;
                    std::getline(std::cin, input);

                    if (input.empty())
                    {
                        break;
                    }
                    int pick;
                    try {
                        pick = std::stoi(input.c_str());
                    }
                    catch (std::exception const& ex) {
                        std::cout << "Not a number." << std::endl;
                        continue;
                    }
                    if (pick < 0 || pick >= static_cast<int>(rule.destinations.size()))
                    {
                        std::cout << "Choice must be between 0 and " << std::to_string(rule.destinations.size() - 1) << std::endl;
                        continue;
                    }
                    destination = rule.destinations[pick];
                    break;
                }

            }
            std::cout << "Using " << destination.config_name << std::endl;
            destination_name = destination.config_name;
            device_directory = destination.path;
            if (!destination.rom_name.empty()) {
                destination_rom_name = destination.rom_name;
            }
        }
    }

    if (device_directory.empty() || device_directory[0] != '/')
    {
        device_directory = "/" + device_directory;
    }

    SNIConnection sni;

    auto device_filter =
        [](const DevicesResponse::Device& device) -> bool
    {
        auto caps = device.capabilities();
        return HasCapability(device, DeviceCapability::MakeDirectory) &&
            HasCapability(device, DeviceCapability::PutFile) &&
            HasCapability(device, DeviceCapability::BootFile);
    };
    std::cout << "Refreshing devices..." << std::endl;
    sni.refreshDevices(device_filter);
    auto uri = sni.getFirstDeviceUri();
    if (!uri)
    {
        do_error("Couldn't find any valid devices.");
        return 1;
    }

    sni.makeDirectory(*uri, device_directory, true);

    std::optional<std::filesystem::path> put_path = sni.putFile(*uri, rom_path, device_directory, destination_rom_name);
    if (!put_path)
    {
        do_error("Couldn't put the file, due to some error.");
        return 1;
    }
    sni.bootFile(*uri, *put_path);

    if (!config.post_upload_script.empty())
    {
        run_post_upload_script(config.post_upload_script, {
            {"rule_name", rule_name},
            {"destination_name", destination_name},
            {"destination_path", device_directory},
            {"destination_rom_name", destination_rom_name},
            {"destination_file", put_path->generic_string()},
            {"source_path", rom_path.string()},
            {"source_name", rom_path.filename().string()},
            });
    }

    return 0;
}
