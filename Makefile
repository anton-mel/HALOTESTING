# =============================================================================
# Neural Data Pipeline - Build Configuration
# =============================================================================

# Compiler Configuration
CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -mmacosx-version-min=14.0
INCLUDES = -I. -Iintan-reader -Idata-analyser \
           -Iintan-reader/Engine/API/Abstract \
           -Iintan-reader/Engine/API/Hardware \
           -Iintan-reader/includes

# =============================================================================
# TARGETS CONFIGURATION
# =============================================================================

# Main Pipeline (Intan Reader + ASIC Sender + Data Logger)
MAIN_TARGET = run_pipeline
MAIN_SOURCES = main.cpp data-analyser/data_logger.cpp data-analyser/fpga_raw_logger.cpp
MAIN_OBJECTS = $(MAIN_SOURCES:.cpp=.o)

# Intan RHX Device Reader (Standalone Neural Data Acquisition)
READER_TARGET = intan-reader/intan_reader
USB3_SOURCES = intan-reader/intan_reader.cpp \
               intan-reader/shared_memory_writer.cpp \
               intan-reader/includes/rhd2000evalboardusb3.cpp \
               intan-reader/includes/rhd2000datablockusb3.cpp \
               intan-reader/includes/rhd2000registersusb3.cpp \
               intan-reader/includes/okFrontPanelDLL.cpp
USB3_OBJECTS = $(USB3_SOURCES:.cpp=.o)

# ASIC FPGA Interface (Seizure Detection FPGA Communication)
ASIC_TARGET = asic/fpga_interface
ASIC_SOURCES = asic/main.cpp asic/fpga_interface.cpp
ASIC_OBJECTS = $(ASIC_SOURCES:.cpp=.o)
ASIC_LDFLAGS = -Lasic -lokFrontPanel -Wl,-rpath,@loader_path/asic

# ASIC Sender (Waveform Data Transmission to Seizure Detection FPGA)
ASIC_SENDER_TARGET = asic-sender/asic_sender
ASIC_SENDER_SOURCES = asic-sender/asic_sender.cpp
ASIC_SENDER_OBJECTS = $(ASIC_SENDER_SOURCES:.cpp=.o)
ASIC_SENDER_LDFLAGS = -Lasic-sender -lokFrontPanel -Wl,-rpath,@loader_path/asic-sender

# Data Analyser (Seizure Detection Logging and Analysis)
DATA_ANALYSER_TARGET = data-analyser/data_logger
DATA_ANALYSER_SOURCES = data-analyser/data_logger.cpp
DATA_ANALYSER_OBJECTS = $(DATA_ANALYSER_SOURCES:.cpp=.o)

# =============================================================================
# PHONY TARGETS
# =============================================================================
.PHONY: all clean run run_main run_reader run_asic run_asic_sender run_data_analyser \
        reader asic asic_sender data_analyser help modified_intan_rhx run_modified_intan_rhx run_pipeline_and_intan

# =============================================================================
# BUILD TARGETS
# =============================================================================

# Default target
all: $(MAIN_TARGET) modified_intan_rhx

# Main Pipeline Executable (Intan Reader + ASIC Sender + Data Logger)
$(MAIN_TARGET): $(MAIN_OBJECTS) $(USB3_OBJECTS) $(ASIC_SENDER_OBJECTS)
	@echo "Building main pipeline..."
	$(CXX) $(MAIN_OBJECTS) $(USB3_OBJECTS) $(ASIC_SENDER_OBJECTS) -o $(MAIN_TARGET) \
		-Lintan-reader -Lasic-sender -lokFrontPanel \
		-Wl,-rpath,@loader_path/intan-reader -Wl,-rpath,@loader_path/asic-sender
	@echo "Main pipeline built: $(MAIN_TARGET)"

# Intan Reader (Standalone Neural Data Acquisition)
reader: $(READER_TARGET)
$(READER_TARGET):
	@echo "Building Intan reader..."
	cd intan-reader && $(MAKE)
	@echo "Intan reader built: $(READER_TARGET)"

# ASIC FPGA Interface (Seizure Detection FPGA Communication)
asic: $(ASIC_TARGET)
$(ASIC_TARGET): $(ASIC_OBJECTS)
	@echo "Building ASIC FPGA interface..."
	$(CXX) $(ASIC_OBJECTS) -o $(ASIC_TARGET) $(ASIC_LDFLAGS)
	@echo "ASIC interface built: $(ASIC_TARGET)"

# ASIC Sender (Waveform Data Transmission)
asic_sender: $(ASIC_SENDER_TARGET)
$(ASIC_SENDER_TARGET): $(ASIC_SENDER_OBJECTS)
	@echo "Building ASIC sender..."
	$(CXX) $(ASIC_SENDER_OBJECTS) -o $(ASIC_SENDER_TARGET) $(ASIC_SENDER_LDFLAGS)
	@echo "ASIC sender built: $(ASIC_SENDER_TARGET)"

# Data Analyser (Seizure Detection Logging and Analysis)
data_analyser: $(DATA_ANALYSER_TARGET)
$(DATA_ANALYSER_TARGET): $(DATA_ANALYSER_OBJECTS)
	@echo "Building data analyser..."
	$(CXX) $(DATA_ANALYSER_OBJECTS) -o $(DATA_ANALYSER_TARGET)
	@echo "Data analyser built: $(DATA_ANALYSER_TARGET)"

# Modified Intan RHX Pipeline
modified_intan_rhx:
	@echo "Building modified Intan RHX pipeline..."
	cd modified-intan-rhx && qmake IntanRHX.pro
	cd modified-intan-rhx && $(MAKE)
	@echo "Modified Intan RHX pipeline built"

# =============================================================================
# COMPILATION RULES
# =============================================================================

# Generic compilation rule for all .cpp files
%.o: %.cpp
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# =============================================================================
# CLEANUP TARGETS
# =============================================================================

# Clean all build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(MAIN_OBJECTS) $(MAIN_TARGET)
	rm -f $(USB3_OBJECTS)
	rm -f $(ASIC_OBJECTS) $(ASIC_TARGET)
	rm -f $(ASIC_SENDER_OBJECTS) $(ASIC_SENDER_TARGET)
	rm -f $(DATA_ANALYSER_OBJECTS) $(DATA_ANALYSER_TARGET)
	cd intan-reader && $(MAKE) clean
	@echo "Cleaning modified-intan-rhx Qt project..."
	cd modified-intan-rhx && $(MAKE) clean 2>/dev/null || true
	rm -f modified-intan-rhx/Makefile modified-intan-rhx/Makefile.Debug modified-intan-rhx/Makefile.Release
	@echo "Cleanup complete"

# =============================================================================
# RUN TARGETS
# =============================================================================

# Default run target
run: all run_pipeline_and_intan

# Run main pipeline (Intan Reader + ASIC Sender + Data Logger)
run_main: $(MAIN_TARGET)
	@echo "Starting main pipeline..."
	./$(MAIN_TARGET)

# Run standalone Intan reader
run_reader: $(READER_TARGET)
	@echo "Starting Intan reader..."
	cd intan-reader && ./intan_reader

# Run ASIC FPGA interface
run_asic: $(ASIC_TARGET)
	@echo "Starting ASIC FPGA interface..."
	cd asic && ./fpga_interface 2437001CWG First.bit

# Run ASIC sender
run_asic_sender: $(ASIC_SENDER_TARGET)
	@echo "Starting ASIC sender..."
	cd asic-sender && ./asic_sender 2437001CWG First.bit

# Run data analyser summary
run_data_analyser: $(DATA_ANALYSER_TARGET)
	@echo "Running data analyser summary..."
	cd data-analyser && python3 scripts/summarize_detections.py

# Run modified Intan RHX pipeline
run_modified_intan_rhx: modified_intan_rhx
	@echo "Starting modified Intan RHX pipeline..."
	cd modified-intan-rhx && ./IntanRHX

# Run pipeline and launch Intan app
run_pipeline_and_intan:
	@echo "Starting main pipeline in background..."
	./$(MAIN_TARGET) &

# =============================================================================
# HELP TARGET
# =============================================================================

help:
	@echo "Decoupled Neural Data Pipeline - Build System"
	@echo "=============================================="
	@echo ""
	@echo "Available Targets:"
	@echo "  all              - Build main pipeline (default)"
	@echo "  reader           - Build standalone Intan reader"
	@echo "  asic             - Build ASIC FPGA interface"
	@echo "  asic_sender      - Build ASIC sender"
	@echo "  data_analyser    - Build data analyser"
	@echo "  modified_intan_rhx - Build modified Intan RHX pipeline"
	@echo ""
	@echo "Run Targets:"
	@echo "  run              - Build everything, run pipeline, then launch Intan app"
	@echo "  run_main         - Run main pipeline only"
	@echo "  run_reader       - Run standalone Intan reader"
	@echo "  run_asic         - Run ASIC FPGA interface"
	@echo "  run_asic_sender  - Run ASIC sender"
	@echo "  run_data_analyser- Run data analyser summary"
	@echo "  run_modified_intan_rhx - Run modified Intan RHX pipeline"
	@echo ""
	@echo "Maintenance:"
	@echo "  clean            - Clean all build artifacts"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Usage Examples:"
	@echo "  make all                    # Build everything"
	@echo "  make run                    # Build everything, run pipeline, launch Intan app"
	@echo "  make run_main               # Run main pipeline only"
	@echo "  make reader && make run_reader  # Build and run Intan reader"
	@echo "  make clean                  # Clean build artifacts"

