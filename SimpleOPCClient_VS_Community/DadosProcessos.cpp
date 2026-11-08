#include "DadosProcessos.h"

// Definicoes (uma unica vez em todo o projeto)
DadosProcesso     dadosGlobais;
std::mutex        mtx_dados;
std::atomic<bool> g_encerrar(false);

void AtualizaDadoOPC(unsigned long hClient, double valor, bool valido)
{
    if (hClient < 1 || hClient > 4) return;

    std::lock_guard<std::mutex> lock(mtx_dados);

    dadosGlobais.valido[hClient - 1] = valido;
    if (!valido) return;                 // mantem o ultimo valor

    switch (hClient) {
    case 1: dadosGlobais.temp_camara = valor; break;
    case 2: dadosGlobais.pressao_camara = valor; break;
    case 3: dadosGlobais.pressao_argonio = valor; break;
    case 4: dadosGlobais.temp_panela = valor; break;
    }
}