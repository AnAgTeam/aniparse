/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <span>
#include <vector>
#include <string>
#include <optional>

namespace aniparse {

class CookieJar {
public:
	virtual ~CookieJar() = default;

	virtual std::optional<std::string> find_cookie(std::string_view name) const = 0;

	virtual void set_cookie(std::string cookie) = 0;

	virtual void clear() = 0;

	virtual std::vector<std::string> serialize() const       = 0;
	virtual void deserialize(std::span<std::string> cookies) = 0;
};
} // namespace aniparse