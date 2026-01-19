/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <aateam.anianglia@gmail.com>
 */
#pragma once
#include <lexbor/dom/collection.h>

namespace aniparse::html {
	/**
	 * @brief Class for storing the HTML elements
	 * walk results
	 * @todo
	 */
	class DOMCollection {
	public:
		//explicit DOMCollection(size_t size);
		DOMCollection(const DOMCollection& other) = delete;
		DOMCollection(DOMCollection&& other) noexcept;
		~DOMCollection() noexcept;

		DOMCollection& operator=(const DOMCollection& other) = delete;
		DOMCollection& operator=(DOMCollection&& other) noexcept;

	private:
		lxb_dom_collection_t* collection_ = nullptr;
		size_t size = 0;
	};
}