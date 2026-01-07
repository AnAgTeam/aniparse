#pragma once
#include "aniparse/Requests.hpp"

namespace aniparse {
	struct Image {
		std::string url;
		int width;
		int height;
	};

	template<typename T, typename Container = std::vector<T>>
	struct ForwardPaginator {
		virtual ~ForwardPaginator() = default;

		virtual Container next() = 0;

		virtual bool end() const = 0;
	};

	struct ImagesGetter {
		virtual ~ImagesGetter() = default;

		virtual OptionalRequest<std::unique_ptr<ForwardPaginator<Image>>> images_search(const GetterContext& context);
	};
}