#include "red.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CERRAR_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define CERRAR_SOCKET close
#endif

/* Envio / recepcion completos.
   Devuelven 0 si OK, -1 si la conexion se cae.  */
static int enviar_todo(socket_t s, const void *buf, int len) {
    const char *p = (const char *)buf;
    int enviados = 0;
    while (enviados < len) {
        int n = (int)send(s, p + enviados, len - enviados, 0);
        if (n <= 0) return -1;
        enviados += n;
    }
    return 0;
}

static int recibir_todo(socket_t s, void *buf, int len) {
    char *p = (char *)buf;
    int recibidos = 0;
    while (recibidos < len) {
        int n = (int)recv(s, p + recibidos, len - recibidos, 0);
        if (n <= 0) return -1;
        recibidos += n;
    }
    return 0;
}

int red_iniciar(void) {
#ifdef _WIN32
    WSADATA wsa;
    return (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) ? 0 : -1;
#else
    return 0;
#endif
}

void red_finalizar(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

socket_t red_conectar(const char *host, int puerto) {
    socket_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == SOCKET_INVALIDO) {
        return SOCKET_INVALIDO;
    }

    struct sockaddr_in dir;
    memset(&dir, 0, sizeof(dir));
    dir.sin_family = AF_INET;
    dir.sin_port   = htons((unsigned short)puerto);

    if (inet_pton(AF_INET, host, &dir.sin_addr) != 1) {
        CERRAR_SOCKET(s);
        return SOCKET_INVALIDO;
    }

    if (connect(s, (struct sockaddr *)&dir, sizeof(dir)) != 0) {
        CERRAR_SOCKET(s);
        return SOCKET_INVALIDO;
    }

    return s;
}

void red_cerrar(socket_t s) {
    if (s != SOCKET_INVALIDO) {
        CERRAR_SOCKET(s);
    }
}

/* Envia una cabecera con el tipo de peticion. */
static int enviar_cabecera(socket_t s, int tipo) {
    CabeceraPeticion cab;
    cab.tipo = (int32_t)tipo;
    return enviar_todo(s, &cab, sizeof(cab));
}

/* ── LOGIN ── */
int red_login(socket_t s, const char *usuario, const char *pass,
              LoginResponse *resp) {
    LoginRequest req;
    memset(&req, 0, sizeof(req));
    strncpy(req.nombre_usuario, usuario, sizeof(req.nombre_usuario) - 1);
    strncpy(req.contrasena,     pass,    sizeof(req.contrasena) - 1);

    if (enviar_cabecera(s, PETICION_LOGIN) != 0) return -1;
    if (enviar_todo(s, &req, sizeof(req)) != 0)  return -1;
    if (recibir_todo(s, resp, sizeof(*resp)) != 0) return -1;
    return 0;
}

/* ── LOGOUT ── (solo cabecera, sin respuesta) */
int red_logout(socket_t s) {
    return enviar_cabecera(s, PETICION_LOGOUT);
}

/* ── REGISTRO ── */
int red_registro(socket_t s, const char *usuario, const char *pass,
                 RegistroResponse *resp) {
    RegistroRequest req;
    memset(&req, 0, sizeof(req));
    strncpy(req.nombre_usuario, usuario, sizeof(req.nombre_usuario) - 1);
    strncpy(req.contrasena,     pass,    sizeof(req.contrasena) - 1);

    if (enviar_cabecera(s, PETICION_REGISTRO) != 0) return -1;
    if (enviar_todo(s, &req, sizeof(req)) != 0)     return -1;
    if (recibir_todo(s, resp, sizeof(*resp)) != 0)  return -1;
    return 0;
}

/* ── MOVIMIENTO ── */
int red_movimiento(socket_t s, const MovimientoStock *mov,
                   RespuestaServidor *resp) {
    if (enviar_cabecera(s, PETICION_MOVIMIENTO) != 0) return -1;
    if (enviar_todo(s, mov, sizeof(*mov)) != 0)       return -1;
    if (recibir_todo(s, resp, sizeof(*resp)) != 0)    return -1;
    return 0;
}

/* ── CONSULTA ── */
int red_consulta(socket_t s, const char *id_producto,
                 ConsultaResponse *resp) {
    ConsultaRequest req;
    memset(&req, 0, sizeof(req));
    strncpy(req.id_producto, id_producto, sizeof(req.id_producto) - 1);

    if (enviar_cabecera(s, PETICION_CONSULTA) != 0) return -1;
    if (enviar_todo(s, &req, sizeof(req)) != 0)     return -1;
    if (recibir_todo(s, resp, sizeof(*resp)) != 0)  return -1;
    return 0;
}

/* ── HISTORIAL ── */
int red_historial(socket_t s, const char *filtro_producto,
                  const char *filtro_operario,
                  const char *fecha_desde, const char *fecha_hasta,
                  HistorialItem *items, int max_items, int *n_items) {
    HistorialRequest req;
    memset(&req, 0, sizeof(req));
    if (filtro_producto != NULL)
        strncpy(req.filtro_producto, filtro_producto, sizeof(req.filtro_producto) - 1);
    if (filtro_operario != NULL)
        strncpy(req.filtro_operario, filtro_operario, sizeof(req.filtro_operario) - 1);
    if (fecha_desde != NULL)
        strncpy(req.fecha_desde, fecha_desde, sizeof(req.fecha_desde) - 1);
    if (fecha_hasta != NULL)
        strncpy(req.fecha_hasta, fecha_hasta, sizeof(req.fecha_hasta) - 1);

    if (enviar_cabecera(s, PETICION_HISTORIAL) != 0) return -1;
    if (enviar_todo(s, &req, sizeof(req)) != 0)      return -1;

    int32_t n = 0;
    if (recibir_todo(s, &n, sizeof(n)) != 0) return -1;

    int recibidos = 0;
    for (int32_t i = 0; i < n; i++) {
        HistorialItem item;
        if (recibir_todo(s, &item, sizeof(item)) != 0) return -1;
        if (recibidos < max_items) {
            items[recibidos++] = item;
        }
    }

    *n_items = recibidos;
    return 0;
}

/* ── RESUMEN ── */
int red_resumen(socket_t s, ResumenItem *items, int max_items, int *n_items) {
    if (enviar_cabecera(s, PETICION_RESUMEN) != 0) return -1;

    int32_t n = 0;
    if (recibir_todo(s, &n, sizeof(n)) != 0) return -1;

    int recibidos = 0;
    for (int32_t i = 0; i < n; i++) {
        ResumenItem item;
        if (recibir_todo(s, &item, sizeof(item)) != 0) return -1;
        if (recibidos < max_items) {
            items[recibidos++] = item;
        }
    }

    *n_items = recibidos;
    return 0;
}

int red_alta_producto(socket_t s, const AltaProductoRequest *req, AltaProductoResponse *resp) {
    if (enviar_cabecera(s, PETICION_ALTA_PRODUCTO) != 0) {
        return -1;
    }

    if (enviar_todo(s, req, sizeof(*req)) != 0) {
        return -1;
    }

    if (recibir_todo(s, resp, sizeof(*resp)) != 0) {
        return -1;
    }

    return 0;
}

/* ── BAJA / EDICION ── */
int red_baja_producto(socket_t s, const char *id_producto, OpProductoResponse *resp) {
    BajaProductoRequest req;
    memset(&req, 0, sizeof(req));
    strncpy(req.id_producto, id_producto, sizeof(req.id_producto) - 1);

    if (enviar_cabecera(s, PETICION_BAJA_PRODUCTO) != 0) return -1;
    if (enviar_todo(s, &req, sizeof(req)) != 0)          return -1;
    if (recibir_todo(s, resp, sizeof(*resp)) != 0)       return -1;
    return 0;
}

int red_editar_producto(socket_t s, const EditarProductoRequest *req, OpProductoResponse *resp) {
    if (enviar_cabecera(s, PETICION_EDITAR_PRODUCTO) != 0) return -1;
    if (enviar_todo(s, req, sizeof(*req)) != 0)            return -1;
    if (recibir_todo(s, resp, sizeof(*resp)) != 0)         return -1;
    return 0;
}

/* Recepcion generica de una lista: int32_t n + n elementos de 'elem_size'.
   Los que excedan 'max_items' se reciben en un buffer de descarte. */
static int recibir_lista(socket_t s, void *items, int elem_size,
                         int max_items, int *n_items) {
    int32_t n = 0;
    if (recibir_todo(s, &n, sizeof(n)) != 0) return -1;

    int recibidos = 0;
    char *base = (char *)items;
    char descarte[512];
    for (int32_t i = 0; i < n; i++) {
        void *dst = (recibidos < max_items)
                        ? (void *)(base + (size_t)recibidos * elem_size)
                        : (void *)descarte;
        if (recibir_todo(s, dst, elem_size) != 0) return -1;
        if (recibidos < max_items) recibidos++;
    }
    *n_items = recibidos;
    return 0;
}

/* ── LISTAS: alertas / proveedores / pedidos ── */
int red_alertas(socket_t s, AlertaItem *items, int max_items, int *n_items) {
    if (enviar_cabecera(s, PETICION_ALERTAS) != 0) return -1;
    return recibir_lista(s, items, sizeof(AlertaItem), max_items, n_items);
}

int red_proveedores(socket_t s, ProveedorItem *items, int max_items, int *n_items) {
    if (enviar_cabecera(s, PETICION_PROVEEDORES) != 0) return -1;
    return recibir_lista(s, items, sizeof(ProveedorItem), max_items, n_items);
}

int red_pedidos(socket_t s, PedidoItem *items, int max_items, int *n_items) {
    if (enviar_cabecera(s, PETICION_PEDIDOS) != 0) return -1;
    return recibir_lista(s, items, sizeof(PedidoItem), max_items, n_items);
}
