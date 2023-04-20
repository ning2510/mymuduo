all: server client

server :
	g++ -o server server.cc chatMsg.pb.cc codec.cc -lmymuduo -lprotobuf -lpthread -g

client :
	g++ -o client client.cc chatMsg.pb.cc codec.cc -lmymuduo -lprotobuf -lpthread -g

clean :
	rm server client
