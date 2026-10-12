CXX      ?= g++
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra
# Optional, faster on your own machine:  make CXXFLAGS="-O3 -march=native -std=c++17"

all: connect4

connect4: src/main.cpp src/solver.hpp src/position.hpp
	$(CXX) $(CXXFLAGS) -o $@ src/main.cpp

reference: tests/reference_solver.cpp
	$(CXX) -O2 -std=c++17 -o $@ tests/reference_solver.cpp

test: connect4
	bash tests/run_tests.sh

crosscheck: connect4 reference
	python3 tests/crosscheck.py

clean:
	rm -f connect4 reference

.PHONY: all test crosscheck clean
