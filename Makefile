CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Iinclude

SRC = $(wildcard src/*.cpp)
APP = quant_engine
TEST_SRC = $(filter-out src/main.cpp,$(SRC)) tests/test_main.cpp
TEST_APP = quant_tests

all:
	$(CXX) $(CXXFLAGS) $(SRC) -o $(APP)

test:
	$(CXX) $(CXXFLAGS) $(TEST_SRC) -o $(TEST_APP)
	./$(TEST_APP)

clean:
	rm -f $(APP) $(TEST_APP)

.PHONY: all test clean
