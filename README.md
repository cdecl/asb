# asb
Http benchmarking tool 
> using C++, boost::asio, libssl-dev(openssl) 


###Usage###


```
Usage: asb <options> <url>
  options:
    -d <N>    duraction (seconds), default : 10
    -t <N>    threads, default is core(thread::hardware_concurrency()) : 4
    -c <N>    connections, default is core x 5 : 20
    -m <N>    method, default GET :
    -i <N>    input, post data
    -f <N>    input, file path
    -h <N>    add hedaers, repeat
    -x <N>    proxy url
    <url>     support http, https
    -test     run test

  example:    asb -d 10 -c 10 -t 2 http://www.some_url/
  example:    asb -once http://www.some_url/
  version:    0.7
  
```

**Example**
```
asb -d 5 -t 3 -c 40 http://localhost/index.html
> Start and Running 5s (2015-03-22.23:41:52)
  http://localhost/index.html
    40 connections and 3 Threads
> Duration         : 5001ms
    Latency        : 0.03ms
    Requests       : 178675
    Response       : 178637
    Transfer       : 61.67MB
> Per seconds
    Requests/sec   : 35727.40
    Transfer/sec   : 12.33MB
> Response Status
    HTTP/1.1 200   : 178637

```


```
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
