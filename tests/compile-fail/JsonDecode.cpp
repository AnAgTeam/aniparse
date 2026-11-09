#include <aniparse/json/Decode.hpp>

namespace duplicate_members {
struct Left { int id; };
BOOST_DESCRIBE_STRUCT(Left, (), (id))
struct Right { int id; };
BOOST_DESCRIBE_STRUCT(Right, (), (id))
struct Model : Left, Right {};
BOOST_DESCRIBE_STRUCT(Model, (Left, Right), ())
}

// One positive control and the three shapes rejected by ordinary Boost.JSON.
#if JSON_INVALID_CASE == 0
using Target = int;
#elif JSON_INVALID_CASE == 1
using Target = duplicate_members::Model;
#elif JSON_INVALID_CASE == 2
using Target = std::tuple<>;
#elif JSON_INVALID_CASE == 3
using Target = std::array<int, 0>;
#endif

void instantiate_conversion(const boost::json::value& input) {
#if JSON_USE_BOOST
    auto result = boost::json::try_value_to<Target>(input);
#else
    auto result = aniparse::json::decode<Target>(input, aniparse::json::Diagnostics{});
#endif
    (void)result;
}
