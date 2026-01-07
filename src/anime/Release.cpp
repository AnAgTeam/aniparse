#include "aniparse/anime/Release.hpp"

namespace aniparse {
	OptionalRequest<std::string> AsyncReleaseGetter::title(const GetterContext& context) const {
		return ClientParsedRequest<std::string> {
			.request = GetRequest {
				.url = "https://www.google.com"
			},
			.parse = [](asyncnet::Response response) {
				return std::to_string(response.get_status_code());
			}
		};
		//return "some title";
	}
}
