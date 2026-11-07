#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <conio.h>
#include <string.h>
#include <stdio.h>
#include <thread>
#include <mutex>
#include <chrono>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "2540"

// Definição da estrutura para os dados de processo (que serão compartilhados) e set-points
struct DadosProcesso {
    double temp_camara = 0.0;
    double pressao_camara = 0.0;
    double pressao_argonio = 0.0;
    double temp_panela = 0.0;

    int sp_pressao_camara = 0;
    double sp_temp_camara = 0.0;
    double sp_pressao_argonio = 0.0;
};

// Instância global dos dados e o mutex de proteção
DadosProcesso dadosGlobais;
std::mutex mtx_dados;

// Thread Cliente Sockets
void threadClienteSockets(const char* ip_servidor) {

    // Implementação sockets TCP/IP

}

// Thread Cliente OPC
void threadClienteOPC() {

    // Implementação OPC

}

int main(int argc, char* argv[]) {
    // Validação do argumento de IP do servidor TCP
    if (argc != 2) {
        std::cout << "Uso correto: " << argv[0] << " <ip-do-servidor-tcp>\n";
        return 1;
    }

    // Criação e disparo concorrente das duas threads
    std::thread tSockets(threadClienteSockets, argv[1]);
    std::thread tOPC(threadClienteOPC);

    tSockets.join();
    tOPC.join();

    return 0;
}
