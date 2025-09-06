# Intan RHX Device Reader Makefile
# For neural data acquisition from Intan devices

CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -mmacosx-version-min=14.0
INCLUDES = -I. -I../logs -IEngine/API/Abstract -IEngine/API/Hardware -Iincludes

# Target executable
TARGET = intan_reader

# Source files
SOURCES = main.cpp intan_reader.cpp shared_memory_writer.cpp \
          Engine/API/Abstract/abstractrhxcontroller.cpp \
          Engine/API/Hardware/rhxcontroller.cpp \
          Engine/API/Hardware/rhxdatablock.cpp \
          Engine/API/Hardware/rhxregisters.cpp \
          includes/rhd2000evalboardusb3.cpp \
          includes/rhd2000datablockusb3.cpp \
          includes/rhd2000registersusb3.cpp \
          includes/okFrontPanelDLL.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# Library path and linking
LDFLAGS = -L. -lokFrontPanel -Wl,-rpath,@loader_path

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

