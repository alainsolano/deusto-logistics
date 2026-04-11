#include "menu.h"
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "logger.h"

static void enviar_movimiento(SOCKET_T sock, const char* id_operario, char tipo) {
    struct MovimientoStock mov;
    memset(&mov, 0, sizeof(mov));

    printf("ID de producto: ");
    scanf("%15s", mov.id_producto);
    printf("Cantidad: ");
    scanf("%d", &mov.cantidad);

    mov.tipo_op = tipo;
    mov.timestamp = (int64_t)time(NULL);
    strncpy(mov.id_operario, id_operario, 15);
    mov.id_operario[15] = '\0';

    send(sock, (const char*)&mov, sizeof(mov), 0);

    struct RespuestaServidor res;
    recv(sock, (char*)&res, sizeof(res), 0);
    char log_msg[128];

    if (res.codigo == 0) {
        printf("✓ %s  (Stock actual: %d)\n", res.mensaje, res.stock_actual);
        if (tipo == 'E') {
            sprintf(log_msg, "Entrada: %s +%d", mov.id_producto, mov.cantidad);
        } else if (tipo == 'S') {
            sprintf(log_msg, "Salida: %s -%d", mov.id_producto, mov.cantidad);
        } else if (tipo == 'C') {
            sprintf(log_msg, "Consulta: %s", mov.id_producto);
        }

        escribir_log(log_msg);
    
    } else {
        printf("✗ Error: %s\n", res.mensaje);

        sprintf(log_msg, "ERROR (%c): %s", tipo, res.mensaje);
        escribir_log(log_msg);
    }
}

void menu_principal(SOCKET_T sock, const char* id_operario) {
    int opcion;

    while (1) {
        printf("\n=== DEUSTO LOGISTICS — TERMINAL ===\n");
        printf(" 1. Registrar entrada de stock\n");
        printf(" 2. Registrar salida de stock\n");
        printf(" 3. Consultar stock\n");
        printf(" 0. Salir\n");
        printf("Opcion: ");
        scanf("%d", &opcion);

        switch (opcion) {
            case 1: enviar_movimiento(sock, id_operario, 'E'); break;
            case 2: enviar_movimiento(sock, id_operario, 'S'); break;
            case 3: enviar_movimiento(sock, id_operario, 'C'); break;
            case 0: return;
            default: printf("Opción no válida.\n");
        }
    }
}