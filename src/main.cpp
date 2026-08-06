#include <any>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <iostream>
#include <exception>
#include <typeinfo>
#include <format>

using ParseFunction = std::function<std::any(std::string)>;
// using ParseFunction = std::any(*)(std::string);

template <typename T> std::any parse(std::string s) {
    T value;
    std::istringstream stream(s);
    if (stream >> value) {
        std::any value_as_any = value;
        return value_as_any;
    }
    std::string message = std::format("Value `{}` cannot be converted to `{}`", s, typeid(T).name());
    throw std::runtime_error(message);
}

class ArgParse {
public:

    template <typename T>
    void add_required_parameter(std::string name) {
        this->parsers[name] = parse<T>;
    }

    template <typename T>
    void add_optional_parameter(std::string name, T default_value) {
        ParseFunction f = parse<T>;
        this->parsers[name] = f;
        this->arguments[name] = std::any(default_value);
    }

    void attempt_parse(std::string parameter_name, std::string input) {
        ParseFunction f = this->parsers[parameter_name];
        std::any boxed = f(input);
        this->arguments[parameter_name] = boxed;
    }

    template <typename T>
    T get(std::string parameter_name) {
        std::any boxed = this->arguments[parameter_name];
        return std::any_cast<T>(boxed);
    }

private:
    std::unordered_map<std::string, ParseFunction> parsers;
    std::unordered_map<std::string, std::any> arguments;
};

int main() {
    ArgParse ap;

    ap.add_required_parameter<int>("age");
    ap.attempt_parse("age", "100");
    auto age = ap.get<int>("age");
    std::cout << age << std::endl;

    ap.add_optional_parameter("float", 5.0);
    ap.attempt_parse("float", "10e2");
    auto f = ap.get<double>("float");
    std::cout << f << std::endl;
}
