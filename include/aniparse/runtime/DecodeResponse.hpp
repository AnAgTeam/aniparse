#pragma once
#include <string_view>
#include <utility>

#include "aniparse/json/Decode.hpp"
#include "aniparse/utility/Result.hpp"

namespace aniparse {

/**
 * @brief Decode a response model and map structural failures to UnexpectedResponse.
 * @tparam T The response model, with its source-specific JSON field types.
 * @tparam Context The conversion policy; defaults to the library build policy.
 * @param document Parsed response, borrowed for this synchronous call.
 * @param description Source/operation description prefixed to conversion errors.
 * @param context Conversion settings forwarded to all nested model conversions.
 * @return The model, or a request error with the conversion message and JSON body.
 * @note The error body is reserialized JSON. JSON decoding remains independent of
 *       transport; this helper only adapts its error at the response boundary.
 * @throws std::bad_alloc If model or error storage cannot be allocated.
 */
template <class T, class Context = json::DefaultContext>
Result<T> decode_response(const boost::json::value& document,
                          std::string_view description = "Invalid JSON response", const Context& context = {}) {
	auto result = json::decode<T>(document, context);
	if (!result) {
		return make_request_error(RequestErrorCode::UnexpectedResponse,
		                          std::string(description) + ": " + result.error().message(),
		                          std::nullopt, boost::json::serialize(document));
	}
	return std::move(*result);
}

} // namespace aniparse
