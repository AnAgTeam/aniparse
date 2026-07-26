/*
 * Copyright (C) 2026 Toilettrauma
 *
 * Measures ParserStore's startup and routing hot paths. Run from a Release
 * build with -DANIPARSE_BUILD_BENCHMARKS=ON, then execute
 * benchmarks/parser-store-benchmark.
 */
#include <aniparse/ParserStore.hpp>
#include <aniparse/detail/DomainScanner.hpp>

#include "BenchmarkParser.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <memory>
#include <numeric>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using ParserPtr = std::shared_ptr<aniparse::Parser>;

template <std::ranges::viewable_range Range>
constexpr auto split_domains(Range&& range) {
	return std::forward<Range>(range)
	       | std::views::reverse
	       | std::views::split('.')
	       | std::views::transform(std::views::reverse);
}

class LegacyDomainAdder final : public aniparse::EmplaceDomainsContext {
public:
	LegacyDomainAdder(aniparse::DomainScanner<ParserPtr>& scanner, ParserPtr parser)
	    : scanner_(scanner), parser_(std::move(parser)) {}

	void add_domain(std::string_view domain) override {
		scanner_.add_domain_parser(split_domains(domain), parser_);
	}

private:
	aniparse::DomainScanner<ParserPtr>& scanner_;
	ParserPtr parser_;
};

class LazyDomainMatcher final : public aniparse::EmplaceDomainsContext {
public:
	explicit LazyDomainMatcher(std::string_view host) : host_(host) {}

	void add_domain(std::string_view domain) override {
		matched_ = matched_ || (host_.ends_with(domain)
		                        && (host_.size() == domain.size()
		                            || host_[host_.size() - domain.size() - 1] == '.'));
	}

	[[nodiscard]] bool matched() const { return matched_; }

private:
	std::string_view host_;
	bool matched_ = false;
};

template <class Operation>
std::vector<double> sample(std::size_t count, Operation&& operation) {
	std::vector<double> milliseconds;
	milliseconds.reserve(count);
	for (std::size_t index = 0; index != count; ++index) {
		auto start = Clock::now();
		operation();
		auto stop = Clock::now();
		milliseconds.push_back(std::chrono::duration<double, std::milli>(stop - start).count());
	}
	return milliseconds;
}

void report(const char* name, std::vector<double> milliseconds) {
	std::sort(milliseconds.begin(), milliseconds.end());
	auto median = milliseconds[milliseconds.size() / 2];
	auto p95 = milliseconds[(milliseconds.size() * 95) / 100];
	std::printf("%-37s median %8.3f ms  p95 %8.3f ms  min %8.3f ms\n",
	            name, median, p95, milliseconds.front());
}

void report_per_lookup(const char* name, std::vector<double> milliseconds, std::size_t lookups) {
	std::sort(milliseconds.begin(), milliseconds.end());
	auto to_nanoseconds = [lookups](double value) { return value * 1'000'000.0 / lookups; };
	std::printf("%-37s median %8.1f ns  p95 %8.1f ns  min %8.1f ns\n",
	            name, to_nanoseconds(milliseconds[milliseconds.size() / 2]),
	            to_nanoseconds(milliseconds[(milliseconds.size() * 95) / 100]),
	            to_nanoseconds(milliseconds.front()));
}

void legacy_build(std::size_t parser_count) {
	aniparse::DomainScanner<ParserPtr> scanner;
	for (std::size_t index = 0; index != parser_count; ++index) {
		auto parser = make_benchmark_parser(static_cast<int>(index));
		LegacyDomainAdder adder(scanner, parser);
		parser->emplace_domains(adder);
	}
}

void one_shot_build(std::size_t parser_count) {
	aniparse::ParserStore store;
	for (std::size_t index = 0; index != parser_count; ++index) {
		store.add_parser(make_benchmark_parser(static_cast<int>(index)));
	}
}

void batch_build(std::size_t parser_count) {
	aniparse::ParserStore store;
	auto edit = store.begin_edit();
	for (std::size_t index = 0; index != parser_count; ++index) {
		edit.add_parser(make_benchmark_parser(static_cast<int>(index)));
	}
	edit.commit();
}

void benchmark_startup(std::size_t parser_count) {
	char label[96];
	std::snprintf(label, sizeof(label), "legacy direct tree, %zu parsers", parser_count);
	report(label, sample(20, [=] { legacy_build(parser_count); }));
	std::snprintf(label, sizeof(label), "one-shot add_parser, %zu parsers", parser_count);
	report(label, sample(10, [=] { one_shot_build(parser_count); }));
	std::snprintf(label, sizeof(label), "one Edit transaction, %zu parsers", parser_count);
	report(label, sample(20, [=] { batch_build(parser_count); }));
}

void benchmark_lookup() {
	aniparse::DomainScanner<ParserPtr> scanner;
	std::vector<ParserPtr> parsers;
	parsers.reserve(1000);
	for (int index = 0; index != 1000; ++index) {
		auto parser = make_benchmark_parser(index);
		LegacyDomainAdder adder(scanner, parser);
		parser->emplace_domains(adder);
		parsers.push_back(std::move(parser));
	}

	aniparse::ParserStore::Domains store;
	auto edit = store.begin_edit();
	for (int index = 0; index != 1000; ++index) {
		auto parser = make_benchmark_parser(index);
		edit.add(parser->identifier(), std::move(parser));
	}
	edit.commit();

	struct LookupCase {
		const char* label;
		std::string_view host;
		int lazy_lookups;
	};
	constexpr std::array cases{
		LookupCase{ "best (#0)", "cdn.video0.media.example.test", 10'000'000 },
		LookupCase{ "middle (#500)", "cdn.video500.media.example.test", 20'000 },
		LookupCase{ "worst (#999)", "cdn.video999.media.example.test", 10'000 },
	};

	for (const auto& test : cases) {
		volatile std::size_t tree_found = 0;
		auto tree_milliseconds = sample(20, [&] {
			for (int iteration = 0; iteration != 1'000'000; ++iteration) {
				auto parser = store.find_by_host(test.host, [](const aniparse::Parser&) { return true; });
				tree_found += parser ? 1 : 0;
			}
		});
		char label[96];
		std::snprintf(label, sizeof(label), "tree lookup %s / 1000", test.label);
		report_per_lookup(label, std::move(tree_milliseconds), 1'000'000);

		volatile std::size_t lazy_found = 0;
		auto lazy_milliseconds = sample(20, [&] {
			for (int iteration = 0; iteration != test.lazy_lookups; ++iteration) {
				for (const auto& parser : parsers) {
					LazyDomainMatcher matcher(test.host);
					parser->emplace_domains(matcher);
					if (matcher.matched()) {
						++lazy_found;
						break;
					}
				}
			}
		});
		std::snprintf(label, sizeof(label), "lazy lookup %s / 1000", test.label);
		report_per_lookup(label, std::move(lazy_milliseconds), test.lazy_lookups);
		std::printf("lookup guards: tree=%zu lazy=%zu\n", static_cast<std::size_t>(tree_found),
		            static_cast<std::size_t>(lazy_found));
	}
}

} // namespace

int main() {
	std::puts("ParserStore benchmark (synthetic: 2 domains/parser, 20 samples unless noted)");
	benchmark_startup(100);
	benchmark_startup(1000);
	benchmark_lookup();
}
