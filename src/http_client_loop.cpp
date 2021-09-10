

#ifdef _MSC_VER
#include "stdafx.h"
#endif

#include <iostream>
using namespace std;
#include "http_client_loop.h"


bool http_client_loop::open(const std::string& url, const std::string& proxy)
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
			sslsocket_.set_verify_callback([](bool preverified, boost::asio::ssl::verify_context& ctx) -> bool
			{
				char subject_name[256];
				X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
				X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
#ifdef BOOST_ASIO_ENABLE_HANDLER_TRACKING 
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

void http_client_loop::close()
{
	if (is_open()) {
		sslsocket_.lowest_layer().close();
	}
}

void http_client_loop::cancel()
{
	sslsocket_.lowest_layer().cancel();
}

void http_client_loop::reopen()
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

void http_client_loop::start_once()
{
	resp_stream_.str("");
	next_session_s();
	next_session = [this]{
		//sslsocket_.get_io_context().stop();
	};
}

void http_client_loop::start()
{
	next_session();
}

bool http_client_loop::is_open()
{
	return sslsocket_.lowest_layer().is_open();
}

std::stringstream& http_client_loop::get_response()
{
	return resp_stream_;
}

Stat& http_client_loop::get_stat()
{
	return stat_;
}

StCode& http_client_loop::get_stcode()
{
	return stcode_;
}


std::string http_client_loop::now()
{
	std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	struct tm ttm = *localtime(&now);

	char buf[80];
	strftime(buf, sizeof(buf), "%Y-%m-%d %X", &ttm);

	return std::string(buf);
}

// private 

void http_client_loop::add_stat(uint64_t req, uint64_t resp, uint64_t transb, uint64_t dur)
{
	std::string sn = now();
	stat_[sn].add_stat(req, resp, transb, dur);
}

bool http_client_loop::urlparser(const std::string &url)
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


void http_client_loop::next_session_s()
{
	async_write();
	async_read_header();
}


void http_client_loop::async_write()
{
	build_reqeust();

	t0_ = std::chrono::high_resolution_clock::now();

	auto async_write_handler = [this/*, t0*/](const boost::system::error_code &err, size_t len)
	{
		if (!err) {
			add_stat(1, 0, len, 0);
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

void http_client_loop::async_read_header()
{
	using namespace boost::asio;
	using namespace std::chrono;

	auto async_read_until_handler = [this /*, t0*/](const boost::system::error_code &err, std::size_t len)
	{
		if (err) {
			close();
			reopen();

			next_session();
		}
		else {
			resp_stream_.str("");
			resp_stream_ << std::string(buffer_cast<const char*>(response_.data()), len);
			response_.consume(len);

			bool chunked = false;
			// header invalid check, get content-length 
			int content_length = parse_header(chunked);

			if (chunked) {
				if (chunked_end()) {
					parse_contents(true);
					next_session();
				}
				else {
					async_read_content(content_length, chunked);
				}
			}
			else {
				if (content_length > 0) {
					if (content_length > (int)response_.size()) {
						async_read_content(content_length, chunked);
					}
					else {
						parse_contents(chunked);
						next_session();
					}
				}
				else {
					next_session();
				}
			}
		}
	};

	// buffer initialize 
	response_.consume(response_.size());

	if (ssl_) {
		boost::asio::async_read_until(sslsocket_, response_, "\r\n\r\n", async_read_until_handler);
	}
	else {
		boost::asio::async_read_until(socket_, response_, "\r\n\r\n", async_read_until_handler);
	}
}

int http_client_loop::parse_header(bool &chunked)
{
	using std::string;
	using namespace boost::algorithm;
	int content_length = 0;

	try {
		// if (!resp_stream_) cerr << "resp_stream_ is null" << endl;
		auto &response_stream = resp_stream_;

		std::string header;
		getline(response_stream, header);

		std::vector<std::string> vs;
		split(vs, header, boost::is_any_of(" "), token_compress_on);

		if (vs.size() < 3) {
			std::ostringstream oss;
			oss << "Invalid http header 1 line, vs.size() :  " << vs.size() << std::endl;
			throw std::logic_error(oss.str());
		}

		std::string http_version = vs[0];
		std::string status_code = vs[1];
		std::string status_msg = vs[2];

		if (!response_stream || http_version.substr(0, 5) != "HTTP/")
		{
			throw std::logic_error("Invalid http header version");
		}

		// status_code statistics
		stcode_[http_version + " " + status_code]++;

		while (getline(response_stream, header) && header != "\r\n") {
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
	catch (std::exception &) {
		content_length = -1;
		//cout << e.what() << endl;
	}

	resp_stream_.clear();
	return content_length;
}


bool http_client_loop::chunked_end()
{
	using namespace boost::asio;

	bool chunked = false;
	if (response_.size() > 5) {
		string s { buffer_cast<const char*>(response_.data()), response_.size() };
		string ends = s.substr(response_.size() - 5);

		if (ends == "0\r\n\r\n") {
			chunked = true;
		}
	}
	return chunked;
}

void http_client_loop::async_read_content(size_t content_length, bool chunked)
{
	auto async_read_handler = [this, content_length, chunked](const boost::system::error_code &err, std::size_t len)
	{
		using namespace boost::asio;

		if (err) {	// err
			if (err != boost::asio::error::eof) {
				close();
				reopen();
			}
			next_session();
		}
		else {
			if (chunked) {
				if (chunked_end()) {
					parse_contents(true);
					next_session();
				}
				else {
					async_read_content(0, true);
				}
			}
			else {
				if (content_length > response_.size()) {
					async_read_content(content_length);
				}
				else {
					parse_contents(false);
					next_session();
				}
			}
		}
	};

	if (ssl_) {
		boost::asio::async_read(sslsocket_, response_, boost::asio::transfer_at_least(1), async_read_handler);
	}
	else {
		boost::asio::async_read(socket_, response_, boost::asio::transfer_at_least(1), async_read_handler);
	}
}

void http_client_loop::parse_contents(bool chunked)
{
	using namespace boost::asio;
	using namespace std::chrono;

	try {
		size_t content_length = 0;

		if (chunked) {
			std::string sbody { buffer_cast<const char*>(response_.data()), response_.size() };
			
			const string NEWLINE = "\r\n";
			const size_t NEWLINE_SIZE = NEWLINE.size();

			size_t spos = 0, epos;
			while (true) {
				epos = sbody.find(NEWLINE, spos);
				int len = std::stoi(sbody.substr(spos, epos), nullptr, 16);
				if (len == 0) break;

				resp_stream_ << sbody.substr(epos + NEWLINE_SIZE, len);

				spos = epos + NEWLINE_SIZE + len + NEWLINE_SIZE;
			}
			content_length = response_.size();
			response_.consume(response_.size());
		}
		else {
			// Content-Length : done
			resp_stream_ << std::string(buffer_cast<const char*>(response_.data()), response_.size());

			content_length = response_.size();
			response_.consume(response_.size());
		}

		auto t1 = high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<ms>(t1 - t0_).count();
		
		resp_stream_.seekp(0, std::ios::end);
		content_length = resp_stream_.tellp();

		add_stat(0, 1, content_length, duration);
	}
	catch (std::exception &) {}
}

void http_client_loop::build_reqeust()
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
