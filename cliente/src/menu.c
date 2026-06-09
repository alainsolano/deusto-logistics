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
static int  ventana_alta_producto(socket_t sock);
static int  ventana_editar_producto(socket_t sock);
static int  ventana_baja_producto(socket_t sock);
static int  ventana_proveedores_pedidos(socket_t sock);
static int  mostrar_alertas(socket_t sock);  /* imprime bloque de alertas; -1 si cae la red */

/*    MENU PRINCIPAL */
int menu_principal(socket_t sock, const char *id_operario, const char *rol) {
    int opcion;
    int primera_vez = 1;
    int es_admin = (strcmp(rol, "admin") == 0);
    while (1) {
        limpiar_menu();
        printf("  ╔══════════════════════════════════════════════════════════╗\n");
        printf("  ║                PANEL DE CONTROL PRINCIPAL                ║\n");
        printf("  ╠══════════════════════════════════════════════════════════╣\n");
        printf("  ║ Operario: %-15s   Rol: %-9s [CONECTADO]  ║\n", id_operario, rol);
        printf("  ╚══════════════════════════════════════════════════════════╝\n");

        /* Alertas automaticas al iniciar sesion (solo la primera vez) */
        if (primera_vez) {
            if (mostrar_alertas(sock) == -1) return -1;
            primera_vez = 0;
        }

        printf("  ╔══════════════════════════════════════════════════════════╗\n");
        printf("  ║  1. [MOVIMIENTO] Registrar Entrada/Salida                ║\n");
        printf("  ║  2. [INVENTARIO] Ver Ficha de Producto                   ║\n");
        printf("  ║  3. [STOCK]      Resumen General de Stock                ║\n");
        printf("  ║  4. [AUDITORIA]  Consultar Historial                     ║\n");
        printf("  ║  5. [ADMIN]      Añadir Nuevo Producto                   ║\n");
        printf("  ║  6. [ADMIN]      Editar Producto                         ║\n");
        printf("  ║  7. [ADMIN]      Dar de Baja Producto                    ║\n");
        printf("  ║  8. [COMPRAS]    Proveedores y Pedidos                   ║\n");
        printf("  ║  0. CERRAR SESION                                        ║\n");
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
            case 5:
            case 6:
            case 7:
                if (!es_admin) {
                    printf("\n  [!] Permiso denegado: esta opcion requiere rol admin.\n");
                    printf("  Pulse Enter para continuar...");
                    getchar(); getchar();
                } else if (opcion == 5) rc = ventana_alta_producto(sock);
                else if (opcion == 6) rc = ventana_editar_producto(sock);
                else                   rc = ventana_baja_producto(sock);
                break;
            case 8: rc = ventana_proveedores_pedidos(sock);      break;
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

/* Obtiene y muestra el bloque de alertas (stock bajo + caducidad).
   Devuelve 0 si OK, -1 si se perdio la conexion. */
static int mostrar_alertas(socket_t sock) {
    static AlertaItem alertas[MAX_ITEMS];
    int n = 0;
    if (red_alertas(sock, alertas, MAX_ITEMS, &n) != 0) {
        return -1;
    }

    if (n == 0) {
        printf("  [OK] Sin alertas: stock y caducidades en orden.\n");
        return 0;
    }

    printf("  ┌─────────────────────── ALERTAS (%2d) ─────────────────────┐\n", n);
    for (int i = 0; i < n; i++) {
        printf("  │ [%-13s] %-10s %-30.30s│\n",
               alertas[i].tipo_alerta, alertas[i].id_producto, alertas[i].nombre);
    }
    printf("  └──────────────────────────────────────────────────────────┘\n");
    return 0;
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

    if (mostrar_alertas(sock) == -1) return -1;

    printf("\n  Presione Enter para volver...");
    getchar(); getchar();
    return 0;
}

/*  4. AUDITORIA / HISTORIAL  (desde la BD del servidor) */
static int ventana_historial(socket_t sock) {
    int opcion_filtro;
    char filtro[16]   = "";
    char operario[32] = "";
    char desde[24]    = "";
    char hasta[24]    = "";

    limpiar_menu();
    printf("  ╔══════════════════════════════════════════════════════════╗\n");
    printf("  ║                  AUDITORIA DE SISTEMA                    ║\n");
    printf("  ╠══════════════════════════════════════════════════════════╣\n");
    printf("  ║  Mostrar:                                                ║\n");
    printf("  ║    [1] Todos los movimientos                             ║\n");
    printf("  ║    [2] Filtrar por ID de producto                        ║\n");
    printf("  ║    [3] Filtrar por operario                              ║\n");
    printf("  ║    [4] Filtrar por rango de fechas                       ║\n");
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
    } else if (opcion_filtro == 3) {
        printf("  >> Nombre de operario a filtrar: ");
        if (scanf("%31s", operario) != 1) { while (getchar() != '\n'); return 0; }
    } else if (opcion_filtro == 4) {
        printf("  >> Fecha desde (YYYY-MM-DD, vacio = sin limite): ");
        if (scanf("%23s", desde) != 1) { while (getchar() != '\n'); return 0; }
        printf("  >> Fecha hasta (YYYY-MM-DD, vacio = sin limite): ");
        if (scanf("%23s", hasta) != 1) { while (getchar() != '\n'); return 0; }
    }

    static HistorialItem items[MAX_ITEMS];
    int n = 0;
    if (red_historial(sock, filtro, operario, desde, hasta, items, MAX_ITEMS, &n) != 0) {
        return -1;
    }

    limpiar_menu();
    printf("  ╔══════════════════════════════════════════════════════════╗\n");
    if (filtro[0] != '\0')
        printf("  ║  HISTORIAL — Producto: %-33s║\n", filtro);
    else if (operario[0] != '\0')
        printf("  ║  HISTORIAL — Operario: %-33s║\n", operario);
    else if (desde[0] != '\0' || hasta[0] != '\0')
        printf("  ║  HISTORIAL — Fechas: %-8s .. %-23s║\n",
               desde[0] ? desde : "inicio", hasta[0] ? hasta : "hoy");
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




static int ventana_alta_producto(socket_t sock) {
    AltaProductoRequest req;
    AltaProductoResponse resp;
    int tipo_sel;
    int estrategia_sel;

    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));

    limpiar_menu();

    printf(" ╔══════════════════════════════════════════════════╗\n");
    printf(" ║           ALTA DE NUEVO PRODUCTO                 ║\n");
    printf(" ╠══════════════════════════════════════════════════╣\n");
    printf(" ║ Tipo de producto:                                ║\n");
    printf(" ║   [1] Generico                                   ║\n");
    printf(" ║   [2] Fragil                                     ║\n");
    printf(" ║   [3] Inflamable                                 ║\n");
    printf(" ║   [4] Perecedero                                 ║\n");
    printf(" ║   [0] Volver                                     ║\n");
    printf(" ╚══════════════════════════════════════════════════╝\n");
    printf(" >> Opcion: ");

    if (scanf("%d", &tipo_sel) != 1 || tipo_sel == 0) {
        while (getchar() != '\n');
        return 0;
    }

    if (tipo_sel < 1 || tipo_sel > 4) {
        printf("\n [!] Tipo no valido. Pulse Enter para continuar...");
        getchar();
        getchar();
        return 0;
    }

    if (tipo_sel == 1) {
        strncpy(req.tipo, "generico", sizeof(req.tipo) - 1);
    } else if (tipo_sel == 2) {
        strncpy(req.tipo, "fragil", sizeof(req.tipo) - 1);
    } else if (tipo_sel == 3) {
        strncpy(req.tipo, "inflamable", sizeof(req.tipo) - 1);
    } else if (tipo_sel == 4) {
        strncpy(req.tipo, "perecedero", sizeof(req.tipo) - 1);
    }

    printf("\n >> ID producto (ej. PROD-0060): ");
    if (scanf("%15s", req.id_producto) != 1) {
        while (getchar() != '\n');
        return 0;
    }

    printf(" >> Nombre producto: ");
    if (scanf(" %63[^\n]", req.nombre) != 1) {
        while (getchar() != '\n');
        return 0;
    }

    printf(" >> Stock inicial: ");
    if (scanf("%d", &req.cantidad_actual) != 1 || req.cantidad_actual < 0) {
        while (getchar() != '\n');
        printf("\n [!] Stock invalido. Pulse Enter...");
        getchar();
        getchar();
        return 0;
    }

    printf(" >> Stock minimo: ");
    if (scanf("%d", &req.stock_minimo) != 1 || req.stock_minimo < 0) {
        while (getchar() != '\n');
        printf("\n [!] Stock minimo invalido. Pulse Enter...");
        getchar();
        getchar();
        return 0;
    }

    printf(" >> Precio unitario: ");
    if (scanf("%lf", &req.precio_unitario) != 1 || req.precio_unitario <= 0) {
        while (getchar() != '\n');
        printf("\n [!] Precio invalido. Pulse Enter...");
        getchar();
        getchar();
        return 0;
    }

    printf(" >> Estrategia salida [1] FIFO [2] LIFO: ");
    if (scanf("%d", &estrategia_sel) != 1) {
        while (getchar() != '\n');
        return 0;
    }

    if (estrategia_sel == 2) {
        strncpy(req.estrategia_salida, "LIFO", sizeof(req.estrategia_salida) - 1);
    } else {
        strncpy(req.estrategia_salida, "FIFO", sizeof(req.estrategia_salida) - 1);
    }

    printf(" >> Ubicacion almacen: ");
    if (scanf("%31s", req.ubicacion_almacen) != 1) {
        while (getchar() != '\n');
        return 0;
    }

    if (tipo_sel == 2) {
        printf(" >> Coste embalaje: ");
        if (scanf("%lf", &req.coste_embalaje) != 1 || req.coste_embalaje < 0) {
            while (getchar() != '\n');
            return 0;
        }

        printf(" >> Instrucciones: ");
        if (scanf(" %127[^\n]", req.instrucciones) != 1) {
            while (getchar() != '\n');
            return 0;
        }
    }

    if (tipo_sel == 3) {
        printf(" >> Nivel riesgo [1-3]: ");
        if (scanf("%d", &req.nivel_riesgo) != 1 || req.nivel_riesgo < 1 || req.nivel_riesgo > 3) {
            while (getchar() != '\n');
            printf("\n [!] Nivel de riesgo invalido. Pulse Enter...");
            getchar();
            getchar();
            return 0;
        }

        printf(" >> Zona almacenamiento: ");
        if (scanf("%31s", req.zona_almacenamiento) != 1) {
            while (getchar() != '\n');
            return 0;
        }
    }

    if (tipo_sel == 4) {
        printf(" >> Fecha caducidad (YYYY-MM-DD): ");
        if (scanf("%15s", req.fecha_caducidad) != 1) {
            while (getchar() != '\n');
            return 0;
        }

        printf(" >> Temperatura maxima: ");
        if (scanf("%lf", &req.temperatura_max) != 1) {
            while (getchar() != '\n');
            return 0;
        }
    }

    if (red_alta_producto(sock, &req, &resp) != 0) {
        return -1;
    }

    limpiar_menu();

    printf(" ╔══════════════════════════════════════════════════╗\n");
    printf(" ║             RESULTADO ALTA PRODUCTO              ║\n");
    printf(" ╠══════════════════════════════════════════════════╣\n");
    printf(" ║ Producto: %-37s║\n", req.id_producto);
    printf(" ║ Tipo    : %-37s║\n", req.tipo);
    printf(" ╠══════════════════════════════════════════════════╣\n");

    if (resp.codigo == RESP_OK) {
        printf(" ║ [OK] %-42s║\n", resp.mensaje);
    } else {
        printf(" ║ [!] %-42s║\n", resp.mensaje);
    }

    printf(" ╚══════════════════════════════════════════════════╝\n");
    printf("\n Presione Enter para volver...");
    getchar();
    getchar();

    return 0;
}

/*  6. EDITAR PRODUCTO  (campos base via servidor) */
static int ventana_editar_producto(socket_t sock) {
    EditarProductoRequest req;
    OpProductoResponse resp;
    int estrategia_sel;

    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));

    limpiar_menu();
    printf(" ╔══════════════════════════════════════════════════╗\n");
    printf(" ║              EDITAR PRODUCTO                     ║\n");
    printf(" ╚══════════════════════════════════════════════════╝\n");

    printf("\n >> ID producto a editar: ");
    if (scanf("%15s", req.id_producto) != 1) { while (getchar() != '\n'); return 0; }

    printf(" >> Nuevo nombre: ");
    if (scanf(" %63[^\n]", req.nombre) != 1) { while (getchar() != '\n'); return 0; }

    printf(" >> Nuevo stock minimo: ");
    if (scanf("%d", &req.stock_minimo) != 1 || req.stock_minimo < 0) {
        while (getchar() != '\n');
        printf("\n [!] Stock minimo invalido. Pulse Enter...");
        getchar(); getchar();
        return 0;
    }

    printf(" >> Nuevo precio unitario: ");
    if (scanf("%lf", &req.precio_unitario) != 1 || req.precio_unitario <= 0) {
        while (getchar() != '\n');
        printf("\n [!] Precio invalido. Pulse Enter...");
        getchar(); getchar();
        return 0;
    }

    printf(" >> Estrategia salida [1] FIFO [2] LIFO: ");
    if (scanf("%d", &estrategia_sel) != 1) { while (getchar() != '\n'); return 0; }
    strncpy(req.estrategia_salida, estrategia_sel == 2 ? "LIFO" : "FIFO",
            sizeof(req.estrategia_salida) - 1);

    printf(" >> Nueva ubicacion almacen: ");
    if (scanf("%31s", req.ubicacion_almacen) != 1) { while (getchar() != '\n'); return 0; }

    if (red_editar_producto(sock, &req, &resp) != 0) {
        return -1;
    }

    limpiar_menu();
    printf(" ╔══════════════════════════════════════════════════╗\n");
    printf(" ║             RESULTADO EDICION                   ║\n");
    printf(" ╠══════════════════════════════════════════════════╣\n");
    printf(" ║ Producto: %-37s║\n", req.id_producto);
    printf(" ╠══════════════════════════════════════════════════╣\n");
    if (resp.codigo == RESP_OK)
        printf(" ║ [OK] %-42s║\n", resp.mensaje);
    else
        printf(" ║ [!] %-42s║\n", resp.mensaje);
    printf(" ╚══════════════════════════════════════════════════╝\n");
    printf("\n Presione Enter para volver...");
    getchar(); getchar();
    return 0;
}

/*  7. DAR DE BAJA PRODUCTO  (baja logica via servidor) */
static int ventana_baja_producto(socket_t sock) {
    char id_prod[16];
    char confirm[8];
    OpProductoResponse resp;
    memset(&resp, 0, sizeof(resp));

    limpiar_menu();
    printf(" ╔══════════════════════════════════════════════════╗\n");
    printf(" ║             DAR DE BAJA PRODUCTO                 ║\n");
    printf(" ╚══════════════════════════════════════════════════╝\n");
    printf("\n >> ID producto a dar de baja: ");
    if (scanf("%15s", id_prod) != 1) { while (getchar() != '\n'); return 0; }

    printf(" >> Confirma la baja de '%s'? (si/no): ", id_prod);
    if (scanf("%7s", confirm) != 1) { while (getchar() != '\n'); return 0; }
    if (strcmp(confirm, "si") != 0 && strcmp(confirm, "SI") != 0) {
        printf("\n [i] Baja cancelada. Pulse Enter...");
        getchar(); getchar();
        return 0;
    }

    if (red_baja_producto(sock, id_prod, &resp) != 0) {
        return -1;
    }

    limpiar_menu();
    printf(" ╔══════════════════════════════════════════════════╗\n");
    printf(" ║             RESULTADO BAJA                      ║\n");
    printf(" ╠══════════════════════════════════════════════════╣\n");
    if (resp.codigo == RESP_OK)
        printf(" ║ [OK] %-42s║\n", resp.mensaje);
    else
        printf(" ║ [!] %-42s║\n", resp.mensaje);
    printf(" ╚══════════════════════════════════════════════════╝\n");
    printf("\n Presione Enter para volver...");
    getchar(); getchar();
    return 0;
}

/*  8. PROVEEDORES Y PEDIDOS DE REPOSICION */
static int ventana_proveedores_pedidos(socket_t sock) {
    static ProveedorItem provs[MAX_ITEMS];
    static PedidoItem     pedidos[MAX_ITEMS];
    int np = 0, npe = 0;

    if (red_proveedores(sock, provs, MAX_ITEMS, &np) != 0) return -1;
    if (red_pedidos(sock, pedidos, MAX_ITEMS, &npe) != 0)  return -1;

    limpiar_menu();
    printf("  ╔══════════════════════════════════════════════════════════╗\n");
    printf("  ║                      PROVEEDORES                         ║\n");
    printf("  ╠══════════════════════════════════════════════════════════╣\n");
    printf("  ║  %-3s %-24s %-26s║\n", "ID", "NOMBRE", "TELEFONO");
    printf("  ╠══════════════════════════════════════════════════════════╣\n");
    if (np == 0) {
        printf("  ║  [i] No hay proveedores registrados.                     ║\n");
    } else {
        for (int i = 0; i < np; i++) {
            printf("  ║  %-3d %-24.24s %-26.26s║\n",
                   provs[i].id_proveedor, provs[i].nombre, provs[i].telefono);
        }
    }
    printf("  ╚══════════════════════════════════════════════════════════╝\n");

    printf("\n  ╔══════════════════════════════════════════════════════════╗\n");
    printf("  ║              PEDIDOS DE REPOSICION                       ║\n");
    printf("  ╠══════════════════════════════════════════════════════════╣\n");
    printf("  ║  %-4s %-11s %-20s %-6s %-7s║\n",
           "ID", "PRODUCTO", "PROVEEDOR", "CANT", "ESTADO");
    printf("  ╠══════════════════════════════════════════════════════════╣\n");
    if (npe == 0) {
        printf("  ║  [i] No hay pedidos de reposicion registrados.           ║\n");
    } else {
        for (int i = 0; i < npe; i++) {
            printf("  ║  %-4d %-11s %-20.20s %-6d %-7s║\n",
                   pedidos[i].id_pedido, pedidos[i].id_producto,
                   pedidos[i].proveedor, pedidos[i].cantidad, pedidos[i].estado);
        }
    }
    printf("  ╠══════════════════════════════════════════════════════════╣\n");
    printf("  ║  Total pedidos: %-41d║\n", npe);
    printf("  ╚══════════════════════════════════════════════════════════╝\n");
    printf("\n  Presione Enter para volver...");
    getchar(); getchar();
    return 0;
}
