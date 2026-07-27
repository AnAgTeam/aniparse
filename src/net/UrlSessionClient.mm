/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
// The NSURLSession HTTP backend. Compiled only when the backend is enabled (CMake
// also drops it from the source list then); this guard makes an accidental build a
// clean no-op rather than a Foundation-not-found error.
#if defined(ANIPARSE_NSURLSESSION_BACKEND) && ANIPARSE_NSURLSESSION_BACKEND

#import <Foundation/Foundation.h>

#include "aniparse/net/UrlSessionClient.hpp"
#include "aniparse/net/CookieStore.hpp"
#include "aniparse/types/Headers.hpp"
#include "aniparse/utility/UrlEncode.hpp"

#include <atomic>
#include <chrono>
#include <coroutine>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

using asyncnet::NetworkTask;
// NB: don't `using asyncnet::Response` here — inside namespace aniparse the
// unqualified name would resolve to aniparse::Response<T> (the expected alias).

// The session delegate. Two jobs, both about NOT taking Foundation's default:
// suppressing redirects, and answering auth challenges (a debugging proxy's TLS,
// or proxy credentials). Stateless apart from its configuration, which is set once
// before the session is built and only read afterwards — which is what makes it
// safe on the concurrent delegate queue this backend uses.
@interface AniparseUrlSessionDelegate : NSObject <NSURLSessionDataDelegate>
@property(nonatomic, assign) BOOL verifySsl;
@property(nonatomic, copy) NSString* proxyUser;
@property(nonatomic, copy) NSString* proxyPassword;
@end

@implementation AniparseUrlSessionDelegate

// Surface the 3xx rather than following it, matching the curl backend (whose
// FollowLocation is off, since asyncnet only enables it for an explicit redirect
// budget that nothing sets). A parser therefore reads Location itself, and does so
// identically on every platform. Passing nil finishes the task with the redirect
// response itself.
- (void)URLSession:(NSURLSession*)session
                          task:(NSURLSessionTask*)task
    willPerformHTTPRedirection:(NSHTTPURLResponse*)response
                    newRequest:(NSURLRequest*)request
             completionHandler:(void (^)(NSURLRequest* _Nullable))completionHandler {
	completionHandler(nil);
}

- (void)URLSession:(NSURLSession*)session
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                  NSURLCredential* _Nullable))completionHandler {
	NSString* method = challenge.protectionSpace.authenticationMethod;

	// A cleared verify_ssl means a debugging proxy is terminating TLS with a
	// certificate nothing can validate; accepting it is the whole point of the flag.
	if ([method isEqualToString:NSURLAuthenticationMethodServerTrust]) {
		SecTrustRef trust = challenge.protectionSpace.serverTrust;
		if (self.verifySsl || trust == NULL) {
			completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
		} else {
			completionHandler(NSURLSessionAuthChallengeUseCredential,
			                  [NSURLCredential credentialForTrust:trust]);
		}
		return;
	}

	if (challenge.previousFailureCount == 0 && self.proxyUser != nil
	    && ([method isEqualToString:NSURLAuthenticationMethodHTTPBasic]
	        || [method isEqualToString:NSURLAuthenticationMethodHTTPDigest]
	        || [method isEqualToString:NSURLAuthenticationMethodNTLM])) {
		NSString* password = self.proxyPassword != nil ? self.proxyPassword : @"";
		completionHandler(NSURLSessionAuthChallengeUseCredential,
		                  [NSURLCredential credentialWithUser:self.proxyUser
		                                             password:password
		                                          persistence:NSURLCredentialPersistenceForSession]);
		return;
	}

	completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
}
@end

namespace aniparse {
namespace {

NSString* to_ns(std::string_view text) {
	return [[NSString alloc] initWithBytes:text.data()
	                                length:static_cast<NSUInteger>(text.size())
	                              encoding:NSUTF8StringEncoding];
}

std::string from_ns(NSString* text) {
	if (text == nil) {
		return {};
	}
	const char* utf8 = [text UTF8String];
	return utf8 ? std::string(utf8) : std::string();
}

// Bodies are arbitrary bytes (an image, a gzip blob), so go through the length
// rather than any NUL-terminated view.
std::string from_data(NSData* data) {
	if (data == nil || data.length == 0) {
		return {};
	}
	return std::string(static_cast<const char*>(data.bytes), data.length);
}

// Map a Foundation transport error onto a coarse RequestErrorCode, matching the
// curl backend's split: cancellation is deliberate, everything else collapses to
// one NetworkError, since the exact code carries nothing the consumer branches on.
RequestErrorCode ns_error_to_code(NSError* error) {
	if ([error.domain isEqualToString:NSURLErrorDomain] && error.code == NSURLErrorCancelled) {
		return RequestErrorCode::Cancelled;
	}
	return RequestErrorCode::NetworkError;
}

Cookie to_cookie(NSHTTPCookie* ns_cookie) {
	Cookie cookie;
	cookie.name      = from_ns(ns_cookie.name);
	cookie.value     = from_ns(ns_cookie.value);
	cookie.path      = from_ns(ns_cookie.path);
	cookie.secure    = ns_cookie.isSecure;
	cookie.http_only = ns_cookie.isHTTPOnly;

	// NSHTTPCookie spells the subdomain opt-in as a leading dot on the domain, the
	// Netscape format as a separate flag. Canonicalize to the flag so a cookie stored
	// here is byte-identical to the one the curl backend would store.
	std::string domain = from_ns(ns_cookie.domain);
	if (domain.starts_with('.')) {
		domain.erase(0, 1);
		cookie.include_subdomains = true;
	}
	cookie.domain = std::move(domain);

	if (ns_cookie.expiresDate != nil) {
		const auto since_epoch =
		    std::chrono::duration<double>([ns_cookie.expiresDate timeIntervalSince1970]);
		cookie.expires = std::chrono::system_clock::time_point(
		    std::chrono::duration_cast<std::chrono::system_clock::duration>(since_epoch));
	}
	return cookie;
}

/**
 * The state a transfer's completion block and its awaiting coroutine share.
 *
 * Shared rather than parked on the coroutine frame because the block can outlive the
 * frame: ~CancellingTask destroys a suspended coroutine unconditionally, so a task
 * dropped mid-flight would leave the block holding freed memory. Holding the state by
 * shared_ptr on both sides makes that a non-event — the awaiter marks it abandoned on
 * the way out and the block finds nobody home.
 */
struct TransferState {
	enum class Stage {
		running,   ///< Started; nobody has claimed the resume yet.
		suspended, ///< The coroutine parked; the completion block owes it a resume.
		completed, ///< The block delivered; the result is readable.
		abandoned, ///< The awaiting frame is gone; the result is nobody's and no one may be resumed.
	};

	std::atomic<Stage> stage{ Stage::running };
	std::coroutine_handle<> handle;
	Response<ResponseData> result = ResponseData{};

	/// Called from the session's completion block, on the delegate queue.
	void complete(Response<ResponseData> value) {
		result = std::move(value);
		// Only a coroutine that actually parked may be resumed — and only if it is
		// still there to resume.
		if (stage.exchange(Stage::completed) == Stage::suspended) {
			handle.resume();
		}
	}

	/// Called as the awaiting frame goes away, so a late completion resumes nothing.
	void abandon() { stage.store(Stage::abandoned); }
};

/**
 * The bridge: starts an NSURLSessionDataTask and parks the coroutine until the
 * session's completion block hands the outcome back through the shared state.
 */
class TransferAwaiter {
public:
	TransferAwaiter(std::shared_ptr<TransferState> state, NSURLSessionDataTask* task,
	                std::stop_token token)
	    : state_(std::move(state)), task_(task), token_(std::move(token)) {}

	TransferAwaiter(const TransferAwaiter&)            = delete;
	TransferAwaiter& operator=(const TransferAwaiter&) = delete;

	~TransferAwaiter() {
		// Whether the transfer finished or this frame is being torn down mid-flight,
		// leaving is the same statement: nobody is left to resume. Cancelling a task
		// that already completed is a no-op, so this needs no special case for the
		// ordinary path.
		state_->abandon();
		[task_ cancel];
	}

	bool await_ready() const noexcept { return false; }

	bool await_suspend(std::coroutine_handle<> awaiting) {
		state_->handle = awaiting;
		[task_ resume];

		// Arm cancellation only now that the task is actually running. Cancelling a
		// task that was never resumed is not guaranteed to deliver a completion, and a
		// lost completion here is a coroutine parked forever.
		cancel_.emplace(token_, [task = task_] { [task cancel]; });

		// Hand off to the completion block, which may already be running on another
		// thread — the delegate queue is concurrent. Whoever arrives second owns the
		// resume: if the block finished first, this CAS fails and the coroutine simply
		// carries on inline instead of parking for a wake-up that will never come.
		TransferState::Stage expected = TransferState::Stage::running;
		return state_->stage.compare_exchange_strong(expected, TransferState::Stage::suspended);
	}

	Response<ResponseData> await_resume() { return std::move(state_->result); }

private:
	std::shared_ptr<TransferState> state_;
	NSURLSessionDataTask* task_ = nil;
	std::stop_token token_;
	std::optional<std::stop_callback<std::function<void()>>> cancel_;
};

// Flatten Foundation's response into the neutral ResponseData that crosses into
// parser code.
//
// NB repeated header names: allHeaderFields is a dictionary, so Foundation has
// already joined repeats (notably several Set-Cookie lines) with ", " and there is
// no public API for the raw block. Cookies are therefore harvested through
// +cookiesWithResponseHeaderFields:, which is built to read exactly this joined
// shape; other repeated headers reach a parser joined.
ResponseData to_response_data(NSHTTPURLResponse* response, NSData* body) {
	ResponseData data;
	data.status_code = static_cast<long>(response.statusCode);
	data.body        = from_data(body);

	NSDictionary* fields = response.allHeaderFields;
	for (id key in fields) {
		id value = fields[key];
		if ([key isKindOfClass:[NSString class]] && [value isKindOfClass:[NSString class]]) {
			data.headers.append(from_ns(key), from_ns(value));
		}
	}

	std::string effective_url = from_ns(response.URL.absoluteString);
	if (!effective_url.empty()) {
		data.effective_url = std::move(effective_url);
	}
	return data;
}

} // namespace

struct UrlSessionClient::Impl {
	AniparseUrlSessionDelegate* delegate = nil;
	NSURLSession* session                = nil;
	/// Guards the session swap in set_config: a request takes a strong reference to
	/// the session it starts on, so it keeps running on that one while later requests
	/// go out on the replacement.
	mutable std::mutex session_mutex;
	std::atomic<uint32_t> max_retries{ UrlSessionClient::default_max_retries };

	NSURLSession* current_session() const {
		std::lock_guard lock(session_mutex);
		return session;
	}

	void replace_session(NSURLSession* new_session) {
		std::lock_guard lock(session_mutex);
		session = new_session;
	}
};

namespace {

NSURLSession* make_session(const ClientConfig& config, AniparseUrlSessionDelegate* delegate) {
	// Ephemeral: no on-disk cache, cookie store or credential store. This backend
	// keeps cookies in the jar (@see UrlSessionClient) and caching is a deliberate
	// layer above the client, so a session-owned store would only be a second,
	// invisible one.
	NSURLSessionConfiguration* session_config = [NSURLSessionConfiguration ephemeralSessionConfiguration];
	session_config.HTTPShouldSetCookies       = NO;
	session_config.HTTPCookieAcceptPolicy     = NSHTTPCookieAcceptPolicyNever;
	session_config.HTTPCookieStorage          = nil;
	session_config.URLCache                   = nil;
	session_config.requestCachePolicy         = NSURLRequestReloadIgnoringLocalCacheData;

	// Unset leaves Foundation's own default (60s), which the contract allows and
	// which is saner than the curl backend's effective "no timeout".
	if (config.timeout) {
		session_config.timeoutIntervalForRequest =
		    std::chrono::duration<double>(*config.timeout).count();
	}

	if (config.proxy) {
		NSString* host  = to_ns(config.proxy->host);
		NSNumber* port  = @(config.proxy->port);
		// Credentials do not belong in this dictionary — they are answered as an auth
		// challenge by the delegate.
		session_config.connectionProxyDictionary = @{
			@"HTTPEnable" : @YES,
			@"HTTPProxy" : host,
			@"HTTPPort" : port,
			@"HTTPSEnable" : @YES,
			@"HTTPSProxy" : host,
			@"HTTPSPort" : port,
		};
	}

	// Concurrent on purpose. A nil delegateQueue gets a SERIAL one, which would funnel
	// every completion — and so every parse resumed from it — through a single thread.
	// That is precisely the bottleneck the curl backend has, where the poll loop parses
	// each response itself.
	NSOperationQueue* queue           = [[NSOperationQueue alloc] init];
	queue.maxConcurrentOperationCount = NSOperationQueueDefaultMaxConcurrentOperationCount;
	queue.name                        = @"aniparse.urlsession";

	return [NSURLSession sessionWithConfiguration:session_config delegate:delegate delegateQueue:queue];
}

} // namespace

UrlSessionClient::UrlSessionClient() : impl_(std::make_unique<Impl>()) {
	impl_->delegate           = [[AniparseUrlSessionDelegate alloc] init];
	impl_->delegate.verifySsl = YES;
	impl_->replace_session(make_session(ClientConfig{}, impl_->delegate));
}

UrlSessionClient::~UrlSessionClient() {
	// Lets in-flight transfers finish and releases the delegate reference the session
	// holds; without it the session keeps both alive for the life of the process.
	if (NSURLSession* session = impl_->current_session()) {
		[session finishTasksAndInvalidate];
	}
}

void UrlSessionClient::set_config(ClientConfig config) {
	impl_->max_retries.store(config.max_retries.value_or(default_max_retries));

	// The delegate is shared with the session already running, so a swap would be seen
	// by in-flight requests too. Give the new session a delegate of its own instead,
	// which keeps "requests already in flight keep the configuration they started
	// with" true for the delegate-borne settings as well.
	AniparseUrlSessionDelegate* delegate = [[AniparseUrlSessionDelegate alloc] init];
	delegate.verifySsl                   = config.flags.has(client_config_flags::verify_ssl) ? YES : NO;
	if (config.proxy && !config.proxy->user.empty()) {
		delegate.proxyUser     = to_ns(config.proxy->user);
		delegate.proxyPassword = to_ns(config.proxy->passwd);
	}

	NSURLSession* new_session = make_session(config, delegate);

	NSURLSession* old_session = impl_->current_session();
	impl_->delegate           = delegate;
	impl_->replace_session(new_session);

	if (old_session != nil) {
		[old_session finishTasksAndInvalidate];
	}
}

std::shared_ptr<CookieJar> UrlSessionClient::make_cookie_jar() {
	return std::make_shared<MemoryCookieJar>();
}

namespace {

// Attach the jar's cookies for this URL. An explicit Cookie header on the request
// wins and is left alone, matching the config-merge rule the layer above uses.
void apply_cookies(NSMutableURLRequest* request, const Headers& headers,
                   const std::shared_ptr<CookieJar>& jar) {
	auto memory_jar = std::dynamic_pointer_cast<MemoryCookieJar>(jar);
	if (!memory_jar || headers.contains("Cookie")) {
		return;
	}

	NSURL* url        = request.URL;
	std::string host  = from_ns(url.host);
	std::string path  = from_ns(url.path);
	const bool secure = [url.scheme caseInsensitiveCompare:@"https"] == NSOrderedSame;

	std::vector<Cookie> cookies = memory_jar->store().cookies_for(host, path.empty() ? "/" : path, secure);
	if (cookies.empty()) {
		return;
	}

	std::string header;
	for (const Cookie& cookie : cookies) {
		if (!header.empty()) {
			header += "; ";
		}
		header += cookie.name;
		header += '=';
		header += cookie.value;
	}
	[request setValue:to_ns(header) forHTTPHeaderField:@"Cookie"];
}

// Harvest Set-Cookie into the jar. Foundation owns the parse: the Set-Cookie grammar
// (attributes, three legal Expires date formats, quoting) is exactly the part not
// worth hand-rolling, and +cookiesWithResponseHeaderFields: reads the joined
// dictionary shape allHeaderFields produces.
void harvest_cookies(NSHTTPURLResponse* response, const std::shared_ptr<CookieJar>& jar) {
	auto memory_jar = std::dynamic_pointer_cast<MemoryCookieJar>(jar);
	if (!memory_jar || response.URL == nil) {
		return;
	}
	NSArray<NSHTTPCookie*>* cookies =
	    [NSHTTPCookie cookiesWithResponseHeaderFields:response.allHeaderFields forURL:response.URL];
	for (NSHTTPCookie* ns_cookie in cookies) {
		memory_jar->store().set(to_cookie(ns_cookie));
	}
}

NSString* to_http_method(HttpMethod method) {
	switch (method) {
	case HttpMethod::Get:
		return @"GET";
	case HttpMethod::Head:
		return @"HEAD";
	case HttpMethod::Post:
		return @"POST";
	case HttpMethod::Put:
		return @"PUT";
	case HttpMethod::Patch:
		return @"PATCH";
	case HttpMethod::Delete:
		return @"DELETE";
	case HttpMethod::Options:
		return @"OPTIONS";
	}
	return nil;
}

// Build the request URL. The query is serialized with the library's own url_encode
// rather than NSURLComponents, so both backends percent-encode identically — the
// escaping rules differ, and a signature computed over the final URL would break.
std::string build_url(const std::string& url, const UrlParameters& params) {
	if (params.empty()) {
		return url_encode_uri(url);
	}
	std::string full = url;
	full += (full.find('?') == std::string::npos) ? '?' : '&';
	full += to_urlencoded(params);
	return url_encode_uri(full);
}

void apply_headers(NSMutableURLRequest* request, const Headers& headers) {
	// addValue, not setValue: Headers keeps repeated names on purpose, and setValue
	// would drop all but the last.
	for (const auto& [name, value] : headers) {
		[request addValue:to_ns(value) forHTTPHeaderField:to_ns(name)];
	}
}

/// Serialize a multipart form. NSURLSession has no form builder, so the boundary
/// framing is written by hand. @throws std::invalid_argument on a caller bug.
NSData* build_multipart_body(const MultipartForm& form, const std::string& boundary) {
	NSMutableData* body = [NSMutableData data];

	auto append = [&](std::string_view text) {
		[body appendBytes:text.data() length:text.size()];
	};

	for (const MultipartPart& part : form) {
		append("--");
		append(boundary);
		append("\r\nContent-Disposition: form-data; name=\"");
		append(part.name);
		append("\"");

		std::optional<std::string> filename = part.filename;
		std::string_view default_content_type;

		if (const auto* file = std::get_if<MultipartPart::File>(&part.source)) {
			// Rejected for parity with the curl backend, whose File part cannot override
			// the presented name — a form built on this backend must behave the same
			// everywhere. @see ClientContext::do_request
			if (part.filename) {
				throw std::invalid_argument(
				    "multipart File part cannot set an explicit filename; use a Buffer part");
			}
			const size_t slash = file->path.find_last_of("/\\");
			filename = slash == std::string::npos ? file->path : file->path.substr(slash + 1);
			default_content_type = "application/octet-stream";
		} else if (std::holds_alternative<MultipartPart::Buffer>(part.source)) {
			default_content_type = "application/octet-stream";
		}

		if (filename) {
			append("; filename=\"");
			append(*filename);
			append("\"");
		}
		append("\r\n");

		if (part.content_type) {
			append("Content-Type: ");
			append(*part.content_type);
			append("\r\n");
		} else if (!default_content_type.empty()) {
			append("Content-Type: ");
			append(default_content_type);
			append("\r\n");
		}
		append("\r\n");

		if (const auto* text = std::get_if<MultipartPart::Text>(&part.source)) {
			append(text->value);
		} else if (const auto* buffer = std::get_if<MultipartPart::Buffer>(&part.source)) {
			append(buffer->data);
		} else if (const auto* file = std::get_if<MultipartPart::File>(&part.source)) {
			// Read eagerly: streaming a body from disk needs an upload task, which this
			// backend does not use. Uploads are small (an avatar, a cover) on every source
			// the library talks to.
			NSData* contents = [NSData dataWithContentsOfFile:to_ns(file->path)];
			if (contents == nil) {
				throw std::invalid_argument("multipart File part is not readable: " + file->path);
			}
			[body appendData:contents];
		}
		append("\r\n");
	}

	append("--");
	append(boundary);
	append("--\r\n");
	return body;
}

} // namespace

namespace {

// One attempt. Split out so the retry loop below re-runs exactly the transfer and
// nothing else — the request is already built, the jar already read.
NetworkTask<Response<ResponseData>> perform_once(NSURLSession* session,
                                                 NSURLRequest* request,
                                                 std::shared_ptr<CookieJar> jar) {
	asyncnet::NetworkPromise<Response<ResponseData>>& promise = co_await asyncnet::awaitables::get_self;

	if (promise.stop_requested()) {
		co_return make_response_error(RequestErrorCode::Cancelled, "request cancelled");
	}

	// The block captures the state by shared_ptr, so it stays valid even if this
	// coroutine is destroyed before the transfer lands. @see TransferState
	auto state = std::make_shared<TransferState>();

	NSURLSessionDataTask* task = [session
	    dataTaskWithRequest:request
	      completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
		      if (error != nil) {
			      state->complete(make_response_error(ns_error_to_code(error),
			                                          from_ns(error.localizedDescription)));
			      return;
		      }
		      auto* http_response = static_cast<NSHTTPURLResponse*>(response);
		      harvest_cookies(http_response, jar);
		      state->complete(to_response_data(http_response, data));
	      }];

	// Taken here rather than at build time on purpose: connect_with SWAPS the promise's
	// stop_source for its parent's, and only the coroutine body is guaranteed to run
	// after that — a token grabbed any earlier would belong to a discarded source, and
	// cancellation would quietly stop arriving.
	TransferAwaiter awaiter(state, task, co_await asyncnet::awaitables::get_stop_token);
	Response<ResponseData> result = co_await awaiter;

	// Progress is reported at the end rather than streamed: a completion-handler task
	// surfaces no intermediate byte counts. @see NetworkPromise::read_bytes
	if (result) {
		promise.set_read_bytes(static_cast<long long>(result->body.size()));
		promise.set_total_bytes(static_cast<long long>(result->body.size()));
	}
	co_return result;
}

// Retry a transport failure until the budget is spent, matching the curl backend's
// policy: a cancelled request was stopped on purpose and is never retried, and an
// HTTP status is not a transport failure, so a 4xx/5xx is returned as a success.
NetworkTask<Response<ResponseData>> perform_with_retry(NSURLSession* session,
                                                       NSURLRequest* request,
                                                       std::shared_ptr<CookieJar> jar,
                                                       uint32_t max_retries) {
	asyncnet::NetworkPromise<Response<ResponseData>>& promise = co_await asyncnet::awaitables::get_self;

	Response<ResponseData> result = make_response_error(RequestErrorCode::NetworkError, "no attempt made");
	for (uint32_t attempt = 0; attempt <= max_retries; ++attempt) {
		result = co_await perform_once(session, request, jar).connect_with(promise);
		if (result || result.error().code == RequestErrorCode::Cancelled) {
			co_return result;
		}
	}
	co_return result;
}

} // namespace

NetworkRequestTask<ResponseData> UrlSessionClient::do_request(ConfiguredGetRequest configured) {
	NSString* method = to_http_method(configured.request.method);
	if (method == nil) {
		co_return make_response_error(RequestErrorCode::NotImplemented, "unsupported HTTP method");
	}

	NSURL* url = [NSURL URLWithString:to_ns(build_url(configured.request.url, configured.request.url_params))];
	if (url == nil) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              "malformed request URL: " + configured.request.url);
	}

	NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:url];
	request.HTTPMethod           = method;
	apply_headers(request, configured.request.headers);
	apply_cookies(request, configured.request.headers, configured.cookies);

	asyncnet::NetworkPromise<Response<ResponseData>>& promise = co_await asyncnet::awaitables::get_self;
	co_return co_await perform_with_retry(impl_->current_session(), request, configured.cookies,
	                                      impl_->max_retries.load())
	    .connect_with(promise);
}

NetworkRequestTask<ResponseData> UrlSessionClient::do_request(ConfiguredPostRequest configured) {
	NSString* method = to_http_method(configured.request.method);
	if (method == nil) {
		co_return make_response_error(RequestErrorCode::NotImplemented, "unsupported HTTP method");
	}

	NSURL* url = [NSURL URLWithString:to_ns(build_url(configured.request.url, configured.request.url_params))];
	if (url == nil) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              "malformed request URL: " + configured.request.url);
	}

	NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:url];
	request.HTTPMethod           = method;
	request.HTTPBody = [NSData dataWithBytes:configured.request.body.data()
	                                  length:configured.request.body.size()];
	apply_headers(request, configured.request.headers);
	apply_cookies(request, configured.request.headers, configured.cookies);

	asyncnet::NetworkPromise<Response<ResponseData>>& promise = co_await asyncnet::awaitables::get_self;
	co_return co_await perform_with_retry(impl_->current_session(), request, configured.cookies,
	                                      impl_->max_retries.load())
	    .connect_with(promise);
}

NetworkRequestTask<ResponseData> UrlSessionClient::do_request(ConfiguredPostMultipartRequest configured) {
	NSString* method = to_http_method(configured.request.method);
	if (method == nil) {
		co_return make_response_error(RequestErrorCode::NotImplemented, "unsupported HTTP method");
	}

	NSURL* url = [NSURL URLWithString:to_ns(build_url(configured.request.url, configured.request.url_params))];
	if (url == nil) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              "malformed request URL: " + configured.request.url);
	}

	// Unique per request: a boundary that collided with the body's own bytes would
	// truncate the form.
	const std::string boundary = "----aniparse" + from_ns([[NSUUID UUID] UUIDString]);

	NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:url];
	request.HTTPMethod           = method;
	// The contract says the transport writes this one, boundary included.
	[request setValue:to_ns("multipart/form-data; boundary=" + boundary)
	    forHTTPHeaderField:@"Content-Type"];
	request.HTTPBody = build_multipart_body(configured.request.forms, boundary);
	apply_headers(request, configured.request.headers);
	apply_cookies(request, configured.request.headers, configured.cookies);

	asyncnet::NetworkPromise<Response<ResponseData>>& promise = co_await asyncnet::awaitables::get_self;
	co_return co_await perform_with_retry(impl_->current_session(), request, configured.cookies,
	                                      impl_->max_retries.load())
	    .connect_with(promise);
}
} // namespace aniparse

#endif // ANIPARSE_NSURLSESSION_BACKEND
