/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <aateam.anianglia@gmail.com>
 */
#pragma once
#include "aniparse/FlagsBitfield.hpp"

#include <asyncnet/Request.hpp>
#include <asyncnet/Response.hpp>
#include <variant>

namespace aniparse {
	using GetterContextFlags = FlagsBitfield<64, struct GetterContextFlagsTag>;

	namespace getter_flags {
		constexpr auto partial_info_flag = GetterContextFlags::make_bit(0);

		constexpr GetterContextFlags default_flags;
	}

	/**
	 * HTTP GET request
	 */
	struct GetRequest {
		std::string url;
		asyncnet::UrlParameters url_params;
		std::list<std::string> headers;
	};

	/**
	 * HTTP POST request
	 */
	struct PostRequest {
		std::string url;
		asyncnet::UrlParameters url_params;
		std::list<std::string> headers;
		std::string body;
	};

	/**
	 * HTTP POST multipart/form-data request
	 */
	struct PostMultipartRequest {
		std::string url;
		asyncnet::UrlParameters url_params;
		std::list<std::string> headers;
		asyncnet::MultipartForms forms;
	};

	/**
	 * Any other HTTP request. Can be used to create custom requests.
	 */
	struct PolymorphicRequest {
		virtual ~PolymorphicRequest() = default;

		/**
		 * Virtual method, that constructs custom HTTP request state.
		 * Not recomended to set shared.
		 * @return Constructed HTTP request
		 */
		virtual asyncnet::Request get_request() = 0;
	};

	/**
	 * HTTP requests, that is supported by aniparse clients.
	 * Used by parser getters to virtually create any HTTP requests.
	 * @see GetRequest, @see PostRequest, @see PostMultipartRequest, @see PolymorphicRequest
	 */
	using ClientRequest = std::variant<
		GetRequest,
		PostRequest,
		PostMultipartRequest,
		std::shared_ptr<PolymorphicRequest>
	>;

	/**
	 * HTTP requests, that is supported by aniparse clients.
	 * Used by parser getters to virtually create any HTTP requests.
	 * The request can be parsed into given type by specifying parse functor.
	 * @see GetRequest, @see PostRequest, @see PostMultipartRequest, @see PolymorphicRequest
	 * @tparam T Type, that parser parses
	 */
	template<typename T>
	struct ClientParsedRequest {
		/// HTTP request
		ClientRequest request;
		/// Parse functor, accepting response for HTTP request, and returning type T
		std::function<T(asyncnet::Response response)> parse;
	};

	/**
	 * Optional HTTP request.
	 * If T is given, then it can be immediately used.
	 * Otherwise the request have to be performed to obtain T
	 * @tparam T Result (return) type
	 */
	template<typename T>
	using OptionalRequest = std::variant<T, ClientParsedRequest<T>>;

	struct GetterContext {
		std::map<std::string, std::string> user_args;

		GetterContextFlags flags = getter_flags::default_flags;
	};
}