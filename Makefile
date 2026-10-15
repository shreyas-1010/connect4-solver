CXX      ?= g++
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -pthread
# Optional, faster on your own machine:  make CXXFLAGS="-O3 -march=native -std=c++17 -pthread"

THREADS    ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)
BOOK_DEPTH ?= 6

SRC = src/main.cpp src/solver.hpp src/position.hpp src/book.hpp src/bookgen.hpp

all: connect4

connect4: $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ src/main.cpp

reference: tests/reference_solver.cpp
	$(CXX) -O2 -std=c++17 -o $@ tests/reference_solver.cpp

# Opening book. Depth 6 took 80 minutes on an Intel i5-8265U; deeper books take much longer.
# Safe to interrupt and re-run: progress is saved.
#   make book                  (depth $(BOOK_DEPTH), $(THREADS) threads)
#   make book BOOK_DEPTH=8
book: connect4
	./connect4 genbook $(BOOK_DEPTH) --threads $(THREADS) --out book.bin

verify-book: connect4
	./connect4 verifybook --book book.bin

test: connect4
	bash tests/run_tests.sh
	bash tests/book_test.sh

crosscheck: connect4 reference
	python3 tests/crosscheck.py

clean:
	rm -f connect4 reference

.PHONY: all test crosscheck clean book verify-book
