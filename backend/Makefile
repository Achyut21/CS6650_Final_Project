CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -g
LDFLAGS = -pthread

# Source files
SOURCES = messages.cpp task_manager.cpp state_machine.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# Test files
TASK_TEST_SOURCES = task_test.cpp
TASK_TEST_OBJECTS = $(TASK_TEST_SOURCES:.cpp=.o)
TASK_TEST_EXEC = run_task_tests

STATE_TEST_SOURCES = state_machine_test.cpp
STATE_TEST_OBJECTS = $(STATE_TEST_SOURCES:.cpp=.o)
STATE_TEST_EXEC = run_state_tests

# Main executable (if you have a main later)
MAIN_EXEC = task_app

.PHONY: all clean test test_task test_state

# Default target: build all tests
all: $(TASK_TEST_EXEC) $(STATE_TEST_EXEC)

# Build task manager test executable
$(TASK_TEST_EXEC): $(OBJECTS) $(TASK_TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Build state machine test executable
$(STATE_TEST_EXEC): $(OBJECTS) $(STATE_TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run all tests
test: test_task test_state

# Run task manager tests only
test_task: $(TASK_TEST_EXEC)
	@echo "========================================="
	@echo "Running Task Manager Tests"
	@echo "========================================="
	./$(TASK_TEST_EXEC)

# Run state machine tests only
test_state: $(STATE_TEST_EXEC)
	@echo "========================================="
	@echo "Running State Machine Tests"
	@echo "========================================="
	./$(STATE_TEST_EXEC)

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TASK_TEST_OBJECTS) $(STATE_TEST_OBJECTS) $(TASK_TEST_EXEC) $(STATE_TEST_EXEC) $(MAIN_EXEC)

# Dependencies
messages.o: messages.cpp messages.h
task_manager.o: task_manager.cpp task_manager.h messages.h
state_machine.o: state_machine.cpp state_machine.h messages.h task_manager.h
task_test.o: task_test.cpp task_manager.h messages.h
state_machine_test.o: state_machine_test.cpp state_machine.h task_manager.h messages.h