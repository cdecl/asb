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

#include "http_client.h"
#include <boost/format.hpp>
using boost::str;
using boost::format;



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
	vector<shared_ptr<http_client>> vCons;
	vector<shared_ptr<boost::asio::io_service>> vio;

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
	{
		Stat stat;
		StCode stcode;
		uint64_t request = 0;
		uint64_t response = 0;
		uint64_t bytes = 0;
		uint64_t totial_duration = chrono::duration_cast<ms>(tm1 - tm0).count();
		
		for (auto &c : vCons) {
			for (auto &val : c->get_stat()) {
				stat[val.first].request += val.second.request;		// count
				stat[val.first].response += val.second.response;		// count
				stat[val.first].recv_bytes += val.second.recv_bytes;	// read data size
			}

			for (auto &val : c->get_stcode()) {
				stcode[val.first] += val.second;	
			}
		}

		for (auto &c : stat) {
			request += c.second.request;
			response += c.second.response;
			bytes += c.second.recv_bytes;
		}

		auto fnFormat = [](uint64_t size) -> string {
			double dbytes;
			string unit = "B";
			if (size > 1024) {		
				dbytes = size / 1024.0;
				unit = "KB";
			}
			else if (size > (1024 * 1024)) {
				dbytes = size / (1024 * 1024.0);
				unit = "MB";
			}
			else if (size > (1024 * 1024 * 1024)) {
				dbytes = size / (1024 * 1024 * 1024.0);
				unit = "GB";
			}
			else {}

			return str(boost::format("%0.2lf%s") % dbytes % unit);
		};
		
		cout << str(format("> %-22s: %sms") % "Duration" % totial_duration) << endl;
		cout << str(format("    %-20s: %0.2lfms") % "Latency" % (totial_duration / (double)response)) << endl;

		cout << str(format("    %-20s: %ld") % "Requests Count" % request) << endl;
		cout << str(format("    %-20s: %ld") % "Response Count" % response) << endl;
		cout << str(format("    %-20s: %s") % "Response Bytes" % fnFormat(bytes)) << endl;
		cout << "> Per seconds" << endl;
		cout << str(format("    %-20s: %0.2lf") % "Requests/sec" % (response / (double)(stat.size() - 1))) << endl;
		cout << str(format("    %-20s: %s") % "Response Bytes/sec" % fnFormat(bytes / (stat.size() - 1))) << endl;
		cout << "> Response Status Code" << endl;
		for (auto &val : stcode) {
			cout << str(format("    %-20s: %d") % val.first % val.second) << endl;
		}

		cout << endl;
	}

	vCons.clear();
}


void usage()
{
	cout << "Usage: asb <options> <url>" << endl;
	cout << "  options:" << endl;
	cout << "    -d <N>    duraction (seconds)" << endl;
	cout << "    -c <N>    connections" << endl;
	cout << "    -t <N>    threads" << endl;
	cout << "    -once     run once, response write, other parameters skip " << endl;
	cout << endl;
	cout << "  example:    asb -d 10 -c 10 -t 2 http://www.some_url/" << endl;
	cout << "  example:    asb -once http://www.some_url/" << endl;
	cout << "  version:    0.1" << endl;
}

int main(int argc, char* argv[])
{
	if (argc < 2) {
		usage();
		return -1;
	}

	int connections = 10;
	int threads = 2;
	int duration = 10;
	bool once = false;
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
			else if (string("-once") == argv[i]) {
				once = true;
			}
		}

		if (i != (argc - 1)) throw std::logic_error("Parameter error");

		url = argv[argc - 1];
	}
	catch (...) {
		usage();
		return -1;
	}
	 
	try {
		Run(url, connections, threads, duration, once);
	}
	catch (exception &e) {
		cout << e.what() << endl;
	}

	return 0;
}

