#pragma once

#include <string>
#include <utility>
#include <vector>

// Replaces every "{placeholder}" in template_string with the matching value from
// variables.  Placeholders with no matching variable are left untouched.
std::string ExpandTemplate(const std::string& template_string,
    const std::vector<std::pair<std::string, std::string>>& variables);

// Escapes value so that the shell hands it to the command as literal text instead of
// acting on anything in it.  This only works if the placeholder is left unquoted in
// the config - see snfm_config_example.yaml.
std::string EscapeForShell(const std::string& value);

// Substitutes the (escaped) variables into script_template and runs the result through
// the shell, reporting anything that goes wrong on stdout.
void RunPostUploadScript(const std::string& script_template,
    const std::vector<std::pair<std::string, std::string>>& variables);
