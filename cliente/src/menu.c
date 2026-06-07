#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "logger.h"
#include "red.h"

#define MAX_ITEMS 256

static void limpiar_menu(void);
static int  ventana_operaciones(socket_t sock, const char *id_operario);
static int  ventana_ficha(socket_t sock);
static int  ventana_resumen_stock(socket_t sock);
static int  ventana_historial(socket_t sock);

/*    MENU PRINCIPAL */
int menu_principal(socket_t sock, const char *id_operario) {
    int opcion;
    while (1) {
        limpiar_menu();
        printf("  ╔══════════════════════════════════════════════════════════╗\n");
        printf("  ║                PANEL DE CONTROL PRINCIPAL                ║\n");
        printf("  ╠══════════════════════════════════════════════════════════╣\n");
        printf("  ║ Operario: %-18s        Estado: [CONECTADO]  ║\n", id_operario);
        printf("  ╠══════════════════════════════════════════════════════════╣\n");
        printf("  ║                                                          ║\n");
        printf("  ║  1. [MOVIMIENTO] Registrar Entrada/Salida                ║\n");
        printf("  ║  2. [INVENTARIO] Ver Ficha de Producto                   ║\n");
        printf("  ║  3. [STOCK]      Resumen General de Stock                ║\n");
        printf("  ║  4. [AUDITORIA]  Consultar Historial                     ║\n");
        printf("  ║                                                          ║\n");
        printf("  ║  0. CERRAR SESION                                        ║\n");
        printf("  ║                                                          ║\n");
        printf("  ╚══════════════════════════════════════════════════════════╝\n");
        printf("  >> Seleccione una seccion: ");

        if (scanf("%d", &opcion) != 1) {
            while (getchar() != '\n');
            continue;
        }

        int rc = 0;
        switch (opcion) {
            case 1: rc = ventana_operaciones(sock, id_operario); break;
            case 2: rc = ventana_ficha(sock);                    break;
            case 3: rc = ventana_resumen_stock(sock);            break;
            case 4: rc = ventana_historial(sock);                break;
            case 0: return 0;
            default:
                printf("\n  [!] Opcion no valida. Pulse Enter para continuar...");
                getchar(); getchar();
        }

        if (rc == -1) {
            return -1;  /* conexion perdida: el cliente debe cerrarse */
        }
    }
}

/*  limpiar pantalla */
static void limpiar_menu(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

/*    1. REGISTRO DE MOVIMIENTOS  (entrada / salida via servidor) */
static int ventana_operaciones(socket_t sock, const char *id_operario) {
    char id_prod[16];
    int  cantidad;
    int  tipo_sel;

    limpiar_menu();
    printf("  ╔══════════════════════════════════════════════════╗\n");
    printf("  ║           REGISTRO DE MOVIMIENTOS                ║\n");
    printf("  ╠══════════════════════════════════════════════════╣\n");
    printf("  ║  Tipo de operacion:                              ║\n");
    printf("  ║    [1] ENTRADA  (+stock)                         ║\n");
    printf("  ║    [2] SALIDA   (-stock)                         ║\n");
    printf("  ║    [0] Volver                                    ║\n");
    printf("  ╚══════════════════════════════════════════════════╝\n");
    printf("  >> Opcion: ");

    if (scanf("%d", &tipo_sel) != 1 || tipo_sel == 0) {
        while (getchar() != '\n');
        return 0;
    }
    if (tipo_sel != 1 && tipo_sel != 2) {
        printf("\n  [!] Opcion no valida. Pulse Enter para continuar...");
        getchar(); getchar();
        return 0;
    }

    char tipo_op = (tipo_sel == 1) ? OP_ENTRADA : OP_SALIDA;

    printf("\n  >> ID de Producto (max 15 chars): ");
    if (scanf("%15s", id_prod) != 1) { while (getchar() != '\n'); return 0; }

    printf("  >> Cantidad: ");
    if (scanf("%d", &cantidad) != 1 || cantidad <= 0) {
        while (getchar() != '\n');
        printf("\n  [!] Cantidad invalida. Pulse Enter para continuar...");
        getchar(); getchar();
        return 0;
    }

    /* Construir la peticion y enviarla al servidor */
    MovimientoStock mov;
    memset(&mov, 0, sizeof(mov));
    strncpy(mov.id_producto, id_prod,     sizeof(mov.id_producto) - 1);
    strncpy(mov.id_operario, id_operario, sizeof(mov.id_operario) - 1);
    mov.cantidad  = cantidad;
    mov.tipo_op   = tipo_op;
    mov.timestamp = (int64_t)time(NULL);

    RespuestaServidor resp;
    memset(&resp, 0, sizeof(resp));
    if (red_movimiento(sock, &mov, &resp) != 0) {
        return -1;  /* conexion perdida */
    }

    /* Log local del intento */
    {
        char log_msg[160];
        snprintf(log_msg, sizeof(log_msg),
            "%s | operario: %s | producto: %s | cantidad: %d | resultado: %s",
            (tipo_op == OP_ENTRADA) ? "ENTRADA" : "SALIDA",
            id_operario, id_prod, cantidad,
            resp.codigo == RESP_OK ? "OK" : "RECHAZADO");
        escribir_log(log_msg);
    }

    /* Confirmacion visual */
    limpiar_menu();
    printf("  ╔══════════════════════════════════════════════════╗\n");
    if (resp.codigo == RESP_OK)
        printf("  ║         MOVIMIENTO REGISTRADO [OK]               ║\n");
    else
        printf("  ║         MOVIMIENTO RECHAZADO [!]                 ║\n");
    printf("  ╠══════════════════════════════════════════════════╣\n");
    printf("  ║  Producto  : %-33s║\n", mov.id_producto);
    printf("  ║  Tipo      : %-33s║\n",
           (tipo_op == OP_ENTRADA) ? "ENTRADA (+stock)" : "SALIDA  (-stock)");
    printf("  ║  Cantidad  : %-33d║\n", mov.cantidad);
    printf("  ║  Operario  : %-33s║\n", mov.id_operario);
    printf("  ╠══════════════════════════════════════════════════╣\n");
    if (resp.codigo == RESP_OK)
        printf("  ║  Stock actual: %-31d║\n", resp.stock_actual);
    printf("  ║  %-48s║\n", resp.mensaje);
    printf("  ╚══════════════════════════════════════════════════╝\n");
    printf("\n  Presione Enter para volver...");
    getchar(); getchar();
    return 0;
}

/*  2. FICHA DE PRODUCTO  (consulta al servidor) */
static int ventana_ficha(socket_t sock) {
    char id_prod[16];

    limpiar_menu();
    printf("  ╔══════════════════════════════════════════════════════════╗\n");
    printf("  ║                  CONSULTA DE INVENTARIO                  ║\n");
    printf("  ╚══════════════════════════════════════════════════════════╝\n");
    printf("\n  >> ID de Producto a consultar (max 15 chars): ");
    if (scanf("%15s", id_prod) != 1) { while (getchar() != '\n'); return 0; }

    ConsultaResponse resp;
    memset(&resp, 0, sizeof(resp));
    if (red_consulta(sock, id_prod, &resp) != 0) {
        return -1;
    }

    {
        char log_msg[128];
        snprintf(log_msg, sizeof(log_msg),
            "CONSULTA_STOCK | producto: %s | stock_actual: %d",
            id_prod, resp.stock_actual);
        escribir_log(log_msg);
    }

    limpiar_menu();

    if (resp.codigo != RESP_OK) {
        printf("  ┌──────────────────────────────────────────────────────────┐\n");
        printf("  │ %-58s │\n", resp.mensaje[0] ? resp.mensaje : "Producto no encontrado.");
        printf("  └──────────────────────────────────────────────────────────┘\n");
        printf("\n  Presione Enter para volver...");
        getchar(); getchar();
        return 0;
    }

    const char *estado;
    if      (resp.stock_actual <= 0)                  estado = "SIN STOCK";
    else if (resp.stock_actual <  resp.stock_minimo)  estado = "CRITICO";
    else if (resp.stock_actual <  resp.stock_minimo * 2) estado = "BAJO";
    else                                              estado = "NORMAL";

    printf("  ┌──────────────────────────────────────────────────────────┐\n");
    printf("  │ DETALLE DE PRODUCTO: %-37s│\n", id_prod);
    printf("  ├──────────────────────────────────────────────────────────┤\n");
    printf("  │  > Nombre:    %-43s│\n", resp.nombre);
    printf("  │  > Ubicacion: %-15s   > Stock min.: %-10d │\n",
           resp.ubicacion[0] ? resp.ubicacion : "-", resp.stock_minimo);
    printf("  ├──────────────────────────────────────────────────────────┤\n");
    printf("  │  STOCK ACTUAL: [ %-5d unidades ]                        │\n", resp.stock_actual);
    printf("  │  ESTADO:       [ %-10s ]                            │\n", estado);
    printf("  ├──────────────────────────────────────────────────────────┤\n");
    printf("  │  MOVIMIENTOS:  Entradas: %-5d  |  Salidas: %-5d        │\n",
           resp.entradas, resp.salidas);
    printf("  └──────────────────────────────────────────────────────────┘\n");
    printf("\n  Presione Enter para volver...");
    getchar(); getchar();
    return 0;
}

/* 3. RESUMEN GENERAL DE STOCK  (todos los productos del servidor) */
static int ventana_resumen_stock(socket_t sock) {
    static ResumenItem items[MAX_ITEMS];
    int n = 0;

    if (red_resumen(sock, items, MAX_ITEMS, &n) != 0) {
        return -1;
    }

    limpiar_menu();
    printf("  ╔══════════════════════════════════════════════════════════╗\n");
    printf("  ║               RESUMEN GENERAL DE STOCK                   ║\n");
    printf("  ╠══════════════════════════════════════════════════════════╣\n");
    printf("  ║  %-12s %-22s %6s  %-10s║\n", "PRODUCTO", "NOMBRE", "STOCK", "ESTADO");
    printf("  ╠══════════════════════════════════════════════════════════╣\n");

    if (n == 0) {
        printf("  ║  [i] No hay productos registrados en el sistema.        ║\n");
    } else {
        for (int i = 0; i < n; i++) {
            printf("  ║  %-12s %-22.22s %6d  %-10s║\n",
                   items[i].id_producto, items[i].nombre,
                   items[i].stock, items[i].estado);
        }
    }

    printf("  ╠══════════════════════════════════════════════════════════╣\n");
    printf("  ║  Total productos: %-39d║\n", n);
    printf("  ╚══════════════════════════════════════════════════════════╝\n");
    printf("\n  Presione Enter para volver...");
    getchar(); getchar();
    return 0;
}

/*  4. AUDITORIA / HISTORIAL  (desde la BD del servidor) */
static int ventana_historial(socket_t sock) {
    int opcion_filtro;
    char filtro[16] = "";

    limpiar_menu();
    printf("  ╔══════════════════════════════════════════════════════════╗\n");
    printf("  ║                  AUDITORIA DE SISTEMA                    ║\n");
    printf("  ╠══════════════════════════════════════════════════════════╣\n");
    printf("  ║  Mostrar:                                                ║\n");
    printf("  ║    [1] Todos los movimientos                             ║\n");
    printf("  ║    [2] Filtrar por ID de producto                        ║\n");
    printf("  ║    [0] Volver                                            ║\n");
    printf("  ╚══════════════════════════════════════════════════════════╝\n");
    printf("  >> Opcion: ");

    if (scanf("%d", &opcion_filtro) != 1 || opcion_filtro == 0) {
        while (getchar() != '\n');
        return 0;
    }

    if (opcion_filtro == 2) {
        printf("  >> ID de Producto a filtrar: ");
        if (scanf("%15s", filtro) != 1) { while (getchar() != '\n'); return 0; }
    }

    static HistorialItem items[MAX_ITEMS];
    int n = 0;
    if (red_historial(sock, filtro, items, MAX_ITEMS, &n) != 0) {
        return -1;
    }

    limpiar_menu();
    printf("  ╔══════════════════════════════════════════════════════════╗\n");
    if (filtro[0] != '\0')
        printf("  ║  HISTORIAL — Filtro: %-35s║\n", filtro);
    else
        printf("  ║  HISTORIAL — Todos los movimientos                      ║\n");
    printf("  ╠══════════════════════════════════════════════════════════╣\n");

    if (n == 0) {
        printf("  ║  [i] Sin movimientos registrados.                       ║\n");
    } else {
        for (int i = 0; i < n; i++) {
            printf("  ║  %-19s %-9s %-10s %+5d -> %-5d ║\n",
                   items[i].timestamp,
                   items[i].id_producto,
                   items[i].tipo_operacion,
                   (items[i].tipo_operacion[0] == 'e') ? items[i].cantidad
                                                       : -items[i].cantidad,
                   items[i].stock_resultante);
        }
    }

    printf("  ╠══════════════════════════════════════════════════════════╣\n");
    printf("  ║  Total registros: %-39d║\n", n);
    printf("  ╚══════════════════════════════════════════════════════════╝\n");
    printf("\n  Presione Enter para volver...");
    getchar(); getchar();
    return 0;
}
