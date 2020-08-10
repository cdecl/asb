import plotly.offline as offline
import plotly.graph_objs as go
import plotly.subplots as subplots
import re
import sys


def getResp(s):
	reg = re.compile(r'[\s]+[\d-]{5}[\s]+([\d:]{8})[\s]+([\d]+)[\s]+([\d.]+)')
	r = reg.findall(s)
	if len(r) > 0:
		return r[0]
	return ()

def main():
	time = []
	con = []
	lat = []

	while True:
		line = sys.stdin.readline()
		if not line:
			break
		r = getResp(line)
		if len(r) == 3:
			time.append(r[0])
			con.append(int(r[1]))
			lat.append(float(r[2]))

	tr1 = go.Scatter(x=time, y=con, name='Resp(c)')
	tr2 = go.Scatter(x=time, y=lat, name='Lat(ms)')

	fig = subplots.make_subplots(rows=2, cols=1)
	fig.append_trace(tr1, 1, 1)
	fig.append_trace(tr2, 2, 1)
	offline.plot(fig, auto_open=False, filename='plot.html', validate=False)

if __name__ == "__main__":
	main()
	