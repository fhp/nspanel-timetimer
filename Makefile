CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O1

build/test_timetimer: tests/test_timetimer.cpp components/timetimer/timetimer.h
	mkdir -p build
	$(CXX) $(CXXFLAGS) -I components/timetimer tests/test_timetimer.cpp -o $@

.PHONY: test
test: build/test_timetimer
	./build/test_timetimer
