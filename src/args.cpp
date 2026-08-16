#include <cstdint>

#include "args.hpp"
#include "match.hpp"

using std::expected;
using std::optional;
using std::string;
using std::string_view;
using std::unexpected;
using std::vector;

using u8 = std::uint8_t;

namespace Args {
    Parser& Parser::add_flag_parameter(const std::string& flag_name, string_view description) {
        // Flag parameters don't have a parser and don't hold a value; instead, "was_provided"
        // defaults to false and gets set to true if the flag was provided
        this->parameters[flag_name] = ProgramParameter {
            nullptr, std::any(), std::string(description), true, false, ""
        };
        return *this;
    }

    vector<string>
    Parser::parse_program_arguments(int argc, char** argv) {
        program_name = argv[0];
        const auto arguments = vector<string_view>(argv + 1, argv + argc);
        auto result = parse_arguments(arguments);
        if (!result) {
            const auto errors = std::move(result).error();
            for (const auto& error : errors) {
                std::cerr << error << std::endl;
            }
            std::exit(1);
        }
        return std::move(result).value();
    }

    /// @brief Represent a token that looks like a positional argument (no leading `--`)
    struct PositionalArgument {
        string value;
    };

    /// @brief Represent a token that looks like a flag parameter (leading `--` with no value)
    struct Flag {
        string name;
    };

    /// @brief Represent a token that looks like a parameter with a value (`--param=value`)
    struct ParameterWithValue {
        string name;
        string value;
    };

    /// @brief Define a type that can hold any of the above three types
    using Token = std::variant<PositionalArgument, Flag, ParameterWithValue>;

    /**
     * @brief Given a raw program argument, extract a `Token` from it based on what symbols the
     * argument contains
     * 
     * @param argument The raw program argument passed by the end user
     * @return Token A `PositionalArgument` if the argument did not have a leading `--`, a `Flag` if
     * the argument had a leading `--` but did not contain an equal sign, or a `ParameterWithValue`
     * if the argument both had a leading `--` and an equal sign
     */
    Token extract_token_from_program_argument(string_view argument) {
        if (argument.starts_with("--")) {
            const auto dashes_removed = argument.substr(2);
            const auto equal_sign_location = dashes_removed.find("=");
            bool contains_equal_sign = equal_sign_location != string::npos;
            if (contains_equal_sign) {
                const auto parameter_name = dashes_removed.substr(0, equal_sign_location);
                const auto argument = dashes_removed.substr(equal_sign_location + 1);
                return ParameterWithValue {
                    string(parameter_name),
                    string(argument)
                };
            } else {
                return Flag { string(dashes_removed) };
            }
        } else {
            return PositionalArgument { string(argument) };
        }
    }

    expected<optional<string>, string>
    Parser::handle_parameter_token_with_value(const string& parameter_name, string_view value) {
        if (is_flag_registered(parameter_name)) {
            return unexpected(
                std::format("--{} is a flag and does not take a value", parameter_name)
            );
        } else if (!is_parameter_registered(parameter_name)) {
            return unexpected(
                std::format("Found unknown parameter: --{}", parameter_name)
            );
        }
        auto result = attempt_parse(parameter_name, value);
        if (!result) {
            return unexpected(std::move(result).error());
        }
        return {};
    }

    expected<optional<string>, string>
    Parser::handle_flag_token(const string& flag_name) {
        if (is_parameter_registered(flag_name)) {
            return unexpected(std::format(
                "Parameter --{} takes a value but no value was provided", flag_name
            ));
        } else if (!is_flag_registered(flag_name)) {
            return unexpected(std::format(
                "Found unknown flag: --{}", flag_name
            ));
        }
        parameters[flag_name].was_provided = true;
        return {};
    }

    expected<optional<string>, string>
    Parser::handle_program_argument(string_view argument) {
        Token token = extract_token_from_program_argument(argument);
        const auto result = std::visit(match {
            [this](const ParameterWithValue& pv) mutable {
                return handle_parameter_token_with_value(pv.name, pv.value);
            },
            [this](const Flag& flag) mutable {
                return handle_flag_token(flag.name);
            },
            [](PositionalArgument& pa) -> expected<optional<string>, string> {
                return std::move(pa.value);
            }
        }, token);
        return result;
    }

    expected<vector<string>, vector<string>>
    Parser::parse_arguments(const vector<string_view>& arguments) {
        vector<string> positional_arguments;
        vector<string> errors;
        for (const auto& arg : arguments) {
            if (arg == "--help" || arg.starts_with("--help=")) {
                print_help();
                std::exit(1);
            }
            auto result = handle_program_argument(arg);
            if (!result) {
                errors.push_back(std::move(result.error()));
                continue;
            }
            // If a positional argument was found, add it to the list
            auto optional_positional_argument = std::move(result).value();
            if (optional_positional_argument.has_value()) {
                positional_arguments.push_back(std::move(optional_positional_argument).value());
            }
        }
        auto result = check_for_missing_parameters();
        if (!result) {
            auto missing_errors = std::move(result).error();
            errors.reserve(errors.size() + missing_errors.size());
            errors.insert(errors.end(),
                          std::make_move_iterator(missing_errors.begin()),
                          std::make_move_iterator(missing_errors.end()));
        }
        if (!errors.empty()) {
            return unexpected(std::move(errors));
        }
        return positional_arguments;
    }

    expected<void, string>
    Parser::attempt_parse(const string& parameter_name, string_view input) {
        if (!this->parameters.contains(parameter_name)) {
            return unexpected(
                std::format("Found unexpected parameter: --{}", parameter_name)
            );
        }
        auto& parameter = parameters[parameter_name];
        ParseFunction f = parameter.parser;
        auto result = f(input);
        if (!result) {
            const auto error_message = std::move(result).error();
            return std::unexpected(
                std::format("For parameter --{}: {}", parameter_name, error_message)
            );
        }
        parameter.value = std::move(result).value();
        parameter.was_provided = true;
        return {};
    }

    expected<void, vector<string>>
    Parser::check_for_missing_parameters() {
        vector<string> errors;
        for (const auto& key : std::views::keys(this->parameters)) {
            auto& parameter = parameters[key];
            if (parameter.is_required && !parameter.was_provided) {
                errors.push_back(
                    std::format("Parameter --{} is required but was not provided", key)
                );
            }
        }
        if (!errors.empty()) {
            return unexpected(std::move(errors));
        }
        return {};
    }

    bool Parser::is_flag_set(const string& flag_name) {
        if (is_parameter_registered(flag_name)) {
            std::cerr << "Developer error: Parameter \"" << flag_name << "\" is not a flag "
                      << "parameter but was accessed as one. Use get instead";
            std::exit(1);
        } else if (!is_flag_registered(flag_name)) {
            std::cerr << "Developer error: This parser is not configured with a flag named \""
                      << flag_name << '"' << std::endl;
            std::exit(1);
        }
        return was_parameter_provided(flag_name);
    }

    bool Parser::does_parameter_have_value(const string& parameter_name) {
        return parameters[parameter_name].value.has_value();
    }

    bool Parser::is_parameter_registered(const string& parameter_name) {
        if (!parameters.contains(parameter_name)) {
            return false;
        }
        const ProgramParameter& param = parameters[parameter_name];
        return !param.is_flag;
    }

    bool Parser::is_flag_registered(const string& flag_name) {
        if (!parameters.contains(flag_name)) {
            return false;
        }
        return parameters[flag_name].is_flag;
    }

    bool Parser::was_parameter_provided(const string& parameter_name) {
        if (!parameters.contains(parameter_name)) {
            std::cerr << "Developer error: This parser is not configured to accept a parameter or "
                         "flag named \"" << parameter_name << '"';
            std::exit(1);
        }
        return parameters[parameter_name].was_provided;
    }

    void Parser::print_help() {
        vector<std::reference_wrapper<const string>> flags;
        vector<std::reference_wrapper<const string>> optional_params;
        vector<std::reference_wrapper<const string>> required_params;
        for (const auto& parameter_name : std::views::keys(parameters)) {
            auto& parameter = parameters[parameter_name];
            if (parameter.is_flag) {
                flags.push_back(std::ref(parameter_name));
            } else if (parameter.is_required) {
                required_params.push_back(std::ref(parameter_name));
            } else {
                optional_params.push_back(std::ref(parameter_name));
            }
        }
        std::cout << "Usage: " << program_name;
        for (const string& name : required_params) {
                const auto& param = parameters[name];
            std::cout << " --" << name << "=<" << param.type_name << '>';
        }
        std::cout << "\n\n";
        if (required_params.size() > 0) {
            std::cout << "Required Parameters:\n\n";
            for (const string& name : required_params) {
                const auto& param = parameters[name];
                std::cout << "  --" << name << "=<" << param.type_name << ">: " << param.description
                          << "\n\n";
            }
        }
        if (optional_params.size() > 0) {
            std::cout << "Optional Parameters:\n\n";
            for (const string& name : optional_params) {
                const auto& param = parameters[name];
                std::cout << "  --" << name << "=<" << param.type_name << ">: " << param.description
                          << "\n\n";
            }
        }
        if (flags.size() > 0) {
            std::cout << "Flags:\n\n";
            for (const string& name : flags) {
                const auto& param = parameters[name];
                std::cout << "  --" << name << ": " << param.description << "\n\n";
            }
        }
    }
}
