#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stdint.h>

#pragma pack(push, 1)

struct MovimientoStock {
    char id_producto[16];
    int32_t cantidad;
    char tipo_op;
    int64_t timestamp;
    char id_operario[16];
};

struct RespuestaServidor
{
    int32_t codigo;
    int32_t stock_actual;
    char mensaje[64];
};

#pragma pack(pop);

#endif
