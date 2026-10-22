/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
// The data model half; this header adds the getter interfaces on top of it.
#include "aniparse/manga/MangaModel.hpp"

#include "aniparse/types/Flags.hpp"
#include "aniparse/types/Pagination.hpp"
#include "aniparse/types/Response.hpp"
#include "aniparse/types/Serialization.hpp"
#include "aniparse/types/Authentication.hpp"
#include "aniparse/ClientContext.hpp"

namespace aniparse {


/// A selectable mirror (base URL) for a source. Selected by index via
/// RequestorContext::alt_link(). Kept as a struct so per-mirror metadata can be
/// added later without changing the collection type.
struct AltLink {
	/// Base URL of this mirror — the host requests are actually sent to when the
	/// mirror is selected. What a mirror picker shows and selects by.
	std::string url;
};

/**
 * @brief The mirror choices to show the user for a @p builtin mirror list, with
 * any live catalog override applied.
 *
 * Equals @p builtin (as AltLinks) when no catalog overrides the context's parser;
 * otherwise the overridden set — so a picker reflects the mirrors actually fetched
 * from, not a stale built-in list. Generic over any source: the override is keyed
 * by the config's parser id carried in @p context. @see ParserProfile::mirror_choices.
 * @param builtin The source's built-in base URLs (its single source of truth).
 * @param context Context carrying the current mirror snapshot and parser id.
 * @return The resolved mirror descriptors, in selection order.
 */
[[nodiscard]] std::vector<AltLink> resolve_alt_links(std::span<const std::string_view> builtin,
                                                     const RequestorContext& context);

/**
 * @brief How one manga getter advertises what it can do.
 *
 * Most MangaGetter methods are optional: a parser that does not override one
 * inherits a default that reports RequestErrorCode::NotImplemented. The flags
 * here are how a parser declares, up front and without a request, which of those
 * it actually implements — so a consumer consults MangaGetter::compatibilities()
 * before offering the corresponding action, instead of discovering the gap by
 * spending a failed request. The NotImplemented default remains the backstop for
 * a caller that skips the check.
 * @see compatibilities_flags
 */
struct MangaGetterCompatibilities {
	/// The capability bits this getter claims, drawn from the
	/// aniparse::compatibilities_flags vocabulary (e.g. supports_commenting gates
	/// MangaGetter::comments). Default = nothing optional claimed.
	CompatibilitiesFlags flags = compatibilities_flags::default_flags;
};



/**
 * @brief Interface for getting one specific manga information.
 *
 * A getter is IMMUTABLE once constructed: its methods read the fields the constructor
 * set and never write them. Anything a getter would otherwise want to remember — a
 * fetched catalog, a warmed snapshot, a preview it was handed — belongs either in the
 * constructor (data known up front, e.g. the short-card info a search result carries) or
 * in RequestorContext::resources() (data fetched at request time, shared and keyed).
 *
 * That is a contract, not an observation: it is what makes concurrent calls on one getter
 * safe without a lock. A consumer may hold a getter and call it from more than one thread,
 * so a getter that caches into a member introduces a data race its caller has no way to
 * see coming.
 */
struct MangaGetter {
	virtual ~MangaGetter() = default;

	/**
	 * @brief The optional operations this getter implements.
	 * Every getter must declare them; the answer is fixed at construction, so the
	 * call is synchronous and performs no request. A consumer reads it before
	 * calling any optional method.
	 * @return The capability flags of this getter.
	 */
	virtual MangaGetterCompatibilities compatibilities() const noexcept = 0;

	/**
	 * @brief The short-card info this getter was handed, if it was handed any.
	 *
	 * Free and synchronous: it reads what the constructor set and never requests
	 * anything. A getter produced by a listing (search or latest) usually carries the
	 * card the listing already returned; one built from a URL or from serialized
	 * identity carries nothing and answers nullopt.
	 *
	 * Nullopt means "nothing known yet", not "no such data" — the caller decides what
	 * that is worth: draw a placeholder card, or spend info() on the full record. That
	 * choice is deliberately the caller's, because it is the caller that knows whether
	 * it is drawing one detail view or fifty rows; a preview that quietly fell back to
	 * info() would turn a list of restored library entries into fifty detail requests.
	 *
	 * The value may leave fields empty that info() fills, so a detail view calls info()
	 * regardless.
	 * @return The card info, or nullopt when this getter has none.
	 */
	[[nodiscard]] virtual std::optional<MangaInfo> preview_info() const noexcept;

	/**
	 * @brief Full metadata of this manga. Every getter must implement it — it is
	 * the one operation no source can omit.
	 * @param context Client to perform HTTP requests
	 * @return The complete MangaInfo, or a RequestError (NotFound when the item is
	 *         gone, UnexpectedResponse when the source's shape changed, ...)
	 */
	virtual NetworkRequestTask<MangaInfo> info(RequestorContext context) const = 0;

	/**
	 * @brief The translations this manga is available in, paginated via @p filters.
	 * The pre-chapter selection axis for sources that carry the same manga in
	 * several languages or from several translator teams: a MangaTranslationInfo::id
	 * from here goes back into chapters_info() and chapter_pages(). By default
	 * reports RequestErrorCode::NotImplemented, which is what a source with no
	 * translation axis leaves in place.
	 * @param context Client to perform HTTP requests
	 * @param filters Pagination (and any supported ordering) for the list
	 * @return A page of translations, or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<MangaTranslationInfo>> translation_info(
	    RequestorContext context,
	    GetFilters filters) const;

	/**
	 * @brief The chapters of this manga, paginated via @p filters and optionally
	 * narrowed to one @p translation. By default reports
	 * RequestErrorCode::NotImplemented: a metadata source that catalogs manga
	 * without hosting them has no chapter list to give.
	 * @param context Client to perform HTTP requests
	 * @param filters Pagination (and any supported ordering) for the chapter list
	 * @param translation Restrict the list to one translation, identified by a
	 *        MangaTranslationInfo::id from translation_info(); nullopt = the
	 *        source's own default. Sources with no translation axis ignore it.
	 * @return A page of chapter descriptors — each carrying the handle that opens
	 *         it (@see MangaChapterInfo::ref) — or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<MangaChapterInfo>> chapters_info(
	    RequestorContext context,
	    GetFilters filters,
	    std::optional<MangaTranslationID> translation = std::nullopt) const;

	/**
	 * @brief User comments on the manga, paginated via @p filters.
	 * Available when the parser advertises supports_commenting; by default reports
	 * NotImplemented. Top-level comments page through @p filters like any list;
	 * thread replies come inline on each @ref Comment.
	 * @param context Client to perform HTTP requests
	 * @param filters Pagination (and any supported ordering) for the comment list
	 * @return A page of comments, or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<Comment>> comments(
	    RequestorContext context,
	    GetFilters filters) const;

	/**
	 * @brief The works the source itself declares as related to this one — a sequel, a
	 * spin-off, another entry of the same franchise, or the story in another medium
	 * (its anime adaptation) — paginated via @p filters. Distinct from similar(): a
	 * relation the source asserts, not a recommendation it computes.
	 *
	 * Returns descriptors, not getters, because a relation may leave this medium and a
	 * MangaGetter cannot address an anime. A RelatedWork carries whatever the source
	 * knows: a title, a handle when this parser can open the work itself, external ids
	 * when it cannot but knows where the work lives. @see RelatedWork
	 *
	 * By default reports RequestErrorCode::NotImplemented.
	 * @param context Client to perform HTTP requests
	 * @param filters Pagination (and any supported ordering) for the list
	 * @return A page of related works, or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<RelatedWork>> related(
	    RequestorContext context,
	    GetFilters filters) const;

	/**
	 * @brief The manga the source recommends as similar to this one, paginated via
	 * @p filters, each as a ready-to-use getter. The recommendation axis, as opposed
	 * to the declared relations of related(). By default reports
	 * RequestErrorCode::NotImplemented.
	 * @param context Client to perform HTTP requests
	 * @param filters Pagination (and any supported ordering) for the list
	 * @return A page of getters for the similar manga, or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> similar(
	    RequestorContext context,
	    GetFilters filters) const;

	/**
	 * @brief The pages of one chapter — the read path of the manga domain.
	 * The chapter is identified by round-trip: pass the MangaChapterInfo::ref() of
	 * an item from chapters_info(), whose id may carry a source-specific handle.
	 * @see MangaChapterRef
	 *
	 * Every getter must implement it, but implementing it does not mean hosting
	 * pages: a metadata source that catalogs manga without serving them overrides
	 * this to report RequestErrorCode::NotImplemented explicitly.
	 * @param context Client to perform HTTP requests
	 * @param filters Pagination (and any supported ordering) over the page list
	 * @param chapter Identity of the chapter to open, from MangaChapterInfo::ref()
	 * @param translation The translation the chapter belongs to, from
	 *        translation_info(); nullopt = the source's own default. Ignored by
	 *        sources with no translation axis.
	 * @return A page of MangaPage fetch descriptors, or a RequestError
	 */
	virtual NetworkRequestTask<PageResults<MangaPage>> chapter_pages(
	    RequestorContext context,
	    MangaChapterRef chapter,
	    GetFilters filters,
	    std::optional<MangaTranslationID> translation = std::nullopt) const = 0;

	/**
	 * @brief Return this manga's stable, public URL.
	 *
	 * The returned absolute URL must route through ParserStore::route_url() and
	 * reconstruct an equivalent getter through CatalogRootGetter::parse_url(). This
	 * method performs no I/O: @p context is supplied only to read the immutable
	 * volatile-catalog snapshot (in particular canonical_base_url), never to make a
	 * request. The getter's own identity supplies the remaining path/query data.
	 * @param context Context carrying the parser-scoped catalog snapshot.
	 * @return An absolute public URL that identifies this manga.
	 */
	[[nodiscard]] virtual std::string canonical_url(RequestorContext context) const = 0;

	/**
	 * @brief This getter's identity in a form that survives the process, so a
	 * consumer can store the manga in a library and rebuild the getter later with
	 * CatalogRootGetter::from_serialized(). Every getter must implement it.
	 *
	 * Encodes identity only, never the fetched info — a restored getter refetches.
	 * The blob is opaque to the consumer and means nothing away from the parser that
	 * produced it. @see SerializedGetterData for the durability contract it carries.
	 *
	 * Coroutine-returning for uniformity with the rest of the interface; an
	 * implementation answers from what its constructor was given and normally
	 * performs no request.
	 * @return The serialized identity, or a RequestError
	 */
	virtual NetworkRequestTask<SerializedGetterData> serialize() const = 0;
};



} // namespace aniparse
