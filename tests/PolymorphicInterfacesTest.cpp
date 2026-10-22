#include <aniparse/anime/AnimeGetter.hpp>
#include <aniparse/catalog/CatalogRootGetter.hpp>
#include <aniparse/engines/booru/BooruEngine.hpp>
#include <aniparse/html/SelectorSource.hpp>
#include <aniparse/images/ImagesGetter.hpp>
#include <aniparse/manga/MangaGetter.hpp>
#include <aniparse/media/ResourceAdapter.hpp>
#include <aniparse/net/CookieJar.hpp>
#include <aniparse/net/HttpClient.hpp>
#include <aniparse/runtime/Logger.hpp>
#include <aniparse/source/Parser.hpp>
#include <aniparse/source/ParserProfile.hpp>
#include <aniparse/source/overrides/SourceOverrides.hpp>
#include <aniparse/utility/RegexSource.hpp>
#include <aniparse/video/VideoExtractor.hpp>
#include <type_traits>

namespace {
using namespace aniparse;

// A base reference must never allow replacing only the base subobject.
template <typename... Bases>
constexpr bool protected_interfaces = (
    (!std::is_copy_constructible_v<Bases> &&
     !std::is_move_constructible_v<Bases> &&
     !std::is_copy_assignable_v<Bases> &&
     !std::is_move_assignable_v<Bases> &&
     std::has_virtual_destructor_v<Bases> &&
     std::is_destructible_v<Bases>) && ...);

static_assert(protected_interfaces<
    Parser, ParserProfile, EmplaceDomainsContext, CatalogRootGetter,
    MangaGetter, AnimeGetter, ImagesGetter, ImageContainerGetter,
    CookieJar, HttpClient, Logger, VideoExtractor, engines::BooruEngine,
    SignatureVerifier, ResourceOutput, ResourceHandle, ResourceAdapter,
    html::SelectorSource, RegexSource>);

// Concrete subclasses retain value semantics through protected special members.
template <typename Base>
struct Derived : Base {};

template <typename... Bases>
constexpr bool copyable_derivatives = (
    (std::is_copy_constructible_v<Derived<Bases>> &&
     std::is_move_constructible_v<Derived<Bases>> &&
     std::is_copy_assignable_v<Derived<Bases>> &&
     std::is_move_assignable_v<Derived<Bases>>) && ...);

static_assert(copyable_derivatives<
    ParserProfile, CatalogRootGetter, ResourceHandle, html::SelectorSource, RegexSource>);
} // namespace
