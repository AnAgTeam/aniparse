/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <aateam.anianglia@gmail.com>
 */
#include "aniparse/html/DOMCollection.hpp"

#include <utility>
#include <lexbor/dom/collection.h>

namespace aniparse::html {
	DOMCollection::DOMCollection(DOMCollection&& other) noexcept : collection_(std::exchange(other.collection_, nullptr)) {

	}

	DOMCollection::~DOMCollection() {
		if (collection_) {
			lxb_dom_collection_clean(collection_);
			lxb_dom_collection_destroy(collection_, false);
		}
	}

	DOMCollection& DOMCollection::operator=(DOMCollection&& other) noexcept {
		if (std::addressof(other) != this) {
			collection_ = std::exchange(other.collection_, nullptr);
		}
		return *this;
	}
}