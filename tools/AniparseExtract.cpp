/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include <aniparse/Aniparse.hpp>
#include <span>
#include <string_view>
#include <print>

static int usage(const char* program_name) {
	std::println(stderr,
		"Usage: {0} [--help] (latest|search|support|parse|list) [<flags> ...]\n"
		"latest - get latest items: {0} latest -p <parser key> [--from <index>] [--limit <number>] [--sort <sort>]\n"
		"search - search items: {0} search -p <parser key> -q <query> [--from <index>] [--limit <number>]\n"
		"         [--filter <key>=<value>] [--filter ...] [--sort <sort>] [--download <output dir>]\n"
		"parse - get info by url (autodetect): {0} parse <url> [-p <parser key>] [--download <output folder>]\n"
		"support - check support of parser: {0} support (latest|search) [-p <parser key>]\n"
		"list - query library list: {0} list (parsers|filters)\n"
		"\n"
		"filter key (duplicate keys will be replaced with latest value):\n"
		"  series - Filter by series. Example: original | \"Blue Archive\"\n"
		"  pages|episodes - Filter by episode/pages count. Example: 5,pages,10 | pages,15 | 2,pages\n"
		"  tag - Specify tag. Example: full_color\n"
		"  updtime - Filter by update time. Example: le,23-01-2026 | ge,2023\n"
		"  ageres: Filter by age restriction (minumin age): Example: 10\n"
		"  status: Filter by status. Example: released | announced\n"
		, program_name);
	return -1;
}

static int real_main(int argc, const char** argv) {
	if (argc == 0) {
		return usage("aniparseExtract");
	}
	if (argc <= 2) {
		std::println(stderr, "Invalid arguments");
		return usage(argv[0]);
	}

	// ...
	return 0;
}

int main(int argc, const char** argv) {
	try {
		return real_main(argc, argv);
	}
	catch (const std::exception& e) {
		std::println(stderr, "Exception: {}", e.what());
		return -1;
	}
	catch (...) {
		std::println(stderr, "Unknown exception");
		return -1;
	}
}