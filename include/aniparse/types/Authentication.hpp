/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <string>
#include <variant>

namespace aniparse {
/**
 * @brief Data for authentification using username and password
 */
struct AuthenticationUserPassword {
	std::string username;
	std::string password;
	bool requires_2fa = false;
};

/**
 * @brief Data for authentification using token
 *        (some string representing all required information
 *        to identify user)
 */
struct AuthenticationToken {
	std::string token;
	std::string type;
};

/**
 * @brief Data that can be used for authentification
 */
using AuthenticationData = std::variant<AuthenticationUserPassword, AuthenticationToken>;
} // namespace aniparse
