#include <aniparse/Version.hpp>

namespace aniparse {

std::string_view version() noexcept {
	return ANIPARSE_VERSION;
}

} // namespace aniparse
