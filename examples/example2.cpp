#include <aniparse/AniParse.hpp>

#include <map>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <span>
#include <optional>

#include <asyncnet/AsyncSession.hpp>
#include <coro/sync_wait.hpp>

#include <print>

struct ApiGetRequest {
	std::string sub_url;
};

template<typename T, typename Request>
struct ParsedApiRequest {
	virtual ~ParsedApiRequest() = default;

	virtual inline void preprocess(asyncnet::Request& req) {};
	virtual T parse(asyncnet::Response& resp) = 0;

	Request request;
};

struct TestRequest : ParsedApiRequest<int, ApiGetRequest> {
	inline TestRequest() {
		request.sub_url = "https://www.google.com";
	}

	int parse(asyncnet::Response& resp) override {
		return 1;
	}
};

template<typename T>
asyncnet::NetworkTask<T> make_request(asyncnet::AsyncSession& sess, ParsedApiRequest<T, ApiGetRequest>& parsed_req) {
	asyncnet::GetRequest req = sess.make_request<asyncnet::GetRequest>(parsed_req.request.sub_url);
	parsed_req.preprocess(req);

	asyncnet::Response resp = co_await sess.perform_request(req);
	co_return parsed_req.parse(resp);
}

void test1() {
	asyncnet::AsyncSession sess;
	TestRequest request;

	int test_resp = coro::sync_wait(make_request(sess, request));

	std::cout << test_resp << '\n';
}

struct ApiGetRequest2 {
	std::string sub_url;
};

template<typename T, typename Request>
struct ParsedApiRequest2 {
	Request request;
	std::function<void(asyncnet::Request& req)> preprocessor;
	std::function<T(asyncnet::Response& resp)> parser;
};

ParsedApiRequest2<int, ApiGetRequest2> make_test_request2() {
	return {
		.request = {
			.sub_url = "https://www.google.com"
		},
		.parser = [](asyncnet::Response& resp) {
			return 1;
		}
	};
}

template<typename T>
asyncnet::NetworkTask<T> make_request2(asyncnet::AsyncSession& sess, ParsedApiRequest2<T, ApiGetRequest2>& parsed_req) {
	asyncnet::GetRequest req = sess.make_request<asyncnet::GetRequest>(parsed_req.request.sub_url);
	if (parsed_req.preprocessor) {
		parsed_req.preprocessor(req);
	}

	asyncnet::Response resp = co_await sess.perform_request(req);
	co_return parsed_req.parser(resp);
}

void test2() {
	asyncnet::AsyncSession sess;
	auto request = make_test_request2();;

	int test_resp = coro::sync_wait(make_request2(sess, request));

	std::cout << test_resp << '\n';
}

struct Requests {
	inline asyncnet::NetworkTask<int> test3_request(asyncnet::AsyncSession& sess) {
		asyncnet::GetRequest req = sess.make_request<asyncnet::GetRequest>("https://www.google.com");
		asyncnet::Response resp = co_await sess.perform_request(req);
		co_return 1;
	}
};

void test3() {
	asyncnet::AsyncSession sess;
	Requests reqs;
	int test_resp = coro::sync_wait(reqs.test3_request(sess));

	std::cout << test_resp << '\n';
}

int main() {
	// test1();
	// test2();
	test3();
}