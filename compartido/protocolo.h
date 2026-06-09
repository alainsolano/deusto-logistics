#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stdint.h>

/* =========================================================
 * protocolo.h  —  Estructuras binarias compartidas
 * Cliente (C)  <-->  Servidor (C++)
 *
 * Comunicacion por sockets TCP/IP con structs de tamano fijo.
 *
 * MODELO DE MENSAJES
 * ------------------
 * Toda peticion empieza por una CabeceraPeticion (un int con el
 * tipo). Segun el tipo, despues se envia la estructura concreta:
 *
 *   PETICION_LOGIN       -> LoginRequest      ->  LoginResponse
 *   PETICION_REGISTRO    -> RegistroRequest   ->  RegistroResponse
 *   PETICION_MOVIMIENTO  -> MovimientoStock   ->  RespuestaServidor
 *   PETICION_CONSULTA    -> ConsultaRequest   ->  ConsultaResponse
 *   PETICION_HISTORIAL   -> HistorialRequest  ->  int n + n HistorialItem
 *   PETICION_RESUMEN     -> (sin cuerpo)      ->  int n + n ResumenItem
 * ========================================================= */

#define HOST_SERVIDOR    "127.0.0.1"
#define PUERTO_SERVIDOR  8080
#define MAX_CLIENTES     10

/* --- Codigos de respuesta del servidor --- */
#define RESP_OK            0
#define RESP_ERROR         1
#define RESP_STOCK_CRITICO 2

/* --- Tipos de operacion sobre stock --- */
#define OP_ENTRADA       'E'
#define OP_SALIDA        'S'

/* --- Tipos de peticion al servidor (CabeceraPeticion.tipo) --- */
#define PETICION_LOGIN       1
#define PETICION_MOVIMIENTO  2
#define PETICION_CONSULTA    3
#define PETICION_REGISTRO    4
#define PETICION_HISTORIAL   5
#define PETICION_RESUMEN     6
#define PETICION_ALTA_PRODUCTO   7
#define PETICION_BAJA_PRODUCTO   8
#define PETICION_EDITAR_PRODUCTO 9
#define PETICION_ALERTAS         10
#define PETICION_PROVEEDORES     11
#define PETICION_PEDIDOS         12
#define PETICION_LOGOUT          13   /* libera la sesion (sin respuesta) */

/* =========================================================
 * CabeceraPeticion — primer mensaje que identifica el tipo
 * ========================================================= */
typedef struct {
    int32_t tipo;              /* PETICION_* */
} CabeceraPeticion;

/* =========================================================
 * LOGIN
 * ========================================================= */
typedef struct {
    char nombre_usuario[32];
    char contrasena[64];
} LoginRequest;

typedef struct {
    int32_t codigo;            /* RESP_OK | RESP_ERROR */
    char    rol[16];           /* "operario" | "admin" */
    char    nombre_completo[64];
    char    mensaje[64];
} LoginResponse;

/* =========================================================
 * REGISTRO DE OPERARIO
 * ========================================================= */
typedef struct {
    char nombre_usuario[32];
    char contrasena[64];
} RegistroRequest;

typedef struct {
    int32_t codigo;            /* RESP_OK | RESP_ERROR */
    char    mensaje[96];
} RegistroResponse;

/* =========================================================
 * MOVIMIENTO DE STOCK (entrada / salida)
 * ========================================================= */
typedef struct {
    char    id_producto[16];
    int32_t cantidad;
    char    tipo_op;           /* 'E' | 'S' */
    int64_t timestamp;
    char    id_operario[32];
} MovimientoStock;

typedef struct {
    int32_t codigo;            /* RESP_OK | RESP_ERROR | RESP_STOCK_CRITICO */
    int32_t stock_actual;
    char    mensaje[96];
} RespuestaServidor;

/* =========================================================
 * CONSULTA DE PRODUCTO (ficha)
 * ========================================================= */
typedef struct {
    char id_producto[16];
} ConsultaRequest;

typedef struct {
    int32_t codigo;            /* RESP_OK | RESP_ERROR */
    int32_t stock_actual;
    int32_t entradas;
    int32_t salidas;
    int32_t stock_minimo;
    char    nombre[64];
    char    ubicacion[32];
    char    mensaje[64];
} ConsultaResponse;

/* =========================================================
 * HISTORIAL / AUDITORIA
 * ========================================================= */
typedef struct {
    char filtro_producto[16];
    char filtro_operario[32];   /* vacio = cualquier operario */
    char fecha_desde[24];       /* "YYYY-MM-DD" o vacio */
    char fecha_hasta[24];       /* "YYYY-MM-DD" o vacio */
} HistorialRequest;

typedef struct {
    char    timestamp[24];
    char    id_producto[16];
    char    tipo_operacion[10];   /* "entrada" | "salida" */
    int32_t cantidad;
    int32_t stock_resultante;
    char    operario[32];
} HistorialItem;

/* =========================================================
 * RESUMEN GENERAL DE STOCK
 * ========================================================= */
typedef struct {
    char    id_producto[16];
    char    nombre[40];
    int32_t stock;
    int32_t stock_minimo;
    char    estado[12];        /* "SIN STOCK" | "CRITICO" | "BAJO" | "NORMAL" */
} ResumenItem;

/* =========================================================
 * ALTA DE NUEVO PRODUCTO
 * ========================================================= */
typedef struct {
    char id_producto[16];
    char nombre[64];
    char tipo[16];              /* generico | fragil | inflamable | perecedero */

    int32_t cantidad_actual;
    int32_t stock_minimo;
    double precio_unitario;
    char estrategia_salida[8];   /* FIFO | LIFO */
    char ubicacion_almacen[32];

    double coste_embalaje;
    char instrucciones[128];

    int32_t nivel_riesgo;
    char zona_almacenamiento[32];

    char fecha_caducidad[16];
    double temperatura_max;
} AltaProductoRequest;

typedef struct {
    int32_t codigo;              /* RESP_OK | RESP_ERROR */
    char mensaje[96];
} AltaProductoResponse;

/* =========================================================
 * BAJA Y EDICION DE PRODUCTO
 * ========================================================= */
typedef struct {
    char id_producto[16];
} BajaProductoRequest;

typedef struct {
    char    id_producto[16];
    char    nombre[64];
    int32_t stock_minimo;
    double  precio_unitario;
    char    estrategia_salida[8];   /* FIFO | LIFO */
    char    ubicacion_almacen[32];
} EditarProductoRequest;

/* Respuesta generica de operaciones sobre producto (baja / edicion) */
typedef struct {
    int32_t codigo;              /* RESP_OK | RESP_ERROR */
    char    mensaje[96];
} OpProductoResponse;

/* =========================================================
 * ALERTAS (stock bajo + caducidad)
 *   PETICION_ALERTAS -> int n + n AlertaItem
 * ========================================================= */
typedef struct {
    char    id_producto[16];
    char    nombre[40];
    int32_t stock;
    int32_t stock_minimo;
    char    tipo_alerta[16];     /* STOCK_BAJO | SIN_STOCK | CADUCA_PRONTO | CADUCADO */
    char    detalle[32];         /* fecha caducidad / dias, o "-" */
} AlertaItem;

/* =========================================================
 * PROVEEDORES
 *   PETICION_PROVEEDORES -> int n + n ProveedorItem
 * ========================================================= */
typedef struct {
    int32_t id_proveedor;
    char    nombre[64];
    char    contacto[64];
    char    telefono[24];
} ProveedorItem;

/* =========================================================
 * PEDIDOS DE REPOSICION
 *   PETICION_PEDIDOS -> int n + n PedidoItem
 * ========================================================= */
typedef struct {
    int32_t id_pedido;
    char    id_producto[16];
    char    proveedor[64];
    int32_t cantidad;
    char    fecha[24];
    char    estado[12];          /* pendiente | recibido | cancelado */
} PedidoItem;

#endif
