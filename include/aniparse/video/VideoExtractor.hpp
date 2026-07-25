/*
 * Copyright (C) 2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/ClientContext.hpp"
#include "aniparse/Parser.hpp"
#include "aniparse/detail/DomainStore.hpp"
#include "aniparse/types/ParsedUrl.hpp"
#include "aniparse/types/Video.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aniparse {

/**
 * @brief A URL handed to another extractor after the current extractor resolves it.
 *
 * It models yt-dlp's URL result: @ref url is routed again by
 * VideoExtractorStore, unless @ref extractor_id pins the next implementation.
 */
struct VideoExtractionLink {
	/// Absolute URL to route through VideoExtractorStore again.
	std::string url;
	/// Optional next extractor identifier; absent means normal domain routing.
	std::optional<std::string> extractor_id;
};

/**
 * @brief Result of extracting one external video URL.
 *
 * An extractor either fills @ref streams with directly playable formats, adds
 * @ref links for another extraction step, or does both when it can offer a
 * native format and delegated alternatives. Headers apply to every direct
 * stream in this result.
 */
struct VideoExtraction {
	/// Directly playable formats, in the extractor's preferred order.
	std::vector<VideoStream> streams;
	/// Deferred URLs to resolve through VideoExtractorStore.
	std::vector<VideoExtractionLink> links;
	/// Headers required when requesting any entry of @ref streams.
	Headers headers;
};

/**
 * @brief A standalone external-video URL handler, analogous to a yt-dlp extractor.
 *
 * It is deliberately not a Parser: it has no catalog, search UI, mirrors, or
 * persisted getter identity. It receives one URL and yields direct formats or
 * links that another extractor can resolve.
 */
struct VideoExtractor {
	virtual ~VideoExtractor() = default;

	/**
	 * @brief Stable registration key for this extractor.
	 * @return A non-empty identifier, unique within a VideoExtractorStore.
	 * @note The identifier may be used by VideoExtractionLink::extractor_id, so it
	 * must remain stable after it is shipped.
	 */
	[[nodiscard]] virtual std::string identifier() const = 0;
	/**
	 * @brief Declare every static host this extractor can receive.
	 * @param context A transient sink owned by the store; call add_domain() once
	 * for each bare host, without a scheme or path.
	 * @note The context is valid only for this call and must not be retained. The
	 * declarations are a coarse routing prefilter; valid_for_url() remains the
	 * exact path/query gate.
	 */
	virtual void emplace_domains(EmplaceDomainsContext& context) const = 0;
	/**
	 * @brief Decide whether this extractor handles one URL under a declared host.
	 * @param url Parsed URL whose host already matched one of emplace_domains().
	 * @return True when extract() can handle the URL; false to reject it.
	 * @note Use this for an exact path/query check or a regex. It is called only
	 * after the domain tree selected this extractor as a candidate.
	 */
	[[nodiscard]] virtual bool valid_for_url(const ParsedUrl& url) const = 0;
	/**
	 * @brief Resolve one matching URL into playable formats or delegated URLs.
	 * @param context Request context used for extractor HTTP requests. It belongs
	 * to the caller and is not retained by the extractor.
	 * @param url A URL previously accepted by valid_for_url().
	 * @return A VideoExtraction containing direct streams, delegated links, or
	 * both; a RequestError if the URL cannot be resolved.
	 * @note A direct stream's request headers belong in VideoExtraction::headers.
	 */
	virtual NetworkRequestTask<VideoExtraction> extract(RequestorContext context, ParsedUrl url) const = 0;
};

/**
 * @brief A selected extractor and the URL ready to hand to VideoExtractor::extract.
 *
 * The route owns both objects, so views returned by @ref url remain valid while
 * the route is alive.
 */
struct VideoExtractionRoute {
	/// Extractor selected by VideoExtractorStore; retained by the route.
	std::shared_ptr<VideoExtractor> extractor;
	/// Parsed URL accepted by @ref extractor.
	ParsedUrl url;
};

/**
 * @brief Snapshot-routed registry of standalone video extractors.
 *
 * Domain routing is the same static-host prefilter used by ParserStore; only a
 * matching extractor's valid_for_url() is called. Register an initial batch
 * through Edit so the tree is built once.
 */
class VideoExtractorStore {
public:
	struct ExtractorDomainEmitter {
		void operator()(const VideoExtractor& extractor, EmplaceDomainsContext& context) const {
			extractor.emplace_domains(context);
		}
	};
	using Domains = detail::DomainStore<VideoExtractor, EmplaceDomainsContext, ExtractorDomainEmitter>;
	using DomainEdit = Domains::Edit;

	class Edit {
	public:
		Edit(const Edit&) = delete;
		Edit& operator=(const Edit&) = delete;
		Edit(Edit&&) noexcept = default;
		Edit& operator=(Edit&&) noexcept = default;

		/**
		 * @brief Add an extractor to this unpublished routing draft.
		 * @param extractor Shared extractor instance to register; must be non-null
		 * and have a unique non-empty identifier.
		 * @return The stored extractor, sharing ownership with the draft and future
		 * snapshot.
		 * @throws std::invalid_argument when @p extractor is null.
		 * @throws std::logic_error when its identifier is empty or duplicates an
		 * extractor already in the draft.
		 */
		std::shared_ptr<VideoExtractor> add_extractor(std::shared_ptr<VideoExtractor> extractor);
		/**
		 * @brief Remove an extractor from this unpublished routing draft.
		 * @param identifier Stable extractor identifier to remove.
		 * @return True if an extractor was removed; false when it was absent.
		 */
		bool remove_extractor(std::string_view identifier);
		/**
		 * @brief Build and publish this draft as one routing snapshot.
		 * @throws std::logic_error when the draft was already committed or another
		 * edit published a newer snapshot first.
		 */
		void commit();

	private:
		friend class VideoExtractorStore;
		explicit Edit(DomainEdit edit) : edit_(std::move(edit)) {}
		DomainEdit edit_;
	};

	/**
	 * @brief Start an unpublished batch of extractor registrations/removals.
	 * @return A mutable Edit based on the current immutable routing snapshot.
	 * @note Call Edit::commit() once to publish all changes atomically.
	 */
	[[nodiscard]] Edit begin_edit() ANIPARSE_LIFETIMEBOUND { return Edit(domains_.begin_edit()); }
	/**
	 * @brief Register one extractor and immediately publish a new snapshot.
	 * @param extractor Shared extractor to register.
	 * @return The stored extractor.
	 * @throws std::invalid_argument or std::logic_error under the same conditions
	 * as Edit::add_extractor().
	 * @note Prefer one Edit for initial or bulk registration to build the domain
	 * tree only once.
	 */
	std::shared_ptr<VideoExtractor> add_extractor(std::shared_ptr<VideoExtractor> extractor);
	/**
	 * @brief Look up an extractor by its stable registration key.
	 * @param identifier Extractor identifier.
	 * @return The registered extractor, or nullptr when none has that identifier.
	 */
	[[nodiscard]] std::shared_ptr<VideoExtractor> find_by_key(std::string_view identifier) const;
	/**
	 * @brief Enumerate all registered extractors in identifier order.
	 * @return A value snapshot of shared extractor pointers; later edits do not
	 * alter the returned vector.
	 */
	[[nodiscard]] std::vector<std::shared_ptr<VideoExtractor>> extractors() const;
	/**
	 * @brief Parse and route an external video URL.
	 * @param url Absolute URL string to parse and route.
	 * @return The selected extractor and parsed URL, or nullopt when parsing fails,
	 * no host matches, or the candidate rejects the URL in valid_for_url().
	 */
	[[nodiscard]] std::optional<VideoExtractionRoute> route_url(std::string_view url) const;
	/**
	 * @brief Route a URL that the caller has already parsed.
	 * @param url Parsed URL, moved into the returned route on success.
	 * @return The selected route, or nullopt when no extractor accepts it.
	 */
	[[nodiscard]] std::optional<VideoExtractionRoute> route_url(ParsedUrl url) const;
	/**
	 * @brief Route an extractor's delegated link, respecting an optional pin.
	 * @param link A URL result from VideoExtraction::links.
	 * @return A route for the pinned extractor when @ref
	 * VideoExtractionLink::extractor_id is set, otherwise a normally routed
	 * route; nullopt for malformed URLs, missing pins, or rejected URLs.
	 */
	[[nodiscard]] std::optional<VideoExtractionRoute> route_link(const VideoExtractionLink& link) const;

private:
	Domains domains_;
};

} // namespace aniparse
