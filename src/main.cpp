// #include <any>
// #include <concepts>
// #include <cstdlib>
// #include <exception>
// #include <expected>
// #include <format>
// #include <functional>
#include <iostream>
// #include <memory>
// #include <ranges>
// #include <sstream>
#include <string>
// #include <typeinfo>
// #include <unordered_map>
// #include <vector>

#include "args.hpp"

struct Banana {
    std::string color;
};

std::istream& operator>>(std::istream& is, Banana& banana) {
    return is >> banana.color;
}

std::ostream& operator<<(std::ostream& os, Banana& banana) {
    return os << "A " << banana.color << " banana";
}



int main(int argc, char** argv) {
    Args::Parser parser;

    parser.add_required_parameter<int>("age")
          .add_optional_parameter("coolness-factor", 5.0)
          .add_required_parameter<Banana>("banana");

    const auto positional_args = parser.parse_program_arguments(argc, argv);

    auto age = parser.get<int>("age");
    std::cout << "age: " << age << std::endl;

    auto f = parser.get<double>("coolness-factor");
    std::cout << "coolness-factor: " << f << std::endl;

    auto banana = parser.get<Banana>("banana");
    std::cout << "Banana: " << banana << std::endl;

    if (positional_args.empty()) {
        std::cout << "No positional arguments were received" << std::endl;
    } else {
        std::cout << "Also received the following positional arguments: ";
        for (const auto& pos_arg : positional_args) {
            std::cout << pos_arg << " ";
        }
        std::cout << std::endl;
    }
}
