/*
 * Copyright (C) 2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once

#include "aniparse/runtime/RequestContext.hpp"
#include "aniparse/net/Headers.hpp"
#include "aniparse/utility/Result.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace aniparse {

/**
 * @brief The semantic role of a resource, independent of its URL suffix and
 * server-provided MIME type.
 */
enum class MediaResourceKind {
	Image,             ///< A single encoded image.
	HLSPlaylist,       ///< An HLS master or media playlist.
	HLSInitialization, ///< An HLS fMP4 initialization section.
	HLSSegment,        ///< An HLS media segment.
	Subtitle,          ///< A timed-text resource.
	Unknown,           ///< A resource whose semantic role is not yet known.
};

/**
 * @brief Parser-provided description of one resource that a consumer may load.
 *
 * A descriptor carries the source-specific request data, but no playback-session
 * IDs or platform types. Manifest handling and routing belong to layers above
 * this interface.
 */
struct MediaResource {
	/// Absolute source URL before the adapter applies any request-time rewrite.
	std::string url;
	/// Headers inherited from the selected source, such as Referer or authorization.
	Headers headers;
	/// Semantic role selected by the parser or manifest layer.
	MediaResourceKind kind = MediaResourceKind::Unknown;
	/// Expected output MIME type when the URL suffix or HTTP response is misleading.
	std::optional<std::string> content_type_hint;
};

/**
 * @brief Context for opening one resource discovered by a manifest or selected
 * directly by a consumer.
 */
struct ResourceContext {
	/// Absolute URL of the manifest that discovered @ref resource, if any.
	std::optional<std::string> parent_url;
	/// The descriptor to prepare and transform.
	MediaResource resource;
};

/**
 * @brief The final HTTP request produced by a resource adapter.
 */
struct ResourceRequest {
	/// Absolute URL to send through the transport.
	std::string url;
	/// Final headers to send with the request.
	Headers headers;
};

/**
 * @brief Metadata supplied by the transport before it delivers the response body.
 *
 * A handle may normalize this object in on_response(), for example by replacing
 * an image-like suffix's incorrect MIME type with @c video/iso.segment.
 */
struct ResourceMetadata {
	/// HTTP status code, or zero when no status line was received.
	long status_code = 0;
	/// Response headers surfaced by the transport.
	Headers headers;
	/// MIME type exposed to the resource consumer after handle normalization.
	std::string content_type;
	/// Resource size when known and still accurate after transformation.
	std::optional<std::int64_t> content_length;
	/// Final URL after redirects, when known.
	std::optional<std::string> effective_url;
};

class ResourceHandle;

/**
 * @brief Result of asynchronously opening one resource.
 *
 * The adapter prepares @ref request and may construct an isolated @ref handle
 * for this open operation. A null @ref handle means the transport must pass
 * response metadata and bytes through unchanged. A non-null handle is owned by
 * the caller until EOF, cancellation, or a transport failure.
 */
struct OpenedResource {
	/// Final request to give to the streaming transport.
	ResourceRequest request;
	/// Optional per-open byte and metadata transformer.
	/// A null value requests byte-for-byte response passthrough.
	std::unique_ptr<ResourceHandle> handle;
};

/**
 * @brief Synchronous destination for transformed response bytes.
 *
 * Implementations consume @p bytes before write() returns. A resource handle
 * must not retain this object or the supplied span. This synchronous borrowing
 * is safe because neither update() nor final() is a coroutine.
 */
class ResourceOutput {
public:
	virtual ~ResourceOutput() = default;

	/**
	 * @brief Consume one contiguous block of transformed bytes.
	 * @param bytes Borrowed bytes valid only for the duration of this call.
	 * @return Success after the block is consumed, or a RequestError when the
	 * consumer cannot accept more bytes.
	 * @note A successful return transfers no ownership; the caller retains bytes.
	 */
	virtual Result<void> write(std::span<const std::byte> bytes) = 0;

protected:
	ResourceOutput() = default;
	ResourceOutput(const ResourceOutput&) = default;
	ResourceOutput(ResourceOutput&&) = default;
	ResourceOutput& operator=(const ResourceOutput&) = default;
	ResourceOutput& operator=(ResourceOutput&&) = default;
};

/**
 * @brief Isolated Init/Update/Final state for one resource response.
 *
 * When present, ResourceAdapter::open() creates one handle per independent
 * resource open. The transport calls on_response() once before the first
 * update(), calls update() serially for each input block, and calls final()
 * exactly once after successful EOF. On cancellation or failure it destroys
 * the handle without calling final(). A null handle means the resource needs
 * request preparation only and the transport passes its response through.
 *
 * Each handle is confined to one response: callbacks must not overlap, even if
 * independent resources are downloaded concurrently. A consumer may pause body
 * delivery between update() calls when its output queue is full. After any
 * callback returns an error, the consumer stops delivery and destroys the handle;
 * it must not retry that input block or call final() to flush a failed response.
 * The consumer bounds its own queued output and cancels the request if it cannot
 * accept more data. A handle is responsible for bounding its internal buffers.
 */
class ResourceHandle {
public:
	ResourceHandle() = default;
	virtual ~ResourceHandle() = default;

    /**
     * @brief Whether response bytes require this handle's update/final pipeline.
     * @return False only when update is byte-for-byte passthrough and final emits
     * nothing; consumers may then bypass body delivery while still calling
     * on_response. True conservatively preserves existing transform behavior.
     * @note The result is stable for the handle's lifetime. Metadata-only handles
     * override this to avoid copying media bytes through a language bridge.
     */
    [[nodiscard]] virtual bool transforms_body() const noexcept { return true; }


	/**
	 * @brief Inspect or normalize response metadata before body delivery begins.
	 * @param metadata Mutable transport metadata for this response.
	 * @return Success when body delivery may begin, or a RequestError when the
	 * response is unusable.
	 * @note The default implementation leaves @p metadata unchanged.
	 */
	virtual Result<void> on_response(ResourceMetadata& metadata) {
		return {};
	}

	/**
	 * @brief Transform one consecutive input block and synchronously emit output.
	 * @param input Borrowed source bytes valid only for this call.
	 * @param output Destination that consumes every emitted block synchronously.
	 * @return Success after all output is consumed, or a RequestError from the
	 * transform or @p output.
	 * @note The default implementation is a zero-copy passthrough. Implementations
	 * may emit zero, one, or many blocks and must not retain @p input or @p output.
	 */
	virtual Result<void> update(std::span<const std::byte> input, ResourceOutput& output) {
		return output.write(input);
	}

	/**
	 * @brief Flush output retained between update() calls after successful EOF.
	 * @param output Destination that consumes every emitted block synchronously.
	 * @return Success after all retained output is consumed, or a RequestError from
	 * the transform or @p output.
	 * @note The default implementation emits nothing. This method is not called
	 * when the transport fails or the consumer cancels the resource.
	 */
	virtual Result<void> final(ResourceOutput& output) {
		return {};
	}

protected:
	ResourceHandle(const ResourceHandle&) = default;
	ResourceHandle(ResourceHandle&&) = default;
	ResourceHandle& operator=(const ResourceHandle&) = default;
	ResourceHandle& operator=(ResourceHandle&&) = default;
};

/**
 * @brief Factory for source-specific request preparation and response transforms.
 *
 * Adapters are reusable and retain no state for one open operation; all such
 * state belongs to the ResourceHandle returned in OpenedResource. Manifest
 * interception, internal URL routing, and streaming transport remain outside
 * this interface.
 *
 * The consumer owns the playback operation and its streaming transport scope.
 * Requests for a selected stream and its discovered resources share that scope,
 * enabling connection reuse without requiring a single connection or serial
 * downloads. Independent playback operations have independent cancellation.
 * Cancelling one resource must not cancel its siblings; ending playback cancels
 * pending open operations and downloads and releases their handles. Platform
 * sessions and internal routing URLs are consumer details, not adapter state.
 * Additional preparation requests made through RequestContext are not promised
 * to use the consumer's streaming connection pool.
 */
class ResourceAdapter {
public:
	virtual ~ResourceAdapter() = default;

	/**
	 * @brief Prepare one resource request and optionally create its isolated transformer.
	 * @param context Request context available to adapters that need asynchronous
	 * preparation, cache policy, redirect policy, or cancellation state.
	 * @param resource Source descriptor and optional manifest ancestry for this
	 * operation.
	 * @return An OpenedResource with a final request and an optional byte-transform
	 * handle, or a RequestError when preparation cannot complete. A null handle
	 * requests byte-for-byte response passthrough.
	 * @note This is the only coroutine phase of the adapter lifecycle. Both
	 * parameters are values because the returned task may outlive its caller.
	 * The coroutine borrows this adapter: the caller must retain the receiver until
	 * the task completes or is destroyed. Concurrent opens must use independent
	 * per-response handles; cancelling an open must not invalidate another open.
	 */
	[[nodiscard]] virtual RequestTask<OpenedResource> open(
	    RequestContext context,
	    ResourceContext resource) const = 0;

	/**
	 * @brief Combine nested resource policies into one adapter.
	 * @param adapters Policies ordered from the outer source to the innermost
	 *        resource provider; null entries are ignored.
	 * @return Null when every entry is null, the sole non-null entry unchanged,
	 *         or an adapter that opens policies in order and applies their response
	 *         transforms in reverse order.
	 * @note The returned adapter is immutable and safe to share. A response flows
	 *       from the innermost provider back through the outer policies, so inner
	 *       response transformations run before outer ones.
	 */
	[[nodiscard]] static std::shared_ptr<const ResourceAdapter> compose(
	    std::vector<std::shared_ptr<const ResourceAdapter>> adapters);

protected:
	ResourceAdapter() = default;
	ResourceAdapter(const ResourceAdapter&) = default;
	ResourceAdapter(ResourceAdapter&&) = default;
	ResourceAdapter& operator=(const ResourceAdapter&) = default;
	ResourceAdapter& operator=(ResourceAdapter&&) = default;
};

} // namespace aniparse
