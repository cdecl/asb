// asb.cpp
//

#ifdef _MSC_VER
 #include "stdafx.h"
#endif

#include "asb.h"

#include "CLI11/CLI11.hpp"
#include "fmt/format.h"


std::string read_file(const std::string path)
{
	ifstream fin(path.c_str(), ios_base::binary);
	ostringstream oss;
	std::copy(istreambuf_iterator<char>(fin), istreambuf_iterator<char>(), ostreambuf_iterator<char>(oss));
	return oss.str();
}

bool PreRun(const Args& args)
{
	boost::asio::io_context io;
	http_client_loop client(io, args.method, args.data, args.headers);
	bool b = client.open(args.url, args.xurl);

	if (!b) {
		cout << "> Connection error OR Invalid url" << endl;
		return false;
	}

	// -test mode 
	if (args.test) {
		client.start_once();
		io.run();

		auto &resp = client.get_response();
		if (resp) cout << resp->str() << endl;
		return false;
	}

	return true;
}

void Run(const Args& args)
{
	if (!PreRun(args)) {
		return;
	}
	
	std::string start = http_client_loop::now();
	cout << fmt::format("> Start and Running {}s ({})", args.duration, start) << endl;
	cout << "  " << args.url << endl;
	cout << fmt::format("    {} connections and {} Threads ", args.connections, args.threads) << endl;

	std::mutex mtx;
	thread_list vThread;
	io_context_list ios;	
	http_client_list vCons;

	boost::asio::io_context io;
	boost::asio::deadline_timer t(io, boost::posix_time::seconds(args.duration));
	t.async_wait([&io, &ios](const boost::system::error_code)
	{
		io.stop();
		for (auto &pio : ios) {
			pio->stop();
		}
	});

	auto tm0 = chrono::high_resolution_clock::now();
	for (int i = 0; i < args.threads; ++i) {
		auto pio = make_shared<boost::asio::io_context>();	

		thread tr([pio, &args, &vCons, &mtx]
		{
			auto &io = *pio;
			int cons = args.connections / args.threads;
			for (int i = 0; i < cons && !io.stopped(); ++i) {

				auto c = make_shared<http_client_loop>(io, args.method, args.data, args.headers);
				c->open(args.url, args.xurl);
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
	Result(start, args.duration, chrono::duration_cast<ms>(tm1 - tm0).count(), vCons);
}

std::tuple<Stat, StCode, result_t> Statistics(http_client_list &vCons)
{
	Stat statistics;
	StCode status_codes;
	result_t result;

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
		result.request += c.second.request;
		result.response += c.second.response;
		result.bytes += c.second.transfer_bytes;
		result.duration_tot += c.second.duration;
	}

	return make_tuple(move(statistics), move(status_codes), move(result));
}

void Result(const std::string& start, int duration, uint64_t total_duration, http_client_list &vCons)
{
	auto [statistics, status_codes, result] = Statistics(vCons);

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
	cout << fmt::format("    {:<15}: {:.2f}ms", "Latency", (result.duration_tot / (double)result.response)) << "\n";
	cout << fmt::format("    {:<15}: {}", "Requests ", result.request) << "\n";
	cout << fmt::format("    {:<15}: {}", "Response ", result.response) << "\n";
	cout << fmt::format("    {:<15}: {}", "Transfer", fnFormat(result.bytes)) << "\n";
	cout << "> Per seconds" << "\n";
	cout << fmt::format("    {:<15}: {:.2f}", "Requests/sec", (result.response / (double)duration)) << "\n";
	cout << fmt::format("    {:<15}: {}", "Transfer/sec", fnFormat(result.bytes / duration)) << "\n";
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

bool Usage(int argc, char* argv[], Args &args) 
{
	std::string data_path;
	args.threads = (int)thread::hardware_concurrency();
	args.connections = args.threads * 10;

	CLI::App app { "Http benchmarking and load test tool for windows, posix"};
	app.add_option("-d", args.duration, "Duraction(sec), default 10");
	app.add_option("-t", args.threads, fmt::format("Threads, default core(thread::hardware_concurrency()), default {}", args.threads));
	app.add_option("-c", args.connections, fmt::format("Connections, default core x 10, default {}", args.connections));
	app.add_option("-m", args.method, "Method, default GET ");
	app.add_option("-i", args.data, "POST input data ");
	app.add_option("-f", data_path, "POST input data, file path ")->check(CLI::ExistingFile);
	app.add_option("-H, --header", args.headers, "Add header, repeat");
	app.add_option("-x", args.xurl, "Proxy url");
	app.add_option("url", args.url, "Url")->required();
	app.add_flag("-T,--test", args.test, "Run test, output response header and body");
	app.footer(
		"  example:    asb -d 10 -c 10 -t 2 http://www.some_url/ \n"
		"  example:    asb --test http://www.some_url/ \n"	
		"  version:    1.5.3"
	);

	try {
		app.parse(argc, argv);
	}
	catch(const CLI::ParseError &e) {
		app.exit(e);
		return false;
	}
	//CLI11_PARSE(app, argc, argv);

	if (!data_path.empty()) {
		args.data = read_file(data_path);
	}

	if (0 != args.url.find("http")) {
		args.url = "http://"s + args.url;
	}
	return true;
}

int main(int argc, char* argv[])
{
	try {
		Args args;
		if (!Usage(argc, argv, args)) {
			return -1;
		}

		Run(args);
	}
	catch (exception &e) {
		cout << "[exception] " << e.what() << endl;
	}

	return 0;
}

