# compiler
CXX = g++

# compiler flags
CXXFLAGS = -Wall -Werror -g

all: dma

debug:
	$(CXX) $(CXXFLAGS) main.cpp -o dma

dma:
	$(CXX) main.cpp -o dma

run:
	$(CXX) main.cpp -o dma && ./dma

clean:
	rm -rf dma
