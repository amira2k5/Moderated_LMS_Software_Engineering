# ==============================================================================
# Makefile for Moderated LMS Project
# Institution: The National University of Lesotho
# Language/Standard: C++17
# Dependencies: libsodium, sqlite3
# ==============================================================================

# Compiler & Flags
CXX       := g++
CXXFLAGS  := -std=c++17 -Wall -Wextra -pedantic -O2
LDFLAGS   := -lsodium -lsqlite3

# Output Binary & Directories
TARGET    := lms
BUILD_DIR := build

# Source Files (mapping _2 variant files if used, or standard names)
SRCS      := common_2.cpp \
             domain.cpp \
             schema_2.cpp \
             security_2.cpp \
             data.cpp \
             services.cpp \
             ui.cpp \
             main_2.cpp

OBJS      := $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(SRCS))
DEPS      := $(OBJS:.o=.d)

# Default Rule
.PHONY: all clean run help

all: $(TARGET)

# Link Executable
$(TARGET): $(OBJS)
	@echo "Linking binary: $@"
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# Compile C++ Source Files to Object Files (with Dependency Tracking)
$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Create Build Directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Run the Compiled Binary
run: $(TARGET)
	./$(TARGET)

# Clean Up Generated Files
clean:
	@echo "Cleaning up build artifacts..."
	rm -rf $(BUILD_DIR) $(TARGET)

# Display Help
help:
	@echo "Available Makefile targets:"
	@echo "  make         - Build the LMS application executable"
	@echo "  make run     - Build and run the executable"
	@echo "  make clean   - Remove object files and executable"
	@echo "  make help    - Show this help message"

# Include Automatically Generated Dependency Files (.d)
-include $(DEPS)