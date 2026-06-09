/* =========================================================
 * test_db.cpp  —  Pruebas del servidor (capa de BD)
 *
 * Binario independiente: enlaza db_manager + clases Producto
 * (NO main.cpp). Crea una BD temporal, le aplica datos/schema.sql
 * y ejercita la API db_* directamente, sin sockets.
 *
 * Ejecutar con:  make test
 * ========================================================= */
#include "db_manager.h"
#include <sqlite3.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <thread>

/* ── Mini-harness de asserts ── */
static int g_total = 0;
static int g_fallos = 0;

static void check(bool cond, const char* nombre) {
    g_total++;
    if (cond) {
        printf("  [OK]   %s\n", nombre);
    } else {
        g_fallos++;
        printf("  [FALLO] %s\n", nombre);
    }
}

/* ── Utilidades ── */
static bool aplicar_schema(const char* db_path, const char* schema_path) {
    FILE* f = fopen(schema_path, "rb");
    if (!f) {
        printf("  [ERROR] No se pudo abrir %s\n", schema_path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string contenido(sz, '\0');
    size_t leidos = fread(&contenido[0], 1, sz, f);
    fclose(f);
    contenido.resize(leidos);

    sqlite3* tmp = nullptr;
    if (sqlite3_open(db_path, &tmp) != SQLITE_OK) return false;
    char* err = nullptr;
    int rc = sqlite3_exec(tmp, contenido.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        printf("  [ERROR] schema.sql: %s\n", err ? err : "?");
        sqlite3_free(err);
    }
    sqlite3_close(tmp);
    return rc == SQLITE_OK;
}

static AltaProductoRequest alta_generico(const char* id, const char* nombre,
                                         int stock, int min, double precio) {
    AltaProductoRequest r;
    memset(&r, 0, sizeof(r));
    strncpy(r.id_producto, id, sizeof(r.id_producto) - 1);
    strncpy(r.nombre, nombre, sizeof(r.nombre) - 1);
    strncpy(r.tipo, "generico", sizeof(r.tipo) - 1);
    r.cantidad_actual = stock;
    r.stock_minimo = min;
    r.precio_unitario = precio;
    strncpy(r.estrategia_salida, "FIFO", sizeof(r.estrategia_salida) - 1);
    strncpy(r.ubicacion_almacen, "T-00", sizeof(r.ubicacion_almacen) - 1);
    return r;
}

static MovimientoStock movimiento(const char* id, int cant, char tipo) {
    MovimientoStock m;
    memset(&m, 0, sizeof(m));
    strncpy(m.id_producto, id, sizeof(m.id_producto) - 1);
    strncpy(m.id_operario, "admin", sizeof(m.id_operario) - 1);
    m.cantidad = cant;
    m.tipo_op = tipo;
    return m;
}

/* ── main ── */
int main() {
    const char* DB = "/tmp/test_deusto.db";
    remove(DB);
    remove("/tmp/test_deusto.db-wal");
    remove("/tmp/test_deusto.db-shm");

    printf("===== TESTS SERVIDOR (capa BD) =====\n");

    if (!aplicar_schema(DB, "datos/schema.sql")) {
        printf("No se pudo preparar la BD de pruebas. Abortando.\n");
        return 1;
    }
    if (!db_init(DB)) {
        printf("db_init fallo. Abortando.\n");
        return 1;
    }

    /* --- LOGIN --- */
    {
        LoginRequest req; memset(&req, 0, sizeof(req));
        strncpy(req.nombre_usuario, "admin", sizeof(req.nombre_usuario) - 1);
        strncpy(req.contrasena, "admin", sizeof(req.contrasena) - 1);
        LoginResponse resp;
        db_login(req, resp);
        check(resp.codigo == RESP_OK, "login admin/admin correcto");
        check(strcmp(resp.rol, "admin") == 0, "login devuelve rol admin");

        strncpy(req.contrasena, "malo", sizeof(req.contrasena) - 1);
        db_login(req, resp);
        check(resp.codigo == RESP_ERROR, "login con contrasena erronea rechazado");
    }

    /* --- ALTA --- */
    {
        AltaProductoRequest a = alta_generico("PROD-TEST", "Test", 50, 10, 5.0);
        AltaProductoResponse resp;
        db_alta_producto(a, resp);
        check(resp.codigo == RESP_OK, "alta producto nuevo");
        db_alta_producto(a, resp);
        check(resp.codigo == RESP_ERROR, "alta producto duplicado rechazada");
    }

    /* --- MOVIMIENTOS --- */
    {
        RespuestaServidor resp;
        db_movimiento(movimiento("PROD-TEST", 10, OP_ENTRADA), resp);
        check(resp.codigo == RESP_OK && resp.stock_actual == 60, "entrada suma stock (60)");
        db_movimiento(movimiento("PROD-TEST", 5, OP_SALIDA), resp);
        check(resp.codigo == RESP_OK && resp.stock_actual == 55, "salida resta stock (55)");
        db_movimiento(movimiento("PROD-TEST", 1000, OP_SALIDA), resp);
        check(resp.codigo == RESP_ERROR, "salida con stock insuficiente rechazada");
    }

    /* --- EDICION --- */
    {
        EditarProductoRequest e; memset(&e, 0, sizeof(e));
        strncpy(e.id_producto, "PROD-TEST", sizeof(e.id_producto) - 1);
        strncpy(e.nombre, "Test Editado", sizeof(e.nombre) - 1);
        e.stock_minimo = 20;
        e.precio_unitario = 9.0;
        strncpy(e.estrategia_salida, "LIFO", sizeof(e.estrategia_salida) - 1);
        strncpy(e.ubicacion_almacen, "T-02", sizeof(e.ubicacion_almacen) - 1);
        OpProductoResponse resp;
        db_editar_producto(e, resp);
        check(resp.codigo == RESP_OK, "edicion de producto correcta");

        ConsultaRequest c; memset(&c, 0, sizeof(c));
        strncpy(c.id_producto, "PROD-TEST", sizeof(c.id_producto) - 1);
        ConsultaResponse cr;
        db_consulta(c, cr);
        check(cr.codigo == RESP_OK && cr.stock_minimo == 20,
              "consulta refleja nuevo stock_minimo (20)");
        check(strcmp(cr.nombre, "Test Editado") == 0, "consulta refleja nuevo nombre");
    }

    /* --- BAJA LOGICA (conserva historial) --- */
    {
        BajaProductoRequest b; memset(&b, 0, sizeof(b));
        strncpy(b.id_producto, "PROD-TEST", sizeof(b.id_producto) - 1);
        OpProductoResponse resp;
        db_baja_producto(b, resp);
        check(resp.codigo == RESP_OK, "baja de producto correcta");

        ConsultaRequest c; memset(&c, 0, sizeof(c));
        strncpy(c.id_producto, "PROD-TEST", sizeof(c.id_producto) - 1);
        ConsultaResponse cr;
        db_consulta(c, cr);
        check(cr.codigo == RESP_ERROR, "producto dado de baja no es consultable");

        std::vector<ResumenItem> resumen;
        db_resumen(resumen);
        bool en_resumen = false;
        for (auto& it : resumen) if (strcmp(it.id_producto, "PROD-TEST") == 0) en_resumen = true;
        check(!en_resumen, "producto dado de baja no aparece en el resumen");

        HistorialRequest h; memset(&h, 0, sizeof(h));
        strncpy(h.filtro_producto, "PROD-TEST", sizeof(h.filtro_producto) - 1);
        std::vector<HistorialItem> hist;
        db_historial(h, hist);
        check(hist.size() >= 2, "historial del producto se conserva tras la baja");
    }

    /* --- FILTROS DE HISTORIAL --- */
    {
        HistorialRequest h; memset(&h, 0, sizeof(h));
        strncpy(h.filtro_operario, "admin", sizeof(h.filtro_operario) - 1);
        std::vector<HistorialItem> hist;
        db_historial(h, hist);
        check(hist.size() >= 1, "historial filtrado por operario 'admin'");

        HistorialRequest h2; memset(&h2, 0, sizeof(h2));
        strncpy(h2.fecha_desde, "2999-01-01", sizeof(h2.fecha_desde) - 1);
        std::vector<HistorialItem> hist2;
        db_historial(h2, hist2);
        check(hist2.empty(), "historial con fecha_desde futura devuelve 0");
    }

    /* --- AUTO-REPOSICION --- (producto propio, independiente del seed) */
    {
        AltaProductoRequest a = alta_generico("PROD-REPO", "Reposicion", 10, 5, 4.0);
        AltaProductoResponse ar;
        db_alta_producto(a, ar);

        /* Salida que deja el stock en 3 (< min 5) */
        RespuestaServidor resp;
        db_movimiento(movimiento("PROD-REPO", 7, OP_SALIDA), resp);
        check(resp.codigo == RESP_OK && resp.stock_actual == 3, "salida deja stock bajo minimo");

        std::vector<PedidoItem> pedidos;
        db_pedidos(pedidos);
        int n_repo = 0;
        for (auto& p : pedidos) if (strcmp(p.id_producto, "PROD-REPO") == 0) n_repo++;
        check(n_repo == 1, "auto-reposicion genera 1 pedido pendiente");

        /* Otra salida no debe duplicar el pedido pendiente */
        db_movimiento(movimiento("PROD-REPO", 1, OP_SALIDA), resp);
        std::vector<PedidoItem> pedidos2;
        db_pedidos(pedidos2);
        int n2 = 0;
        for (auto& p : pedidos2)
            if (strcmp(p.id_producto, "PROD-REPO") == 0 && strcmp(p.estado, "pendiente") == 0) n2++;
        check(n2 == 1, "no se duplica el pedido si ya hay uno pendiente");
    }

    /* --- ALERTAS (stock bajo + caducidad) --- */
    {
        std::vector<AlertaItem> alertas;
        db_alertas(alertas);
        bool stock_bajo = false;
        for (auto& a : alertas)
            if (strcmp(a.id_producto, "PROD-REPO") == 0) stock_bajo = true;
        check(stock_bajo, "alertas incluye producto con stock bajo");

        /* Perecedero ya caducado -> alerta CADUCADO */
        AltaProductoRequest a; memset(&a, 0, sizeof(a));
        strncpy(a.id_producto, "PROD-CAD", sizeof(a.id_producto) - 1);
        strncpy(a.nombre, "Caducado", sizeof(a.nombre) - 1);
        strncpy(a.tipo, "perecedero", sizeof(a.tipo) - 1);
        a.cantidad_actual = 10; a.stock_minimo = 1; a.precio_unitario = 2.0;
        strncpy(a.estrategia_salida, "FIFO", sizeof(a.estrategia_salida) - 1);
        strncpy(a.ubicacion_almacen, "C-99", sizeof(a.ubicacion_almacen) - 1);
        strncpy(a.fecha_caducidad, "2000-01-01", sizeof(a.fecha_caducidad) - 1);
        a.temperatura_max = 5.0;
        AltaProductoResponse ar;
        db_alta_producto(a, ar);

        std::vector<AlertaItem> alertas2;
        db_alertas(alertas2);
        bool caducado = false;
        for (auto& al : alertas2)
            if (strcmp(al.id_producto, "PROD-CAD") == 0 &&
                strcmp(al.tipo_alerta, "CADUCADO") == 0) caducado = true;
        check(caducado, "alertas incluye perecedero caducado (CADUCADO)");
    }

    /* --- CONCURRENCIA (valida g_db_mutex) --- */
    {
        AltaProductoRequest a = alta_generico("PROD-CONC", "Concurrencia", 0, 1, 1.0);
        AltaProductoResponse ar;
        db_alta_producto(a, ar);

        const int N_HILOS = 8;
        const int N_OPS = 50;
        std::vector<std::thread> hilos;
        for (int t = 0; t < N_HILOS; t++) {
            hilos.emplace_back([&]() {
                for (int i = 0; i < N_OPS; i++) {
                    RespuestaServidor r;
                    db_movimiento(movimiento("PROD-CONC", 1, OP_ENTRADA), r);
                }
            });
        }
        for (auto& h : hilos) h.join();

        ConsultaRequest c; memset(&c, 0, sizeof(c));
        strncpy(c.id_producto, "PROD-CONC", sizeof(c.id_producto) - 1);
        ConsultaResponse cr;
        db_consulta(c, cr);
        check(cr.stock_actual == N_HILOS * N_OPS,
              "sin perdida de actualizaciones bajo concurrencia (stock == 400)");
    }

    db_close();

    printf("=====================================\n");
    printf("Resultado: %d/%d pruebas OK", g_total - g_fallos, g_total);
    if (g_fallos == 0) printf("  ✓ TODO CORRECTO\n");
    else               printf("  ✗ %d FALLO(S)\n", g_fallos);

    return g_fallos == 0 ? 0 : 1;
}
