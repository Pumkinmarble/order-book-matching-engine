CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -pthread

# Target executable
TARGET = order_book

# Source files
SRCS = main.cpp order_book.cpp

# Default target
all: $(TARGET)

# Build executable
$(TARGET): $(SRCS) order_book.h
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

# Run the demo and benchmarks
run: $(TARGET)
	./$(TARGET)

# Clean build artifacts
clean:
	rm -f $(TARGET)

# Phony targets
.PHONY: all run clean
