# ==============================================================================
# Makefile for Moderated LMS Project
# Institution: The National University of Lesotho
# ==============================================================================

CXX       := g++
CXXFLAGS  := -std=c++17 -Wall -Wextra -pedantic -O2
LDFLAGS   := -lsodium -lsqlite3

TARGET    := lms
BUILD_DIR := build

# Source Files (including data.cpp)
SRCS      := common.cpp \
             domain.cpp \
             data.cpp \
             schema.cpp \
             security.cpp \
             services.cpp \
             ui.cpp \
             main.cpp

OBJS      := $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(SRCS))
DEPS      := $(OBJS:.o=.d)

.PHONY: all clean run help

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "Linking binary: $@"
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: $(TARGET)
	./$(TARGET)

clean:
	@echo "Cleaning up build artifacts..."
	rm -rf $(BUILD_DIR) $(TARGET)

help:
	@echo "Available Makefile targets:"
	@echo "  make         - Build the LMS application executable"
	@echo "  make run     - Build and run the executable"
	@echo "  make clean   - Remove object files and executable"

-include $(DEPS)
