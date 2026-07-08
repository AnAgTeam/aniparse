/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
/*
 * The coroutine task types (asyncnet::NetworkTask / CancellingTask) appear in
 * every getter signature and are needed regardless of the HTTP backend. They are
 * curl-free, but they live in libasyncnet, which is only linked for the curl
 * backend. So: pull them from asyncnet when the curl backend is on, otherwise
 * from a byte-identical vendored copy. Both branches yield the same
 * asyncnet::NetworkTask, so a curl-backed client and the parse core interop.
 */
#if defined(ANIPARSE_CURL_BACKEND) && ANIPARSE_CURL_BACKEND
	#include <asyncnet/CancellingTask.hpp>
#else
	#include "aniparse/net/vendor/CancellingTask.hpp"
#endif
