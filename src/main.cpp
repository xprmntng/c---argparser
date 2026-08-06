#include <any>
#include <exception>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include <cxxabi.h>

using ParseFunction = std::function<std::any(std::string)>;
// using ParseFunction = std::any(*)(std::string);

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

template <typename T> std::any parse(std::string s) {
    T value;
    std::istringstream stream(s);
    if (stream >> value) {
        std::any value_as_any = value;
        return value_as_any;
    }
    std::string message = std::format("Value `{}` cannot be converted to `{}`", s, demangle(typeid(T).name()));
    throw std::runtime_error(message);
}

struct Banana {
    std::string color;
};

std::istream& operator>>(std::istream& is, Banana& banana) {
    return is >> banana.color;
}

std::ostream& operator<<(std::ostream& os, Banana& banana) {
    return os << "A " << banana.color << " banana";
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

    ap.add_required_parameter<Banana>("banana");
    ap.attempt_parse("banana", "brown");
    auto banana = ap.get<Banana>("banana");
    std::cout << banana << std::endl;
}
