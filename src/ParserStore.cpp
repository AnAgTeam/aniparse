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
		constexpr auto split_domains(Range&& range) {
			return std::forward<Range>(range)
				// split domain in reverse order
				| std::views::reverse
				| std::views::split('.')
				| std::views::transform(std::views::reverse);
		}

		//template<typename Char>
		constexpr auto split_url_domains(const std::basic_string_view<char>& url) {
			size_t protocol_end = url.find("://");
			return url
				// cut domain from url
				| std::views::drop(protocol_end + 3)
				| std::views::take_while([](char c) { return c != '?'; })
				// split domain in reverse order
				| std::views::reverse
				| std::views::split('.')
				| std::views::transform(std::views::reverse);
		}
	}

	std::shared_ptr<Parser> ParserStore::add_parser(std::string_view domain, std::shared_ptr<Parser> parser) {
		auto domain_iter = detail::split_domains(domain);
		auto& inserted_parser = parsers_[parser->identifier()] = std::move(parser);
		parsers_scanner_.add_domain_parser(domain_iter, inserted_parser);
		return inserted_parser;
	}

	std::shared_ptr<Parser> ParserStore::find_by_domain(std::string_view domain) {
		auto domain_iter = domain
			// split domain in reverse order
			| std::views::reverse
			| std::views::split('.')
			| std::views::transform(std::views::reverse);
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
}