#include <iostream>
#include <string>
#include <sstream>

#include "args.hpp"

// Define a custom type
struct Banana {
    std::string color;

public:
    // Define how to create a Banana from a string. This makes the type compatible with Arg::Parser
    Banana(std::string_view s) : color(s) {}
};

static_assert(std::constructible_from<Banana, std::string_view>);

// Define how to write a Banana to a text stream
std::ostream& operator<<(std::ostream& os, Banana& banana) {
    return os << "A " << banana.color << " banana";
}

int main(int argc, char** argv) {
    Args::Parser parser;

    parser.add_required_parameter<int>("age", "Your age")
          .add_required_parameter<std::string>("name", "Your name")
          .add_required_parameter<Banana>("banana", "Tell me about that banana")
          .add_optional_parameter("coolness-factor", 5.0, "Just how cool are you?")
          .add_flag_parameter("super-cool", "Are you super cool?");

    const auto positional_args = parser.parse_program_arguments(argc, argv);

    auto age = parser.get<int>("age");
    std::cout << "age: " << age << std::endl;

    auto name = parser.get<std::string>("name");
    std::cout << "name: " << name << std::endl;

    auto f = parser.get<double>("coolness-factor");
    std::cout << "coolness-factor: " << f << std::endl;

    auto banana = parser.get<Banana>("banana");
    std::cout << "banana: " << banana << std::endl;

    bool super_cool = parser.is_flag_set("super-cool");
    std::cout << "super-cool: " << super_cool << std::endl;

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
