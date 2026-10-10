CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Icore

CORE := core/sand.cpp core/session.cpp core/font.cpp core/scene.cpp core/menu.cpp
HDRS := $(wildcard core/*.hpp)
TESTS := host/test_sand host/test_session host/test_scene host/test_menu

.PHONY: test sim build flash clean

test: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

host/test_%: host/test_%.cpp $(CORE) $(HDRS)
	$(CXX) $(CXXFLAGS) $< $(CORE) -o $@

sim: host/sim
	./host/sim

host/sim: host/sim.cpp $(CORE) $(HDRS)
	$(CXX) $(CXXFLAGS) $$(sdl2-config --cflags) host/sim.cpp $(CORE) $$(sdl2-config --libs) -o $@

build:
	./scripts/idf set-target esp32s3
	./scripts/idf build

flash:
	PORT=$(PORT) ./scripts/idf -p $(PORT) flash

clean:
	rm -rf build $(TESTS) host/sim host/out
