# =============================================================================
# Neural Data Pipeline - Build Configuration
# =============================================================================

# Compiler Configuration
# Platform detection and compiler/tool defaults
ifeq ($(OS),Windows_NT)
  EXEEXT := .exe
  # Prefer clang++ from LLVM; fall back to g++ from MinGW if needed
  CXX ?= clang++
  # Remove macOS-specific flags; use standard warnings and optimizations
  CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
  # Utilities: use cmd's del for Windows shells
  RM := cmd /C del /Q /F
  PY := python
  # No rpath on Windows; DLLs must be discoverable via PATH next to exe
  RPATH_INTAN :=
  RPATH_ASIC_SENDER :=
  # Use dynamic loader shim; do not link import lib
  OKFRONTPANEL_LIB :=
else
  EXEEXT :=
  CXX = clang++
  CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -mmacosx-version-min=14.0
  RM := rm -f
  PY := python3
  RPATH_INTAN := -Wl,-rpath,@loader_path/intan-reader
  RPATH_ASIC_SENDER := -Wl,-rpath,@loader_path/asic-sender
  OKFRONTPANEL_LIB := -lokFrontPanel
  NULLDEV := /dev/null
endif
INCLUDES = -I. -Iintan-reader -Idata-analyser \
           -Iintan-reader/Engine/API/Abstract \
           -Iintan-reader/Engine/API/Hardware \
           -Iintan-reader/includes

# =============================================================================
# TARGETS CONFIGURATION
# =============================================================================

# Main Pipeline (Intan Reader + ASIC Sender + Data Logger)
MAIN_TARGET = run_pipeline$(EXEEXT)
MAIN_SOURCES = main.cpp data-analyser/fpga_logger.cpp data-analyser/halo_response_decoder.cpp intan-reader/shared_memory_reader.cpp
MAIN_OBJECTS = $(MAIN_SOURCES:.cpp=.o)

# Intan RHX Device Reader (Standalone Neural Data Acquisition)
READER_TARGET = intan-reader/intan_reader$(EXEEXT)
USB3_SOURCES = intan-reader/intan_reader.cpp \
               intan-reader/shared_memory_writer.cpp \
               intan-reader/includes/rhd2000evalboardusb3.cpp \
               intan-reader/includes/rhd2000datablockusb3.cpp \
               intan-reader/includes/rhd2000registersusb3.cpp \
               intan-reader/includes/okFrontPanelDLL.cpp
# Keep okFrontPanelDLL.cpp to dynamically load the DLL on Windows
USB3_OBJECTS = $(USB3_SOURCES:.cpp=.o)

# ASIC FPGA Interface (Seizure Detection FPGA Communication)
ASIC_TARGET = asic/fpga_interface$(EXEEXT)
ASIC_SOURCES = asic/main.cpp asic/fpga_interface.cpp
ASIC_OBJECTS = $(ASIC_SOURCES:.cpp=.o)
ASIC_LDFLAGS = -Lasic $(OKFRONTPANEL_LIB) -Wl,-rpath,@loader_path/asic

# ASIC Sender (Waveform Data Transmission to Seizure Detection FPGA)
ASIC_SENDER_TARGET = asic-sender/asic_sender$(EXEEXT)
ASIC_SENDER_SOURCES = asic-sender/asic_sender.cpp
ASIC_SENDER_OBJECTS = $(ASIC_SENDER_SOURCES:.cpp=.o)
ASIC_SENDER_LDFLAGS = -Lasic-sender $(OKFRONTPANEL_LIB) -Wl,-rpath,@loader_path/asic-sender

# Data Analyser
DATA_ANALYSER_TARGET = data-analyser/fpga_logger$(EXEEXT)
DATA_ANALYSER_SOURCES = data-analyser/fpga_logger.cpp data-analyser/halo_response_decoder.cpp
DATA_ANALYSER_OBJECTS = $(DATA_ANALYSER_SOURCES:.cpp=.o)

# Modified Intan RHX application output (Qt project)
INTAN_DIR := modified-intan-rhx
APP_TARGET := $(INTAN_DIR)/IntanRHX$(EXEEXT)
APP_TARGET_RELEASE := $(INTAN_DIR)/release/IntanRHX$(EXEEXT)
QMAKE ?= qmake

# =============================================================================
# PHONY TARGETS
# =============================================================================
.PHONY: all app clean clean-app clean-all run run-all run_main run_reader run_asic run_asic_sender run_data_analyser \
        reader asic asic_sender data_analyser help modified_intan_rhx run_modified_intan_rhx run_pipeline_and_intan

# =============================================================================
# BUILD TARGETS
# =============================================================================

# Default target - build pipeline and modified Intan RHX app
all: $(MAIN_TARGET) app

# Build the modified Intan RHX app separately
app: modified_intan_rhx

# Main Pipeline Executable (Intan Reader + ASIC Sender + Data Logger)
$(MAIN_TARGET): $(MAIN_OBJECTS) $(USB3_OBJECTS) $(ASIC_SENDER_OBJECTS)
	@echo "Building main pipeline..."
	$(CXX) $(MAIN_OBJECTS) $(USB3_OBJECTS) $(ASIC_SENDER_OBJECTS) -o $(MAIN_TARGET) \
		-Lintan-reader -Lasic-sender $(OKFRONTPANEL_LIB) \
		$(RPATH_INTAN) $(RPATH_ASIC_SENDER)
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

# Data Analyser
data_analyser: $(DATA_ANALYSER_TARGET)
$(DATA_ANALYSER_TARGET): $(DATA_ANALYSER_OBJECTS)
	@echo "Building data analyser..."
	$(CXX) $(DATA_ANALYSER_OBJECTS) -o $(DATA_ANALYSER_TARGET)
	@echo "Data analyser built: $(DATA_ANALYSER_TARGET)"

# Test decoder
test_decoder: data-analyser/tests/test_decoder
data-analyser/tests/test_decoder: data-analyser/tests/test_decoder.o data-analyser/halo_response_decoder.o
	@echo "Building HALO decoder test..."
	$(CXX) data-analyser/tests/test_decoder.o data-analyser/halo_response_decoder.o -o data-analyser/tests/test_decoder$(EXEEXT)
	@echo "HALO decoder test built: data-analyser/tests/test_decoder"

data-analyser/tests/test_decoder.o: data-analyser/tests/test_decoder.cpp data-analyser/halo_response_decoder.h
	$(CXX) $(CXXFLAGS) -c data-analyser/tests/test_decoder.cpp -o data-analyser/tests/test_decoder.o

# Modified Intan RHX Pipeline
modified_intan_rhx: $(APP_TARGET)

$(APP_TARGET): $(INTAN_DIR)/IntanRHX.pro
	@echo "Building modified Intan RHX pipeline..."
	cd $(INTAN_DIR) && $(QMAKE) -spec win32-g++ IntanRHX.pro || $(QMAKE) IntanRHX.pro
	cd $(INTAN_DIR) && $(MAKE)
	@if [ -f "$(APP_TARGET_RELEASE)" ]; then \
		cp "$(APP_TARGET_RELEASE)" "$(APP_TARGET)"; \
	fi
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

# Clean pipeline build artifacts only
clean:
	@echo "Cleaning pipeline build artifacts..."
	- $(RM) $(MAIN_OBJECTS) $(MAIN_TARGET) 2>nul
	- $(RM) $(USB3_OBJECTS) 2>nul
	- $(RM) $(ASIC_OBJECTS) $(ASIC_TARGET) 2>nul
	- $(RM) $(ASIC_SENDER_OBJECTS) $(ASIC_SENDER_TARGET) 2>nul
	- $(RM) $(DATA_ANALYSER_OBJECTS) $(DATA_ANALYSER_TARGET) 2>nul
	- $(RM) data-analyser\tests\test_decoder.o data-analyser\tests\test_decoder$(EXEEXT) 2>nul
	- $(RM) asic-sender\tests\test_xem7310.o asic-sender\tests\test_xem7310$(EXEEXT) 2>nul
	cd intan-reader && $(MAKE) clean
	@echo "Pipeline cleanup complete"

# Clean modified Intan RHX app build artifacts
clean-app:
	@echo "Cleaning modified Intan RHX app build artifacts..."
	cd $(INTAN_DIR) && $(MAKE) clean || true
	rm -f $(INTAN_DIR)/Makefile $(INTAN_DIR)/Makefile.Debug $(INTAN_DIR)/Makefile.Release
	rm -f $(APP_TARGET) $(APP_TARGET_RELEASE)
	@echo "App cleanup complete"

# Clean everything
clean-all: clean clean-app

# =============================================================================
# RUN TARGETS
# =============================================================================

# Default run target - just the pipeline
run: all

# Run both pipeline and app
run-all: all app run_pipeline_and_intan

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
	cd data-analyser && $(PY) scripts/summarize_detections.py

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
	@echo "  app              - Build modified Intan RHX app"
	@echo "  reader           - Build standalone Intan reader"
	@echo "  asic             - Build ASIC FPGA interface"
	@echo "  asic_sender      - Build ASIC sender"
	@echo "  data_analyser    - Build data analyser"
	@echo ""
	@echo "Run Targets:"
	@echo "  run              - Build and run main pipeline only"
	@echo "  run-all          - Build everything, run pipeline, then launch Intan app"
	@echo "  run_main         - Run main pipeline only"
	@echo "  run_reader       - Run standalone Intan reader"
	@echo "  run_asic         - Run ASIC FPGA interface"
	@echo "  run_asic_sender  - Run ASIC sender"
	@echo "  run_data_analyser- Run data analyser summary"
	@echo "  run_modified_intan_rhx - Run modified Intan RHX pipeline"
	@echo ""
	@echo "Maintenance:"
	@echo "  clean            - Clean pipeline build artifacts"
	@echo "  clean-app        - Clean modified Intan RHX app build artifacts"
	@echo "  clean-all        - Clean everything"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Usage Examples:"
	@echo "  make all                    # Build pipeline only"
	@echo "  make app                    # Build modified Intan RHX app"
	@echo "  make run                    # Build and run pipeline only"
	@echo "  make run-all                 # Build everything, run pipeline, launch Intan app"
	@echo "  make run_main               # Run main pipeline only"
	@echo "  make reader && make run_reader  # Build and run Intan reader"
	@echo "  make clean                  # Clean pipeline build artifacts"
	@echo "  make clean-app               # Clean app build artifacts"
	@echo "  make clean-all               # Clean everything"

