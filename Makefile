#---------------------------------------------------------------------------------------------------
#                                             VARIABLES
#---------------------------------------------------------------------------------------------------

# Define the output executable name
TARGET = argparser

# Define where the build system should look for header files
INCLUDE_DIRECTORY = ./include

# Define C++ compiler
CXX_COMPILER = g++

# Define C++ compiler flags. Here's what they do:
# -std: Defines which C or C++ standard to target
# -Wall: Turns on most critical and commonly-encountered warnings
# -Wextra: Turns on additional useful warnings not covered by -Wall
# -MMD: Creates dependency files with `.d` extension that contain a list of
#       your C++ source file's header file dependencies, causing your source
#		code files to be rebuilt if a change to a header file is made
# -MP: Tells gcc/g++ to pay attention to deleted or renamed header files
# -g: Tells gcc to compile with debugging symbols included
CXX_COMPILER_FLAGS = -I$(INCLUDE_DIRECTORY) -std=c++23 -Wall -Wextra -MMD -MP -g

# Define C++ linker flags
CXX_LINKER_FLAGS =

# Create a list of `.cpp` input files found within `./src` by doing a recursive search
CXX_SOURCES := $(shell find ./src -type f -iname \*.cpp)

# Create a list of build targets from the above list of input files by
# replacing `./src/` prefix and `.cpp` suffix with `./build/` prefix, `.o` suffix
CXX_OBJECTS := $(CXX_SOURCES:./src/%.cpp=./build/%.o)

# Use the above list to create a list of .o files needed to build the project into a static library
# with .a extension by removing `main.o` from the original list
CXX_OBJECTS_NO_MAIN := $(filter-out main.o,$(CXX_OBJECTS))

# Turn the list of paths to .o files into a list of paths to dependency files (.d) which will help
# Make determine whether any header files changed
CXX_DEPENDENCIES := $(CXX_OBJECTS:%.o=%.d)


#---------------------------------------------------------------------------------------------------
#                                              RULES
#---------------------------------------------------------------------------------------------------

# Rule to build all targets defined by this Makefile
all: ./build/$(TARGET) ./lib/lib$(TARGET).a


# Define how we build the code into an executable
./build/$(TARGET): $(CXX_OBJECTS)
	mkdir --parents $(@D)
	$(CXX_COMPILER) $(CXX_LINKER_FLAGS) -o ./build/$(TARGET) $(CXX_OBJECTS)


# Define how we build the code into a static library (libTARGET.a)
./lib/lib$(TARGET).a: $(CXX_OBJECTS_NO_MAIN)
	mkdir --parents $(@D)
	ar crs ./lib/lib$(TARGET).a $(CXX_OBJECTS_NO_MAIN)


# Define how we compile .cpp files into .o files
./build/%.o: ./src/%.cpp
	mkdir --parents `dirname $@`
	$(CXX_COMPILER) $(CXX_COMPILER_FLAGS) -c $< -o $@


# `make clean` clears out the `build` directory
clean:
	rm -rf ./build/*


# Track dependency (.d) files, which will catch if a header file changes
-include $(CXX_DEPENDENCIES)
