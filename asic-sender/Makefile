# ASIC Sender Makefile
# For sending waveform data to seizure detection FPGA

CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -mmacosx-version-min=14.0 -I.

# Target executable
TARGET = asic_sender

# Source files (using official FrontPanel SDK - no okFrontPanelDLL.cpp needed)
SOURCES = asic_sender.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# Library path and linking
LDFLAGS = -L. -lokFrontPanel -Wl,-rpath,@loader_path

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

run: $(TARGET)
	./$(TARGET) 2437001CWG First.bit