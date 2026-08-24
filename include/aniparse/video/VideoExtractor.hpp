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

#include <map>
#include <memory>
#include <optional>
#include <span>
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
	 * @brief Declare the extractor's catalog-visible defaults into @p sink.
	 *
	 * The extractor counterpart of Parser::emplace_catalog: the volatile
	 * catalog's authoring data comes from LIVE objects — the dumper hands each
	 * registered extractor a CatalogSink and the extractor declares its pattern
	 * sets (`sink.emplace_set<...Patterns>()`, strictly compiling every built-in
	 * literal). Extractors fetch from the incoming URL's own origin, so they
	 * declare no canonical base URL.
	 *
	 * Contract: an override DECLARES only — it calls sink methods and never
	 * stores the sink (valid only for the duration of the call). The empty
	 * default means "no catalog sets": such an extractor simply has no pattern
	 * hotfix channel, discovered at first use.
	 * @param sink The sink collecting this extractor's catalog defaults
	 * @throws std::logic_error from the sink on a cross-set key collision; a
	 *         set's broken built-in literal likewise propagates (a programming
	 *         error the dump must fail on)
	 */
	virtual void emplace_catalog([[maybe_unused]] CatalogSink& sink) const {}
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
	/**
	 * @brief A mirror view for this extractor: the volatile-catalog override
	 * keyed by this extractor's identifier() (if any) combined with the @p builtin
	 * fallback.
	 *
	 * Non-virtual; the extractor counterpart of RequestorContext::mirrors(). An
	 * extractor carries no stamped ParserConfig, so it names its own stable
	 * identifier() as the override key. Overrides keyed by extractor identifier
	 * reach the extractor only when the consumer builds its context on a
	 * ServiceState whose mirror holder carries the extractor mirror source.
	 * @param context Request context holding the mirror snapshot.
	 * @param builtin The extractor's built-in fallback base URLs.
	 * @return The combined mirror view (@see Mirrors).
	 */
	[[nodiscard]] Mirrors mirrors(const RequestorContext& context,
	                              std::span<const std::string_view> builtin) const;
	/**
	 * @brief The base URL this extractor should fetch from.
	 *
	 * Folds @ref mirrors into a selection; extractors have no per-user mirror
	 * switch (ParserConfig::alt_link is a parser concept), so the first mirror
	 * is always used. Resolve it once into a local at the start of an extraction
	 * so a concurrent catalog swap cannot split it across mirrors.
	 * @param context Request context holding the mirror snapshot.
	 * @param builtin The extractor's built-in fallback base URLs.
	 * @return The first mirror's base URL, or an empty view when there is none.
	 */
	[[nodiscard]] std::string_view base_url(const RequestorContext& context,
	                                         std::span<const std::string_view> builtin) const;
	/**
	 * @brief Build (cached) the pattern set @p T scoped to this extractor's
	 * identifier, from the volatile-catalog regex source.
	 *
	 * The extractor counterpart of RequestorContext::patterns(): an extractor
	 * carries no stamped ParserConfig, so it names its own stable identifier() as
	 * the override scope. Overrides reach the extractor only when the consumer
	 * builds its context on a ServiceState whose pattern holder carries the
	 * extractor pattern source.
	 * @tparam T Pattern set type exposing `static T create(const RegexSource&)`.
	 * @param context Request context holding the pattern snapshot.
	 * @return Shared handle to the built set; hold it for the whole extraction so
	 * a concurrent catalog swap cannot free it underneath.
	 */
	template <class T>
	[[nodiscard]] std::shared_ptr<const T> patterns(const RequestorContext& context) const {
		return context.patterns<T>(identifier());
	}
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
 * @brief Default cap on delegated-link chasing in VideoExtractorStore::extract.
 *
 * VideoExtractionLink chains let one extractor hand a URL off to another; the
 * cap bounds that recursion the way yt-dlp bounds nested URL results, so an
 * extractor→extractor cycle returns its unresolved links instead of looping
 * forever.
 */
inline constexpr int default_max_extraction_depth = 4;

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
		 * @brief Replace the volatile (catalog-supplied) domains in this draft.
		 *
		 * Each extractor's static domains (from emplace_domains()) are always
		 * kept; the volatile domains here are merged on top, keyed by extractor
		 * identifier. Applied on @ref commit as part of the same snapshot.
		 * @param volatile_domains Extractor identifier -> extra domains it should
		 * route.
		 */
		void refresh_domains(std::map<std::string, std::vector<std::string>, std::less<>> volatile_domains);
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
	 * @brief Replace the volatile (catalog-supplied) domains and rebuild routing.
	 *
	 * Each extractor's static domains (from emplace_domains()) are always kept;
	 * the volatile domains here are merged on top, keyed by extractor identifier.
	 * Passing an empty map falls back to static domains only. The routing index
	 * is rebuilt as a fresh snapshot and swapped in atomically, so extractions
	 * already in progress keep using their snapshot until they finish.
	 * @param volatile_domains Extractor identifier -> extra domains it should
	 * route.
	 */
	void refresh_domains(std::map<std::string, std::vector<std::string>, std::less<>> volatile_domains);
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
	/**
	 * @brief Route an external video URL and run the selected extractor, chasing
	 * delegated links until an extraction yields direct streams.
	 *
	 * Combines route_url() and VideoExtractor::extract() into the one call a
	 * consumer needs before playback: when the first extraction returns
	 * VideoExtraction::links instead of streams, each link is routed through
	 * route_link() and extracted in turn; the first extraction with streams wins.
	 * @param context Request context handed to the extractors; belongs to the
	 * caller and is not retained past the returned task.
	 * @param url URL to route through this store and extract.
	 * @param max_depth Cap on delegated-link hops; once reached, the deepest
	 * extraction is returned unresolved (streams empty, links intact), so an
	 * extractor cycle cannot loop.
	 * @return The first extraction that yields direct streams; the deepest
	 * extraction (streams empty) when every delegated link failed to resolve or
	 * the depth cap cut the chain; RequestErrorCode::NotImplemented when no
	 * extractor accepts @p url; or the extractor's own RequestError.
	 * @note Member coroutine: the store must outlive the returned task, under the
	 * same caller-ownership convention as VideoExtractor::extract.
	 */
	[[nodiscard]] NetworkRequestTask<VideoExtraction> extract(
	    RequestorContext context, ParsedUrl url, int max_depth = default_max_extraction_depth) const;
	/**
	 * @brief Overload taking an unparsed URL string.
	 *
	 * The URL is taken by value on purpose: a NetworkRequestTask is lazy and its
	 * body — including the parse — starts only when the task is first awaited,
	 * so a borrowed view could dangle before the parse ever runs.
	 * @param context Request context handed to the extractors; belongs to the
	 * caller and is not retained past the returned task.
	 * @param url Absolute URL string; parsed before routing.
	 * @param max_depth Cap on delegated-link hops, as in the ParsedUrl overload.
	 * @return As in the ParsedUrl overload; RequestErrorCode::InvalidArguments
	 * when @p url does not parse.
	 * @note Same lifetime convention as the ParsedUrl overload.
	 */
	[[nodiscard]] NetworkRequestTask<VideoExtraction> extract(
	    RequestorContext context, std::string url, int max_depth = default_max_extraction_depth) const;

private:
	Domains domains_;
};

} // namespace aniparse
