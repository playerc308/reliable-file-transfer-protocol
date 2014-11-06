all: sendfile

sendfile: sendfile.cc
	g++ -Wall -o sendfile sendfile.cc

clean:
	rm -f sendfile
	rm -f recvfile
