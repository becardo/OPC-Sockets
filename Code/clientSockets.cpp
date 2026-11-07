#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <conio.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <mutex>
#include <chrono>
#include <functional>
#include <boost/asio.hpp>

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

// Instância global ou compartilhada dos dados e o Mutex correspondente
DadosProcesso dadosGlobais;
std::mutex mtx_dados;

int main(int argc, char** argv)
{
    // ------ Conexão TCP com o Servidor de Processo (Sockets) ------
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;

    struct addrinfo* result = NULL;
    struct addrinfo* ptr = NULL;
    struct addrinfo hints;

    if (argc != 2) {
        printf("Uso: %s <ip-do-servidor>\n", argv[0]);
        return 1;
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Falha na inicializacao do Winsock.\n");
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(argv[1], DEFAULT_PORT, &hints, &result) != 0) {
        printf("Falha no getaddrinfo para o IP: %s\n", argv[1]);
        WSACleanup();
        return 1;
    }

    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) continue;

        if (connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen) != SOCKET_ERROR) {
            break;
        }

        closesocket(ConnectSocket);
        ConnectSocket = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Nao foi possivel conectar ao servidor em %s na porta %s.\n", argv[1], DEFAULT_PORT);
        WSACleanup();
        return 1;
    }

    printf("Conexao TCP estabelecida com sucesso com o servidor %s na porta %s.\n", argv[1], DEFAULT_PORT);
    // ------ Fim do protocolo de conexão TCP com o Servidor de Processo (Sockets) -----

    long int numero_seq = 1;
    char recvbuf[512];
    int recvbuflen = sizeof(recvbuf);

    // Configuração do Boost.Asio io_context e steady_timer (substituindo o Sleep)
    boost::asio::io_context io_context;
    boost::asio::steady_timer timer_ciclo(io_context, boost::asio::chrono::milliseconds(2000));

    std::function<void(const boost::system::error_code&)> loop_envio;

    loop_envio = [&](const boost::system::error_code& erro) {
        if (erro) return;

        printf("\n--- Novo Ciclo de Envio (Seq atual: %ld) ---\n", numero_seq);

        std::cin.clear();

        double temp_camara_digitada = 0.0;
        double pressao_camara_digitada = 0.0;
        double pressao_argonio_digitada = 0.0;
        double temp_panela_digitada = 0.0;

        printf("Digite o valor da temperatura da camara a vacuo: ");
        std::cin >> temp_camara_digitada;

        printf("Digite o valor da pressao da camara a vacuo: ");
        std::cin >> pressao_camara_digitada;

        printf("Digite o valor da pressao do argonio: ");
        std::cin >> pressao_argonio_digitada;

        printf("Digite o valor da temperatura da panela: ");
        std::cin >> temp_panela_digitada;

        // Escreve o valor digitado na struct - seguro pelo mutex
        {
            std::lock_guard<std::mutex> lock(mtx_dados);
            dadosGlobais.temp_camara = temp_camara_digitada;
            dadosGlobais.pressao_camara = pressao_camara_digitada;
            dadosGlobais.pressao_argonio = pressao_argonio_digitada;
            dadosGlobais.temp_panela = temp_panela_digitada;
        }

        // Lê o valor da struct para montar a mensagem TCP 
        double enviar_temp_camara = 0.0;
        double enviar_pressao_camara = 0.0;
        double enviar_pressao_argonio = 0.0;
        double enviar_temp_panela = 0.0;

        {
            std::lock_guard<std::mutex> lock(mtx_dados);
            enviar_temp_camara = dadosGlobais.temp_camara;
            enviar_pressao_camara = dadosGlobais.pressao_camara;
            enviar_pressao_argonio = dadosGlobais.pressao_argonio;
            enviar_temp_panela = dadosGlobais.temp_panela;
        }

        char sendbuf[256];

        int envio_dados = snprintf(
            sendbuf,
            sizeof(sendbuf),
            "%06ld$255$%04.1f$%04.1f$%04.1f$%04.1f",
            numero_seq,
            enviar_temp_camara,
            enviar_pressao_camara,
            enviar_pressao_argonio,
            enviar_temp_panela
        );

        // Envia para o servidor
        int iSendResult = send(ConnectSocket, sendbuf, envio_dados, 0);
        if (iSendResult == SOCKET_ERROR) {
            printf("Erro no send: %d. Conexao perdida.\n", WSAGetLastError());
            return;
        }
        printf("[Enviado] -> %s\n", sendbuf);

        // Aguarda a resposta de ACK do servidor
        int iRecvResult = recv(ConnectSocket, recvbuf, recvbuflen - 1, 0);
        if (iRecvResult > 0) {
            recvbuf[iRecvResult] = '\0';
            printf("[Recebido] <- %s\n", recvbuf);

            char temp_seq[7] = { 0 };
            memcpy(temp_seq, recvbuf, 6);
            long int seq_recebido = atol(temp_seq);

            numero_seq = seq_recebido + 1;

            if (numero_seq > 999999) {
                numero_seq = 1;
            }
        }
        else if (iRecvResult == 0) {
            printf("Conexao fechada pelo servidor.\n");
            return;
        }
        else {
            printf("Erro no recv: %d\n", WSAGetLastError());
            return;
        }

        timer_ciclo.expires_at(timer_ciclo.expiry() + boost::asio::chrono::milliseconds(2000));
        timer_ciclo.async_wait(loop_envio);
        };

    // Dispara o primeiro ciclo assíncrono
    timer_ciclo.async_wait(loop_envio);
    io_context.run();

    closesocket(ConnectSocket);
    WSACleanup();

    return 0;
}
