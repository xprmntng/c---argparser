#include <iostream>
#include <string>

#include "args.hpp"

// Define a custom type
struct Banana {
    std::string color;
};

// Define how to read a Banana from a text stream (used by Args::parse)
std::istream& operator>>(std::istream& is, Banana& banana) {
    return is >> banana.color;
}

// Define how to write a Banana to a text stream
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
    std::cout << "banana: " << banana << std::endl;

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
