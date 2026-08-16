
/// @brief Templates that allow for inline visitor functions that can be passed to std::visit
/// @tparam ...Ts Args containing the return type and parameters accepted by the match expression
template<class... Ts> struct match : Ts... { using Ts::operator()...; };
template<class... Ts> match(Ts...) -> match<Ts...>;
