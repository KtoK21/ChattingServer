all:
	g++ -std=c++11 -o server server.cpp -lpthread -g
	g++ -std=c++11 -o client client.cpp -lpthread -g
clean:
	rm server
