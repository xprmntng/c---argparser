# Define the output executable name
TARGET = argparse

# Define where the build system should look for header files (both C and C++)
INCLUDE_DIRECTORY = ./include

# Define C++ compiler
CXX_COMPILER = g++

# Define C++ compiler flags. Here's what they do:
# -std: Defines which C or C++ standard to target; We target C++ 2020
# -Wall: Turns on most critical and commonly-encounted warnings
# -Wextra: Turns on additional useful warnings not covered by -Wall
# -MMD: Creates dependency files with `.d` extension that contain a list of
#       your C++ source file's header file dependencies, causing your source
#		code files to be rebuilt if a change to a header file is made
# -MP: Tells gcc/g++ to pay attention to deleted or renamed header files
CXX_COMPILER_FLAGS := -I$(INCLUDE_DIRECTORY) -std=c++20 -Wall -Wextra -MMD -MP

# Define C++ linker flags
CXX_LINKER_FLAGS = 

# Create a list of `.cpp` input files found within `./src` by doing a recursive search
CXX_SOURCES := $(shell find ./src -type f -iname \*.cpp)

# Create a list of build targets from the above list of input files by
# replacing `./src/` prefix and `.cpp` suffix with `./build/` prefix, `.o` suffix
CXX_OBJECTS := $(CXX_SOURCES:./src/%.cpp=./build/%.o)

# Rule to build all targets defined by this Makefile
all: ./build/$(TARGET)

# Define how we build our target
./build/$(TARGET): $(CXX_OBJECTS) $(C_OBJECTS)
	mkdir --parents ./build
	$(CXX_COMPILER) $(CXX_LINKER_FLAGS) -o ./build/$(TARGET) $(CXX_OBJECTS)

# Define how we compile .cpp files into .o files
./build/%.o: ./src/%.cpp
	mkdir --parents `dirname $@`
	$(CXX_COMPILER) $(CXX_COMPILER_FLAGS) -c $< -o $@

# `make clean` clears out the `build` directory
clean:
	rm -rf ./build/*