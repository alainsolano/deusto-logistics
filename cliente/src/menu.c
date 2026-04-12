#include <stdio.h>
#include <string.h>
#include <time.h>
#include "menu.h"
#include "logger.h"
#include "../../compartido/protocolo.h"

static void enviar_movimiento(SOCKET_T sock, const char* id_operario, char tipo){
    struct MovimientoStock mov;
    memset(&mov, 0, sizeof(mov));

    printf("ID de producto: ");
    scanf("%15s", mov.id_producto);

    if (tipo != 'C') {
        printf("Cantidad: ");
        scanf("%d", &mov.cantidad);
    } else {
        mov.cantidad = 0;
    }

    mov.tipo_op = tipo;
    mov.timestamp = (int64_t)time(NULL);
    strncpy(mov.id_operario, id_operario, 15);
    mov.id_operario[15] = '\0';

    struct RespuestaServidor res;
    res.codigo = 0; //EXITO
    res.stock_actual = 100; //VALOR SIMULADO
    strcpy(res.mensaje, "Operacion registrada con exito.");
    
    char log_msg[128];

    if (res.codigo == 0) {
        if (tipo == 'C') {
            printf("Cantidad actual es de %s: %d\n", mov.id_producto, res.stock_actual);
            snprintf(log_msg, sizeof(log_msg), "Consulta: %s (Stock: %d)", mov.id_producto, res.stock_actual);
        } else {
            printf("OK %s (Stock actual: %d)\n", res.mensaje, res.stock_actual);
            if (tipo == 'E') snprintf(log_msg, sizeof(log_msg), "Entrada: %s +%d", mov.id_producto, mov.cantidad);
            else snprintf(log_msg, sizeof(log_msg), "Salida: %s -%d", mov.id_producto, mov.cantidad);
        }
        escribir_log(log_msg);
        
    } else {
        printf("Error: %s\n", res.mensaje);
        sprintf(log_msg, "ERROR (%c): %s", tipo, res.mensaje);
        escribir_log(log_msg);
    }
}

void menu_principal(SOCKET_T sock, const char* id_operario) {
    int opcion;
    while(1) {
        printf("\n=== DEUSTO LOGISTICS - TERMINAL ===\n");
        printf(" 1. Registrar entrada de stock\n");
        printf(" 2. Registrar salida de stock\n");
        printf(" 3. Consultar stock\n");
        printf(" 0. Salir\n");
        printf("Opcion: ");

        if (scanf("%d", &opcion) != 1) break;

        switch (opcion)
        {
        case 1: enviar_movimiento(sock, id_operario, 'E'); break;
        case 2: enviar_movimiento(sock, id_operario, 'S'); break;
        case 3: enviar_movimiento(sock, id_operario, 'C'); break;
        case 0: return;
        default:
            printf("Opcion no valida.\n");
        }
    }
}