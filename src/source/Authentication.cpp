/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/source/Authentication.hpp"

namespace aniparse {

AuthKeys auth_keys_from_fields(const std::vector<AuthField>& fields) {
	AuthKeys keys;
	for (const AuthField& field : fields) {
		switch (field.channel) {
		case AuthField::Channel::Header:   keys.headers.push_back(field.target);    break;
		case AuthField::Channel::Cookie:   keys.cookies.push_back(field.target);    break;
		case AuthField::Channel::UrlParam: keys.url_params.push_back(field.target); break;
		}
	}
	return keys;
}

} // namespace aniparse
