CMAKE_FLAGS := -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -Wno-dev

.PHONY: all configure build clean debug

all: build

configure:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=release $(CMAKE_FLAGS)

build: configure
	cmake --build build -j$$(nproc)

debug:
	cmake -S . -B  debug -DCMAKE_BUILD_TYPE=debug $(CMAKE_FLAGS) -DCMAKE_C_FLAGS="-fsanitize=address -g -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
	cmake --build debug -j$$(nproc)

clean:
	rm -rf build
	rm -rf debug