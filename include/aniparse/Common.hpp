/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once

// Aggregate ("umbrella") header. Pulls in the whole shared vocabulary
// from types/. New code may include the specific types/*.hpp it needs
// instead of this file.
#include "aniparse/types/Ids.hpp"
#include "aniparse/types/Flags.hpp"
#include "aniparse/types/Text.hpp"
#include "aniparse/types/Model.hpp"
#include "aniparse/types/Pagination.hpp"
#include "aniparse/types/Response.hpp"
#include "aniparse/types/Search.hpp"
#include "aniparse/types/Serialization.hpp"
#include "aniparse/types/Authentication.hpp"

// Kept for backward compatibility: the previous Common.hpp exposed
// ClientContext transitively. Not a "type" itself, safe to drop later
// if consumers include it directly.
#include "aniparse/ClientContext.hpp"
