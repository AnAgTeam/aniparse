#include "aniparse/ParserStore.hpp"

#include <ranges>

namespace aniparse {
	namespace detail {
		//struct SplitDomains {
		//	template<std::ranges::viewable_range Range, typename Pattern>
		//	constexpr auto operator()(Range&& range, Pattern&& pattern) {
		//		return std::forward<Range>(range)
		//			// split domain in reverse order
		//			| std::views::reverse
		//			| std::views::split('.')
		//			| std::views::transform(std::views::reverse);
		//	}
		//};

		//constexpr SplitDomains split_domains;

		template<std::ranges::viewable_range Range>
		static constexpr auto split_domains(Range&& range) {
			return std::forward<Range>(range)
				// split domain in reverse order
				| std::views::reverse
				| std::views::split('.')
				| std::views::transform(std::views::reverse);
		}

		//template<typename Char>
		static constexpr auto split_url_domains(const std::basic_string_view<char>& url) {
			size_t protocol_end = url.find("://");
			return url
				// cut domain from url
				| std::views::drop(protocol_end + 3)
				| std::views::take_while([](char c) { return c != '/' || c != '\\'; })
				// split domain in reverse order
				| std::views::reverse
				| std::views::split('.')
				| std::views::transform(std::views::reverse);
		}

		class DomainAdder : public aniparse::EmplaceDomainsContext {
		public:
			DomainAdder(DomainScanner<std::shared_ptr<Parser>>& scanner, std::shared_ptr<Parser> parser) :
				scanner_(scanner),
				parser_(std::move(parser)) {

			}

			void add_domain(std::string_view domain) override {
				auto domain_iter = detail::split_domains(domain);
				scanner_.add_domain_parser(domain_iter, parser_);
			}

		private:
			DomainScanner<std::shared_ptr<Parser>>& scanner_;
			std::shared_ptr<Parser> parser_;
		};
	}

	ParserStore::ParserStore() {

	}

	ParserStore ParserStore::make_default_store() {
		ParserStore store;

		return store;
	}

	std::shared_ptr<Parser> ParserStore::add_parser(std::shared_ptr<Parser> parser) {
		if (check_is_conflicting(parser)) {
			throw std::logic_error("Conflicting parser name: " + parser->identifier());
		}

		auto& inserted_parser = parsers_[parser->identifier()] = std::move(parser);
		detail::DomainAdder adder(parsers_scanner_, inserted_parser);
		inserted_parser->emplace_domains(adder);
		return inserted_parser;
	}

	std::shared_ptr<Parser> ParserStore::find_by_domain(std::string_view domain) {
		auto domain_iter = detail::split_domains(domain);
		std::string url = "https://" + std::string(domain);
		auto found_parser = parsers_scanner_.search_all(domain_iter, [&url](std::shared_ptr<Parser>& parser) {
			return parser->valid_for_url(url);
		});
		return found_parser ? *found_parser : nullptr;
	}

	std::shared_ptr<Parser> ParserStore::find_for_url(std::string_view url) {
		auto domain_iter = detail::split_url_domains(url);
		auto found_parser = parsers_scanner_.search_all(domain_iter, [&url](std::shared_ptr<Parser>& parser) {
			return parser->valid_for_url(url);
		});
		return found_parser ? *found_parser : nullptr;
	}

	std::shared_ptr<Parser> ParserStore::find_by_key(std::string_view key) {
		auto iter = parsers_.find(key);
		return iter != parsers_.end() ? iter->second : nullptr;
	}

	bool ParserStore::check_is_conflicting(const std::shared_ptr<Parser>& parser) const {
		auto iter = parsers_.find(parser->identifier());
		return iter != parsers_.end();
	}
}