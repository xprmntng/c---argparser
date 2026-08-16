
#include <cxxabi.h>

#include <any>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <ranges>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Args {

/**
 * @brief Get the name of a type
 *
 * @tparam T The type whose name should be retrieved
 * @return std::string A string containing the demangled name of the type if
 * demangling was successful, otherwise one containing the mangled name
 */
template <typename T>
std::string get_type_name() {
  // std::string gets a really ugly demangled type name; override that with a
  // check here
  if constexpr (std::is_same_v<T, std::string>) {
    return "string";
  } else {
    const char* mangled_name = typeid(T).name();
    int status = 0;
    // abi::__cxa_demangle allocates a raw C-string using std::malloc
    char* demangled =
        abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status);

    if (status == 0 && demangled != nullptr) {
      // Wrap in a unique_ptr to prevent a dynamic memory leak
      std::unique_ptr<char, void (*)(void*)> cleanup(demangled, std::free);
      return std::string(demangled);
    }
    return mangled_name;
  }
}

/**
 * @brief Defines a concept which identifies types who can be created using the
 * STL function `std::from_chars`
 *
 * @tparam T The type to be assessed
 */
template <typename T>
concept ImplementsFromChars =
    requires(const char* start, const char* end, T& out) {
      {
        std::from_chars(start, end, out)
      } -> std::same_as<std::from_chars_result>;
    };

/**
 * @brief Defines a concept which identifies types that have a static member
 * function, `from_string(std::string_view) -> std::expected<T, std::string>`,
 * which can be used to attempting creating a `T` from a `std::string_view`
 * input
 *
 * @tparam T The type to be assessed
 */
template <typename T>
concept TypeImplementsFromString = requires(std::string_view s) {
  { T::from_string(s) } -> std::same_as<std::expected<T, std::string>>;
};

/**
 * @brief Defines a concept for types that are parsable. A type is considered
 * parsable if: a) The standard library implements `std::from_chars` for that
 * type OR b) The type has a constructor that takes a single argument:
 * `std::string_view` OR c) The type implements a static function,
 * `from_string(std::string_view s)`, with return type `std::expected<T,
 * std::string>`, which attempts to create a `T` object from a
 * `std::string_view`
 *
 * @tparam T The type to be assessed
 */
template <typename T>
concept Parsable = ImplementsFromChars<T> || TypeImplementsFromString<T> ||
                   std::constructible_from<T, std::string_view>;

/**
 * @brief Template that produces a `from_string` function for any type that has a constructor that
 * creates an object of that type from a `std::string_view`. The constructor may throw an
 * exception. The template function will attempt to construct a `T` object from the `string_view`
 *
 * @tparam T The type whose `T(std::string_view)` constructor  should be called
 * @param s The string from which to attempt to construct a `T`
 * @return A `T` object wrapped in a `std::expected` upon success, otherwise a `std::string` 
 * containing an error message wrapped in a `std::unexpected`
 */
template <typename T>
  requires std::constructible_from<T, std::string_view>
std::expected<T, std::string> from_string(std::string_view s) {
  try {
    return T(s);
  } catch (std::exception& err) {
    return std::unexpected(
        std::format("A \"{}\" could not be constructed from \"{}\"",
                    get_type_name<T>(), s));
  }
}

/**
 * @brief Template that produces a `from_string` function for any type that has
 * a `std::from_chars` implementation. The function will attempt to create a `T`
 * from a string input
 *
 * @tparam T The type whose `std::from_chars` implementation should be called
 * @param s The input string to attempt to create a `T` from via
 * `std::from_chars`
 * @return std::expected<T, std::string> A `T` resulting from a successful call
 * to `std::from_chars`, otherwise a `std::unexpected<std::string>` containing
 * an error message explaining why parsing failed
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
    return std::unexpected(std::format("\"{}\" could not be converted to {}", s,
                                       get_type_name<T>()));
  }
  return out;
}

/**
 * @brief Template that wraps a successful call to `from_string<T>` for any type
 * that has a `std::from_chars` implementation in a `std::any`. This allows for
 * "erasing" the type of the parsing result until unboxing occurs
 *
 * @tparam T The type who has a `std::from_chars` implementation
 * @param s The string input to parse
 * @return std::expected<std::any, std::string> A `std::any` wrapping the result
 * of calling `from_string<T>` on the input string if parsing was successful,
 * otherwise a `std::unexpected<std::string>` containing an error message
 * explaining why parsing failed
 */
template <typename T>
  requires ImplementsFromChars<T> ||
           std::constructible_from<T, std::string_view>
std::expected<std::any, std::string> parse(std::string_view s) {
  auto result = from_string<T>(s);
  if (!result) {
    return std::unexpected(std::move(result).error());
  }
  return std::any(std::move(result).value());
}

/**
 * @brief Template that wraps a successful call to `T::from_string` in a
 * `std::any` for any type that has a static `from_string(std::string_view) ->
 * std::expected<T, std::string>` function. This allows for "erasing" the type
 * of the parsing result until unboxing occurs
 *
 * @tparam T The type who has a `T::from_string` function
 * @param s The string input to parse
 * @return std::expected<std::any, std::string> A `std::any` wrapping the result
 * of calling `from_string<T>` on the input string if parsing was successful,
 * otherwise a `std::unexpected<std::string>` containing an error message
 * explaining why parsing failed
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
 * @brief Defines a function pointer type for parse functions that have one
 * parameter, a `std::string_view`, and return a `std::any` upon success or a
 * `std::string` containing an error message upon failure
 */
using ParseFunction =
    std::function<std::expected<std::any, std::string>(std::string_view)>;

/**
 * @brief Struct that holds the state for a single program parameter
 */
struct ProgramParameter {
  ParseFunction parser;
  std::any value;
  std::string description;
  bool is_flag;
  bool is_required;
  std::string type_name;
  bool was_provided = false;
};

/**
 * @brief Program argument parser which can be used to define a list of
 * parameters and flags that a program accepts. A parser will attempt to parse
 * the user's input during program invocation. If parsing is successful, the
 * developer can then retrieve the user's input from the parser, "automagically"
 * converting the user's input to the types assigned to each parameter by the
 * developer
 */
class Parser {
 public:
  /**
   * @brief Define a program parameter that the end user MUST provide when invoking the program at
   * the commandline. The argument parser will then attempt to convert the user's input into the
   * type passed via the template type parameter when the developer calls `parse_program_parameters`
   *
   * @tparam T The type to attempt converting user input into
   * @param name The name of the parameter. Do not include the leading `--`
   * @param description Text for the help menu that will explain to the end user what the program is
   * expecting with this parameter
   * @return Parser& A reference to the `Parser` object this method was invoked upon
   */
  template <Parsable T>
  Parser& add_required_parameter(const std::string& name,
                                 std::string_view description) {
    if (is_parameter_registered(name) || is_flag_registered(name)) {
      std::cerr << "Developer error: Parameter \"" << name << "\" was defined multiple times which "
                << "is illegal";
      std::exit(1);
    }
    this->parameters[name] =
        ProgramParameter{parse<T>, std::any(), std::string(description),
                         false,    true,       get_type_name<T>()};
    return *this;
  }

  /**
   * @brief Define an optional program parameter that the end user may omit when invoking the
   * program at the commandline. To make the parameter optional, the developer must provide a
   * default fallback value that will be used in the event that the end user omits the parameter.
   * If the end user DOES pass the optional parameter, the parser will then attempt to convert the
   * user's input into the type passed via the template type parameter when the developer calls
   * `parse_program_parameters`
   *
   * @tparam T The type to attempt converting user input into
   * @param name The name of the parameter. Do not include the leading `--`
   * @param default_value The fallback value for the parameter in the event that the end user omits
   * the parameter during program invocation
   * @param description Text for the help menu that will explain to the end user what the program is
   * expecting with this parameter
   * @return Parser& A reference to the `Parser` object this method was invoked upon
   */
  template <Parsable T>
  Parser& add_optional_parameter(const std::string& name, T default_value,
                                 std::string_view description) {
    if (is_parameter_registered(name) || is_flag_registered(name)) {
      std::cerr << "Developer error: Parameter \"" << name << "\" was defined multiple times which "
                << "is illegal";
      std::exit(1);
    }
    this->parameters[name] = ProgramParameter{
        parse<T>, std::any(default_value), std::string(description), false,
        false,    get_type_name<T>()};
    return *this;
  }

  /**
   * @brief Define a flag parameter that defaults to `false` (unset) unless the flag is provided by
   * the end user of the program, in which case it will be set to `true`. The status of whether a
   * flag parameter was provided can be checked using the `is_flag_set` method
   *
   * @param flag_name The name under which to register the flag
   * @param description Text for the help menu that will explain the meaning of the flag to the user
   * @return Parser& A reference to the `Parser` object this method was invoked upon
   */
  Parser& add_flag_parameter(const std::string& flag_name,
                             std::string_view description);

  /**
   * @brief Determine whether a flag parameter was passed by the end user at the command line
   *
   * @param flag_name The name of the flag to look up
   * @return true If the flag was provided by the end user
   * @return false If the flag was NOT provided by the end user
   */
  bool is_flag_set(const std::string& flag_name);

  /**
   * @brief Get the argument for a non-flag parameter given by the end user. If the parameter is
   * optional and the end user did not supply the parameter, this method returns the default
   * argument provided by the developer instead
   *
   * @tparam T The type to retrieve the stored data as
   * @param parameter_name The name of the parameter to look up
   * @return T The result of parsing the user's input into a `T` if the user provided input. If the
   * parameter is an optional parameter and the end user did not provide the parameter, this instead
   * returns the default value provided by the developer
   */
  template <typename T>
  T get(const std::string& parameter_name) {
    std::any boxed;
    if (is_flag_registered(parameter_name)) {
      std::cerr
          << "Developer error: " << parameter_name << " is a non-flag program "
          << "parameter but was accessed as a flag. Use is_flag_set instead.";
      std::exit(1);
    } else if (!is_parameter_registered(parameter_name)) {
      std::cerr
          << "Developer error: Program is not configured with a parameter named"
          << parameter_name << '"';
      std::exit(1);
    }
    return std::any_cast<T>(parameters[parameter_name].value);
  }

  /**
   * @brief Parse the program's argument list. Named program parameters are identified by a leading
   * `--`. Any argument without a leading `--` is considered a positional argument and collected by
   * the argument parser. Named parameters with arguments provided by the end user are parsed into
   * the type assigned to the parameter by the developer. If valid input was provided, the value is
   * stored internally within the `Parser` and can be retrieved using the `get` method
   * 
   * This method is intended to be called directly from a `main` function, taking in a program's
   * `argc` and `argv` arguments. If parsing the program arguments fails for any reason, this method
   * will exit the program
   * 
   * Reasons this method might fail:
   * - The end user did not provide a required argument
   * - The end user provided a value for an argument could not be parsed into its assigned type
   * - The end user failed to provide a value for a parameter that takes a value
   * - The end user provided a value for a flag parameter, which don't take values
   *
   * @param argc The number of arguments passed to the program, plus 1 because of the inclusion of
   * the program path and name
   * @param argv The list of program arguments, with the first element being the executable passed
   * to the operating system as the program was executed
   * @return std::vector<std::string> A list of positional arguments found by the parser
   */
  std::vector<std::string> parse_program_arguments(int argc, char** argv);

  /**
   * @brief Parse a list of arguments using the parser. This method exists so that the
   * parser may be unit tested without relying on a `main` function
   *
   * @param arguments A list of arguments to parse
   * @return std::expected<std::vector<std::string>, std::vector<std::string>> A list of positional
   * arguments found by the parser wrapped in a `std::expected` upon success, otherwise a
   * `std::unexpected` containing a list of errors that occurred
   */
  std::expected<std::vector<std::string>, std::vector<std::string>>
  parse_arguments(const std::vector<std::string_view>& arguments);

  /**
   * @brief Determine whether the end user provided a given parameter at the command line
   *
   * @param parameter_name The name of the parameter to look up
   * @return true If the parameter was provided
   * @return false If the parameter was NOT provide
   */
  bool was_parameter_provided(const std::string& parameter_name);

 private:
  /**
   * @brief Attempt to parse a user input string into the type assigned to a parameter by the
   * developer. Upon success, store it internally within the `Parser` to be retrieved later by the
   * developer via the `get` function
   *
   * @param parameter_name The name of the parameter that the value should be associated with
   * @param input The user's input
   * @return std::expected<void, std::string> An empty `std::expected` upon success, otherwise a
   * `std::unexpected` containing an error message explaining why parsing the user's input failed
   */
  std::expected<void, std::string> attempt_parse(
      const std::string& parameter_name, std::string_view input);

  /**
   * @brief Determine whether the end user provided all required input parameters
   *
   * @return std::expected<void, std::vector<std::string>> An empty `std::expected` if all required
   * program parameters were provided by the end user, otherwise a `std::unexpected` containing a
   * string with a list of parameters that were missing
   */
  std::expected<void, std::vector<std::string>> check_for_missing_parameters();

  /**
   * @brief Attempt handling a single program argument, which could be a parameter in
   * `--param=value` format, a flag in `--flag` format, or a positional argument (no leading `--`)
   *
   * Reasons this method might fail:
   * - The end user provided a value for an argument could not be parsed into its assigned type
   * - The end user failed to provide a value for a parameter that takes a value
   * - The end user provided a value for a flag parameter, which don't take values
   * 
   * @param argument The argument to handle
   * @return std::expected<std::optional<std::string>, std::string> A `std::expected` containing an
   * optional `std::string` value. A string is returned if the program argument was a positional
   * argument. A `std::unexpected` wrapping a `std::string` error message is returned in the event
   * that an error occurred
   */
  std::expected<std::optional<std::string>, std::string>
  handle_program_argument(std::string_view argument);

  /**
   * @brief Given a parameter name and value provided by the end user, ensure that a) the parameter
   * is a known program parameter, b) it accepts a value (i.e., is a non-flag parameter), and c)
   * the provided value can be parsed into the type assigned to the parameter by the developer
   * 
   * If all the above criteria are met, the resulting parsed value is stored internally and can be
   * retrieved using the `get` function
   * 
   * In any other case, an error message is returned
   *
   * @param parameter_name The text found after the `--` and before the `=`
   * @param value The text found after the `=`
   * @return std::expected<std::optional<std::string>, std::string> Upon success, always returns
   * an empty `std::optional` wrapped in a `std::expected`. Upon failure, returns a
   * `std::unexpected` with an error message explaining why handling the parameter failed
   */
  std::expected<std::optional<std::string>, std::string>
  handle_parameter_token_with_value(const std::string& parameter_name,
                                    std::string_view value);

  /**
   * @brief Given a flag name, ensure that the program does indeed accept a flag with that name. If
   * one does, this method sets that flag to `true`, and `is_flag_set` will return `true`
   * 
   * If the program doesn't take a flag with that name, an error message is returned
   *
   * @param flag_name The name of the parameter that was passed as a flag
   * @return std::expected<std::optional<std::string>, std::string> An empty `std::optional` upon
   * success, otherwise a `std::unexpected` containing a `std::string` error message
   */
  std::expected<std::optional<std::string>, std::string> handle_flag_token(
      const std::string& flag_name);

  /**
   * @brief Determine whether a given program parameter has a value assigned to it
   *
   * @param parameter_name The name of the parameter to look up
   * @return true In the case that the end user provided the parameter with a value OR the parameter
   * is optional and thus has a default value
   * @return false
   */
  bool does_parameter_have_value(const std::string& parameter_name);

  /**
   * @brief Determine whether a program parameter that takes a value is registered with this
   * `Parser`
   *
   * @param parameter_name The name of the parameter to look up
   * @return true If the parameter exists and takes a value
   * @return false If the parameter does not exist or is a flag parameter
   */
  bool is_parameter_registered(const std::string& parameter_name);

  /**
   * @brief Determine whether a flag parameter is registered with this `Parser`
   *
   * @param flag_name The name of the flag to look up
   * @return true If the parameter exists and is a flag
   * @return false If the parameter does not exist or takes a value
   */
  bool is_flag_registered(const std::string& flag_name);

  /**
   * @brief Prints the program's help menu, which includes a list of each program parameter, whether
   * that parameter is required, a developer-defined description of the parameter, and the type of
   * data that the program is expecting to be passed with that parameter if the parameter takes a
   * value
   */
  void print_help();

  /// @brief Where program parameters are stored. Their name is the key, their state is the value
  std::unordered_map<std::string, ProgramParameter> parameters;

  /// @brief The program path and name used to invoke the program. Set in `parse_program_arguments`
  std::string_view program_name = "";
};
}  // namespace Args
