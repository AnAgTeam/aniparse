/*
 * Copyright (C) 2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once

#include "aniparse/ClientContext.hpp"
#include "aniparse/types/Headers.hpp"
#include "aniparse/types/Response.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

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
 * The adapter prepares @ref request and constructs an isolated @ref handle for
 * this open operation. The handle is non-null on success and is owned by the
 * caller until EOF, cancellation, or a transport failure.
 */
struct OpenedResource {
	/// Final request to give to the streaming transport.
	ResourceRequest request;
	/// Per-open byte and metadata transformer.
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
	virtual Response<void> write(std::span<const std::byte> bytes) = 0;
};

/**
 * @brief Isolated Init/Update/Final state for one resource response.
 *
 * ResourceAdapter::open() creates one handle per independent resource open.
 * The transport calls on_response() once before the first update(), calls
 * update() serially for each input block, and calls final() exactly once after
 * successful EOF. On cancellation or failure it destroys the handle without
 * calling final().
 */
class ResourceHandle {
public:
	virtual ~ResourceHandle() = default;

	/**
	 * @brief Inspect or normalize response metadata before body delivery begins.
	 * @param metadata Mutable transport metadata for this response.
	 * @return Success when body delivery may begin, or a RequestError when the
	 * response is unusable.
	 * @note The default implementation leaves @p metadata unchanged.
	 */
	virtual Response<void> on_response(ResourceMetadata& metadata) {
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
	virtual Response<void> update(std::span<const std::byte> input, ResourceOutput& output) {
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
	virtual Response<void> final(ResourceOutput& output) {
		return {};
	}
};

/**
 * @brief Factory for source-specific request preparation and response transforms.
 *
 * Adapters are reusable and retain no state for one open operation; all such
 * state belongs to the ResourceHandle returned in OpenedResource. Manifest
 * interception, internal URL routing, and streaming transport remain outside
 * this interface.
 */
class ResourceAdapter {
public:
	virtual ~ResourceAdapter() = default;

	/**
	 * @brief Prepare one resource request and create its isolated transformer.
	 * @param context Request context available to adapters that need asynchronous
	 * preparation, cache policy, redirect policy, or cancellation state.
	 * @param resource Source descriptor and optional manifest ancestry for this
	 * operation.
	 * @return An OpenedResource with a final request and non-null byte-transform
	 * handle, or a RequestError when preparation cannot complete.
	 * @note This is the only coroutine phase of the adapter lifecycle. Both
	 * parameters are values because the returned task may outlive its caller.
	 */
	[[nodiscard]] virtual NetworkRequestTask<OpenedResource> open(
	    RequestorContext context,
	    ResourceContext resource) const = 0;
};

} // namespace aniparse
