#include "args.hpp"

namespace Args {
    Parser& Parser::add_flag_parameter(std::string flag_name) {
        this->parsers[flag_name] = parse<bool>;
        this->arguments[flag_name] = false;
        return *this;
    }

    std::vector<std::string>
    Parser::parse_program_arguments(int argc, char** argv) {
        const auto arguments = std::vector<std::string_view>(argv + 1, argv + argc);
        const auto result = parse_arguments(arguments);
        if (!result) {
            const auto errors = result.error();
            for (const auto& error : errors) {
                std::cerr << error << std::endl;
            }
            std::exit(1);
        }
        return *result;
    }

    std::expected<std::vector<std::string>, std::vector<std::string>>
    Parser::parse_arguments(const std::vector<std::string_view>& arguments) {
        std::vector<std::string> positional_arguments;
        std::vector<std::string> errors;
        for (const auto& arg : arguments) {
            // TODO: Make sure user isn't passing an parameter that takes a value as a flag and
            // vice versa
            if (arg.starts_with("--")) {
                const auto arg_no_dashes = arg.substr(2);
                const auto equal_sign_location = arg_no_dashes.find("=");
                if (equal_sign_location == std::string::npos) {
                    const std::string parameter_name = std::string(arg_no_dashes);
                    if (parsers.contains(parameter_name)) {
                        this->arguments[parameter_name] = true;
                    } else {
                        errors.push_back("Found unrecognized flag parameter: --" + parameter_name);
                    }
                } else {
                    const auto parameter_name = std::string(arg_no_dashes.substr(0, equal_sign_location));
                    const auto argument = std::string(arg_no_dashes.substr(equal_sign_location + 1));
                    const auto result = attempt_parse(parameter_name, argument);
                    if (!result) {
                        errors.push_back(result.error());
                    }
                }
            } else {
                positional_arguments.push_back(std::string(arg));
            }
        }
        const auto result = check_for_missing_parameters();
        if (!result) {
            const auto& missing_errors = result.error();
            errors.reserve(errors.size() + result.error().size());
            errors.insert(errors.end(), missing_errors.begin(), missing_errors.end());
        }
        if (!errors.empty()) {
            return std::unexpected(errors);
        }
        return positional_arguments;
    }

    std::expected<void, std::string>
    Parser::attempt_parse(std::string parameter_name, std::string input) {
        if (!this->parsers.contains(parameter_name)) {
            return std::unexpected(
                std::format("Found unexpected parameter \"--{}\"", parameter_name)
            );
        }
        ParseFunction f = this->parsers[parameter_name];
        const auto result = f(input);
        if (!result) {
            return std::unexpected(
                std::format("For parameter --{}: {}", parameter_name, result.error())
            );
        }
        std::any boxed = *result;
        this->arguments[parameter_name] = boxed;
        return {};
    }

    std::expected<void, std::vector<std::string>>
    Parser::check_for_missing_parameters() {
        std::vector<std::string> errors;
        for (const auto& key : std::views::keys(this->parsers)) {
            if (!this->arguments.contains(key)) {
                errors.push_back(
                    std::format("Parameter --{} is required but was not provided", key)
                );
            }
        }
        if (!errors.empty()) {
            return std::unexpected(errors);
        }
        return {};
    }

    bool Parser::is_flag_set(std::string flag_name) {
        return this->get<bool>(flag_name);
    }
}
