.PHONY: configure build run test clean

configure:
	cmake -S . -B build -G Ninja

build:
	cmake --build build

run: build
	./build/ctsms_server

test: build
	ctest --test-dir build --output-on-failure

clean:
	cmake -E rm -rf build
