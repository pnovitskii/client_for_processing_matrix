﻿#include <iostream>
#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <exception>
#include <vector>
#include <type_traits>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <chrono>
#include <thread>
#include <mutex>
#include <sstream>

template <typename T>
using Matrix = std::vector<std::vector<T>>;

#pragma warning(disable: 4996)

std::mutex output_mutex;

template <typename T>
std::string getStringOfMatrix(Matrix<T> matrix) {
    std::ostringstream oss;
    if (matrix.size() == 0) {
        std::cout << "Matrix empty." << std::endl;
        return "";
    }
    for (auto x : matrix) {
        for (auto y : x) {
            oss << std::setw(5) << y << " ";
        }
        oss << "\n";
    }
    oss << "\n";
    return oss.str();
    //std::cout << oss.str();
}

template <typename T>
Matrix<T> generateRandomMatrix(size_t size) {
    
    Matrix<T> matrix(size, std::vector<T>(size));
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            matrix[i][j] = (rand() % 200 - 100) + (rand() % 10) / 10.f;
            //oss << std::setw(5) << matrix[i][j] << " ";
        }  
        //oss << "\n";
    }
    //oss << "\n";
    //std::cout << oss.str();
    std::ostringstream oss;
    oss << "Initial matrix:\n" << getStringOfMatrix(matrix);
    std::cout << oss.str();
    return matrix;
}



class Client {
private:
    WSAData wsaData;
    WORD DLLversion = MAKEWORD(2, 1);
    bool connected = false;
public:
    Client() {
        if (WSAStartup(DLLversion, &wsaData) != 0) {
            std::cout << "Error" << std::endl;
            exit(1);
        }

        Connection = socket(AF_INET, SOCK_STREAM, 0); // Инициализация сокета

        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port = htons(1111);
        addr.sin_family = AF_INET;

        this->connect_to_server();
    }
    ~Client() {
        closesocket(Connection);
        WSACleanup();
        std::cout << "Disconnected." << std::endl;
    }
private:
    SOCKADDR_IN addr;
    SOCKET Connection;
public:
    void connect_to_server() {
        //if (connected) { return; };
        if (connect(Connection, (SOCKADDR*)&addr, sizeof(addr)) != 0) {
            std::cout << "Error: failed to connect to server." << WSAGetLastError() << std::endl;
            throw std::exception();
            //return -1;
        }
        std::cout << "Connected\n\n";
        this->connected = true;
    }
    template <typename T, typename = std::enable_if_t<std::is_fundamental_v<T>>>
    void receive(std::vector<T>& data) {
        recv(Connection, (char*)data.data(), sizeof(T) * data.size(), 0);
    }

    template<typename T>
    void receive_matrix(Matrix<T>& data, int size) {
        for (int i = 0; i < size; i++) {
            recv(Connection, (char*)data[i].data(), sizeof(T) * size, 0);
        }
    }

    template <typename T>//, typename = enable_if_t<is_fundamental_v<T>>>
    void send_data(Matrix<T> data) {
        //data.insert(data.begin(), data.size());
        for (int i = 0; i < data.size(); i++) {
            send(Connection, (char*)data[i].data(), sizeof(T) * data.size(), 0);
        }

    }

    void send_configuration(size_t rows, size_t cols, size_t threads) {
        std::vector<size_t> a = { rows, cols, threads };
        send(Connection, reinterpret_cast<const char*>(a.data()), sizeof(size_t) * a.size(), 0);
    }

    void send_command(std::string command) {
        send(Connection, (char*)command.data(), sizeof(command), 0);
        //std::function<void()> dummy = []() {};
        //return this->receive_command(Connection, std::string("1"));
    }


    bool receive_command(std::string command) {
        std::vector<char> buffer(1024);
        recv(Connection, (char*)buffer.data(), sizeof(char) * buffer.size(), 0);
        std::string received_command(std::move(buffer.data()));
        //std::cout << "Command is " << received_command << std::endl;
        if (received_command.find(command) != std::string::npos) {
            return true;
        }
        return false;
    }

    bool ping_pong() {
        //u_long mode = 1; // для Windows
        //ioctlsocket(Connection, FIONBIO, &mode);
        bool pong = false;
        //while (!pong) {
            this->send_command("PING");
            std::cout << "PING sent. Waiting for PONG... \n";
            if (!this->receive_command("PONG")) {
                std::cout << "No PONG received." << std::endl;
                return false;
            }
            else {
                std::cout << "PONG received." << std::endl;
                //mode = 0;
                //ioctlsocket(Connection, FIONBIO, &mode);
                return true;
            }   
        //}
    }

    template <typename T>
    Matrix<T> process_matrix_on_server(Matrix<T> matrix, size_t threads) {
        //this->connect_to_server();
        if (!this->ping_pong()) {
            return Matrix<T>();
        }

        int rows = matrix.size();
        int cols = matrix[0].size();

        this->send_configuration(rows, cols, threads);
        std::cout << "Configuration sent. ";
        this->send_data(matrix);
        std::cout << "Data sent. ";
        this->send_command("START");
        std::cout << "START command sent. ";
        bool done = false;
        u_long mode = 1; // для Windows
        std::cout << "Check status.\n";
        this->send_command("STATUS");

        ioctlsocket(Connection, FIONBIO, &mode);
        while (!done) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "Let's check status.\n";
            if (this->receive_command("1")) {
                std::cout << "Done\n";
                done = true;
                break;
            }
            else {
                std::cout << "Not yet\n";
            }
            this->send_command("STATUS");
        }
        mode = 0;
        ioctlsocket(Connection, FIONBIO, &mode);
        this->send_command("GET");
        this->receive_matrix(matrix, rows);
        std::cout << "Data received.\n\n";

        return matrix;
    }
};

int main()
{
    srand(time(0));
    
    Client client;

    std::thread x([]() {
        Client c;
        std::cout << getStringOfMatrix(c.process_matrix_on_server(generateRandomMatrix<int>(4), 2));
        std::cout << getStringOfMatrix(c.process_matrix_on_server(generateRandomMatrix<int>(4), 2));
        });
    x.detach();
    

    std::cout << getStringOfMatrix(client.process_matrix_on_server(generateRandomMatrix<int>(5), 2));
    //client.connect_to_server();
    std::cout << getStringOfMatrix(client.process_matrix_on_server(generateRandomMatrix<int>(5), 2));
    std::cout << getStringOfMatrix(client.process_matrix_on_server(generateRandomMatrix<int>(5), 2));
    /*while (true) {
        std::chrono::seconds duration(rand() % 6);
        this_thread::sleep_for(duration);
        client.server_request();
    }});*/

    //while (true) {

        /*for (auto x : clients) {
            x.server_request();

        }*/
        //}

    return 0;
}