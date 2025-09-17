# Data Analyser Makefile

# Directories
SRC_DIR = src
GUI_DIR = $(SRC_DIR)/gui
BUILD_DIR = build
TARGET = $(BUILD_DIR)/seizure_analyzer.app/Contents/MacOS/seizure_analyzer

# Default target
all: $(TARGET)

# Build using Qt's qmake system
$(TARGET):
	cd $(GUI_DIR) && qmake && make
	mkdir -p $(BUILD_DIR)/seizure_analyzer.app/Contents/MacOS
	cp $(GUI_DIR)/seizure_analyzer.app/Contents/MacOS/seizure_analyzer $(TARGET)

# Clean build artifacts
clean:
	cd $(GUI_DIR) && make clean 2>/dev/null || true
	rm -rf $(BUILD_DIR)/obj
	rm -rf $(BUILD_DIR)/seizure_analyzer.app
	rm -f $(BUILD_DIR)/*.o

# Clean everything including logs
distclean: clean
	rm -rf $(BUILD_DIR)
	rm -rf logs/*

# Install (copy executable to a convenient location)
install: $(TARGET)
	cp $(TARGET) $(BUILD_DIR)/seizure_analyzer
	@echo "Executable installed to $(BUILD_DIR)/seizure_analyzer"

# Run the application
run: $(TARGET)
	$(TARGET)

# Debug build
debug:
	cd $(GUI_DIR) && qmake CONFIG+=debug && make
	mkdir -p $(BUILD_DIR)/seizure_analyzer.app/Contents/MacOS
	cp $(GUI_DIR)/seizure_analyzer.app/Contents/MacOS/seizure_analyzer $(TARGET)

# Help
help:
	@echo "Available targets:"
	@echo "  all      - Build the seizure analyzer application (default)"
	@echo "  clean    - Remove build artifacts"
	@echo "  distclean- Remove all build files and logs"
	@echo "  install  - Install executable to build directory"
	@echo "  run      - Build and run the application"
	@echo "  debug    - Build with debug symbols"
	@echo "  help     - Show this help message"

.PHONY: all clean distclean install run debug help