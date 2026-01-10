#pragma once
#include "aniparse/ClientContext.hpp"
#include "aniparse/Common.hpp"

namespace aniparse {
	struct ImageContainerGetter : ForwardPaginator<Image> {
		virtual ~ImageContainerGetter() = default;

		virtual asyncnet::NetworkTask<std::vector<Image>> next() = 0;
		virtual bool end() const = 0;

		virtual asyncnet::NetworkTask<std::string> title();

		virtual asyncnet::NetworkTask<std::string> description();
		
		virtual asyncnet::NetworkTask<std::vector<std::string>> tags();

		virtual bool update_client(std::shared_ptr<ClientContext> new_client);

	protected:
		std::shared_ptr<ClientContext> client;
	};

	using ImageContainerPaginator = ForwardPaginator<std::unique_ptr<ImageContainerGetter>>;

	struct ImagesGetter {
		virtual ~ImagesGetter() = default;

		virtual asyncnet::NetworkTask<std::unique_ptr<ImageContainerPaginator>> search(std::shared_ptr<ClientContext> client, std::string query, GetFilters filters);

		virtual asyncnet::NetworkTask<std::unique_ptr<ImageContainerPaginator>> latest(std::shared_ptr<ClientContext> client, GetFilters filters);

	protected:
		GetterContext context;
	};
}