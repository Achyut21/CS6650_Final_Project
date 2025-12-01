CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -g
LDFLAGS = -pthread

# Source files
SOURCES = messages.cpp task_manager.cpp state_machine.cpp Socket.cpp ClientStub.cpp ServerStub.cpp replication.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# Test files
TEST_SOURCES = task_test.cpp
TEST_OBJECTS = $(TEST_SOURCES:.cpp=.o)
TEST_EXEC = run_tests

# State machine test files
SM_TEST_SOURCES = state_machine_test.cpp
SM_TEST_OBJECTS = $(SM_TEST_SOURCES:.cpp=.o)
SM_TEST_EXEC = run_sm_tests

# Marshalling test files
MARSHAL_TEST_SOURCES = marshalling_test.cpp
MARSHAL_TEST_OBJECTS = $(MARSHAL_TEST_SOURCES:.cpp=.o)
MARSHAL_TEST_EXEC = run_marshal_tests

# Conflict test files
CONFLICT_TEST_SOURCES = conflict_test.cpp
CONFLICT_TEST_OBJECTS = $(CONFLICT_TEST_SOURCES:.cpp=.o)
CONFLICT_TEST_EXEC = run_conflict_tests

# Network test files
NETWORK_TEST_SOURCES = network_test.cpp
NETWORK_TEST_OBJECTS = $(NETWORK_TEST_SOURCES:.cpp=.o)
NETWORK_TEST_EXEC = run_network_tests

# Master server
MASTER_SOURCES = master.cpp
MASTER_OBJECTS = $(MASTER_SOURCES:.cpp=.o)
MASTER_EXEC = master

# Backup server
BACKUP_SOURCES = backup.cpp
BACKUP_OBJECTS = $(BACKUP_SOURCES:.cpp=.o)
BACKUP_EXEC = backup

# Test client
TEST_CLIENT_SOURCES = test_client.cpp
TEST_CLIENT_OBJECTS = $(TEST_CLIENT_SOURCES:.cpp=.o)
TEST_CLIENT_EXEC = test_client

.PHONY: all clean test test_sm test_marshal test_conflict test_network test_all build_master build_backup build_test_client

# Default target: build all
all: $(TEST_EXEC) $(SM_TEST_EXEC) $(MARSHAL_TEST_EXEC) $(CONFLICT_TEST_EXEC) $(NETWORK_TEST_EXEC) $(MASTER_EXEC) $(BACKUP_EXEC) $(TEST_CLIENT_EXEC)

# Build test executable
$(TEST_EXEC): $(OBJECTS) $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Build state machine test executable
$(SM_TEST_EXEC): $(OBJECTS) $(SM_TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Build marshalling test executable
$(MARSHAL_TEST_EXEC): $(OBJECTS) $(MARSHAL_TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Build conflict test executable
$(CONFLICT_TEST_EXEC): $(OBJECTS) $(CONFLICT_TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Build network test executable
$(NETWORK_TEST_EXEC): $(OBJECTS) $(NETWORK_TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Build master executable
$(MASTER_EXEC): $(OBJECTS) $(MASTER_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Build backup executable
$(BACKUP_EXEC): $(OBJECTS) $(BACKUP_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Build test client executable
$(TEST_CLIENT_EXEC): $(OBJECTS) $(TEST_CLIENT_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run task tests
test: $(TEST_EXEC)
	./$(TEST_EXEC)

# Run state machine tests
test_sm: $(SM_TEST_EXEC)
	./$(SM_TEST_EXEC)

# Run marshalling tests
test_marshal: $(MARSHAL_TEST_EXEC)
	./$(MARSHAL_TEST_EXEC)

# Run conflict tests
test_conflict: $(CONFLICT_TEST_EXEC)
	./$(CONFLICT_TEST_EXEC)

# Run network tests
test_network: $(NETWORK_TEST_EXEC)
	./$(NETWORK_TEST_EXEC)

# Run all tests
test_all: test test_sm test_marshal test_conflict test_network
	@echo ""
	@echo "=========================================="
	@echo "All test suites completed!"
	@echo "=========================================="

# Build master only
build_master: $(MASTER_EXEC)

# Build backup only
build_backup: $(BACKUP_EXEC)

# Build test client only
build_test_client: $(TEST_CLIENT_EXEC)

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TEST_OBJECTS) $(SM_TEST_OBJECTS) $(MARSHAL_TEST_OBJECTS) $(CONFLICT_TEST_OBJECTS) $(NETWORK_TEST_OBJECTS) $(MASTER_OBJECTS) $(BACKUP_OBJECTS) $(TEST_CLIENT_OBJECTS) $(TEST_EXEC) $(SM_TEST_EXEC) $(MARSHAL_TEST_EXEC) $(CONFLICT_TEST_EXEC) $(NETWORK_TEST_EXEC) $(MASTER_EXEC) $(BACKUP_EXEC) $(TEST_CLIENT_EXEC)

# Dependencies
messages.o: messages.cpp messages.h
task_manager.o: task_manager.cpp task_manager.h messages.h
state_machine.o: state_machine.cpp state_machine.h messages.h task_manager.h
task_test.o: task_test.cpp task_manager.h messages.h
state_machine_test.o: state_machine_test.cpp state_machine.h task_manager.h messages.h
marshalling_test.o: marshalling_test.cpp messages.h
conflict_test.o: conflict_test.cpp task_manager.h messages.h
network_test.o: network_test.cpp Socket.h ClientStub.h ServerStub.h messages.h
Socket.o: Socket.cpp Socket.h
ClientStub.o: ClientStub.cpp ClientStub.h Socket.h messages.h
ServerStub.o: ServerStub.cpp ServerStub.h Socket.h messages.h
replication.o: replication.cpp replication.h Socket.h ClientStub.h messages.h
master.o: master.cpp Socket.h ServerStub.h task_manager.h messages.h
backup.o: backup.cpp Socket.h ServerStub.h task_manager.h state_machine.h messages.h
test_client.o: test_client.cpp ClientStub.h Socket.h messages.h
