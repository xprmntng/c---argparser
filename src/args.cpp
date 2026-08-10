#include "args.hpp"
#include "match.hpp"

using std::expected;
using std::optional;
using std::string;
using std::string_view;
using std::unexpected;
using std::vector;

namespace Args {
    Parser& Parser::add_flag_parameter(const std::string& flag_name) {
        this->parsers[flag_name] = parse<bool>;
        this->arguments[flag_name] = false;
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

    using Extraction = std::variant<PositionalArgument, Flag, ParameterWithValue>;

    Extraction extract(string_view argument) {
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
    Parser::handle_parameter_with_value(const string& parameter_name, const string& value) {
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
    Parser::handle_flag(const string& flag_name) {
        if (registered_parameters.contains(flag_name)) {
            return unexpected(std::format(
                "Parameter --{} takes a value but no value was provided", flag_name
            ));
        } else if (!registered_flags.contains(flag_name)) {
            return unexpected(std::format(
                "Found unknown flag: --{}", flag_name
            ));
        }
        arguments[flag_name] = true;
        return {};
    }

    expected<optional<string>, string>
    Parser::handle_program_argument(string_view argument) {
        Extraction extraction = extract(argument);
        const auto result = std::visit(match {
            [this](const ParameterWithValue& pv) mutable {
                return handle_parameter_with_value(pv.name, pv.value);
            },
            [this](const Flag& flag) mutable {
                return handle_flag(flag.name);
            },
            [](PositionalArgument& pa) -> expected<optional<string>, string> {
                return std::move(pa.value);
            }
        }, extraction);
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
    Parser::attempt_parse(const string& parameter_name, const string& input) {
        if (!this->parsers.contains(parameter_name)) {
            return unexpected(
                std::format("Found unexpected parameter: --{}", parameter_name)
            );
        }
        ParseFunction f = this->parsers[parameter_name];
        auto result = f(input);
        if (!result) {
            const auto error_message = std::move(result).error();
            return std::unexpected(
                std::format("For parameter --{}: {}", parameter_name, error_message)
            );
        }
        std::any boxed = *result;
        this->arguments[parameter_name] = boxed;
        return {};
    }

    expected<void, vector<string>>
    Parser::check_for_missing_parameters() {
        vector<string> errors;
        for (const auto& key : std::views::keys(this->parsers)) {
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
        return get<bool>(flag_name);
    }
}
