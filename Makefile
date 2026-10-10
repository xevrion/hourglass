CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Icore

.PHONY: test preview build flash clean

test: host/test_sand
	./host/test_sand

host/test_sand: host/test_sand.cpp core/sand.cpp core/sand.hpp
	$(CXX) $(CXXFLAGS) host/test_sand.cpp core/sand.cpp -o $@

host/preview: host/preview.cpp core/sand.cpp core/sand.hpp
	$(CXX) $(CXXFLAGS) host/preview.cpp core/sand.cpp -o $@

preview: host/preview
	mkdir -p host/frames
	./host/preview host/frames

build:
	./scripts/idf set-target esp32s3
	./scripts/idf build

flash:
	PORT=$(PORT) ./scripts/idf -p $(PORT) flash

clean:
	rm -rf build host/test_sand host/preview host/frames
