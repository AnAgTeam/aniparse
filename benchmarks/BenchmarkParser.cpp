/*
 * Copyright (C) 2026 Toilettrauma
 */
#include "BenchmarkParser.hpp"

#include <string>

namespace {
class BenchmarkParser final : public aniparse::Parser {
public:
	explicit BenchmarkParser(int number)
	    : identifier_("Benchmark" + std::to_string(number))
	    , domain_("video" + std::to_string(number) + ".media.example.test")
	    , mirror_domain_("video" + std::to_string(number) + ".mirror.example.test") {}

	aniparse::ParserInfo info() const override { return { .name = identifier_ }; }
	std::string identifier() const override { return identifier_; }
	aniparse::ParserCompatibilities compatibilities() const override { return {}; }
	bool valid_for_url(const aniparse::ParsedUrl&) const override { return true; }
	void emplace_domains(aniparse::EmplaceDomainsContext& context) const override {
		context.add_domain(domain_);
		context.add_domain(mirror_domain_);
	}

private:
	std::string identifier_;
	std::string domain_;
	std::string mirror_domain_;
};
} // namespace

std::shared_ptr<aniparse::Parser> make_benchmark_parser(int number) {
	return std::make_shared<BenchmarkParser>(number);
}
