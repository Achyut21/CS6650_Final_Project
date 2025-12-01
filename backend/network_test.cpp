/**
 * Network Protocol Tests
 * Tests TCP communication, stub methods, and binary protocol
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>
#include "Socket.h"
#include "ClientStub.h"
#include "ServerStub.h"
#include "messages.h"

int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) void name()
#define RUN_TEST(test) \
    std::cout << "Running " << #test << "..." << std::flush; \
    try { test(); tests_passed++; std::cout << " PASSED\n"; } \
    catch (const std::exception& e) { tests_failed++; std::cout << " FAILED: " << e.what() << "\n"; } \
    catch (...) { tests_failed++; std::cout << " FAILED: Unknown error\n"; }

#define ASSERT_TRUE(x) if (!(x)) { throw std::runtime_error("Assertion failed: " #x); }
#define ASSERT_EQ(a, b) if ((a) != (b)) { throw std::runtime_error("Assertion failed: values not equal"); }

const int TEST_PORT_BASE = 13000;
std::atomic<int> port_counter(0);

int get_test_port() {
    return TEST_PORT_BASE + port_counter++;
}

/* ============ Socket Basic Tests ============ */

TEST(test_socket_bind_listen) {
    int port = get_test_port();
    Socket server;
    
    ASSERT_TRUE(server.Bind(port));
    ASSERT_TRUE(server.Listen());
    
    server.Close();
}

TEST(test_socket_connect_accept) {
    int port = get_test_port();
    std::atomic<bool> connected(false);
    Socket* accepted = nullptr;
    
    // Server thread
    std::thread server_thread([&]() {
        Socket server;
        server.Bind(port);
        server.Listen();
        accepted = server.Accept();
        if (accepted && accepted->IsValid()) {
            connected = true;
        }
        server.Close();
    });
    
    // Wait for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Client
    Socket client;
    ASSERT_TRUE(client.Connect("127.0.0.1", port));
    
    server_thread.join();
    
    ASSERT_TRUE(connected.load());
    
    client.Close();
    if (accepted) {
        accepted->Close();
        delete accepted;
    }
}

TEST(test_socket_send_receive_int) {
    int port = get_test_port();
    int received_value = 0;
    
    std::thread server_thread([&]() {
        Socket server;
        server.Bind(port);
        server.Listen();
        Socket* client = server.Accept();
        
        if (client) {
            int value;
            client->Receive(&value, sizeof(int));
            received_value = ntohl(value);
            client->Close();
            delete client;
        }
        server.Close();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    Socket client;
    client.Connect("127.0.0.1", port);
    
    int send_value = htonl(42);
    client.Send(&send_value, sizeof(int));
    
    client.Close();
    server_thread.join();
    
    ASSERT_EQ(received_value, 42);
}

TEST(test_socket_send_receive_string) {
    int port = get_test_port();
    std::string received_str;
    
    std::thread server_thread([&]() {
        Socket server;
        server.Bind(port);
        server.Listen();
        Socket* client = server.Accept();
        
        if (client) {
            int len;
            client->Receive(&len, sizeof(int));
            len = ntohl(len);
            
            char* buffer = new char[len + 1];
            client->Receive(buffer, len);
            buffer[len] = '\0';
            received_str = buffer;
            delete[] buffer;
            
            client->Close();
            delete client;
        }
        server.Close();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    Socket client;
    client.Connect("127.0.0.1", port);
    
    std::string test_str = "Hello, World!";
    int len = htonl(test_str.length());
    client.Send(&len, sizeof(int));
    client.Send(test_str.c_str(), test_str.length());
    
    client.Close();
    server_thread.join();
    
    ASSERT_EQ(received_str, "Hello, World!");
}

TEST(test_socket_large_transfer) {
    int port = get_test_port();
    std::string large_data(10000, 'X');  // 10KB of data
    std::string received_data;
    
    std::thread server_thread([&]() {
        Socket server;
        server.Bind(port);
        server.Listen();
        Socket* client = server.Accept();
        
        if (client) {
            int len;
            client->Receive(&len, sizeof(int));
            len = ntohl(len);
            
            char* buffer = new char[len];
            client->Receive(buffer, len);
            received_data.assign(buffer, len);
            delete[] buffer;
            
            client->Close();
            delete client;
        }
        server.Close();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    Socket client;
    client.Connect("127.0.0.1", port);
    
    int len = htonl(large_data.length());
    client.Send(&len, sizeof(int));
    client.Send(large_data.c_str(), large_data.length());
    
    client.Close();
    server_thread.join();
    
    ASSERT_EQ(received_data.length(), large_data.length());
    ASSERT_EQ(received_data, large_data);
}

/* ============ Stub Communication Tests ============ */

TEST(test_stub_send_receive_task) {
    int port = get_test_port();
    Task received_task;
    
    std::thread server_thread([&]() {
        Socket server;
        server.Bind(port);
        server.Listen();
        Socket* client_socket = server.Accept();
        
        if (client_socket) {
            ServerStub stub;
            stub.Init(client_socket);
            received_task = stub.ReceiveTask();
            stub.Close();
            delete client_socket;
        }
        server.Close();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ClientStub client;
    client.Init("127.0.0.1", port);
    
    Task task(5, "Test Title", "Test Description", "board-1", "user", Column::IN_PROGRESS, 100);
    client.SendTask(task);
    
    client.Close();
    server_thread.join();
    
    ASSERT_EQ(received_task.get_task_id(), 5);
    ASSERT_EQ(received_task.get_title(), "Test Title");
    ASSERT_EQ(received_task.get_description(), "Test Description");
    ASSERT_EQ(received_task.get_column(), Column::IN_PROGRESS);
    ASSERT_EQ(received_task.get_client_id(), 100);
}

TEST(test_stub_send_receive_optype) {
    int port = get_test_port();
    OpType received_op = OpType::GET_BOARD;
    
    std::thread server_thread([&]() {
        Socket server;
        server.Bind(port);
        server.Listen();
        Socket* client_socket = server.Accept();
        
        if (client_socket) {
            ServerStub stub;
            stub.Init(client_socket);
            received_op = stub.ReceiveOpType();
            stub.Close();
            delete client_socket;
        }
        server.Close();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ClientStub client;
    client.Init("127.0.0.1", port);
    client.SendOpType(OpType::CREATE_TASK);
    
    client.Close();
    server_thread.join();
    
    ASSERT_EQ(received_op, OpType::CREATE_TASK);
}

TEST(test_stub_send_receive_log_entry) {
    int port = get_test_port();
    LogEntry received_entry(-1, OpType::GET_BOARD, VectorClock(0), -1, "", "", "", Column::TODO, 0);
    
    std::thread server_thread([&]() {
        Socket server;
        server.Bind(port);
        server.Listen();
        Socket* client_socket = server.Accept();
        
        if (client_socket) {
            ServerStub stub;
            stub.Init(client_socket);
            received_entry = stub.ReceiveLogEntry();
            stub.Close();
            delete client_socket;
        }
        server.Close();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ClientStub client;
    client.Init("127.0.0.1", port);
    
    VectorClock vc(1);
    vc.increment();
    LogEntry entry(10, OpType::UPDATE_TASK, vc, 5, "Title", "Desc", "user", Column::DONE, 1);
    client.SendLogEntry(entry);
    
    client.Close();
    server_thread.join();
    
    ASSERT_EQ(received_entry.get_entry_id(), 10);
    ASSERT_EQ(received_entry.get_op_type(), OpType::UPDATE_TASK);
    ASSERT_EQ(received_entry.get_task_id(), 5);
    ASSERT_EQ(received_entry.get_column(), Column::DONE);
}

TEST(test_stub_send_receive_task_list) {
    int port = get_test_port();
    std::vector<Task> received_tasks;
    
    std::thread server_thread([&]() {
        Socket server;
        server.Bind(port);
        server.Listen();
        Socket* client_socket = server.Accept();
        
        if (client_socket) {
            // Receive count
            int net_count;
            client_socket->Receive(&net_count, sizeof(int));
            int count = ntohl(net_count);
            
            ServerStub stub;
            stub.Init(client_socket);
            
            for (int i = 0; i < count; i++) {
                Task task = stub.ReceiveTask();
                received_tasks.push_back(task);
            }
            
            stub.Close();
            delete client_socket;
        }
        server.Close();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Client sends task list
    Socket client;
    client.Connect("127.0.0.1", port);
    
    std::vector<Task> tasks;
    tasks.push_back(Task(1, "Task 1", "Desc 1", "board", "user", Column::TODO, 1));
    tasks.push_back(Task(2, "Task 2", "Desc 2", "board", "user", Column::IN_PROGRESS, 1));
    tasks.push_back(Task(3, "Task 3", "Desc 3", "board", "user", Column::DONE, 1));
    
    int net_count = htonl(tasks.size());
    client.Send(&net_count, sizeof(int));
    
    for (const Task& task : tasks) {
        int size = task.Size();
        char* buffer = new char[size];
        task.Marshal(buffer);
        
        int net_size = htonl(size);
        client.Send(&net_size, sizeof(int));
        client.Send(buffer, size);
        delete[] buffer;
    }
    
    client.Close();
    server_thread.join();
    
    ASSERT_EQ(received_tasks.size(), 3);
    ASSERT_EQ(received_tasks[0].get_task_id(), 1);
    ASSERT_EQ(received_tasks[1].get_task_id(), 2);
    ASSERT_EQ(received_tasks[2].get_task_id(), 3);
}

TEST(test_stub_success_response) {
    int port = get_test_port();
    bool received_success = false;
    
    std::thread server_thread([&]() {
        Socket server;
        server.Bind(port);
        server.Listen();
        Socket* client_socket = server.Accept();
        
        if (client_socket) {
            ServerStub stub;
            stub.Init(client_socket);
            stub.SendSuccess(true);
            stub.Close();
            delete client_socket;
        }
        server.Close();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ClientStub client;
    client.Init("127.0.0.1", port);
    received_success = client.ReceiveSuccess();
    
    client.Close();
    server_thread.join();
    
    ASSERT_TRUE(received_success);
}

TEST(test_stub_operation_response) {
    int port = get_test_port();
    int response_buffer[4];
    
    std::thread server_thread([&]() {
        Socket server;
        server.Bind(port);
        server.Listen();
        Socket* client_socket = server.Accept();
        
        if (client_socket) {
            ServerStub stub;
            stub.Init(client_socket);
            
            OperationResponse response;
            response.success = true;
            response.conflict = true;
            response.rejected = false;
            response.updated_task_id = 42;
            
            stub.SendOperationResponse(response);
            stub.Close();
            delete client_socket;
        }
        server.Close();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    Socket client;
    client.Connect("127.0.0.1", port);
    client.Receive(response_buffer, sizeof(response_buffer));
    
    client.Close();
    server_thread.join();
    
    ASSERT_EQ(ntohl(response_buffer[0]), 1);   // success
    ASSERT_EQ(ntohl(response_buffer[1]), 1);   // conflict
    ASSERT_EQ(ntohl(response_buffer[2]), 0);   // rejected
    ASSERT_EQ(ntohl(response_buffer[3]), 42);  // task_id
}

/* ============ Multiple Message Tests ============ */

TEST(test_multiple_operations_same_connection) {
    int port = get_test_port();
    std::vector<OpType> received_ops;
    
    std::thread server_thread([&]() {
        Socket server;
        server.Bind(port);
        server.Listen();
        Socket* client_socket = server.Accept();
        
        if (client_socket) {
            ServerStub stub;
            stub.Init(client_socket);
            
            for (int i = 0; i < 5; i++) {
                OpType op = stub.ReceiveOpType();
                received_ops.push_back(op);
            }
            
            stub.Close();
            delete client_socket;
        }
        server.Close();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ClientStub client;
    client.Init("127.0.0.1", port);
    
    client.SendOpType(OpType::CREATE_TASK);
    client.SendOpType(OpType::UPDATE_TASK);
    client.SendOpType(OpType::MOVE_TASK);
    client.SendOpType(OpType::DELETE_TASK);
    client.SendOpType(OpType::GET_BOARD);
    
    client.Close();
    server_thread.join();
    
    ASSERT_EQ(received_ops.size(), 5);
    ASSERT_EQ(received_ops[0], OpType::CREATE_TASK);
    ASSERT_EQ(received_ops[1], OpType::UPDATE_TASK);
    ASSERT_EQ(received_ops[2], OpType::MOVE_TASK);
    ASSERT_EQ(received_ops[3], OpType::DELETE_TASK);
    ASSERT_EQ(received_ops[4], OpType::GET_BOARD);
}

TEST(test_heartbeat_protocol) {
    int port = get_test_port();
    bool heartbeat_received = false;
    bool ack_received = false;
    
    std::thread server_thread([&]() {
        Socket server;
        server.Bind(port);
        server.Listen();
        Socket* client_socket = server.Accept();
        
        if (client_socket) {
            ServerStub stub;
            stub.Init(client_socket);
            
            OpType op = stub.ReceiveOpType();
            heartbeat_received = (op == OpType::HEARTBEAT_PING);
            
            stub.SendSuccess(true);  // Send ack
            stub.Close();
            delete client_socket;
        }
        server.Close();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ClientStub client;
    client.Init("127.0.0.1", port);
    
    client.SendHeartbeat();
    ack_received = client.ReceiveHeartbeatAck();
    
    client.Close();
    server_thread.join();
    
    ASSERT_TRUE(heartbeat_received);
    ASSERT_TRUE(ack_received);
}

/* ============ Edge Cases ============ */

TEST(test_empty_task_fields) {
    int port = get_test_port();
    Task received_task;
    
    std::thread server_thread([&]() {
        Socket server;
        server.Bind(port);
        server.Listen();
        Socket* client_socket = server.Accept();
        
        if (client_socket) {
            ServerStub stub;
            stub.Init(client_socket);
            received_task = stub.ReceiveTask();
            stub.Close();
            delete client_socket;
        }
        server.Close();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ClientStub client;
    client.Init("127.0.0.1", port);
    
    Task task(0, "", "", "", "", Column::TODO, 0);
    client.SendTask(task);
    
    client.Close();
    server_thread.join();
    
    ASSERT_EQ(received_task.get_title(), "");
    ASSERT_EQ(received_task.get_description(), "");
}

TEST(test_unicode_in_task) {
    int port = get_test_port();
    Task received_task;
    
    std::thread server_thread([&]() {
        Socket server;
        server.Bind(port);
        server.Listen();
        Socket* client_socket = server.Accept();
        
        if (client_socket) {
            ServerStub stub;
            stub.Init(client_socket);
            received_task = stub.ReceiveTask();
            stub.Close();
            delete client_socket;
        }
        server.Close();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ClientStub client;
    client.Init("127.0.0.1", port);
    
    Task task(1, "日本語タイトル", "中文描述", "board", "пользователь", Column::TODO, 1);
    client.SendTask(task);
    
    client.Close();
    server_thread.join();
    
    ASSERT_EQ(received_task.get_title(), "日本語タイトル");
    ASSERT_EQ(received_task.get_description(), "中文描述");
}

/* ============ Main ============ */

int main() {
    std::cout << "==========================================\n";
    std::cout << "Running Network Protocol Tests\n";
    std::cout << "==========================================\n\n";
    
    std::cout << "--- Socket Basic Tests ---\n";
    RUN_TEST(test_socket_bind_listen);
    RUN_TEST(test_socket_connect_accept);
    RUN_TEST(test_socket_send_receive_int);
    RUN_TEST(test_socket_send_receive_string);
    RUN_TEST(test_socket_large_transfer);
    
    std::cout << "\n--- Stub Communication Tests ---\n";
    RUN_TEST(test_stub_send_receive_task);
    RUN_TEST(test_stub_send_receive_optype);
    RUN_TEST(test_stub_send_receive_log_entry);
    RUN_TEST(test_stub_send_receive_task_list);
    RUN_TEST(test_stub_success_response);
    RUN_TEST(test_stub_operation_response);
    
    std::cout << "\n--- Multiple Message Tests ---\n";
    RUN_TEST(test_multiple_operations_same_connection);
    RUN_TEST(test_heartbeat_protocol);
    
    std::cout << "\n--- Edge Case Tests ---\n";
    RUN_TEST(test_empty_task_fields);
    RUN_TEST(test_unicode_in_task);
    
    std::cout << "\n==========================================\n";
    std::cout << "Results: " << tests_passed << " passed, " << tests_failed << " failed\n";
    std::cout << "==========================================\n";
    
    return tests_failed > 0 ? 1 : 0;
}
