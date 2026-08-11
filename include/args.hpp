
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

    template <typename T>
    concept HasStdFromChars = requires(const char* start, const char* end, T& out) {
        {std::from_chars(start, end, out)} -> std::same_as<std::from_chars_result>;
    };

    template <HasStdFromChars T>
    std::expected<T, std::string> from_string(std::string_view s) {
        T out;
        const char* start = s.data();
        const char* end = s.data() + s.size();
        auto [ptr, ec] = std::from_chars(start, end, out);
        bool value_found = ec == std::errc();
        bool entire_string_consumed = ptr == end;
        if (!value_found || !entire_string_consumed) {
            return std::unexpected(std::format(
                "\"{}\" could not be converted to {}", s, demangle(typeid(T).name())
            ));
        }
        return out;
    }

    template <std::constructible_from<std::string_view> T>
    std::expected<T, std::string> from_string(std::string_view s) {
        return T(s);
    }

    template <typename T>
    concept ParsableFromStringTemplate = requires(std::string_view s) {
        { Args::from_string<T>(s) } -> std::same_as<std::expected<T, std::string>>;
    };

    template <typename T>
    concept ParsableFromStringStatic = requires(std::string_view s) {
        { T::from_string(s) } -> std::same_as<std::expected<T, std::string>>;
    };

    template <typename T>
    concept Parsable = ParsableFromStringTemplate<T> || ParsableFromStringStatic<T>;

    using ParseFunction = std::function<std::expected<std::any, std::string>(std::string_view)>;

    template <ParsableFromStringTemplate T>
    std::expected<std::any, std::string> parse(std::string_view s) {
        auto result = from_string<T>(s);
        if (!result) {
            return std::unexpected(std::move(result).error());
        }
        return std::any(std::move(result).value());
    }

    template <ParsableFromStringStatic T>
    std::expected<std::any, std::string> parse(std::string_view s) {
        auto result = T::from_string(s);
        if (!result) {
            return std::unexpected(std::move(result).error());
        }
        return std::any(std::move(result).value());
    }

    class Parser {
    public:

        template <Parsable T>
        Parser& add_required_parameter(const std::string& name) {
            this->parsers[name] = parse<T>;
            this->registered_parameters.insert(name);
            return *this;
        }

        template <Parsable T>
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
            return std::any_cast<T>(this->arguments[parameter_name]);
        }

        std::vector<std::string>
        parse_program_arguments(int argc, char** argv);

        std::expected<std::vector<std::string>, std::vector<std::string>>
        parse_arguments(const std::vector<std::string_view>& arguments);

    private:
        std::expected<void, std::string>
        attempt_parse(const std::string& parameter_name, std::string_view input);

        std::expected<void, std::vector<std::string>>
        check_for_missing_parameters();

        std::expected<std::optional<std::string>, std::string>
        handle_program_argument(std::string_view argument);

        std::expected<std::optional<std::string>, std::string>
        handle_parameter_with_value(const std::string& parameter_name, std::string_view value);

        std::expected<std::optional<std::string>, std::string>
        handle_flag(const std::string& flag_name);

        std::unordered_map<std::string, ParseFunction> parsers;
        std::unordered_map<std::string, std::any> arguments;
        std::unordered_set<std::string> registered_flags;
        std::unordered_set<std::string> registered_parameters;
        std::unordered_set<std::string> set_flags;
    };
}
