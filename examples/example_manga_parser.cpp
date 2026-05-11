#include <aniparse/Parser.hpp>
#include <aniparse/ParserStore.hpp>
#include <aniparse/Client.hpp>

// для coro::sync_wait
#include <coro/sync_wait.hpp>
// Для std::println
#include <print>

using namespace aniparse;

/**
 * Класс для получения различных сведений о конкретной манге
 */
class ExampleMangaGetter : public MangaGetter {
public:
	/// Коструктор геттера
	ExampleMangaGetter() : private_url_("https://example.com") {

	}
	ExampleMangaGetter(std::string url) : private_url_(std::move(url)) {

	}

	// Метод для получения базовой информации о парсере. 
	MangaGetterCompatibilities compatibilies() const noexcept override {
		using namespace compatibilities_flags;
		return {
			// Общее количество альтернативных ссылок у геттера
			.alt_links_count = 1,
			// Поддерживает комментарии, поддерживает голоса (пока не сделано)
			.flags = supports_commenting
				| supports_voting
		};
	}

	// Опционально. Если не перегружено, то используется info(...).
	// Этот метод должен вызываться, когда нужна минимальная информация (название, превью, ...)
	// и эта информация уже есть в геттере.
	NetworkRequestTask<MangaInfo> preview_info(RequestorContext context) noexcept override {
		co_return MangaInfo{
			.title = "Тест",
			.description = "Какое-то описание",
			.status = AiredStatus {
				.name = std::string(aired_status_released)
			}
		};
	}

	// Получение всей информации о манге
	NetworkRequestTask<MangaInfo> info(RequestorContext context) noexcept override {
		co_return MangaInfo{
			.title = "Тест",
			.description = "Какое-то описание",
			.status = AiredStatus {
				.name = std::string(aired_status_released)
			}
		};
	}

	// Получение информации о переводах манги, может быть несколько
	NetworkRequestTask<PageResults<MangaTranslationInfo>> translation_info(
		RequestorContext context,
		GetFilters filters) noexcept {
		PageResults<MangaTranslationInfo> pages;

		// Вернуть 0 страниц, если столько запрашивается.
		// Иначе вернуть сколько возможно, то есть 1. Лимит лишь указывает максимальное количество
		if (filters.limit <= 0) {
			co_return std::move(pages);
		}

		// Общее количество найденных, без учёта какие сейчас запрашиваются
		pages.total_count = 10;
		// Результаты
		pages.results = {
			PageItem<MangaTranslationInfo> {
				.item = {
					.id = 0,
					.language = "ru-ru",
					.translator = "Ру переводчик"
				},
				.offset = 0
			}
		};

		co_return std::move(pages);
	}

	// Получение информации о главах манги, может быть несколько
	NetworkRequestTask<PageResults<MangaChapterInfo>> chapters_info(
		RequestorContext context,
		GetFilters filters,
		MangaTranslationID translation = any_manga_translation) noexcept override {
		PageResults<MangaChapterInfo> pages;

		// Вернуть 0 страниц, если столько запрашивается.
		// Иначе вернуть сколько возможно, то есть 1. Лимит лишь указывает максимальное количество
		if (filters.limit <= 0) {
			co_return std::move(pages);
		}

		// Общее количество найденных, без учёта какие сейчас запрашиваются
		pages.total_count = 1;
		// Результаты
		pages.results = {
			PageItem<MangaChapterInfo> {
				.item = {
					.volume = 0,
					.chapter = 0,
					.name = "Глава 1",
				},
				.offset = 0
			}
		};

		co_return std::move(pages);
	}

	// Опционально. Получение похожей манги, если поддерживается
	// Если неподдерживается, то необходимо вернуть NotImplemented
	NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> related(
		RequestorContext context,
		GetFilters filters) noexcept override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "Parser doesn't support related");
	}

	// Получение всех страниц у конкретной главы с ссылками на изображения.
	NetworkRequestTask<PageResults<MangaPage>> chapter_pages(
		RequestorContext context,
		int volume,
		int chapter,
		GetFilters filters,
		MangaTranslationID translation = any_manga_translation) noexcept override {
		PageResults<MangaPage> pages;

		// Вернуть 0 страниц, если столько запрашивается.
		// Иначе вернуть сколько возможно, то есть 1. Лимит лишь указывает максимальное количество
		if (filters.limit <= 0) {
			co_return std::move(pages);
		}

		// Общее количество найденных, без учёта какие сейчас запрашиваются
		pages.total_count = 2;
		// Результаты
		// Отступы должны начинаться с 0 и идти дальше
		pages.results = {
			PageItem<MangaPage> {
				.item = {
					.image = {
						// ID опционален, используется только парсерами для доп информации
						.id = 0,
						.url = "https://example.com/image1"
					}
				},
				.offset = 0
			},
			PageItem<MangaPage> {
				.item = {
					.image = {
						// ID опционален, используется только парсерами для доп информации
						.id = 1,
						.url = "https://example.com/image2"
					}
				},
				.offset = 1
			}
		};

		co_return std::move(pages);
	}

	// Преобразование всех данных геттера в сериализованную структуру.
	// Например, необходима для сохранения результатов в файл с последующим получением этого геттера.
	// Используется в купе с ExampleParser::from_serialized(...)
	NetworkRequestTask<SerializedGetterData> serialize() noexcept override {
		co_return SerializedGetterData{
			.url = private_url_
		};
	}

private:

	std::string private_url_;
};

class ExampleMangaRootGetter : public MangaRootGetter {
public:

	// Получение информации о возможностях поиска манги
	// К этому относятся фильтры, сортировка, ...
	SearchCompatibilities search_support() const noexcept override {
		return {
			// no sort, no filters
		};
	}

	// Получение информации о возможных способах получениия последний манг.
	MangaGetterRootCompatibilities latest_support() const noexcept override {
		using namespace filtering_flags;
		return {
			.filtering_support = sort_title
				| sort_popularity
				| sort_release_time
				| sort_downloads_desc
				| sort_views_desc
		};
	}

	// Авторизация клиента, если для получения какой-то информации необходимо входить в сервис
	NetworkRequestTask<std::shared_ptr<const ClientConfig>> authenticate_context(
		RequestorContext context,
		AuthenticationData data) noexcept override {
		co_return context.config();
	}

	// Создать готовый конфиг для парсера
	std::shared_ptr<ClientConfig> default_config_from(std::shared_ptr<const ClientConfig> base_config) const override {
		auto new_config = std::make_shared<ClientConfig>(*base_config);
		new_config->headers["User-Agent"] = "ExampleParser/1.0";
		return new_config;
	}

	// Поиск по указаным критериям. Для получения возможностей поиска используется search_support()
	NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> search(
		RequestorContext context,
		SearchRequestQuery query,
		GetFilters filters) noexcept override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "The parser cannot search");
	}

	// Получение последних предметов по выбранным фильтрам
	NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> latest(
		RequestorContext context,
		GetFilters filters) noexcept override {
		PageResults<std::unique_ptr<MangaGetter>> pages;
		// Логирование
		context.info("Вызван latest(), от {}, лимит {}", filters.from, filters.limit);

		// Вернуть 0 страниц, если столько запрашивается.
		// Иначе вернуть сколько возможно, то есть 1. Лимит лишь указывает максимальное количество
		if (filters.limit <= 0) {
			co_return std::move(pages);
		}
		
		// Общее количество найденных, без учёта какие сейчас запрашиваются
		pages.total_count = 2;
		// Результаты
		PageItem<std::unique_ptr<MangaGetter>> manga_item = {
			.item = std::make_unique<ExampleMangaGetter>(),
			.offset = 0
		};
		pages.results.push_back(std::move(manga_item));
		
		co_return std::move(pages);
	}

	// Получение манги из ссылки
	NetworkRequestTask<std::unique_ptr<MangaGetter>> parse_url(
		RequestorContext context,
		std::string url) noexcept override {
		co_return make_response_error(RequestErrorCode::NotImplemented, "Парсер не умеет парсить ссылки");
	}

	// Получение манги из сериализованных данных
	NetworkRequestTask<std::unique_ptr<MangaGetter>> from_serialized(SerializedGetterData data) noexcept override {
		if (data.url.empty()) {
			co_return make_response_error(RequestErrorCode::NotImplemented, "Invalid serialized url");
		}
		co_return std::make_unique<ExampleMangaGetter>(data.url);
	}

private:

};

class ExampleParser : public Parser {

	// Получение имени парсера, которое может отображаться пользователю
	std::string name() const override {
		return "Пример";
	}

	// Получение идентификатора парсера, должен быть уникальным
	// Может быть использован для получения парсера из списка
	std::string identifier() const override {
		return "Example";
	}

	// Проверка может ли парсер обрабатывать данную ссылку.
	// Вызывается только если домен в ссылке соответствует тем, что были вставлены в emplace_domains(...).
	// Скорее всего далее будет использоваться ExampleMangaRootGetter::parse_url(...)
	bool valid_for_url(std::string_view url) const override {
		return true;
	}

	// Получение возможностей парсера
	ParserCompatibilities compatibilities() const override {
		using namespace compatibilities_flags;
		return {
			// Основной язык парсера: русский
			.primary_language = "ru-ru",
			// Флаги: поддерживает получение манги
			.flags = supports_manga_store
		};
	}

	// Вставка всех известных доменов, которые умеет обрабатывать парсер.
	// Вызывается, когда пользователь добавляет этот парсер в общий список парсеров
	void emplace_domains(EmplaceDomainsContext& context) const override {
		context.add_domain("example.com");
		context.add_domain("example-test.com");
	}

	// Получение геттера манги для данного парсера
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
			"[{:%T} {} {}:{}] {}",
			zoned_now,
			message_type,
			loc.file_name(),
			loc.line(),
			message) = '\n';
	}
};

int main() {
	std::locale::global(std::locale("ru.utf-8"));

	// Список парсеров, в него можно добавить парсеры, а потом получить при необходимости
	ParserStore parser_store;

	// Базовый клиент, которым парсер будет получать информации из интернета
	auto client = std::make_shared<AsyncClient>();
	// Контекст для парсера, который содержит клиент, логгер и указание на альт. ссылку
	RequestorContext client_context(client, std::make_shared<ConsoleLogger>(), nullptr);

	// Добавить парсер в список
	parser_store.add_parser(std::make_shared<ExampleParser>());

	// Далее его можно получить по ключу
	auto example_parser = parser_store.find_by_key("Example");

	// Получить геттер для манги у парсера
	auto example_manga_root = example_parser->mangas_getter();

	// Получить клиент, готовый для работы для данного парсера
	auto ready_to_work_client = client_context.new_with_config(example_manga_root->default_config_from(client_context.config()));

	// Получить список последних манг у парсера.
	// Также корутина выполняется синхронно благодаря sync_wait.
	// В других корутинах можно использовать co_await.
	auto latest_mangas_response = coro::sync_wait(example_manga_root->latest(ready_to_work_client, {
		.from = 0,
		.limit = 10,
		// Можно указать сортировку, но в парсере для примера это не используется
		.sort = FilterSort::TitleAsc
		}));

	if (!latest_mangas_response) {
		// Ошибка при запросе
		std::println("Не удалось получить последние манги: {}", latest_mangas_response.error().message);
		return -1;
	}

	// Получить геттеры последних манг из ответа
	auto& latest_mangas = latest_mangas_response.value();
	if (latest_mangas.results.empty()) {
		// Нет результатов
		std::println("Последних манг нет");
		return -1;
	}
	std::unique_ptr<MangaGetter> first_manga_getter = std::move(latest_mangas.results[0].item);

	// Получить информацию о первой манге
	auto first_manga_info_response = coro::sync_wait(first_manga_getter->info(ready_to_work_client));
	if (!first_manga_info_response) {
		// Ошибка при запросе
		std::println("Не удалось получить информацию о манге: {}", first_manga_info_response.error().message);
		return -1;
	}

	auto& first_manga_info = first_manga_info_response.value();
	std::println("Название: {}", first_manga_info.title);
	std::println("Описание: {}", first_manga_info.description.text);

	// Пример запроса с ошибкой, в примере попробовать распарсить URL
	auto parsed_manga_response = coro::sync_wait(example_manga_root->parse_url(ready_to_work_client, {}));
	if (!parsed_manga_response) {
		// Ошибка при запросе
		std::println("Не удалось распарсить мангу: {}", parsed_manga_response.error().message);
	}
}