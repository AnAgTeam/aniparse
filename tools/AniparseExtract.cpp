#include <aniparse/Aniparse.hpp>
#include <span>
#include <string_view>
#include <print>

static int usage(const char* program_name) {
	std::println(stderr, "Usage: {} <url>", program_name);
	return -1;
}

static int real_main(int argc, const char** argv) {
	if (argc == 0) {
		return usage("aniparseExtract");
	}
	if (argc <= 2) {
		return usage(argv[0]);
	}

	// ...
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