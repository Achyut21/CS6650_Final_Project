CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -g
LDFLAGS = -pthread

# Source files
SOURCES = messages.cpp task_manager.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# Test files
TEST_SOURCES = task_test.cpp
TEST_OBJECTS = $(TEST_SOURCES:.cpp=.o)
TEST_EXEC = run_tests

# Main executable (if you have a main later)
MAIN_EXEC = task_app

.PHONY: all clean test

# Default target: build tests
all: $(TEST_EXEC)

# Build test executable
$(TEST_EXEC): $(OBJECTS) $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run tests
test: $(TEST_EXEC)
	./$(TEST_EXEC)

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TEST_OBJECTS) $(TEST_EXEC) $(MAIN_EXEC)

# Dependencies
messages.o: messages.cpp messages.h
task_manager.o: task_manager.cpp task_manager.h messages.h
task_test.o: task_test.cpp task_manager.h messages.h