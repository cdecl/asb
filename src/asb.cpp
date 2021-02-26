// asb.cpp
//

#ifdef _MSC_VER
 #include "stdafx.h"
#endif

#include <iostream>
#include <fstream>
#include <iomanip>  
#include <thread>
#include <future>
#include <functional>
#include <mutex>
#include <vector>
using namespace std;

#include "CLI11/CLI11.hpp"
#include "fmt/format.h"

#ifdef _DEBUG 
#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#endif

#include "http_client_loop.h"

using http_client_list = vector<shared_ptr<http_client_loop>>;
using io_context_list = vector<shared_ptr<boost::asio::io_context>>;
using thread_list = std::vector<std::thread>;

void Result(const std::string& start, int duration, uint64_t total_duration, http_client_list &vCons);

void Run(const std::string &url, const std::string &xurl, int connections, int threads, int duration, bool once,
	const std::string &method, const std::string &data, header_t &headers)
{
	// connection test
	{
		boost::asio::io_context io;
		http_client_loop client(io, method, data, headers);
		bool b = client.open(url, xurl);

		if (!b) {
			cout << "> Connection error OR Invalid url" << endl;
			return;
		}

		// -test mode 
		if (once) {
			client.start_once();
			io.run();

			auto &resp = client.get_response();
			if (resp) cout << resp->str() << endl;
			return;
		}
	}
	
	std::string start = http_client_loop::now();
	cout << fmt::format("> Start and Running {}s ({})", duration, start) << endl;
	cout << "  " << url << endl;
	cout << fmt::format("    {} connections and {} Threads ", connections, threads) << endl;

	std::mutex mtx;
	thread_list vThread;
	io_context_list ios;	
	http_client_list vCons;

	boost::asio::io_context io;
	boost::asio::deadline_timer t(io, boost::posix_time::seconds(duration));
	t.async_wait([&io, &ios](const boost::system::error_code)
	{
		io.stop();
		for (auto &pio : ios) {
			pio->stop();
		}
	});

	auto tm0 = chrono::high_resolution_clock::now();
	for (int i = 0; i < threads; ++i) {
		auto pio = make_shared<boost::asio::io_context>();	

		thread tr([pio, url, xurl, connections, threads, &method, &data, &headers, &vCons, &mtx]
		{
			auto &io = *pio;
			int cons = connections / threads;
			for (int i = 0; i < cons && !io.stopped(); ++i) {

				auto c = make_shared<http_client_loop>(io, method, data, headers);
				c->open(url, xurl);
				c->start();

				{
					std::lock_guard<std::mutex> lock(mtx);
					vCons.push_back(c);
				}
			}
		
			io.run();
		});

		ios.emplace_back(move(pio));
		vThread.push_back(move(tr));
	}

	io.run();

	// thread join
	for (auto &t : vThread) {
		t.join();
	}

	auto tm1 = chrono::high_resolution_clock::now();
	Result(start, duration, chrono::duration_cast<ms>(tm1 - tm0).count(), vCons);

	vCons.clear();
}


void Result(const std::string& start, int duration, uint64_t total_duration, http_client_list &vCons)
{
	Stat statistics;
	StCode status_codes;
	uint64_t request = 0;
	uint64_t response = 0;
	uint64_t bytes = 0;
	uint64_t duration_tot = 0;

	for (auto &c : vCons) {
		for (auto &val : c->get_stat()) {
			statistics[val.first].request += val.second.request;					// count
			statistics[val.first].response += val.second.response;				// count
			statistics[val.first].transfer_bytes += val.second.transfer_bytes;	// transfer  data size
			statistics[val.first].duration += val.second.duration;
		}

		for (auto &val : c->get_stcode()) {
			status_codes[val.first] += val.second;
		}
	}

	for (auto &c : statistics) {
		request += c.second.request;
		response += c.second.response;
		bytes += c.second.transfer_bytes;
		duration_tot += c.second.duration;
	}

	auto fnFormat = [](uint64_t size) -> string {
		double dbytes{};
		string unit = "B";

		if (size > uint64_t(1024 * 1024 * 1024)) {
			dbytes = size / (1024 * 1024 * 1024.0);
			unit = "GB";
		}
		else if (size > uint64_t(1024 * 1024)) {
			dbytes = size / (1024 * 1024.0);
			unit = "MB";
		}
		else if (size > 1024) {
			dbytes = size / 1024.0;
			unit = "KB";
		}
		else {}

		return fmt::format("{:.2f}{}", dbytes, unit);  // %0.2lf%s
	};

	cout << fmt::format("> {:<17}: {}ms", "Duration", total_duration) << "\n";
	cout << fmt::format("    {:<15}: {:.2f}ms", "Latency", (duration_tot / (double)response)) << "\n";
	cout << fmt::format("    {:<15}: {}", "Requests ", request) << "\n";
	cout << fmt::format("    {:<15}: {}", "Response ", response) << "\n";
	cout << fmt::format("    {:<15}: {}", "Transfer", fnFormat(bytes)) << "\n";
	cout << "> Per seconds" << "\n";
	cout << fmt::format("    {:<15}: {:.2f}", "Requests/sec", (response / (double)duration)) << "\n";
	cout << fmt::format("    {:<15}: {}", "Transfer/sec", fnFormat(bytes / duration)) << "\n";
	cout << "> Response Status" << "\n";
	for (auto &val : status_codes) {
		cout << fmt::format("    {:<15}: {}", val.first, val.second) << "\n";
	}

	cout << "> Response" << "\n";
	cout << fmt::format("  {:<14} {:>8} {:>8}", "Time", "Resp(c)", "Lat(ms)") << "\n";
	for (auto &v : statistics) {
		// linux bug : now() function 
		if (v.first >= start) {
			cout << fmt::format("  {:<14} {:>8} {:>8.2f}", v.first.substr(5), v.second.response, (v.second.duration / (double)v.second.response)) << "\n";
		}
	}

	cout << endl;
}


std::string read_file(const std::string path)
{
	ifstream fin(path.c_str(), ios_base::binary);
	ostringstream oss;
	std::copy(istreambuf_iterator<char>(fin), istreambuf_iterator<char>(), ostreambuf_iterator<char>(oss));
	return oss.str();
}

void usage(int duration, int threads, int connections)
{
	cout << "Usage: asb <options> <url>" << "\n";
	cout << "  options:" << "\n";
	cout << "    -d <N>    duraction(sec), default : 10" << "\n";
	cout << "    -t <N>    threads, default core(thread::hardware_concurrency()) : " << threads << "\n";
	cout << "    -c <N>    connections, default core x 10 : " << connections << "\n";
	cout << "    -m <N>    method, default GET : " << "\n";
	cout << "    -i <N>    POST input data " << "\n";
	cout << "    -f <N>    POST input data, file path " << "\n";
	cout << "    -h <N>    add header, repeat " << "\n";
	cout << "    -x <N>    proxy url" << "\n";
	cout << "    <url>     url" << "\n";
	cout << "    -test     run test, output response header and body" << "\n";
	cout << "\n";
	cout << "  example:    asb -d 10 -c 10 -t 2 http://www.some_url/" << "\n";
	cout << "  example:    asb -test http://www.some_url/" << "\n";
	cout << "  version:    1.4 " << "\n";
#if _MSC_VER 
	cout << "  bulid: _MSC_VER " << _MSC_VER << "\n";
#endif 
}

int main(int argc, char* argv[])
{
	try {
		int duration = 10;
		int threads = (int)thread::hardware_concurrency();
		int connections = threads * 10;
		std::string method = "GET";
		std::string data;
		header_t headers;
		std::string xurl;
		std::string url;
		bool test = false;

		CLI::App app { "Http benchmarking and load test tool for windows, posix"};
		app.add_option("-d", duration, "Duraction(sec), default 10");
		app.add_option("-t", threads, fmt::format("Threads, default core(thread::hardware_concurrency()), default {}", threads));
		app.add_option("-c", connections, fmt::format("Connections, default core x 10, default {}", connections));
		app.add_option("-m", method, "Method, default GET ");
		app.add_option("-i", data, "POST input data ");
		app.add_option("-H, --header", headers, "Add header, repeat");
		app.add_option("-x", xurl, "Proxy url");
		app.add_option("url", url, "Url")->required();
		app.add_flag("-T,--test", test, "Run test, output response header and body");
		app.footer(
			"  example:    asb -d 10 -c 10 -t 2 http://www.some_url/ \n"
			"  example:    asb --test http://www.some_url/ \n"
			"  version:    1.4              \n"
		);

		CLI11_PARSE(app, argc, argv);

		if (0 != url.find("http")) {
			url = "http://"s + url;
		}

		Run(url, xurl, connections, threads, duration, test, method, data, headers);
	}
	catch (exception &e) {
		cout << e.what() << endl;
	}

	return 0;
}

