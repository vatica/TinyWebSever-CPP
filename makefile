CXX ?= g++
CFLAGS = -std=c++11 -O2 -Wall -g 

TARGET = server
OBJS = ./code/log/*.cpp ./code/timer/*.cpp \
       ./code/http/*.cpp ./code/server/*.cpp ./code/CGImysql/*.cpp \
       ./code/config/*.cpp ./code/main.cpp

server: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o ./bin/$(TARGET) -pthread -lmysqlclient

clean:
	rm  -r ./bin/server