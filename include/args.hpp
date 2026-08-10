
#include <any>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <ranges>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <cxxabi.h>

namespace Args {

    // Anonymous namespace hides this from being accessed
    namespace {
        std::string demangle(const char* mangled_name) {
            int status = 0;
            // abi::__cxa_demangle allocates a raw C-string using std::malloc
            char* demangled = abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status);

            if (status == 0 && demangled != nullptr) {
                // Wrap in a unique_ptr to prevent a dynamic memory leak
                std::unique_ptr<char, void(*)(void*)> cleanup(demangled, std::free);
                return std::string(demangled);
            }
            return mangled_name; // Fallback to raw string if demangling failed
        }
    }

    using ParseFunction = std::function<std::expected<std::any, std::string>(const std::string&)>;

    // Define a template type parameter that limits the types to those who implement the >> operator
    // for std::istream&
    template <typename T>
    concept Parsable = requires(std::istream& is, T& out) {
        { is >> out } -> std::same_as<std::istream&>;
    };

    template <Parsable T>
    std::expected<std::any, std::string> parse(const std::string& s) {
        T value;
        std::istringstream stream(s);
        if ((stream >> value) && stream.eof()) {
            std::any value_as_any = value;
            return value_as_any;
        }
        std::string message = std::format(
            "Value \"{}\" cannot be converted to `{}`", s, demangle(typeid(T).name())
        );
        return std::unexpected(message);
    }

    class Parser {
    public:

        template <typename T>
        Parser& add_required_parameter(const std::string& name) {
            this->parsers[name] = parse<T>;
            this->registered_parameters.insert(name);
            return *this;
        }

        template <typename T>
        Parser& add_optional_parameter(const std::string& name, T default_value) {
            ParseFunction f = parse<T>;
            this->parsers[name] = f;
            this->arguments[name] = std::any(default_value);
            this->registered_parameters.insert(name);
            return *this;
        }

        Parser& add_flag_parameter(const std::string& flag_name);

        bool is_flag_set(const std::string& flag_name);

        template <typename T>
        T get(const std::string& parameter_name) {
            std::any boxed;
            if (!this->arguments.contains(parameter_name)) {
                std::cerr << "Developer error: Program is not configured to accept a parameter "
                          << "named \"" << parameter_name << '"';
                std::exit(1);
            }
            boxed = this->arguments[parameter_name];
            return std::any_cast<T>(boxed);
        }

        std::vector<std::string>
        parse_program_arguments(int argc, char** argv);

        std::expected<std::vector<std::string>, std::vector<std::string>>
        parse_arguments(const std::vector<std::string_view>& arguments);

    private:
        std::expected<void, std::string>
        attempt_parse(const std::string& parameter_name, const std::string& input);

        std::expected<void, std::vector<std::string>>
        check_for_missing_parameters();

        std::expected<std::optional<std::string>, std::string>
        handle_program_argument(std::string_view argument);

        std::expected<std::optional<std::string>, std::string>
        handle_parameter_with_value(const std::string& parameter_name, const std::string& value);

        std::expected<std::optional<std::string>, std::string>
        handle_flag(const std::string& flag_name);

        std::unordered_map<std::string, ParseFunction> parsers;
        std::unordered_map<std::string, std::any> arguments;
        std::unordered_set<std::string> registered_flags;
        std::unordered_set<std::string> registered_parameters;
    };
}
