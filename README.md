http-server
===========

A simple threaded http server in C

### Example Usage - Compile & Run

	gcc -pthread http-server.c -o http-server
	./http-server 8090 path/to/document-root

Currently only supports GET requests and 10 preset content types.