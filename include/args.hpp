
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

    /**
     * @brief Get the name of a type
     * 
     * @tparam T The type whose name should be retrieved
     * @return std::string A string containing the demangled name of the type if demangling was
     * successful, otherwise one containing the mangled name
     */
    template <typename T>
    std::string get_type_name() {
        const char* mangled_name = typeid(T).name();
        int status = 0;
        // abi::__cxa_demangle allocates a raw C-string using std::malloc
        char* demangled = abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status);

        if (status == 0 && demangled != nullptr) {
            // Wrap in a unique_ptr to prevent a dynamic memory leak
            std::unique_ptr<char, void(*)(void*)> cleanup(demangled, std::free);
            return std::string(demangled);
        }
        return mangled_name;
    }

    /**
     * @brief Defines a concept which identifies types who can be created using the STL function
     * `std::from_chars`
     * 
     * @tparam T The type to be assessed
     */
    template <typename T>
    concept ImplementsFromChars = requires(const char* start, const char* end, T& out) {
        { std::from_chars(start, end, out) } -> std::same_as<std::from_chars_result>;
    };

    /**
     * @brief Defines a concept which identifies types that have a static member function,
     * `from_string(std::string_view) -> std::expected<T, std::string>`, which can be used to
     * attempting creating a `T` from a `std::string_view` input
     * 
     * @tparam T The type to be assessed
     */
    template <typename T>
    concept TypeImplementsFromString = requires(std::string_view s) {
        { T::from_string(s) } -> std::same_as<std::expected<T, std::string>>;
    };

    /**
     * @brief Defines a concept for types that are parsable. A type is considered parsable if:
     * a) The standard library implements `std::from_chars` for that type OR
     * b) The type has a constructor that takes a single argument: `std::string_view` OR
     * c) The type implements a static function, `from_string(std::string_view s)`, with return type
     * `std::expected<T, std::string>`, which attempts to create a `T` object from a
     * `std::string_view`
     * 
     * @tparam T The type to be assessed
     */
    template <typename T>
    concept Parsable = ImplementsFromChars<T>
                       || TypeImplementsFromString<T>
                       || std::constructible_from<T, std::string_view>;

    template <typename T>
    requires std::constructible_from<T, std::string_view>
    std::expected<T, std::string> from_string(std::string_view s) {
        try {
            return T(s);
        } catch (std::exception& err) {
            return std::unexpected(std::format(
                "A \"{}\" could not be constructed from \"{}\"", get_type_name<T>(), s
            ));
        }
        
    }

    /**
     * @brief Template that produces a `from_string` function for any type that has a
     * `std::from_chars` implementation. The function will attempt to create a `T` from a string
     * input
     * 
     * @tparam T The type whose `std::from_chars` implementation should be called
     * @param s The input string to attempt to create a `T` from via `std::from_chars`
     * @return std::expected<T, std::string> A `T` resulting from a successful call to
     * `std::from_chars`, otherwise a `std::unexpected<std::string>` containing an error message
     * explaining why parsing failed
     */
    template <ImplementsFromChars T>
    std::expected<T, std::string> from_string(std::string_view s) {
        T out;
        const char* start = s.data();
        const char* end = s.data() + s.size();
        auto [ptr, ec] = std::from_chars(start, end, out);
        bool value_found = ec == std::errc();
        bool entire_string_consumed = ptr == end;
        if (!value_found || !entire_string_consumed) {
            return std::unexpected(std::format(
                "\"{}\" could not be converted to {}", s, get_type_name<T>())
            );
        }
        return out;
    }

    /**
     * @brief Template that wraps a successful call to `from_string<T>` for any type that has a
     * `std::from_chars` implementation in a `std::any`. This allows for "erasing" the type of the
     * parsing result until unboxing occurs
     * 
     * @tparam T The type who has a `std::from_chars` implementation
     * @param s The string input to parse
     * @return std::expected<std::any, std::string> A `std::any` wrapping the result of calling
     * `from_string<T>` on the input string if parsing was successful, otherwise a
     * `std::unexpected<std::string>` containing an error message explaining why parsing failed
     */
    template <typename T>
    requires ImplementsFromChars<T> || std::constructible_from<T, std::string_view>
    std::expected<std::any, std::string> parse(std::string_view s) {
        auto result = from_string<T>(s);
        if (!result) {
            return std::unexpected(std::move(result).error());
        }
        return std::any(std::move(result).value());
    }

    /**
     * @brief Template that wraps a successful call to `T::from_string` in a `std::any` for any type
     * that has a static `from_string(std::string_view) -> std::expected<T, std::string>` function.
     * This allows for "erasing" the type of the parsing result until unboxing occurs
     * 
     * @tparam T The type who has a `T::from_string` function
     * @param s The string input to parse
     * @return std::expected<std::any, std::string> A `std::any` wrapping the result of calling
     * `from_string<T>` on the input string if parsing was successful, otherwise a
     * `std::unexpected<std::string>` containing an error message explaining why parsing failed
     */
    template <TypeImplementsFromString T>
    std::expected<std::any, std::string> parse(std::string_view s) {
        auto result = T::from_string(s);
        if (!result) {
            return std::unexpected(std::move(result).error());
        }
        return std::any(std::move(result).value());
    }

    /**
     * @brief Defines a function pointer type for parse functions that have one parameter, a
     * `std::string_view`, and return a `std::any` upon success or a `std::string` containing an
     * error message upon failure
     */
    using ParseFunction = std::function<std::expected<std::any, std::string>(std::string_view)>;

    /**
     * @brief Struct that holds the state for a single program parameter
     */
    struct ProgramParameter {
        ParseFunction parser;
        std::any value;
        std::string description;
        bool is_flag;
        bool is_required;
        bool was_provided = false;
    };

    /**
     * @brief Program argument parser which can be used to define a list of parameters and flags
     * that a program accepts. A parser will attempt to parse the user's input during program
     * invocation. If parsing is successful, the developer can then retrieve the user's input
     * from the parser, "automagically" converting the user's input to the types assigned to each
     * parameter by the developer
     */
    class Parser {
    public:

        template <Parsable T>
        Parser& add_required_parameter(const std::string& name, std::string_view description) {
            this->parameters[name] = ProgramParameter {
                parse<T>, std::any(), std::string(description), false, true
            };
            return *this;
        }

        template <Parsable T>
        Parser& add_optional_parameter(const std::string& name, T default_value,
                                       std::string_view description) {
            this->parameters[name] = ProgramParameter {
                parse<T>, std::any(default_value), std::string(description), false, false
            };
            return *this;
        }

        Parser& add_flag_parameter(const std::string& flag_name, std::string_view description);

        bool is_flag_set(const std::string& flag_name);

        template <typename T>
        T get(const std::string& parameter_name) {
            std::any boxed;
            if (is_flag_registered(parameter_name)) {
                std::cerr << "Developer error: " << parameter_name << " is a non-flag program "
                          << "parameter but was accessed as a flag. Use is_flag_set instead.";
                std::exit(1);
            } else if (!is_parameter_registered(parameter_name)) {
                std::cerr << "Developer error: Program is not configured with a parameter named"
                          << parameter_name << '"';
                std::exit(1);
            }
            return std::any_cast<T>(parameters[parameter_name].value);
        }

        std::vector<std::string>
        parse_program_arguments(int argc, char** argv);

        std::expected<std::vector<std::string>, std::vector<std::string>>
        parse_arguments(const std::vector<std::string_view>& arguments);

        bool was_parameter_provided(const std::string& parameter_name);

    private:
        std::expected<void, std::string>
        attempt_parse(const std::string& parameter_name, std::string_view input);

        std::expected<void, std::vector<std::string>>
        check_for_missing_parameters();

        std::expected<std::optional<std::string>, std::string>
        handle_program_argument(std::string_view argument);

        std::expected<std::optional<std::string>, std::string>
        handle_parameter_token_with_value(const std::string& parameter_name, std::string_view value);

        std::expected<std::optional<std::string>, std::string>
        handle_flag_token(const std::string& flag_name);

        bool does_parameter_have_value(const std::string& parameter_name);

        bool is_parameter_registered(const std::string& parameter_name);

        bool is_flag_registered(const std::string& flag_name);

        // TODO: Implement me
        void print_help();

        std::unordered_map<std::string, ProgramParameter> parameters;
        std::string_view program_name;
    };
}
