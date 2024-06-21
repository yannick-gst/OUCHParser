
#INC = -I./asio/include -I./args
CXX = g++
CXXFLAGS = -Wall -g $(INC) -std=c++17 -pthread

ouch: main.o ouchparser.o
	$(CXX) $(CXXFLAGS) -o ouch main.o ouchparser.o

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c main.cpp

ouchparser.o: ouchparser.cpp
	$(CXX) $(CXXFLAGS) -c ouchparser.cpp

.PHONY: clean
clean:
	rm -rf *.o ouch
