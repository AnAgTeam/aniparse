#pragma once
#include "aniparse/utility/InteractionState.hpp"
#include "aniparse/net/CookieJar.hpp"
#include "aniparse/utility/FlagsBitfield.hpp"
#include <optional>
#include <string>
#include <vector>

namespace aniparse {

/// A reason to recheck the current browser session; not proof of authentication.
enum class WebLoginEventKind {
    Started, NavigationFinished, CookiesChanged, StorageChanged,
    PopupClosed, TimerElapsed, ManualCheckRequested,
    /// Exclusive upper bound. Keep event values contiguous and append new events before Count.
    Count
};

/// Typed subscription mask. One 64-bit word leaves room for future event kinds.
/// This is a set of interests; an emitted event remains one WebLoginEventKind.
inline constexpr size_t web_login_event_capacity = 64;
using WebLoginEventFlags = FlagsBitfield<web_login_event_capacity, struct WebLoginEventFlagsTag>;
static_assert(static_cast<size_t>(WebLoginEventKind::Count) <= web_login_event_capacity);

/**
 * @brief Convert one event kind to its subscription flag.
 * @param event Event whose ordinal selects its bit position.
 * @return A single-bit mask, or an empty mask for an unknown event kind.
 */
constexpr WebLoginEventFlags web_login_event_flag(WebLoginEventKind event) noexcept {
    const auto index = static_cast<size_t>(event);
    // Negative enum values convert to large unsigned indices and fail this bound too.
    if (index >= static_cast<size_t>(WebLoginEventKind::Count)) {
        return {};
    }
    return WebLoginEventFlags::make_bit(index);
}

/// All currently supported events; reserved bits remain clear.
inline constexpr auto all_web_login_events = [] {
    WebLoginEventFlags events;
    for (size_t index = 0; index < static_cast<size_t>(WebLoginEventKind::Count); ++index) {
        events |= web_login_event_flag(static_cast<WebLoginEventKind>(index));
    }
    return events;
}();

/// Browser storage areas are independent, even for the same origin and key.
enum class WebStorageKind { Local, Session };

/// One explicitly requested storage key. Origin may be an absolute frontend URL.
struct WebStorageKey {
    std::string origin;
    WebStorageKind storage = WebStorageKind::Local;
    std::string key;
};

/// A sampled storage value; nullopt means the key is absent at an observed origin.
struct WebStorageEntry {
    WebStorageKey location;
    std::optional<std::string> value;
};

/// One live browser window; IDs remain stable until the window closes.
struct WebWindowState {
    std::string id;
    std::string url;
    bool popup = false;
};

/// Owned snapshot. Only declared storage keys are collected; cookies include HttpOnly.
/// Storage entries from origins not currently accessible may be absent.
struct WebLoginState {
    std::vector<WebWindowState> windows;
    std::vector<Cookie> cookies;
    std::vector<WebStorageEntry> storage;
};

/// Declarative browser observations. Hosts serialize checks and coalesce event bursts.
struct WebLoginObservationConfig {
    WebLoginEventFlags events = all_web_login_events;
    std::vector<WebStorageKey> storage;
    /// Two seconds between fallback checks; zero disables the fallback timer.
    unsigned int interval_ms = 2000;

    /**
     * @brief Check whether this configuration subscribes to a single event.
     * @param event Event being considered by the observer.
     * @return True for a subscribed, known event; false for unknown event kinds.
     */
    [[nodiscard]] bool observes(WebLoginEventKind event) const noexcept {
        const auto flag = web_login_event_flag(event);
        return flag.any() && events.has(flag);
    }

};

/**
 * @brief Browser-specific login settings, independent of credential form fields.
 * @note Describes the browser flow; completion is verified by the profile.
 */
struct WebLoginInfo {
	/// Where the value the fields inject is lifted from once the web login completes.
	enum class Material { Cookies, StorageToken };

	/// The URL the web view opens for the user to sign in.
	std::string login_url;
	/// Where the credential is lifted from after login.
	Material material = Material::Cookies;
	/// StorageToken: the localStorage key the token sits under.
	std::string storage_key;
	/// StorageToken: dot-path to the token inside the JSON value at @ref storage_key
	/// (e.g. "session.access_token" when the stored value is a JSON object nesting the
	/// token). Empty when the stored value is the raw token string.
	std::string storage_token_path;
	/// Browser event subscriptions and additional storage keys to observe.
	WebLoginObservationConfig observation;
};

/**
 * @brief Normalize an HTTP(S) URL to its storage origin, including a non-default port.
 * @param url Absolute URL or origin.
 * @return Normalized origin, or an empty string for an invalid/unsupported URL.
 */
std::string web_login_origin(std::string_view url);

} // namespace aniparse
