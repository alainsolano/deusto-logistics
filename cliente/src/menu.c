#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../compartido/protocolo.h"

void limpiar_menu();
void mostrar_ficha_producto(const char* id_prod);
void ventana_operaciones(SOCKET_T sock, const char* id_operario);
void ventana_historial();
S
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
            case 2: mostrar_ficha_producto("PROD-DEUSTO-XX"); break; 
            case 3: ventana_historial(); break;
            case 0: return;
        }
    }
}

void mostrar_ficha_producto(const char* id_prod) {
    limpiar_menu();
    printf("  ┌──────────────────────────────────────────────────────────┐\n");
    printf("  │ DETALLE DE PRODUCTO: %-35s                      │\n", id_prod);
    printf("  ├──────────────────────────────────────────────────────────┤\n");
    printf("  │  > Categoria: %-15s   > Pasillo: %-10s │\n", "Logistica", "A-12");
    printf("  │  > Proveedor: %-15s   > Estante: %-10s │\n", "DeustoCorp", "04");
    printf("  ├──────────────────────────────────────────────────────────┤\n");
    printf("  │  STOCK ACTUAL: [ 100 unidades ]                          │\n");
    printf("  │  ESTADO:       [ XXX ]                                   │\n");
    printf("  └──────────────────────────────────────────────────────────┘\n");
    printf("\n  Presione Enter para volver...");
    getchar(); getchar(); 
}

void limpiar_menu() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void ventana_operaciones(SOCKET_T sock, const char* id_operario) {
    limpiar_menu();
    printf("  ╔══════════════════════════════════════════╗\n");
    printf("  ║        REGISTRO DE MOVIMIENTOS           ║\n");
    printf("  ╚══════════════════════════════════════════╝\n");
    printf("\n  [ Tarea: Capturar ID, Cantidad y llamar a escribir_log ]\n");
    printf("\n  Presione Enter para volver...");
    getchar(); getchar();
}

void ventana_historial() {
    limpiar_menu();
    printf("  ╔══════════════════════════════════════════╗\n");
    printf("  ║         AUDITORIA DE SISTEMA             ║\n");
    printf("  ╚══════════════════════════════════════════╝\n");
    printf("\n  [ Tarea: Implementar lectura de logs/log.txt ]\n");
    printf("\n  Presione Enter para volver...");
    getchar(); getchar();
}