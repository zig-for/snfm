#include "shell.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>

#ifndef _WIN32
#include <sys/wait.h>
#endif

std::string ExpandTemplate(const std::string& template_string,
    const std::vector<std::pair<std::string, std::string>>& variables)
{
    std::string result;
    size_t pos = 0;
    while (pos < template_string.size())
    {
        const size_t open = template_string.find('{', pos);
        if (open == std::string::npos)
        {
            result.append(template_string, pos, std::string::npos);
            break;
        }
        const size_t close = template_string.find('}', open);
        if (close == std::string::npos)
        {
            result.append(template_string, pos, std::string::npos);
            break;
        }
        result.append(template_string, pos, open - pos);

        const std::string name = template_string.substr(open + 1, close - open - 1);
        auto variable = std::find_if(variables.begin(), variables.end(),
            [&name](const auto& v) { return v.first == name; });
        if (variable != variables.end())
        {
            result.append(variable->second);
        }
        else
        {
            // Unknown placeholder - leave it alone so it's obvious what went wrong.
            std::cout << "Unknown template variable {" << name << "}" << std::endl;
            result.append(template_string, open, close - open + 1);
        }
        pos = close + 1;
    }
    return result;
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

void RunPostUploadScript(const std::string& script_template,
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
