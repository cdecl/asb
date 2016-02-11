asb
======
Http benchmarking and load test tool for windows, posix 

> C++11, boost::asio, openssl



## Build

### Ubuntu

```sh

# build tool 
sudo apt-get install build-essential 

# boost - thread, regex, system, date-time
sudo apt-get install libboost-thread-dev libboost-regex-dev libb oost-system-dev libboost-date-time-dev

# openssl 
sudo apt-get install libssl-dev

git clone https://github.com/cdecl/asb.git
cd asb 

# bulid
make 

```

### Windows
> Visual C++ 2013

```bash

# boost download & build 
# win32
bjam.exe toolset=msvc-12.0 link=static runtime-link=static --with-system --with-date_time --with-regex --with-thread -j 4 stage
# x64
bjam.exe toolset=msvc-12.0 address-model=64 link=static runtime-link=static --with-system --with-date_time --with-regex --with-thread -j 4 stage

## openssl - NuGet package install  

```

## Usage

```bash
Usage: asb <options> <url>
  options:
    -d <N>    duraction(sec), default : 10
    -t <N>    threads, default core(thread::hardware_concurrency()) : ?
    -c <N>    connections, default core x 10 : ?
    -m <N>    method, default GET :
    -i <N>    POST input data
    -f <N>    POST input data, file path
    -h <N>    add header, repeat
    -x <N>    proxy url
    <url>     url
    -test     run test, output response header and body

  example:    asb -d 10 -c 10 -t 2 http://www.some_url/
  example:    asb -test http://www.some_url/
  version:    1.1.1
  
```

**Example**
```bash
asb -d 5 -t 3 -c 80 http://localhost/index.html
> Start and Running 5s (2015-04-02 22:22:20)
  http://localhost/index.html
    80 connections and 3 Threads
> Duration         : 5003ms
    Latency        : 1.73ms
    Requests       : 186768
    Response       : 186690
    Transfer       : 65.53MB
> Per seconds
    Requests/sec   : 37338.00
    Transfer/sec   : 13.11MB
> Response Status
    HTTP/1.1 200   : 186690
> Response
  Time           Resp(c) Lat(ms)
  04-02 22:22:20   15312    2.48
  04-02 22:22:21   35721    1.83
  04-02 22:22:22   37513    1.70
  04-02 22:22:23   38372    1.67
  04-02 22:22:24   40951    1.56
  04-02 22:22:25   18821    1.46


```


```bash
asb -test -h "User-Agent: cdecl/asb" -h "Referer: http://httpbin.org" -m POST -i "post data" http://httpbin.org/post
HTTP/1.1 200 OK
Server: nginx
Date: Sun, 22 Mar 2015 14:48:15 GMT
Content-Type: application/json
Content-Length: 324
Connection: keep-alive
Access-Control-Allow-Origin: *
Access-Control-Allow-Credentials: true

{
  "args": {},
  "data": "post data",
  "files": {},
  "form": {},
  "headers": {
    "Accept": "*/*",
    "Content-Length": "9",
    "Host": "httpbin.org",
    "Referer": "http://httpbin.org",
    "User-Agent": "cdecl/asb"
  },
  "json": null,
  "origin": "124.49.100.161",
  "url": "http://httpbin.org/post"
}
```
