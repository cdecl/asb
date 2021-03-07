
#pragma once 


#include <iostream>
#include <fstream>
#include <iomanip>  
#include <thread>
#include <future>
#include <functional>
#include <mutex>
#include <vector>
#include <tuple>
using namespace std;

#ifdef _DEBUG 
#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#endif

#include "http_client_loop.h"

using http_client_list = vector<shared_ptr<http_client_loop>>;
using io_context_list = vector<shared_ptr<boost::asio::io_context>>;
using thread_list = std::vector<std::thread>;

struct Args {
	std::string url;
	std::string xurl;
	int connections;
	int threads;
	int duration  = 10;
	bool test = false;
	std::string method = "GET";
	std::string data;
	header_t headers;
};

struct result_t 
{
	uint64_t request = 0;
	uint64_t response = 0;
	uint64_t bytes = 0;
	uint64_t duration_tot = 0;
};

void Result(const std::string& start, int duration, uint64_t total_duration, http_client_list &vCons);
bool PreRun(const Args& args);
void Run(const Args& args);
std::tuple<Stat, StCode, result_t> Statistics(http_client_list &vCons);
void Result(const std::string& start, int duration, uint64_t total_duration, http_client_list &vCons);
std::string read_file(const std::string path);
bool Usage(int argc, char* argv[], Args &args);

