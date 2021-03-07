![C/C++ CI](https://github.com/cdecl/asb/workflows/C/C++%20CI/badge.svg)

# asb
Http benchmarking and load test tool for windows, posix 

> C++11, boost::asio, openssl

## Build 

### CMake (with vcpkg)

- vcpkg package install 
```sh
# windows : default --triplet=x86-windows
$ vcpkg.exe install boost-asio boost-regex openssl --triplet=x64-windows-static

# linux : x64-linux
$ ./vcpkg install boost-asio boost-regex openssl

$ ./vcpkg integrate install
Applied user-wide integration for this vcpkg root.

CMake projects should use: "-DCMAKE_TOOLCHAIN_FILE=/<path>/vcpkg/scripts/buildsystems/vcpkg.cmake"
```

- build 
```sh
# build directory
$ mkdir build && cd build

# linux cmake init
$ cmake .. -DCMAKE_TOOLCHAIN_FILE=/<path>/vcpkg/scripts/buildsystems/vcpkg.cmake

# windows cmake init - triplet x64-windows-static
$ cmake .. -DCMAKE_TOOLCHAIN_FILE=/<path>/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static

# build
$ cmake --build . --config Release
```



### Ubuntu
```sh
# boost regex, system, openssl 
$ sudo apt-get install libboost-regex-dev libboost-system-dev libssl-dev

$ mkdir build && cd build

# cmake init
$ cmake .. 

# build
# $ cmake --build . --config Release
$ make  
```

### Windows
> Visual C++ 2019

```cmd
cd src

REM nuget package restore 
nuget.exe restore asb.sln

REM build → src\x64\release\asb.exe
msbuild asb.vcxproj /p:configuration=release /p:platform=x64
```

## Usage

```bash
$  ./asb -h
Http benchmarking and load test tool for windows, posix
Usage: ./asb [OPTIONS] url

Positionals:
  url TEXT REQUIRED           Url

Options:
  -h,--help                   Print this help message and exit
  -d INT                      Duraction(sec), default 10
  -t INT                      Threads, default core(thread::hardware_concurrency()), default 4
  -c INT                      Connections, default core x 10, default 40
  -m TEXT                     Method, default GET
  -i TEXT                     POST input data
  -f TEXT:FILE                POST input data, file path
  -H,--header TEXT ...        Add header, repeat
  -x TEXT                     Proxy url
  -T,--test                   Run test, output response header and body

  example:    asb -d 10 -c 10 -t 2 http://www.some_url/
  example:    asb --test http://www.some_url/
  version:    1.5.1
```

- **Example**
```bash
$ ./asb -d 5 -t 3 -c 80 http://localhost/index.html
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
$ ./asb --test -H "User-Agent: cdecl/asb" -H "Referer: http://httpbin.org" -m POST -i "post data" http://httpbin.org/post
HTTP/1.1 200 OK
Date: Fri, 26 Feb 2021 06:59:12 GMT
Content-Type: application/json
Content-Length: 433
Server: gunicorn/19.9.0
Access-Control-Allow-Origin: *
Access-Control-Allow-Credentials: true
Connection: close

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
    "User-Agent": "cdecl/asb",
    "X-Amzn-Trace-Id": "Root=1-60389c40-4d769edb108bdfc9661adee2",
    "X-Bluecoat-Via": "ce2cfae06b3f12b4"
  },
  "json": null,
  "origin": "180.70.97.80",
  "url": "http://httpbin.org/post"
}
```
