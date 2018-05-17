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

#ifdef _DEBUG 
#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#endif

#include "http_client_loop.h"
#include <boost/format.hpp>
using boost::str;
using boost::format;

using http_client_list = vector < shared_ptr<http_client_loop> > ;
using io_service_list = vector < shared_ptr<boost::asio::io_service> > ;

void Result(const std::string& start, int duration, uint64_t total_duration, http_client_list &vCons);

void Run(const std::string &url, const std::string &xurl, int connections, int threads, int duration, bool once,
	const std::string &method, const std::string &data, header_t &headers)
{
	// connection test
	{
		boost::asio::io_service io;
		http_client_loop client(io, method, data, std::move(headers));
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
	cout << str(format("> Start and Running %ds (%s)") % duration % start) << endl;
	cout << "  " << url << endl;
	cout << str(format("    %d connections and %d Threads ")
		% connections % threads) << endl;

	std::vector<thread> vThread;
	std::vector<thread> ios;

	std::mutex mtx;
	http_client_list vCons;

	boost::asio::io_service io;
	boost::asio::deadline_timer t(io, boost::posix_time::seconds(duration));
	t.async_wait([&io](const boost::system::error_code)
	{
		io.stop();
	});

	auto tm0 = chrono::high_resolution_clock::now();
	for (int i = 0; i < threads; ++i) {

		thread tr([&io, url, xurl, connections, threads, &method, &data, &headers, &vCons, &mtx]()
		{
			int cons = connections / threads;
			for (int i = 0; i < cons && !io.stopped(); ++i) {

				auto c = make_shared<http_client_loop>(io, method, data, std::move(headers));
				c->open(url, xurl);
				c->start();

				{
					std::lock_guard<std::mutex> lock(mtx);
					vCons.push_back(c);
				}
			}
			
			io.run();
		});

		vThread.push_back(move(tr));
	}

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
		double dbytes;
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

		return str(boost::format("%0.2lf%s") % dbytes % unit);
	};

	cout << str(format("> %-17s: %sms") % "Duration" % total_duration) << endl;
	cout << str(format("    %-15s: %0.2lfms") % "Latency" % (duration_tot / (double)response)) << endl;
	cout << str(format("    %-15s: %ld") % "Requests " % request) << endl;
	cout << str(format("    %-15s: %ld") % "Response " % response) << endl;
	cout << str(format("    %-15s: %s") % "Transfer" % fnFormat(bytes)) << endl;
	cout << "> Per seconds" << endl;
	cout << str(format("    %-15s: %0.2lf") % "Requests/sec" % (response / (double)duration)) << endl;
	cout << str(format("    %-15s: %s") % "Transfer/sec" % fnFormat(bytes / duration)) << endl;
	cout << "> Response Status" << endl;
	for (auto &val : status_codes) {
		cout << str(format("    %-15s: %d") % val.first % val.second) << endl;
	}

	cout << "> Response" << endl;
	cout << str(format("  %-14s %-7d %-7d") % "Time" % "Resp(c)" % "Lat(ms)") << endl;
	for (auto &v : statistics) {
		// linux bug : now() function 
		if (v.first >= start) {
			cout << str(format("  %14s %7d %7.2lf") % v.first.substr(5) % v.second.response % (v.second.duration / (double)v.second.response)) << endl;
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
	cout << "Usage: asb <options> <url>" << endl;
	cout << "  options:" << endl;
	cout << "    -d <N>    duraction(sec), default : 10" << endl;
	cout << "    -t <N>    threads, default core(thread::hardware_concurrency()) : " << threads << endl;
	cout << "    -c <N>    connections, default core x 10 : " << connections << endl;
	cout << "    -m <N>    method, default GET : " << endl;
	cout << "    -i <N>    POST input data " << endl;
	cout << "    -f <N>    POST input data, file path " << endl;
	cout << "    -h <N>    add header, repeat " << endl;
	cout << "    -x <N>    proxy url" << endl;
	cout << "    <url>     url" << endl;
	cout << "    -test     run test, output response header and body" << endl;
	cout << endl;
	cout << "  example:    asb -d 10 -c 10 -t 2 http://www.some_url/" << endl;
	cout << "  example:    asb -test http://www.some_url/" << endl;
	cout << "  version:    1.2 " << endl;
}

int main(int argc, char* argv[])
{
	int duration = 10;
	int threads = (int)thread::hardware_concurrency();
	int connections = threads * 10;
	std::string method = "GET";
	std::string data;
	header_t headers;
	
	if (argc < 2) {
		usage(duration, threads, connections);
		return -1;
	}
	
	bool test = false;
	std::string url;
	std::string xurl;

	try {
		int i = 1;
		for (; i < (argc - 1); ++i) {
			if (string("-d") == argv[i]) {
				duration = std::stoi(argv[++i]);
			}
			else if (string("-c") == argv[i]) {
				connections = std::stoi(argv[++i]);
			}
			else if (string("-t") == argv[i]) {
				threads = std::stoi(argv[++i]);
			}
			else if (string("-m") == argv[i]) {
				method = boost::algorithm::to_upper_copy(string(argv[++i]));
			}
			else if (string("-i") == argv[i]) {
				data = argv[++i];
			}
			else if (string("-f") == argv[i]) {
				data = read_file(argv[++i]);
			}
			else if (string("-h") == argv[i]) {
				headers.push_back(argv[++i]);
			}
			else if (string("-x") == argv[i]) {
				xurl = argv[++i];
			}
			else if (string("-test") == argv[i]) {
				test = true;
			}
		}

		if (i != (argc - 1)) throw std::logic_error("Parameter error");

		url = argv[argc - 1];
	}
	catch (...) {
		usage(duration, threads, connections);
		return -1;
	}
	 
	try {
		Run(url, xurl, connections, threads, duration, test, method, data, headers);
	}
	catch (exception &e) {
		cout << e.what() << endl;
	}

	return 0;
}

