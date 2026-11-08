#pragma once
#include <mutex>
#include <atomic>

struct DadosProcesso {
    // Valores lidos do OPC por callback (hClient 1..4)
    double temp_camara = 0.0;   // hClient 1 - Saw-toothed Waves.Real4
    double pressao_camara = 0.0;   // hClient 2 - Square Waves.Real4
    double pressao_argonio = 0.0;   // hClient 3 - Triangle Waves.Real4
    double temp_panela = 0.0;   // hClient 4 - Random.Real4
    bool   valido[4] = { false, false, false, false };

    // Set-points recebidos do Computador de Processo (msg 200)
    int    sp_pressao_camara = 0;     // Bucket Brigade.Int4
    double sp_temp_camara = 0.0;   // Bucket Brigade.Real4
    double sp_pressao_argonio = 0.0;   // Bucket Brigade.Real8
    bool   sp_pendente = false; // true = ha set-points novos para escrever no OPC
};

extern DadosProcesso     dadosGlobais;
extern std::mutex        mtx_dados;
extern std::atomic<bool> g_encerrar;    // true = pede para as threads terminarem

// Chamada pelo callback OPC (hClient 1..4)
void AtualizaDadoOPC(unsigned long hClient, double valor, bool valido);