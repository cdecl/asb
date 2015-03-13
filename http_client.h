
#pragma once

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <future>
#include <chrono>
#include <map>
#include <regex>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

using boost::asio::ip::tcp;

using ms = std::chrono::milliseconds;

struct http_stat
{
	http_stat() : request(0), response(0), recv_bytes(0) {}
	uint64_t request;
	uint64_t response;
	uint64_t recv_bytes;
};

using Stat = std::map <string, http_stat>;
using StCode = std::map <string, int>;


class http_client
{
public:
	http_client(boost::asio::io_service& io_service) 
		: resolver_(io_service), socket_(io_service)
	{
		next_session = std::bind(&http_client::next_session_s, this);
	}

	~http_client()
	{
		close();
	}

	bool open(const std::string& url)
	{
		bool r = false;

		try {
			if (!urlparser(url, host_, port_, path_)) {
				throw std::logic_error("error : invalid url");
			}

			tcp::resolver::query query(host_, port_);
			auto endpoint_iter = resolver_.resolve(query);
			boost::asio::connect(socket_, endpoint_iter);
			r = true;
		}
		catch (...) {}
		return r;
	}

	void close()
	{
		if (is_open()) {
			socket_.close();
		}
	}

	void cancel()
	{
		socket_.cancel();
	}

	void reopen()
	{
		if (!is_open()) {
			tcp::resolver::query query(host_, port_);
			auto endpoint_iter = resolver_.resolve(query);
			boost::asio::connect(socket_, endpoint_iter);
		}
	}

	void start_once()
	{
		next_session_s();
		next_session = [this]{
			socket_.get_io_service().stop();
		};
	}

	void start()
	{
		next_session();
	}

	bool is_open()
	{
		return socket_.is_open();
	}

	std::stringstream& get_response()
	{
		return resp_stream;
	}

	Stat& get_stat()
	{
		return stat_;
	}

	StCode& get_stcode()
	{
		return stcode_;
	}

	static std::string now()
	{
		std::time_t now = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
		string s = std::ctime(&now);
		return s.substr(0, s.find_last_of('\n'));
	}

	static bool urlparser(std::string url, std::string &host, std::string &port, std::string &path)
	{
		bool ret = false;

		boost::smatch m;
		boost::regex r(R"((https?):\/\/([^\/:]*):?([^\/]*)(\/?.*))");

		ret = boost::regex_search(url, m, r);

		if (ret) {
			host = m[2].str();
			port = m[3].str();
			if (port.empty()) port = "80";

			path = m[4].str();
			if (path.empty()) path = "/";
		}
		
		return ret;
	}


private:

	void next_session_s()
	{
		async_write();
		async_read_header();
	}


	void async_write()
	{
		build_reqeust();

		t0_ = std::chrono::high_resolution_clock::now();
		boost::asio::async_write(socket_, request_,
			[this/*, t0*/](const boost::system::error_code &err, size_t len)
		{
			if (!err) {
				std::string sn = now();
				stat_[sn].request++;

			}
			else {
				this->socket_.close();
			}

		});

	}

	void async_read_header()
	{
		// buuffer initialize 
		response_.consume(response_.size());

		boost::asio::async_read_until(socket_, response_, "\r\n\r\n",
			[this /*, t0*/](const boost::system::error_code &err, std::size_t len)
		{
			int nRecv = response_.size();
			// close 
			if (err) {
				socket_.close();
				reopen();

				next_session();
			}
			else {
				resp_stream.str("");
				resp_stream << boost::asio::buffer_cast<const char*>(response_.data());
				
				// header invalid check, get content-length 
				int content_length = parse_header();

				// logging
				if (content_length >= 0) {

					std::string sn = now();
					stat_[sn].response++;
					stat_[sn].recv_bytes += nRecv;
				}

				//cout << "content_length: " << content_length << ", " << "response_.size: " << response_.size();
				if (content_length > 0 && content_length > (int)response_.size()) {
					async_read_content(content_length - response_.size());
				}
				else {
					next_session();
				}
			}
		});
	}

	void async_read_content(size_t left)
	{
		boost::asio::async_read(this->socket_, this->response_, boost::asio::transfer_at_least(1),
			[this, left](const boost::system::error_code &err, std::size_t len)
		{
			if (!err) {
				std::string sn = now();
				stat_[sn].recv_bytes += len;

				resp_stream << boost::asio::buffer_cast<const char*>(response_.data());

				if (left > len) {
					async_read_content(left - len);
				}
				else {
					parse_contents();
					next_session();
				}
			}
			else if (err) {	// err
				if (err != boost::asio::error::eof) {
					this->socket_.close();
					this->reopen();
				}

				next_session();
			}
		});
	}


	int parse_header() 
	{
		using namespace boost::algorithm;
		int content_length = 0;

		try {
			std::istream response_stream(&response_);
			
			string header;
			getline(response_stream, header);

			vector<string> vs;
			split(vs, header, boost::is_any_of(" "), token_compress_on);

			if (vs.size() < 3) {
				std::ostringstream oss;
				oss << "Invalid http hedaer 1 line, vs.size() :  " << vs.size() << endl;
				throw logic_error(oss.str());
			}

			string http_version = vs[0];
			string status_code = vs[1];
			string status_msg = vs[2];

			if (!response_stream || http_version.substr(0, 5) != "HTTP/")
			{
				throw logic_error("Invalid http header version");
			}

			// status_code statistics
			stcode_[http_version + " " + status_code]++;

			//if (status_code != 200)
			//{
			//	std::ostringstream oss;
			//	oss << "Response returned with status code " << status_code << endl;
			//	throw logic_error(oss.str());
			//}

			//cout << status_code << endl;
			while (getline(response_stream, header) && header != "\r") {
				string::size_type pos = header.find("Content-Length");
				if (pos != string::npos) {
					pos = header.find(":");
					content_length = std::stoi(header.substr(pos + 1));
				}
				//cout << header << endl;
			}
			
		}
		catch (exception &) {
			content_length = -1;
			//cout << e.what() << endl;
		}

		return content_length;
	}

	bool parse_contents()
	{
		try {
			std::istream response_stream(&response_);

			std::string s;
			while (!response_stream.eof()) {
				std::getline(response_stream, s);
			}
		}
		catch (exception &) {
		}

		return true;
	}

	void build_reqeust()
	{
		this->request_.consume(this->request_.size());
		std::ostream oss(&request_);
		oss << "GET http://" << host_ << ":" << port_ << path_ << " HTTP/1.1\r\n";
		oss << "Host: " << host_ << ":" << port_ << "\r\n";
		oss << "Accept: */*\r\n";
		oss << "Connection: keep-alive\r\n";
		oss << "\r\n";
	}

private:
	tcp::resolver resolver_;
	tcp::socket socket_;
	boost::asio::streambuf request_;
	boost::asio::streambuf response_;
	std::stringstream resp_stream;

	std::string host_;
	std::string path_;
	std::string port_;

	std::function<void()> next_session;

	Stat stat_;
	StCode stcode_;
	decltype(chrono::high_resolution_clock::now()) t0_;
};
