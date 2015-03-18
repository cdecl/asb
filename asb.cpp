// asb.cpp
//

#ifdef _MSC_VER
 #include "stdafx.h"
#endif

#include <iostream>
#include <iomanip>  
#include <thread>
#include <future>
#include <functional>
#include <mutex>
using namespace std;

#ifdef _DEBUG 
#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#endif

#include "http_client.h"
#include <boost/format.hpp>
using boost::str;
using boost::format;

using http_client_list = vector < shared_ptr<http_client> > ;
using io_service_list = vector < shared_ptr<boost::asio::io_service> > ;

void Run(const std::string &url, int connections, int threads, int duration, bool once);
void Result(int duration, uint64_t total_duration, http_client_list &vCons);

void Run(const std::string &url, int connections, int threads, int duration, bool once)
{
	// connection test
	{
		boost::asio::io_service io;
		http_client client(io);
		bool b = client.open(url);

		if (!b) {
			cout << "> Connection error OR Invalid url" << endl;
			return;
		}

		if (once) {
			client.start_once();
			io.run();

			cout << client.get_response().str() << endl;
			return;
		}
	}
	
	cout << str(format("> Start and Running %ds (%s)") % duration % http_client::now()) << endl;
	cout << "  " << url << endl;
	cout << str(format("    %d connections and %d Threads ")
		% connections % threads) << endl;

	std::vector<thread> vThread;
	std::vector<thread> ios;

	std::mutex mtx;
	http_client_list vCons;
	io_service_list vio;

	auto tm0 = chrono::high_resolution_clock::now();
	for (int i = 0; i < threads; ++i) {

		auto sio = make_shared<boost::asio::io_service>();
		vio.push_back(sio);

		thread tr([sio, url, connections, threads, duration, &vCons, &mtx]()
		{
			boost::asio::deadline_timer t(*sio, boost::posix_time::seconds(duration));
			t.async_wait([sio](const boost::system::error_code){
				sio->stop();
			});

			int cons = connections / threads;
			for (int i = 0; i < cons; ++i) {

				auto c = make_shared<http_client>(*sio);
				c->open(url);
				c->start();

				{
					std::lock_guard<std::mutex> lock(mtx);
					vCons.push_back(c);
				}
			}
			
			sio->run();
		});

		vThread.push_back(move(tr));
	}

	// thread join
	for (auto &t : vThread) {
		t.join();
	}

	auto tm1 = chrono::high_resolution_clock::now();
	Result(duration, chrono::duration_cast<ms>(tm1 - tm0).count(), vCons);

	vCons.clear();
}


void Result(int duration, uint64_t total_duration, http_client_list &vCons)
{
	Stat stat;
	StCode stcode;
	uint64_t request = 0;
	uint64_t response = 0;
	uint64_t bytes = 0;

	for (auto &c : vCons) {
		for (auto &val : c->get_stat()) {
			stat[val.first].request += val.second.request;					// count
			stat[val.first].response += val.second.response;				// count
			stat[val.first].transfer_bytes += val.second.transfer_bytes;	// transfer  data size
		}

		for (auto &val : c->get_stcode()) {
			stcode[val.first] += val.second;
		}
	}

	for (auto &c : stat) {
		request += c.second.request;
		response += c.second.response;
		bytes += c.second.transfer_bytes;
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
	cout << str(format("    %-15s: %0.2lfms") % "Latency" % (total_duration / (double)response)) << endl;

	cout << str(format("    %-15s: %ld") % "Requests " % request) << endl;
	cout << str(format("    %-15s: %ld") % "Response " % response) << endl;
	cout << str(format("    %-15s: %s") % "Transfer" % fnFormat(bytes)) << endl;
	cout << "> Per seconds" << endl;
	cout << str(format("    %-15s: %0.2lf") % "Requests/sec" % (response / (double)duration)) << endl;
	cout << str(format("    %-15s: %s") % "Transfer/sec" % fnFormat(bytes / duration)) << endl;
	cout << "> Response Status" << endl;
	for (auto &val : stcode) {
		cout << str(format("    %-15s: %d") % val.first % val.second) << endl;
	}

	cout << endl;
}


void usage(int duration, int threads, int connections)
{
	cout << "Usage: asb <options> <url>" << endl;
	cout << "  options:" << endl;
	cout << "    -d <N>    duraction (seconds), default : 10" << endl;
	cout << "    -t <N>    threads, default is core(thread::hardware_concurrency()) : " << threads << endl;
	cout << "    -c <N>    connections, default is core x 5 : " << connections << endl;
	cout << "    <url>     support http, https" << endl;
	cout << "    -test     run test, other parameters ignore " << endl;
	cout << endl;
	cout << "  example:    asb -d 10 -c 10 -t 2 http://www.some_url/" << endl;
	cout << "  example:    asb -once http://www.some_url/" << endl;
	cout << "  version:    0.5" << endl;
}

int main(int argc, char* argv[])
{
	int duration = 10;
	int threads = (int)thread::hardware_concurrency();
	int connections = threads * 5;

	if (argc < 2) {
		usage(duration, threads, connections);
		return -1;
	}
	
	bool test = false;
	std::string url;

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
		Run(url, connections, threads, duration, test);
	}
	catch (exception &e) {
		cout << e.what() << endl;
	}

	return 0;
}

