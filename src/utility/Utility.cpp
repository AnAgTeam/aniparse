
namespace aniparse {
#if defined(ANIPARSE_USE_TL_EXPECTED) || !defined(__cpp_lib_expected)
	const int tl_expected_yes = 1;
#else
	const int tl_expected_no = 1;
#endif

#if defined(ANIPARSE_USE_LIBFMT)
	const int fmt_format_yes = 1;
#else
	const int fmt_format_no = 1;
#endif

#if defined(ANIPARSE_USE_BOOST_REGEX)
	const int boost_regex_yes = 1;
#else
	const int boost_regex_no = 1;
#endif
};