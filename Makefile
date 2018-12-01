CFLAGS = -Wall -pedantic -g -pthread

.PHONY: all
all: chat-server chat-client 

chat-server: chat-server.c
	gcc $(CFLAGS) -o $@ $^

chat-client: chat-client.c
	gcc $(CFLAGS) -o $@ $^	

.PHONY: test
test: test-server test-client 

test-server: test-server.c
	gcc $(CFLAGS) -o $@ $^

test-client: test-client.c
	gcc $(CFLAGS) -o $@ $^	

.PHONY: clean
clean:
	rm -f chat-server chat-client test-server test-client 