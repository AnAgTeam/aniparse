/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/types/Request.hpp"

namespace aniparse {

bool operator==(const GetRequest& left, const GetRequest& right) {
	return left.url == right.url && left.url_params == right.url_params && left.headers == right.headers;
}

bool operator==(const PostRequest& left, const PostRequest& right) {
	return left.url == right.url && left.url_params == right.url_params && left.headers == right.headers && left.body == right.body;
}

bool operator==(const PostMultipartRequest& left, const PostMultipartRequest& right) {
	return left.url == right.url && left.url_params == right.url_params && left.headers == right.headers && left.forms == right.forms;
}
} // namespace aniparse