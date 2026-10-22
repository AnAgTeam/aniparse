/*
 * Copyright (C) 2026 Toilettrauma
 */
#pragma once
#include <string>
#include <string_view>

namespace aniparse {

/// The reserved ID of the default request profile.
inline constexpr std::string_view default_profile_id = "main";

/**
 * @brief Stable, parser-local identity and presentation of a request profile.
 */
struct ProfileInfo {
	std::string id; ///< Opaque stable key used for persistence.
	std::string name; ///< Display name for profile settings and sign-in.
};

} // namespace aniparse
