#include <aniparse/Parser.hpp>
#include <aniparse/ParserStore.hpp>
#include <aniparse/net/Client.hpp>

// for coro::sync_wait
#include <coro/sync_wait.hpp>
// for std::println
#include <print>

using namespace aniparse;

/**
 * Getter for various pieces of information about a specific manga
 */
class ExampleMangaGetter : public MangaGetter {
public:
	/// Getter constructor
	ExampleMangaGetter() : private_url_("https://example.com") {

	}
	ExampleMangaGetter(std::string url) : private_url_(std::move(url)) {

	}

	// Returns the getter's basic capability information.
	MangaGetterCompatibilities compatibilities() const noexcept override {
		using namespace compatibilities_flags;
		return {
			// Supports commenting, supports voting (not implemented yet)
			.flags = supports_commenting
				| supports_voting
		};
	}

	// Optional. If not overridden, info(...) is used instead.
	// Called when only minimal information is needed (title, preview, ...)
	// and that information is already available in the getter.
	NetworkRequestTask<MangaInfo> preview_info(RequestorContext context) const override {
		co_return MangaInfo{
			.title = "Test",
			.description = "Some description",
			.status = AiredStatus {
				.name = std::string(aired_status_released)
			}
		};
	}

	// Fetch the full information about the manga
	NetworkRequestTask<MangaInfo> info(RequestorContext context) const override {
		co_return MangaInfo{
			.title = "Test",
			.description = "Some description",
			.status = AiredStatus {
				.name = std::string(aired_status_released)
			}
		};
	}

	// Fetch the manga's translation information; there can be several
	NetworkRequestTask<PageResults<MangaTranslationInfo>> translation_info(
		RequestorContext context,
		GetFilters filters) const override {
		PageResults<MangaTranslationInfo> pages;

		// Return 0 pages if that is what was requested.
		// Otherwise return as many as possible (here 1). The limit is only an upper bound.
		if (filters.limit == 0) {
			co_return std::move(pages);
		}

		// Total number found, regardless of which are currently requested
		pages.total_count = 10;
		// Results
		pages.results = {
			PageItem<MangaTranslationInfo> {
				.item = {
					.id = 0,
					.language = "ru-ru",
					.translator = "Ru translator"
				},
				.offset = 0
			}
		};

		co_return std::move(pages);
	}

	// Fetch the manga's chapter information; there can be several
	NetworkRequestTask<PageResults<MangaChapterInfo>> chapters_info(
		RequestorContext context,
		GetFilters filters,
		std::optional<MangaTranslationID> translation) const override {
		PageResults<MangaChapterInfo> pages;

		// Return 0 pages if that is what was requested.
		// Otherwise return as many as possible (here 1). The limit is only an upper bound.
		if (filters.limit == 0) {
			co_return std::move(pages);
		}

		// Total number found, regardless of which are currently requested
		pages.total_count = 1;
		// Results
		pages.results = {
			PageItem<MangaChapterInfo> {
				.item = {
					.volume = 0,
					.chapter = 0,
					.name = "Chapter 1",
				},
				.offset = 0
			}
		};

		co_return std::move(pages);
	}

	// Optional. Fetch related manga, if supported.
	// If unsupported, return NotImplemented.
	NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> related(
		RequestorContext context,
		GetFilters filters) const override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "Parser doesn't support related");
	}

	// Fetch all pages of a specific chapter, with image links.
	// The chapter is identified by round-trip: the ref() of an element from
	// chapters_info() is passed here (its id may carry a parser-internal key).
	NetworkRequestTask<PageResults<MangaPage>> chapter_pages(
		RequestorContext context,
		MangaChapterRef chapter,
		GetFilters filters,
	    std::optional<MangaTranslationID> translation) const override {
		PageResults<MangaPage> pages;

		// Return 0 pages if that is what was requested.
		// Otherwise return as many as possible (here 1). The limit is only an upper bound.
		if (filters.limit == 0) {
			co_return std::move(pages);
		}

		// Total number found, regardless of which are currently requested
		pages.total_count = 2;
		// Results
		// Offsets must start at 0 and increase from there
		pages.results = {
			PageItem<MangaPage> {
				.item = {
					.image = {
						// The id is optional, used only by parsers for extra info
						.id = 0,
						.url = "https://example.com/image1"
					}
				},
				.offset = 0
			},
			PageItem<MangaPage> {
				.item = {
					.image = {
						// The id is optional, used only by parsers for extra info
						.id = 1,
						.url = "https://example.com/image2"
					}
				},
				.offset = 1
			}
		};

		co_return std::move(pages);
	}

	// Serialize all of the getter's data into a serializable struct.
	// Needed e.g. to save results to a file and reconstruct this getter later.
	// Used together with ExampleParser::from_serialized(...)
	NetworkRequestTask<SerializedGetterData> serialize() const override {
		co_return SerializedGetterData{
			.url = private_url_
		};
	}

private:

	std::string private_url_;
};

class ExampleMangaRootGetter : public MangaRootGetter {
public:

	// Describe the manga search capabilities.
	// This covers filters, sorting, ...
	NetworkRequestTask<SearchCompatibilities> search_support(RequestorContext) const override {
		co_return SearchCompatibilities{
			// no sort, no filters
		};
	}

	// Describe the ways the latest manga can be fetched.
	// Sort keys are open: the standard ones live in sort_keys, but a site may
	// declare its own. The descriptor sets the supported directions
	// (descending = true by default).
	MangaGetterRootCompatibilities latest_support() const noexcept override {
		using namespace sort_keys;
		return {
			.supported_sorts = {
				{ std::string(title), { .ascending = true } },
				{ std::string(popularity), { .ascending = true } },
				{ std::string(release_time), { .ascending = true } },
				{ std::string(downloads), {} },
				{ std::string(views), {} },
			}
		};
	}

	// Search by the given criteria. Capabilities come from search_support().
	NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> search(
		RequestorContext context,
		SearchRequestQuery query,
		GetFilters filters) const override {
		// Check the search has no invalid/extra entries.
		// search_support() is the reference.
		auto support = co_await search_support(context);
		if (!support) {
			co_return unexpected(std::move(support.error()));
		}
		if (auto errors = validate_query(*support, query, filters); !errors.empty()) {
			// Don't throw; return the error through the common channel.
			// describe_search_query_errors collects the violations into a readable message.
			co_return make_response_error(RequestErrorCode::InvalidArguments, describe_search_query_errors(errors));
		}
		co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot search");
	}

	// Fetch the latest items for the chosen filters
	NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> latest(
		RequestorContext context,
		GetFilters filters) const override {
		PageResults<std::unique_ptr<MangaGetter>> pages;
		// Logging
		context.info("latest() called, from {}, limit {}", filters.from, filters.limit);

		// Return 0 pages if that is what was requested.
		// Otherwise return as many as possible (here 1). The limit is only an upper bound.
		if (filters.limit == 0) {
			co_return std::move(pages);
		}

		// Total number found, regardless of which are currently requested
		pages.total_count = 2;
		// Results
		PageItem<std::unique_ptr<MangaGetter>> manga_item = {
			.item = std::make_unique<ExampleMangaGetter>(),
			.offset = 0
		};
		pages.results.push_back(std::move(manga_item));

		co_return std::move(pages);
	}

	// Build a manga getter from a URL
	NetworkRequestTask<std::unique_ptr<MangaGetter>> parse_url(
		RequestorContext context,
		ParsedUrl url) const override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot parse URLs");
	}

	// Build a manga getter from serialized data
	NetworkRequestTask<std::unique_ptr<MangaGetter>> from_serialized(SerializedGetterData data) const override {
		if (data.url.empty()) {
			co_return make_response_error(RequestErrorCode::NotImplemented, "Invalid serialized url");
		}
		co_return std::make_unique<ExampleMangaGetter>(data.url);
	}

private:

};

class ExampleParser : public Parser {

	// Display metadata shown to the user in a source list: name, language, and
	// optional artwork. Distinct from identifier() (the stable routing key).
	ParserInfo info() const override {
		return {
			.name             = "Example",
			// The parser's primary language: Russian
			.primary_language = "ru-ru",
		};
	}

	// The parser's identifier; must be unique.
	// Can be used to look the parser up in the store.
	std::string identifier() const override {
		return "Example";
	}

	// Classify a URL to a getter category, used to route parse_url. Called only if
	// the URL's domain matches one added in emplace_domains(...). valid_for_url is
	// derived from this by default, so a URL-entry parser overrides only this.
	GetterSuggestionType suggest_getter(const ParsedUrl& url) const override {
		// A real parser would inspect url.path() / url.query(); this example is
		// manga-only, so any URL on its domains routes to the manga getter.
		(void)url;
		return GetterSuggestionType::Manga;
	}

	// Describe the parser's capabilities
	ParserCompatibilities compatibilities() const override {
		using namespace compatibilities_flags;
		return {
			// Flags: supports the manga store
			.flags = supports_manga_store
		};
	}

	// Register every domain the parser can handle.
	// Called when the user adds this parser to the store.
	void emplace_domains(EmplaceDomainsContext& context) const override {
		context.add_domain("example.com");
		context.add_domain("example-test.com");
	}

	// Authenticate the service. Login is per parser (site), not per category:
	// the returned config authorizes all of this parser's getters.
	NetworkRequestTask<std::shared_ptr<const ParserConfig>> authenticate_context(
		RequestorContext context,
		AuthenticationData data) override {
		co_return context.config();
	}

	// Seed parser-wide request defaults onto a freshly derived config. Called by
	// make_config after the common derivation; one shape serves all getters.
	void configure(ParserConfig& config) const override {
		config.headers.set("User-Agent", "ExampleParser/1.0");
	}

	// The source's built-in mirrors: the fetch path (context.base_url) and the UI
	// picker (mirror_choices) both read this; a catalog override can supersede it.
	std::span<const std::string_view> mirrors() const override {
		static constexpr std::array<std::string_view, 1> urls{ "https://example.com" };
		return urls;
	}

	// The manga root getter for this parser
	std::unique_ptr<MangaRootGetter> mangas_getter() const override {
		return std::make_unique<ExampleMangaRootGetter>();
	}
};

template<>
struct std::formatter<LogLevel> {
	constexpr auto parse(auto& ctx) {
		return ctx.begin();
	}

	auto format(LogLevel level, auto& ctx) const {
		std::string_view name;
		switch (level) {
		case LogLevel::Debug:   name = "DEBUG"; break;
		case LogLevel::Info:    name = "INFO"; break;
		case LogLevel::Warning: name = "WARNING"; break;
		case LogLevel::Error:   name = "ERROR"; break;
		case LogLevel::Fatal:   name = "FATAL"; break;
		default:                name = "UNKNOWN";
		}
		return std::format_to(ctx.out(), "{}", name);
	}
};

struct ConsoleLogger : LoggerContext {
	void log(LogLevel message_type,
		std::string_view message,
		const std::source_location loc = std::source_location::current()) override {
		using namespace std::chrono;

		auto now = floor<seconds>(system_clock::now());
		auto zoned_now = zoned_time{ current_zone(), now };

		std::format_to(std::ostream_iterator<char>(std::cout),
			"[{:%T} {} {}:{}] {}\n",
			zoned_now,
			message_type,
			loc.file_name(),
			loc.line(),
			message);
	}
};

int main() {
	// Locale names are platform-dependent; on a system without "ru.utf-8" this
	// throws, which is fine for the example — we just keep the default locale.
	try {
		std::locale::global(std::locale("ru.utf-8"));
	} catch (const std::exception&) {
	}

	// The parser store: parsers can be added to it and looked up when needed
	ParserStore parser_store;

	// The base client the parser uses to fetch data from the internet
	auto client = std::make_shared<AsyncClient>();
	// The parser context: holds the client, logger and the alt-link selection
	RequestorContext client_context(client, std::make_shared<ConsoleLogger>(), nullptr);

	// Add the parser to the store
	parser_store.add_parser(std::make_shared<ExampleParser>());

	// It can then be looked up by key
	auto example_parser = parser_store.find_by_key("Example");

	// Get the manga getter from the parser
	auto example_manga_root = example_parser->mangas_getter();

	// Get a context ready to work for this parser
	auto ready_to_work_client = client_context.new_with_config(example_parser->make_config(client_context.config()));

	// Fetch the parser's latest manga.
	// The coroutine also runs synchronously thanks to sync_wait.
	// Inside other coroutines you can use co_await.
	auto latest_mangas_response = coro::sync_wait(example_manga_root->latest(ready_to_work_client, {
		.from = 0,
		.limit = 10,
		// A sort can be specified, though the example parser ignores it
		.sort = SortOrder{ .key = std::string(sort_keys::title), .ascending = true }
		}));

	if (!latest_mangas_response) {
		// Request error
		std::println("Failed to fetch the latest manga: {}", latest_mangas_response.error().message);
		return -1;
	}

	// Get the latest manga getters from the response
	auto& latest_mangas = latest_mangas_response.value();
	if (latest_mangas.results.empty()) {
		// No results
		std::println("No latest manga");
		return -1;
	}
	std::unique_ptr<MangaGetter> first_manga_getter = std::move(latest_mangas.results[0].item);

	// Fetch information about the first manga
	auto first_manga_info_response = coro::sync_wait(first_manga_getter->info(ready_to_work_client));
	if (!first_manga_info_response) {
		// Request error
		std::println("Failed to fetch manga info: {}", first_manga_info_response.error().message);
		return -1;
	}

	auto& first_manga_info = first_manga_info_response.value();
	std::println("Title: {}", first_manga_info.title);
	std::println("Description: {}", first_manga_info.description.text);

	// Route a pasted URL to its parser and getter category, then build the getter
	// from it. The store parses the URL once and hands it back in route->url.
	if (auto route = parser_store.route_url("https://example.com/manga/123")) {
		std::println("Routed to '{}' (manga: {})",
			route->parser->info().name,
			route->type == GetterSuggestionType::Manga);

		auto routed_getter = route->parser->mangas_getter();
		auto parsed_manga = coro::sync_wait(routed_getter->parse_url(ready_to_work_client, std::move(route->url)));
		if (!parsed_manga) {
			// The example parser leaves parse_url unimplemented.
			std::println("Failed to parse manga: {}", parsed_manga.error().message);
		}
	}
}
