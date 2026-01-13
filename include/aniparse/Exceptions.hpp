#pragma once
#include <stdexcept>

namespace aniparse {

	struct NotImplementedError : std::runtime_error {
		using std::runtime_error::runtime_error;
	};
}