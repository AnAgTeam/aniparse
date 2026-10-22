/*
 * Copyright (C) 2026 Toilettrauma
 */
#pragma once
#include "aniparse/net/CookieJar.hpp"
#if defined(ANIPARSE_CURL_BACKEND) && ANIPARSE_CURL_BACKEND
#include <asyncnet/AsyncSession.hpp>
namespace aniparse {
class CurlCookieJar : public CookieJar {
public:
	CurlCookieJar();
	CurlCookieJar(std::shared_ptr<asyncnet::CurlShared> shared);

	[[nodiscard]] std::optional<Cookie> find_cookie(std::string_view name) const override;

	[[nodiscard]] std::vector<Cookie> cookies() const override;

	void set_cookie(const Cookie& cookie) override;

	void clear() override;

	[[nodiscard]] std::vector<std::string> serialize() const override;
	void deserialize(std::span<std::string> cookies) override;

	[[nodiscard]] const std::shared_ptr<asyncnet::CurlShared>& shared() const;

private:
	std::shared_ptr<asyncnet::CurlShared> shared_;
};

} // namespace aniparse
#endif // ANIPARSE_CURL_BACKEND
