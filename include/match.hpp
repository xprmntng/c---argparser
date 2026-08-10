
// Template that allows for inline definitions of visitor functions that can be passed to std::visit
template<class... Ts> struct match : Ts... { using Ts::operator()...; };
template<class... Ts> match(Ts...) -> match<Ts...>;
