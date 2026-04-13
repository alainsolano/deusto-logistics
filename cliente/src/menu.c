#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "logger.h"
#include "../../compartido/protocolo.h"

/* ── Prototipos internos ── */
void limpiar_menu();
void mostrar_ficha_producto(const char* id_prod);
void ventana_operaciones(SOCKET_T sock, const char* id_operario);
void ventana_historial();

/* ══════════════════════════════════════════════════════════
   MENU PRINCIPAL
   ══════════════════════════════════════════════════════════ */
void menu_principal(SOCKET_T sock, const char* id_operario) {
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
        printf("  ║  3. [AUDITORIA]  Consultar Historial de Logs             ║\n");
        printf("  ║                                                          ║\n");
        printf("  ║  0. CERRAR SESION                                        ║\n");
        printf("  ║                                                          ║\n");
        printf("  ╚══════════════════════════════════════════════════════════╝\n");
        printf("  >> Seleccione una seccion: ");

        if (scanf("%d", &opcion) != 1) {
            while(getchar() != '\n');
            continue;
        }

        switch (opcion) {
            case 1: ventana_operaciones(sock, id_operario); break;
            case 2: mostrar_ficha_producto(NULL);           break;
            case 3: ventana_historial();                    break;
            case 0: return;
            default:
                printf("\n  [!] Opcion no valida. Pulse Enter para continuar...");
                getchar(); getchar();
        }
    }
}

/* ══════════════════════════════════════════════════════════
   UTILIDAD: limpiar pantalla
   ══════════════════════════════════════════════════════════ */
void limpiar_menu() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

/* ══════════════════════════════════════════════════════════
   1. REGISTRO DE MOVIMIENTOS
      Captura ID producto, tipo y cantidad, llama a escribir_log
   ══════════════════════════════════════════════════════════ */
void ventana_operaciones(SOCKET_T sock, const char* id_operario) {
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
        while(getchar() != '\n');
        return;
    }
    if (tipo_sel != 1 && tipo_sel != 2) {
        printf("\n  [!] Opcion no valida. Pulse Enter para continuar...");
        getchar(); getchar();
        return;
    }

    char tipo_op = (tipo_sel == 1) ? 'E' : 'S';

    printf("\n  >> ID de Producto (max 15 chars): ");
    if (scanf("%15s", id_prod) != 1) { while(getchar()!='\n'); return; }

    printf("  >> Cantidad: ");
    if (scanf("%d", &cantidad) != 1 || cantidad <= 0) {
        while(getchar() != '\n');
        printf("\n  [!] Cantidad invalida. Pulse Enter para continuar...");
        getchar(); getchar();
        return;
    }

    /* Construir struct y registrar en log */
    struct MovimientoStock mov;
    memset(&mov, 0, sizeof(mov));
    strncpy(mov.id_producto, id_prod,     sizeof(mov.id_producto) - 1);
    strncpy(mov.id_operario, id_operario, sizeof(mov.id_operario) - 1);
    mov.cantidad  = (int32_t)cantidad;
    mov.tipo_op   = tipo_op;
    mov.timestamp = (int64_t)time(NULL);

    char log_msg[128];
    snprintf(log_msg, sizeof(log_msg),
        "[%s] Operario=%-12s  Producto=%-15s  Tipo=%c  Cantidad=%d",
        (tipo_op == 'E') ? "ENTRADA" : "SALIDA ",
        mov.id_operario, mov.id_producto, mov.tipo_op, mov.cantidad);

    escribir_log(log_msg);

    /* Confirmacion visual */
    limpiar_menu();
    printf("  ╔══════════════════════════════════════════════════╗\n");
    printf("  ║         MOVIMIENTO REGISTRADO [OK]               ║\n");
    printf("  ╠══════════════════════════════════════════════════╣\n");
    printf("  ║  Producto  : %-33s║\n", mov.id_producto);
    printf("  ║  Tipo      : %-33s║\n", (tipo_op=='E')?"ENTRADA (+stock)":"SALIDA  (-stock)");
    printf("  ║  Cantidad  : %-33d║\n", mov.cantidad);
    printf("  ║  Operario  : %-33s║\n", mov.id_operario);
    printf("  ╠══════════════════════════════════════════════════╣\n");
    printf("  ║  [OK] Guardado en logs/log.txt                   ║\n");
    printf("  ╚══════════════════════════════════════════════════╝\n");
    printf("\n  Presione Enter para volver...");
    getchar(); getchar();
}

/* ══════════════════════════════════════════════════════════
   2. INVENTARIO
      Pide ID, calcula stock leyendo el log y muestra la ficha
   ══════════════════════════════════════════════════════════ */
void mostrar_ficha_producto(const char* id_prod_param) {
    char id_buf[16];
    const char* id_prod;

    if (id_prod_param == NULL || id_prod_param[0] == '\0') {
        limpiar_menu();
        printf("  ╔══════════════════════════════════════════════════════════╗\n");
        printf("  ║                  CONSULTA DE INVENTARIO                  ║\n");
        printf("  ╚══════════════════════════════════════════════════════════╝\n");
        printf("\n  >> ID de Producto a consultar (max 15 chars): ");
        if (scanf("%15s", id_buf) != 1) { while(getchar()!='\n'); return; }
        id_prod = id_buf;
    } else {
        id_prod = id_prod_param;
    }

    /* Calcular stock leyendo el log */
    FILE *flog = fopen("logs/log.txt", "r");
    int stock = 0, entradas = 0, salidas = 0;

    if (flog != NULL) {
        char linea[256];
        while (fgets(linea, sizeof(linea), flog)) {
            if (strstr(linea, id_prod) == NULL) continue;
            int cant = 0;
            char tipo_str[8];
            if (sscanf(linea, "%*[^[][%7[^]]]%*[^C]Cantidad=%d", tipo_str, &cant) == 2) {
                if (strncmp(tipo_str, "ENTRADA", 7) == 0) { stock+=cant; entradas+=cant; }
                else                                       { stock-=cant; salidas +=cant; }
            }
        }
        fclose(flog);
    }

    const char* estado;
    if      (stock <= 0)  estado = "SIN STOCK";
    else if (stock < 10)  estado = "CRITICO";
    else if (stock < 50)  estado = "BAJO";
    else                  estado = "NORMAL";

    limpiar_menu();
    printf("  ┌──────────────────────────────────────────────────────────┐\n");
    printf("  │ DETALLE DE PRODUCTO: %-37s│\n", id_prod);
    printf("  ├──────────────────────────────────────────────────────────┤\n");
    printf("  │  > Categoria: %-15s   > Pasillo: %-10s │\n", "Logistica", "A-12");
    printf("  │  > Proveedor: %-15s   > Estante: %-10s │\n", "DeustoCorp", "04");
    printf("  ├──────────────────────────────────────────────────────────┤\n");
    printf("  │  STOCK ACTUAL: [ %-5d unidades ]                        │\n", stock);
    printf("  │  ESTADO:       [ %-10s ]                            │\n", estado);
    printf("  ├──────────────────────────────────────────────────────────┤\n");
    printf("  │  MOVIMIENTOS:  Entradas: %-5d  |  Salidas: %-5d        │\n", entradas, salidas);
    printf("  └──────────────────────────────────────────────────────────┘\n");
    printf("\n  Presione Enter para volver...");
    getchar(); getchar();
}

/* ══════════════════════════════════════════════════════════
   3. AUDITORIA
      Lee logs/log.txt, permite filtrar por ID de producto
   ══════════════════════════════════════════════════════════ */
void ventana_historial() {
    int opcion_filtro;
    char filtro[32] = "";

    limpiar_menu();
    printf("  ╔══════════════════════════════════════════════════════════╗\n");
    printf("  ║                  AUDITORIA DE SISTEMA                    ║\n");
    printf("  ╠══════════════════════════════════════════════════════════╣\n");
    printf("  ║  Mostrar:                                                ║\n");
    printf("  ║    [1] Todos los registros                               ║\n");
    printf("  ║    [2] Filtrar por ID de producto                        ║\n");
    printf("  ║    [0] Volver                                            ║\n");
    printf("  ╚══════════════════════════════════════════════════════════╝\n");
    printf("  >> Opcion: ");

    if (scanf("%d", &opcion_filtro) != 1 || opcion_filtro == 0) {
        while(getchar() != '\n');
        return;
    }

    if (opcion_filtro == 2) {
        printf("  >> ID de Producto a filtrar: ");
        if (scanf("%31s", filtro) != 1) { while(getchar()!='\n'); return; }
    }

    FILE *flog = fopen("logs/log.txt", "r");

    limpiar_menu();
    printf("  ╔══════════════════════════════════════════════════════════╗\n");
    if (filtro[0] != '\0')
        printf("  ║  HISTORIAL DE LOGS — Filtro: %-27s║\n", filtro);
    else
        printf("  ║  HISTORIAL DE LOGS — Todos los registros             ║\n");
    printf("  ╠══════════════════════════════════════════════════════════╣\n");

    if (flog == NULL) {
        printf("  ║  [!] No se encontro el archivo logs/log.txt          ║\n");
        printf("  ║      Aun no hay movimientos registrados.             ║\n");
    } else {
        char linea[256];
        int total = 0;

        while (fgets(linea, sizeof(linea), flog)) {
            size_t len = strlen(linea);
            if (len > 0 && linea[len-1] == '\n') linea[len-1] = '\0';

            if (filtro[0] != '\0' && strstr(linea, filtro) == NULL) continue;

            char ts[24]      = "";
            char cuerpo[220] = "";

            if (sscanf(linea, "[%23[^]]] %219[^\n]", ts, cuerpo) == 2) {
                printf("  ║  %s                                    ║\n", ts);
                int pos = 0, clen = (int)strlen(cuerpo);
                while (pos < clen) {
                    char frag[57] = "";
                    strncpy(frag, cuerpo + pos, 56);
                    printf("  ║    %-54s  ║\n", frag);
                    pos += 56;
                }
            } else {
                printf("  ║  %-56.56s  ║\n", linea);
            }
            printf("  ╠══════════════════════════════════════════════════════════╣\n");
            total++;
        }
        fclose(flog);

        if (total == 0) {
            if (filtro[0] != '\0')
                printf("  ║  [i] Sin registros para: %-31s  ║\n", filtro);
            else
                printf("  ║  [i] El log esta vacio. Sin movimientos aun.         ║\n");
            printf("  ╠══════════════════════════════════════════════════════════╣\n");
        }
        printf("  ║  Total registros mostrados: %-28d  ║\n", total);
    }

    printf("  ╚══════════════════════════════════════════════════════════╝\n");
    printf("\n  Presione Enter para volver...");
    getchar(); getchar();
}