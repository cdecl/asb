
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
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

using boost::asio::ip::tcp;
using ms = std::chrono::milliseconds;

struct http_stat
{
	http_stat() : request(0), response(0), transfer_bytes(0) {}
	uint64_t request;
	uint64_t response;
	uint64_t transfer_bytes;
};

using Stat = std::map <string, http_stat>;
using StCode = std::map <string, int>;
using header_t = std::vector < std::string > ;


class http_client
{
public:
	http_client(boost::asio::io_service& io_service, const string &method, const string &data, header_t headers)
		: resolver_(io_service), socket_(io_service), ctx_(boost::asio::ssl::context::sslv23), sslsocket_(socket_, ctx_),
		method_(method), data_(data), headers_(headers)
	{
		next_session = std::bind(&http_client::next_session_s, this);
	}

	~http_client()
	{
		close();
	}

	static std::string ver()
	{
		return "0.8";
	}

	bool open(const std::string& url, const std::string& proxy = "")
	{
		bool r = false;

		try {
			bool xssl = false;
			proxy_ = !proxy.empty();

			if (proxy_) {
				if (!urlparser(proxy)) {
					throw std::logic_error("error : invalid proxy url");
				}

				xhost_ = host_;
				xport_ = port_;
				xssl = ssl_;

				if (!urlparser(url)) {
					throw std::logic_error("error : invalid url");
				}

				ssl_ = xssl;
			}
			else {
				if (!urlparser(url)) {
					throw std::logic_error("error : invalid url");
				}

				xhost_ = host_;
				xport_ = port_;
			}


			if (ssl_) {
				ctx_.set_default_verify_paths();

				sslsocket_.set_verify_mode(boost::asio::ssl::verify_peer);
				sslsocket_.set_verify_callback([this](bool preverified, boost::asio::ssl::verify_context& ctx) -> bool
				{
					char subject_name[256];
					X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
					X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
#ifdef _DEBUG 
					std::cout << "Verifying: " << subject_name << "\n";
#endif
					return true || preverified;
				});
			}

			tcp::resolver::query query(xhost_, xport_);
			auto endpoint_iter = resolver_.resolve(query);
			boost::asio::connect(sslsocket_.lowest_layer(), endpoint_iter);

			if (ssl_) {
				boost::system::error_code err;
				sslsocket_.handshake(boost::asio::ssl::stream_base::client, err);
			}

			r = true;
		}
		catch (...) {}
		return r;
	}

	void close()
	{
		if (is_open()) {
			sslsocket_.lowest_layer().close();
		}
	}

	void cancel()
	{
		sslsocket_.lowest_layer().cancel();
	}

	void reopen()
	{
		if (!is_open()) {
			tcp::resolver::query query(xhost_, xport_);
			auto endpoint_iter = resolver_.resolve(query);
			boost::asio::connect(sslsocket_.lowest_layer(), endpoint_iter);

			if (ssl_) {
				boost::system::error_code err;
				sslsocket_.handshake(boost::asio::ssl::stream_base::client, err);
			}
		}
	}

	void start_once()
	{
		next_session_s();
		next_session = [this]{
			sslsocket_.get_io_service().stop();
		};
	}

	void start()
	{
		next_session();
	}

	bool is_open()
	{
		return sslsocket_.lowest_layer().is_open();
	}

	std::stringstream& get_response()
	{
		return resp_stream_;
	}

	Stat& get_stat()
	{
		return stat_;
	}

	StCode& get_stcode()
	{
		return stcode_;
	}


	bool urlparser(const std::string &url)
	{
		bool ret = false;

		boost::smatch m;
		boost::regex r(R"((https?):\/\/([^\/:]*):?([^\/]*)(\/?.*))");

		ret = boost::regex_search(url, m, r);

		if (ret) {
			protocol_ = boost::algorithm::to_lower_copy(m[1].str());
			host_ = m[2].str();
			port_ = m[3].str();

			ssl_ = (protocol_ == "https") ? true : false;

			if (port_.empty()) {
				port_ = (!ssl_) ? "80" : "443";
			}

			path_ = m[4].str();
			if (path_.empty()) path_ = "/";
		}
		
		return ret;
	}

	static std::string now()
	{
		std::time_t now = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
		struct tm ttm = *localtime(&now);

		char buf[80];
		strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &ttm);

		return std::string(buf);
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

		auto async_write_handler = [this/*, t0*/](const boost::system::error_code &err, size_t len)
		{
			if (!err) {
				std::string sn = now();
				stat_[sn].request++;
				stat_[sn].transfer_bytes += len;

			}
			else {
				close();
			}

		};

		if (ssl_) {
			boost::asio::async_write(sslsocket_, request_, async_write_handler);
		}
		else {
			boost::asio::async_write(socket_, request_, async_write_handler);
		}

	}

	void async_read_header()
	{
		using namespace boost::asio;

		auto async_read_until_handler = [this /*, t0*/](const boost::system::error_code &err, std::size_t len)
		{
			int nRecv = response_.size();
			// close 
			if (err) {
				close();
				reopen();

				next_session();
			}
			else {
				resp_stream_.str("");
				resp_stream_ << string(buffer_cast<const char*>(response_.data()), response_.size());

				bool chunked = false;
				// header invalid check, get content-length 
				int content_length = parse_header(chunked);

				// logging
				if (content_length >= 0) {
					std::string sn = now();
					stat_[sn].response++;
					stat_[sn].transfer_bytes += nRecv;
				}

				if (chunked) {
					content_length = parse_contents(chunked);
				}

				if (content_length > 0 && content_length > (int)response_.size()) {
					async_read_content(content_length - response_.size(), chunked);
				}
				else {
					next_session();
				}
			}
		};

		// buuffer initialize 
		response_.consume(response_.size());

		if (ssl_) {
			boost::asio::async_read_until(sslsocket_, response_, "\r\n\r\n", async_read_until_handler);
		}
		else {
			boost::asio::async_read_until(socket_, response_, "\r\n\r\n", async_read_until_handler);
		}

		
	}

	void async_read_content(size_t left, bool chunked = false)
	{
		auto async_read_handler = [this, left, chunked](const boost::system::error_code &err, std::size_t len)
		{
			using namespace boost::asio;

			if (!err) {
				std::string sn = now();
				stat_[sn].transfer_bytes += len;

				std::string s = string(buffer_cast<const char*>(response_.data()), response_.size());
				resp_stream_ << s.substr(s.length() - len);


				if (chunked) {
					int content_length = parse_contents(chunked);
					if (content_length > 0) {
						async_read_content(content_length);
					}
					else {
						next_session();
					}
				}
				else {
					if (left > len) {
						async_read_content(left - len);
					}
					else {
						parse_contents();
						next_session();
					}
				}
			}
			else if (err) {	// err
				if (err != boost::asio::error::eof) {
					close();
					reopen();
				}

				next_session();
			}
		};

		if (ssl_) {
			boost::asio::async_read(sslsocket_, response_, boost::asio::transfer_at_least(1), async_read_handler);
		}
		else {
			boost::asio::async_read(socket_, response_, boost::asio::transfer_at_least(1), async_read_handler);
		}
	}


	int parse_header(bool &chunked)
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

			while (getline(response_stream, header) && header != "\r") {
				string::size_type pos = header.find("Content-Length");
				if (pos != string::npos) {
					pos = header.find(":");
					content_length = std::stoi(header.substr(pos + 1));
				}

				// Transfer-Encoding : chunked
				pos = header.find("Transfer-Encoding");
				if (pos != string::npos) {
					pos = header.find(":");

					if (string::npos != header.substr(pos + 1).find("chunked")) {
						chunked = true;
					}
				}
			}
			
		}
		catch (exception &) {
			content_length = -1;
			//cout << e.what() << endl;
		}

		return content_length;
	}

	int parse_contents(bool chunked = false)
	{
		using namespace boost::asio;
		int content_length = 0;

		try {
			std::istream response_stream(&response_);

			if (chunked) {
				string header;

				while (true) {
					if (response_.size() == 0) break;

					string s = std::string(buffer_cast<const char*>(response_.data()), response_.size());

					string::size_type pos = s.find("\n");
					if (string::npos != pos) {
						content_length = stoi(s.substr(0, pos), nullptr, 16);
						
						if (content_length == 0) {
							response_.consume(response_.size());
							break;
						}
					}

					// content_length + "\r\n"
					if ((content_length) <= (int)response_.size()) {
						if (content_length) { content_length += 2; }

						response_.consume(content_length);
					}
					else {
						break;
					}
				}
			}
			else {
				std::string s;
				while (!response_stream.eof()) {
					std::getline(response_stream, s);
				}
			}
		}
		catch (exception &) {}

		return content_length;
	}

	void build_reqeust()
	{
		if (method_.empty()) {
			method_ = "GET";
		}

		size_t content_length = data_.size();
		this->request_.consume(this->request_.size());

		std::ostream oss(&request_);

		if (proxy_) {
			// absoluteURI
			oss << method_ << " " << protocol_ << "://" << host_ << ":" << port_ << path_ << " HTTP/1.1\r\n";  
		}
		else {
			//abs_path
			oss << method_ << " " << path_ << " HTTP/1.1\r\n";	
		}
		oss << "Host: " << host_ << ":" << port_ << "\r\n";
		oss << "Accept: */*\r\n";
		
		if (content_length > 0) {
			oss << "Content-Length: " << content_length << "\r\n";
		}

		for (auto h : headers_) {
			oss << h << "\r\n";
		}

		//oss << "User-Agent: asb/" << ver() << "\r\n" << endl;
		oss << "Connection: keep-alive\r\n";
		oss << "\r\n";

		if (content_length > 0) {
			oss << data_;
		}

	}

private:
	tcp::resolver resolver_;
	tcp::socket socket_;
	boost::asio::ssl::context ctx_;
	boost::asio::ssl::stream<tcp::socket&> sslsocket_;
	
	boost::asio::streambuf request_;
	boost::asio::streambuf response_;
	std::stringstream resp_stream_;

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
	decltype(chrono::high_resolution_clock::now()) t0_;
};
