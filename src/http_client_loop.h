
#pragma once

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <future>
#include <chrono>
#include <map>
#include <regex>
#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

using boost::asio::ip::tcp;
using ms = std::chrono::milliseconds;

struct http_stat
{
	http_stat() : request(0), response(0), transfer_bytes(0), duration(0) {}
	uint64_t request;
	uint64_t response;
	uint64_t transfer_bytes;
	uint64_t duration;
};

using Stat = std::map <std::string, http_stat>;
using StCode = std::map <std::string, int>;
using header_t = std::vector < std::string >;


class http_client_loop
{
public:
	http_client_loop(boost::asio::io_context& io_context, const std::string &method, const std::string &data, header_t headers)
		: resolver_(io_context), socket_(io_context), ctx_(boost::asio::ssl::context::sslv23), sslsocket_(socket_, ctx_),
		method_(method), data_(data), headers_(headers)
	{
		next_session = std::bind(&http_client_loop::next_session_s, this);
	}

	~http_client_loop()
	{
		close();
	}

	bool open(const std::string& url, const std::string& proxy = "");
	void close();
	void cancel();
	void reopen();
	void start_once();
	void start();
	bool is_open();
	std::unique_ptr<std::stringstream>& get_response();
	Stat& get_stat();
	StCode& get_stcode();

	static std::string now();

private:
	bool urlparser(const std::string &url);
	void next_session_s();
	void async_write();
	void async_read_header();
	int parse_header(bool &chunked);
	void async_read_content(size_t left, bool chunked = false);
	int parse_contents(bool chunked = false);
	void build_reqeust();

private:
	tcp::resolver resolver_;
	tcp::socket socket_;
	boost::asio::ssl::context ctx_;
	boost::asio::ssl::stream<tcp::socket&> sslsocket_;

	boost::asio::streambuf request_;
	boost::asio::streambuf response_;

	std::unique_ptr<std::stringstream> resp_stream_;

	std::string xhost_;
	std::string xport_;

	bool proxy_;
	bool ssl_;
	std::string protocol_;
	std::string host_;
	std::string port_;
	std::string path_;
	std::string method_;
	std::string data_;
	header_t headers_;

	std::function<void()> next_session;

	Stat stat_;
	StCode stcode_;
	decltype(std::chrono::high_resolution_clock::now()) t0_;
};
