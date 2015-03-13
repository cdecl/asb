# asb
simple bechmarket tool (using boost::asio) 


'''
Usage: asb <options> <url>
  options:
    -d <N>    duraction (seconds), default 10
    -c <N>    connections, default 10
    -t <N>    threads, default 2
    -once     run once, response console write, other parameters ignored

  example:    asb -d 10 -c 10 -t 2 http://www.some_url/
  example:    asb -once http://www.some_url/
  version:    0.3

'''
