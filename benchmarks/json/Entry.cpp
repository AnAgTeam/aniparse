#include "Models.hpp"
// JSON_DIAGNOSTICS selects 0 = plain Boost, 1 = eager, 2 = error-branch replay.
#if JSON_DIAGNOSTICS
#include <aniparse/json/Decode.hpp>
#if JSON_DIAGNOSTICS == 1
#define ENTRY(name) diagnostic_##name
#else
#define ENTRY(name) lazy_##name
#endif
#else
#define ENTRY(name) boost_##name
#endif

namespace {
template<class T, class Context = std::nullptr_t>
int convert(const boost::json::value& input, const Context& context = {}) {
#if JSON_DIAGNOSTICS == 1
    auto result = aniparse::json::decode<T>(input,
        std::tuple<const Context&, aniparse::json::Diagnostics>{context, {}});
#else
    auto result = boost::json::try_value_to<T>(input, context);
#endif
    // Make the entire constructed result observable, including fields that the
    // tiny return value does not inspect. No artificial noinline on conversion.
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "g"(&result) : "memory");
#endif
    if (result) {
        return 0;
    }
#if JSON_DIAGNOSTICS == 1
    return result.error().code.value();
#elif JSON_DIAGNOSTICS == 2
    auto error = aniparse::json::decode_error<T>(input, context);
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "g"(&error) : "memory");
#endif
    return error.code.value();
#else
    return result.error().value();
#endif
}
}

extern "C" {
int ENTRY(numbers)(const boost::json::value& input) { return convert<json_benchmark::Numbers>(input); }
int ENTRY(array)(const boost::json::value& input) { return convert<std::array<int, 1024>>(input); }
int ENTRY(page)(const boost::json::value& input) { return convert<json_benchmark::Page>(input); }
int ENTRY(tolerant)(const boost::json::value& input) { return convert<json_benchmark::Forgiving>(input); }
int ENTRY(strict)(const boost::json::value& input) {
    return convert<json_benchmark::Forgiving>(input, aniparse::json::Strict{});
}
int ENTRY(variant)(const boost::json::value& input) { return convert<json_benchmark::Choice>(input); }
int ENTRY(tuple)(const boost::json::value& input) { return convert<json_benchmark::Tuple>(input); }
int ENTRY(tree)(const boost::json::value& input) { return convert<json_benchmark::Tree>(input); }
}
