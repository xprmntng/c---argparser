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
        this->parameters[flag_name] = ProgramParameter{ parse<u8>, std::string(description) };
        // Bools are stored as 8-bit unsigned integers, given that the bool type doesn't implement
        // std::from_chars
        this->arguments[flag_name] = (u8)0;
        this->registered_flags.insert(flag_name);
        return *this;
    }

    vector<string>
    Parser::parse_program_arguments(int argc, char** argv) {
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

    struct PositionalArgument {
        string value;
    };

    struct Flag {
        string name;
    };

    struct ParameterWithValue {
        string name;
        string value;
    };

    using Token = std::variant<PositionalArgument, Flag, ParameterWithValue>;

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
        if (registered_flags.contains(parameter_name)) {
            return unexpected(
                std::format("--{} is a flag and does not take a value", parameter_name)
            );
        } else if (!registered_parameters.contains(parameter_name)) {
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
        if (registered_parameters.contains(flag_name)) {
            return unexpected(std::format(
                "Parameter --{} takes a value but no value was provided", flag_name
            ));
        } else if (!registered_flags.contains(flag_name)) {
            return unexpected(std::format(
                "Found unknown flag: --{}", flag_name
            ));
        }
        arguments[flag_name] = (u8)1;
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
        const auto& parameter = parameters[parameter_name];
        ParseFunction f = parameter.parser;
        auto result = f(input);
        if (!result) {
            const auto error_message = std::move(result).error();
            return std::unexpected(
                std::format("For parameter --{}: {}", parameter_name, error_message)
            );
        }
        this->arguments[parameter_name] = std::move(result).value();
        return {};
    }

    expected<void, vector<string>>
    Parser::check_for_missing_parameters() {
        vector<string> errors;
        for (const auto& key : std::views::keys(this->parameters)) {
            if (!this->arguments.contains(key)) {
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
        if (!registered_flags.contains(flag_name)) {
            std::cerr << "Developer error: This parser is not configured with a flag named \""
                      << flag_name << '"' << std::endl;
            std::exit(1);
        }
        return get<u8>(flag_name) != 0;
    }
}
