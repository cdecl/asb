# asb
Simple http benchmarking tool 
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
asb$ asb -d 5 -c 40 http://localhost
> Start and Running 5s (2015-03-17.10:35:49)
  http://localhost
    40 connections and 2 Threads
> Duration         : 5007ms
    Latency        : 0.07ms
    Requests       : 76434
    Response       : 75660
    Transfer       : 68.76MB
> Per seconds
    Requests/sec   : 15132.00
    Transfer/sec   : 13.75MB
> Response Status
    HTTP/1.1 200   : 75660
```


```
asb$ asb -d 5 https://www.google.co.kr/?gws_rd=ssl
> Start and Running 5s (2015-03-17.10:24:41)
  https://www.google.co.kr/?gws_rd=ssl
    10 connections and 2 Threads
> Duration         : 5007ms
    Latency        : 17.09ms
    Requests       : 580
    Response       : 293
    Transfer       : 12.17MB
> Per seconds
    Requests/sec   : 73.25
    Transfer/sec   : 3.04MB
> Response Status
    HTTP/1.1 200   : 293
```

```
asb$ asb -test https://www.google.co.kr/?gws_rd=ssl
HTTP/1.1 200 OK
Date: Tue, 17 Mar 2015 14:25:54 GMT
Expires: -1
Cache-Control: private, max-age=0
Content-Type: text/html; charset=ISO-8859-1
Set-Cookie: PREF=ID=e82da8266e8de950:FF=0:NW=1:TM=1426602354:LM=1426602354:S=xagZL1GoBJZNCDZo; expires=Thu, 16-Mar-2017 14:25:54 GMT; path=/; domain=.google.co.kr
Set-Cookie: NID=67=q7Mb2Ih2weD7bF9ruv5mAxJUh8VBAXJtRy0bNNpMAyepO-xGTI0prIRfBmzucZICbWaOimge_nv3T8Oqq32Pz02qEAU3K56LpvTvKOnZF_vnMAvym48koALliYO4slze; expires=Wed, 16-Sep-2015 14:25:54 GMT; path=/; domain=.google.co.kr; HttpOnly
P3P: CP="This is not a P3P policy! See http://www.google.com/support/accounts/bin/answer.py?hl=en&answer=151657 for more info."
Server: gws
X-XSS-Protection: 1; mode=block
X-Frame-Options: SAMEORIGIN
Alternate-Protocol: 80:quic,p=0.5
Accept-Ranges: none
Vary: Accept-Encoding
Transfer-Encoding: chunked

8000
<!doctype html><html itemscope="" itemtype="http://schema.org/WebPage" lang="ko"><head><meta content="/images/google_favicon_128.png" itemprop="image" ................................

```
