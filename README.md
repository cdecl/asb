# asb
simple benchmarking tool 
> using boost::asio, libssl-dev(openssl) 


```
Usage: asb <options> <url>
  options:
    -d <N>    duraction (seconds), default 10
    -c <N>    connections, default 10
    -t <N>    threads, default 2
    -once     run once, response console write, other parameters ignored

  example:    asb -d 10 -c 10 -t 2 http://www.some_url/
  example:    asb -once http://www.some_url/
  version:    0.3

```


```
asb$ asb -d 5 http://en.cppreference.com/w/
> Start and Running 5s (Fri Mar 13 11:18:41 2015)
  http://en.cppreference.com/w/
    10 connections and 2 Threads
> Duration         : 5201ms
    Latency        : 24.89ms
    Requests       : 228
    Response       : 209
    Transfer       : 189.64KB
> Per seconds
    Requests/sec   : 52.25
    Transfer/sec   : 47.41KB
> Response Status
    HTTP/1.1 404   : 209

```

```
asb$ asb -once  http://en.cppreference.com/w/
HTTP/1.1 404 Not Found
Date: Sat, 14 Mar 2015 02:02:01 GMT
Server: Apache
Accept-Ranges: bytes
Keep-Alive: timeout=2, max=10
Connection: Keep-Alive
Transfer-Encoding: chunked
Content-Type: text/html; charset=iso-8859-1

1d9
<?xml version="1.0" encoding="ISO-8859-1"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<head>
<title>404 Not Found</title>
<style type="text/css"><!--/*--><![CDATA[/*><!--*/
    body { color: #000000; background-color: #FFFFFF; font-family: sans-serif;}
/*]]>*/--></style>
</head>

<body>
<h1>404 Not Found</h1>

<p>
The requested URL (
56
http://en.cppreference.com:80/w/)
was not found on this server.
</p>

</body>
</html>

0

```
