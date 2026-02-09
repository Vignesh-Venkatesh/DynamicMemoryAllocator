# compiler
CXX = g++

# compiler flags
CXXFLAGS = -Wall -Werror -g

all: correctness

# debug:
# 	$(CXX) $(CXXFLAGS) main.cpp -o bin/dma

# dma:
# 	$(CXX) main.cpp -o bin/dma

# run:
# 	$(CXX) main.cpp -o bin/dma && ./bin/dma

correctness:
	$(CXX) $(CXXFLAGS) src/allocator.cpp tests/correctness_test.cpp -o bin/dma_correctness && ./bin/dma_correctness

clean:
	rm -rf bin/dma bin/dma_correctness
