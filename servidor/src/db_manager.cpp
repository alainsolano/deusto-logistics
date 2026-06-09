#include "db_manager.h"
#include <iostream>
#include <sqlite3.h>
#include <cstring>
#include <string>
#include <mutex>
#include "productoGenerico.h"
#include "productoFragil.h"
#include "productoInflamable.h"
#include "producto_perecedero.h"

#include <memory>
#include <iomanip>

static sqlite3* db = nullptr;
static std::mutex g_db_mutex;  

static void copiar(char* dst, size_t dst_size, const char* src) {
    if (!src) src = "";
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static int obtener_id_usuario(const char* nombre_usuario) {
    const char* sql = "SELECT id_usuario FROM USUARIOS WHERE nombre_usuario = ?;";
    sqlite3_stmt* stmt = nullptr;
    int id_usuario = -1;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }
    sqlite3_bind_text(stmt, 1, nombre_usuario, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id_usuario = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id_usuario;
}

/* Devuelve true si el producto existe y rellena datos opcionales. */
static bool obtener_producto(const char* id_producto, int& stock,
                             int& stock_minimo, std::string& nombre,
                             std::string& ubicacion) {
    const char* sql =
        "SELECT cantidad_actual, stock_minimo, nombre, COALESCE(ubicacion_almacen,'') "
        "FROM PRODUCTOS WHERE id_producto = ? AND activo = 1;";
    sqlite3_stmt* stmt = nullptr;
    bool encontrado = false;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, id_producto, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        encontrado   = true;
        stock        = sqlite3_column_int(stmt, 0);
        stock_minimo = sqlite3_column_int(stmt, 1);
        const unsigned char* nom = sqlite3_column_text(stmt, 2);
        const unsigned char* ubi = sqlite3_column_text(stmt, 3);
        nombre    = nom ? reinterpret_cast<const char*>(nom) : "";
        ubicacion = ubi ? reinterpret_cast<const char*>(ubi) : "";
    }
    sqlite3_finalize(stmt);
    return encontrado;
}

static void sumar_movimientos(const char* id_producto, int& entradas, int& salidas) {
    const char* sql =
        "SELECT "
        "COALESCE(SUM(CASE WHEN tipo_operacion='entrada' THEN cantidad ELSE 0 END),0), "
        "COALESCE(SUM(CASE WHEN tipo_operacion='salida'  THEN cantidad ELSE 0 END),0) "
        "FROM HISTORIAL_MOVIMIENTOS WHERE id_producto = ?;";
    sqlite3_stmt* stmt = nullptr;
    entradas = 0;
    salidas  = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }
    sqlite3_bind_text(stmt, 1, id_producto, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        entradas = sqlite3_column_int(stmt, 0);
        salidas  = sqlite3_column_int(stmt, 1);
    }
    sqlite3_finalize(stmt);
}

static const char* calcular_estado(int stock, int stock_minimo) {
    if (stock <= 0)                  return "SIN STOCK";
    if (stock <  stock_minimo)       return "CRITICO";
    if (stock <  stock_minimo * 2)   return "BAJO";
    return "NORMAL";
}

/* Crea un pedido de reposicion automatico si el stock quedo bajo el minimo y
 * no hay ya un pedido pendiente para ese producto. NO bloquea el mutex: debe
 * llamarse desde una funcion que ya lo tenga tomado (p. ej. db_movimiento) y
 * dentro de su transaccion. Devuelve true si creo un pedido. */
static bool crear_pedido_reposicion(const char* id_producto,
                                    int stock_resultante, int stock_minimo) {
    /* Ya hay uno pendiente? */
    const char* sql_chk =
        "SELECT COUNT(*) FROM PEDIDOS_REPOSICION "
        "WHERE id_producto = ? AND estado = 'pendiente';";
    sqlite3_stmt* stmt = nullptr;
    int pendientes = 0;
    if (sqlite3_prepare_v2(db, sql_chk, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, id_producto, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) pendientes = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    if (pendientes > 0) return false;

    int cantidad = stock_minimo * 2 - stock_resultante;
    if (cantidad < 1) cantidad = stock_minimo > 0 ? stock_minimo : 1;

    /* id_proveedor se toma del propio producto mediante subconsulta */
    const char* sql_ins =
        "INSERT INTO PEDIDOS_REPOSICION (id_producto, id_proveedor, cantidad) "
        "VALUES (?, (SELECT id_proveedor FROM PRODUCTOS WHERE id_producto = ?), ?);";
    bool creado = false;
    if (sqlite3_prepare_v2(db, sql_ins, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, id_producto, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, id_producto, -1, SQLITE_STATIC);
        sqlite3_bind_int (stmt, 3, cantidad);
        creado = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return creado;
}

/* Devuelve true si 'tabla' tiene una columna llamada 'col'. */
static bool columna_existe(const char* tabla, const char* col) {
    std::string sql = "PRAGMA table_info(" + std::string(tabla) + ");";
    sqlite3_stmt* stmt = nullptr;
    bool existe = false;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* nombre = sqlite3_column_text(stmt, 1); /* col 1 = name */
        if (nombre && std::string(reinterpret_cast<const char*>(nombre)) == col) {
            existe = true;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return existe;
}

/* Migracion idempotente: crea tablas/columnas nuevas sobre una BD existente
 * sin perder datos. Seguro de ejecutar en cada arranque. */
static void db_migrar() {
    /* Tablas nuevas (no rompen instalaciones antiguas) */
    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS PROVEEDORES ("
        " id_proveedor INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nombre TEXT NOT NULL,"
        " contacto TEXT,"
        " telefono TEXT,"
        " activo INTEGER NOT NULL DEFAULT 1 CHECK (activo IN (0,1)));",
        nullptr, nullptr, nullptr);

    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS PEDIDOS_REPOSICION ("
        " id_pedido INTEGER PRIMARY KEY AUTOINCREMENT,"
        " id_producto TEXT NOT NULL,"
        " id_proveedor INTEGER,"
        " cantidad INTEGER NOT NULL CHECK (cantidad > 0),"
        " fecha TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        " estado TEXT NOT NULL DEFAULT 'pendiente' "
        "   CHECK (estado IN ('pendiente','recibido','cancelado')),"
        " FOREIGN KEY (id_producto) REFERENCES PRODUCTOS(id_producto),"
        " FOREIGN KEY (id_proveedor) REFERENCES PROVEEDORES(id_proveedor));",
        nullptr, nullptr, nullptr);

    /* Columnas nuevas en PRODUCTOS (SQLite no tiene ADD COLUMN IF NOT EXISTS) */
    if (!columna_existe("PRODUCTOS", "activo")) {
        sqlite3_exec(db,
            "ALTER TABLE PRODUCTOS ADD COLUMN activo INTEGER NOT NULL DEFAULT 1;",
            nullptr, nullptr, nullptr);
    }
    if (!columna_existe("PRODUCTOS", "id_proveedor")) {
        sqlite3_exec(db,
            "ALTER TABLE PRODUCTOS ADD COLUMN id_proveedor INTEGER;",
            nullptr, nullptr, nullptr);
    }

    /* Seed de proveedores si la tabla esta vacia (BD ya existente) */
    sqlite3_stmt* stmt = nullptr;
    int n_prov = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM PROVEEDORES;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) n_prov = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    if (n_prov == 0) {
        sqlite3_exec(db,
            "INSERT INTO PROVEEDORES (nombre, contacto, telefono) VALUES "
            "('Suministros Bilbao S.L.', 'ventas@sumbilbao.es', '944111222'),"
            "('Iberica de Almacenaje', 'pedidos@iberalmacen.es', '915333444'),"
            "('FrioNorte Distribucion', 'comercial@frionorte.es', '946555666');",
            nullptr, nullptr, nullptr);
        /* Asignar un proveedor por defecto a los productos que no tengan */
        sqlite3_exec(db,
            "UPDATE PRODUCTOS SET id_proveedor = 1 WHERE id_proveedor IS NULL;",
            nullptr, nullptr, nullptr);
    }
}

/* API publica */
bool db_init(const char* ruta) {
    if (sqlite3_open(ruta, &db) != SQLITE_OK) {
        std::cerr << "Error abriendo BD: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_busy_timeout(db, 3000);
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
    db_migrar();
    std::cout << "[BD] Archivo abierto: " << ruta << std::endl;
    return true;
}

void db_close() {
    std::lock_guard<std::mutex> lock(g_db_mutex);
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

/* ── LOGIN ── */
void db_login(const LoginRequest& req, LoginResponse& resp) {
    std::lock_guard<std::mutex> lock(g_db_mutex);
    memset(&resp, 0, sizeof(resp));

    const char* sql =
        "SELECT rol, activo FROM USUARIOS "
        "WHERE nombre_usuario = ? AND hash_contrasena = ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        resp.codigo = RESP_ERROR;
        copiar(resp.mensaje, sizeof(resp.mensaje), "Error interno del servidor");
        return;
    }
    sqlite3_bind_text(stmt, 1, req.nombre_usuario, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, req.contrasena,     -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* rol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        int activo = sqlite3_column_int(stmt, 1);
        if (activo == 1) {
            resp.codigo = RESP_OK;
            copiar(resp.rol, sizeof(resp.rol), rol);
            copiar(resp.nombre_completo, sizeof(resp.nombre_completo), req.nombre_usuario);
            copiar(resp.mensaje, sizeof(resp.mensaje), "Acceso concedido");
        } else {
            resp.codigo = RESP_ERROR;
            copiar(resp.mensaje, sizeof(resp.mensaje), "Usuario desactivado");
        }
    } else {
        resp.codigo = RESP_ERROR;
        copiar(resp.mensaje, sizeof(resp.mensaje), "Usuario o contrasena incorrectos");
    }
    sqlite3_finalize(stmt);
}

/* ── REGISTRO ── */
void db_registro(const RegistroRequest& req, RegistroResponse& resp) {
    std::lock_guard<std::mutex> lock(g_db_mutex);
    memset(&resp, 0, sizeof(resp));

    if (strlen(req.nombre_usuario) == 0 || strlen(req.contrasena) == 0) {
        resp.codigo = RESP_ERROR;
        copiar(resp.mensaje, sizeof(resp.mensaje), "Usuario y contrasena obligatorios");
        return;
    }
    if (strlen(req.contrasena) < 4) {
        resp.codigo = RESP_ERROR;
        copiar(resp.mensaje, sizeof(resp.mensaje), "La contrasena debe tener 4+ caracteres");
        return;
    }

    const char* sql =
        "INSERT INTO USUARIOS (nombre_usuario, hash_contrasena, rol) "
        "VALUES (?, ?, 'operario');";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        resp.codigo = RESP_ERROR;
        copiar(resp.mensaje, sizeof(resp.mensaje), "Error interno del servidor");
        return;
    }
    sqlite3_bind_text(stmt, 1, req.nombre_usuario, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, req.contrasena,     -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        resp.codigo = RESP_OK;
        snprintf(resp.mensaje, sizeof(resp.mensaje),
                 "Operario '%s' registrado", req.nombre_usuario);
    } else if (rc == SQLITE_CONSTRAINT) {
        resp.codigo = RESP_ERROR;
        copiar(resp.mensaje, sizeof(resp.mensaje), "El usuario ya existe");
    } else {
        resp.codigo = RESP_ERROR;
        copiar(resp.mensaje, sizeof(resp.mensaje), "No se pudo registrar el operario");
    }
}

/* ── MOVIMIENTO (entrada / salida) ── */
void db_movimiento(const MovimientoStock& mov, RespuestaServidor& resp) {
    std::lock_guard<std::mutex> lock(g_db_mutex);
    memset(&resp, 0, sizeof(resp));

    if (mov.cantidad <= 0) {
        resp.codigo = RESP_ERROR;
        copiar(resp.mensaje, sizeof(resp.mensaje), "Cantidad invalida");
        return;
    }
    if (mov.tipo_op != OP_ENTRADA && mov.tipo_op != OP_SALIDA) {
        resp.codigo = RESP_ERROR;
        copiar(resp.mensaje, sizeof(resp.mensaje), "Tipo de operacion no valido");
        return;
    }

    int id_usuario = obtener_id_usuario(mov.id_operario);
    if (id_usuario == -1) {
        resp.codigo = RESP_ERROR;
        copiar(resp.mensaje, sizeof(resp.mensaje), "Operario no reconocido");
        return;
    }

    int stock_actual = 0, stock_minimo = 0;
    std::string nombre, ubicacion;
    if (!obtener_producto(mov.id_producto, stock_actual, stock_minimo, nombre, ubicacion)) {
        resp.codigo = RESP_ERROR;
        resp.stock_actual = 0;
        copiar(resp.mensaje, sizeof(resp.mensaje), "Producto no encontrado");
        return;
    }

    int stock_resultante;
    if (mov.tipo_op == OP_ENTRADA) {
        stock_resultante = stock_actual + mov.cantidad;
    } else {
        if (stock_actual < mov.cantidad) {
            resp.codigo = RESP_ERROR;
            resp.stock_actual = stock_actual;
            copiar(resp.mensaje, sizeof(resp.mensaje), "Stock insuficiente");
            return;
        }
        stock_resultante = stock_actual - mov.cantidad;
    }

    /* Transaccion: actualizar PRODUCTOS + insertar en HISTORIAL */
    sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);

    const char* sql_update =
        "UPDATE PRODUCTOS SET cantidad_actual = ? WHERE id_producto = ?;";
    sqlite3_stmt* stmt_u = nullptr;
    bool ok = sqlite3_prepare_v2(db, sql_update, -1, &stmt_u, nullptr) == SQLITE_OK;
    if (ok) {
        sqlite3_bind_int(stmt_u, 1, stock_resultante);
        sqlite3_bind_text(stmt_u, 2, mov.id_producto, -1, SQLITE_STATIC);
        ok = (sqlite3_step(stmt_u) == SQLITE_DONE);
    }
    sqlite3_finalize(stmt_u);

    if (ok) {
        const char* sql_insert =
            "INSERT INTO HISTORIAL_MOVIMIENTOS "
            "(id_producto, id_usuario, tipo_operacion, cantidad, stock_resultante, origen) "
            "VALUES (?, ?, ?, ?, ?, 'manual');";
        sqlite3_stmt* stmt_i = nullptr;
        ok = sqlite3_prepare_v2(db, sql_insert, -1, &stmt_i, nullptr) == SQLITE_OK;
        if (ok) {
            const char* tipo = (mov.tipo_op == OP_ENTRADA) ? "entrada" : "salida";
            sqlite3_bind_text(stmt_i, 1, mov.id_producto, -1, SQLITE_STATIC);
            sqlite3_bind_int (stmt_i, 2, id_usuario);
            sqlite3_bind_text(stmt_i, 3, tipo, -1, SQLITE_STATIC);
            sqlite3_bind_int (stmt_i, 4, mov.cantidad);
            sqlite3_bind_int (stmt_i, 5, stock_resultante);
            ok = (sqlite3_step(stmt_i) == SQLITE_DONE);
        }
        sqlite3_finalize(stmt_i);
    }

    /* Auto-reposicion: si una salida deja el stock bajo el minimo, generar
     * pedido de reposicion automatico (dentro de la misma transaccion). */
    bool pedido_creado = false;
    if (ok && mov.tipo_op == OP_SALIDA && stock_resultante < stock_minimo) {
        pedido_creado = crear_pedido_reposicion(mov.id_producto,
                                                stock_resultante, stock_minimo);
    }

    if (ok) {
        sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
        resp.stock_actual = stock_resultante;
        if (stock_resultante < stock_minimo) {
            resp.codigo = RESP_OK;
            snprintf(resp.mensaje, sizeof(resp.mensaje),
                     "Registrado. AVISO: stock (%d) < minimo (%d)%s",
                     stock_resultante, stock_minimo,
                     pedido_creado ? ". Pedido de reposicion generado" : "");
        } else {
            resp.codigo = RESP_OK;
            copiar(resp.mensaje, sizeof(resp.mensaje), "Operacion registrada correctamente");
        }
    } else {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        resp.codigo = RESP_ERROR;
        resp.stock_actual = stock_actual;
        copiar(resp.mensaje, sizeof(resp.mensaje), "Error al registrar el movimiento");
    }
}

/* ── CONSULTA DE PRODUCTO ── */
void db_consulta(const ConsultaRequest& req, ConsultaResponse& resp) {
    std::lock_guard<std::mutex> lock(g_db_mutex);
    memset(&resp, 0, sizeof(resp));

    int stock = 0, stock_minimo = 0;
    std::string nombre, ubicacion;
    if (!obtener_producto(req.id_producto, stock, stock_minimo, nombre, ubicacion)) {
        resp.codigo = RESP_ERROR;
        copiar(resp.mensaje, sizeof(resp.mensaje), "Producto no encontrado");
        return;
    }

    int entradas = 0, salidas = 0;
    sumar_movimientos(req.id_producto, entradas, salidas);

    resp.codigo       = RESP_OK;
    resp.stock_actual = stock;
    resp.entradas     = entradas;
    resp.salidas      = salidas;
    resp.stock_minimo = stock_minimo;
    copiar(resp.nombre,    sizeof(resp.nombre),    nombre.c_str());
    copiar(resp.ubicacion, sizeof(resp.ubicacion), ubicacion.c_str());
    copiar(resp.mensaje,   sizeof(resp.mensaje),   "OK");
}

/* ── HISTORIAL ── */
int db_historial(const HistorialRequest& req, std::vector<HistorialItem>& out) {
    std::lock_guard<std::mutex> lock(g_db_mutex);
    out.clear();

    bool f_producto = (req.filtro_producto[0] != '\0');
    bool f_operario = (req.filtro_operario[0] != '\0');
    bool f_desde    = (req.fecha_desde[0]    != '\0');
    bool f_hasta    = (req.fecha_hasta[0]    != '\0');

    std::string sql =
        "SELECT h.timestamp, h.id_producto, h.tipo_operacion, h.cantidad, "
        "       h.stock_resultante, COALESCE(u.nombre_usuario,'?') "
        "FROM HISTORIAL_MOVIMIENTOS h "
        "LEFT JOIN USUARIOS u ON u.id_usuario = h.id_usuario ";

    std::vector<std::string> clausulas;
    if (f_producto) clausulas.push_back("h.id_producto = ?");
    if (f_operario) clausulas.push_back("u.nombre_usuario = ?");
    if (f_desde)    clausulas.push_back("date(h.timestamp) >= date(?)");
    if (f_hasta)    clausulas.push_back("date(h.timestamp) <= date(?)");

    for (size_t i = 0; i < clausulas.size(); ++i) {
        sql += (i == 0 ? "WHERE " : "AND ") + clausulas[i] + " ";
    }
    sql += "ORDER BY h.id_movimiento DESC LIMIT 500;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    int idx = 1;
    if (f_producto) sqlite3_bind_text(stmt, idx++, req.filtro_producto, -1, SQLITE_STATIC);
    if (f_operario) sqlite3_bind_text(stmt, idx++, req.filtro_operario, -1, SQLITE_STATIC);
    if (f_desde)    sqlite3_bind_text(stmt, idx++, req.fecha_desde,     -1, SQLITE_STATIC);
    if (f_hasta)    sqlite3_bind_text(stmt, idx++, req.fecha_hasta,     -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        HistorialItem it;
        memset(&it, 0, sizeof(it));
        copiar(it.timestamp,      sizeof(it.timestamp),      reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        copiar(it.id_producto,    sizeof(it.id_producto),    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        copiar(it.tipo_operacion, sizeof(it.tipo_operacion), reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
        it.cantidad         = sqlite3_column_int(stmt, 3);
        it.stock_resultante = sqlite3_column_int(stmt, 4);
        copiar(it.operario,       sizeof(it.operario),       reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
        out.push_back(it);
    }
    sqlite3_finalize(stmt);
    return (int)out.size();
}

/* ── RESUMEN DE STOCK ── */
int db_resumen(std::vector<ResumenItem>& out) {
    std::lock_guard<std::mutex> lock(g_db_mutex);
    out.clear();

    const char* sql =
        "SELECT id_producto, nombre, cantidad_actual, stock_minimo "
        "FROM PRODUCTOS WHERE activo = 1 ORDER BY id_producto;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ResumenItem it;
        memset(&it, 0, sizeof(it));
        copiar(it.id_producto, sizeof(it.id_producto), reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        copiar(it.nombre,      sizeof(it.nombre),      reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        it.stock        = sqlite3_column_int(stmt, 2);
        it.stock_minimo = sqlite3_column_int(stmt, 3);
        copiar(it.estado, sizeof(it.estado), calcular_estado(it.stock, it.stock_minimo));
        out.push_back(it);
    }
    sqlite3_finalize(stmt);
    return (int)out.size();
}

void db_mostrar_costes_almacenamiento() {
    std::lock_guard<std::mutex> lock(g_db_mutex);

    const char* sql =
        "SELECT p.id_producto, p.nombre, p.tipo, p.cantidad_actual, "
        "p.stock_minimo, p.precio_unitario, p.estrategia_salida, "
        "COALESCE(p.ubicacion_almacen,''), "
        "COALESCE(f.coste_embalaje,0), COALESCE(f.instrucciones,''), "
        "COALESCE(i.nivel_riesgo,0), COALESCE(i.zona_almacenamiento,''), "
        "COALESCE(pe.fecha_caducidad,''), COALESCE(pe.temperatura_max,0) "
        "FROM PRODUCTOS p "
        "LEFT JOIN PRODUCTOS_FRAGILES f ON p.id_producto = f.id_producto "
        "LEFT JOIN PRODUCTOS_INFLAMABLES i ON p.id_producto = i.id_producto "
        "LEFT JOIN PRODUCTOS_PERECEDEROS pe ON p.id_producto = pe.id_producto "
        "WHERE p.activo = 1 "
        "ORDER BY p.id_producto;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cout << "[ERROR] No se pudieron consultar los costes.\n";
        return;
    }

    std::cout << "\n===== COSTES DE ALMACENAMIENTO =====\n";

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string nombre = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string tipo = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        int stock = sqlite3_column_int(stmt, 3);
        int stock_minimo = sqlite3_column_int(stmt, 4);
        double precio = sqlite3_column_double(stmt, 5);
        std::string estrategia = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        std::string ubicacion = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));

        std::unique_ptr<Producto> producto;

        if (tipo == "generico") {
            producto = std::make_unique<ProductoGenerico>(
                id, nombre, stock, stock_minimo, precio, estrategia, ubicacion
            );
        }
        else if (tipo == "fragil") {
            double coste_embalaje = sqlite3_column_double(stmt, 8);
            std::string instrucciones = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));

            producto = std::make_unique<ProductoFragil>(
                id, nombre, stock, stock_minimo, precio, estrategia, ubicacion,
                coste_embalaje, instrucciones
            );
        }
        else if (tipo == "inflamable") {
            int nivel_riesgo = sqlite3_column_int(stmt, 10);
            std::string zona = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));

            producto = std::make_unique<ProductoInflamable>(
                id, nombre, stock, stock_minimo, precio, estrategia, ubicacion,
                nivel_riesgo, zona
            );
        }
        else if (tipo == "perecedero") {
            std::string fecha = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
            double temperatura = sqlite3_column_double(stmt, 13);

            producto = std::make_unique<ProductoPerecedero>(
                id, nombre, stock, stock_minimo, precio, estrategia, ubicacion,
                fecha, temperatura
            );
        }

        if (producto) {
            std::cout << *producto << "\n";
            std::cout << "-------------------------------------\n";
        }
    }

    sqlite3_finalize(stmt);
}

void db_alta_producto(const AltaProductoRequest& req,
                      AltaProductoResponse& resp)
{
    std::lock_guard<std::mutex> lock(g_db_mutex);
    memset(&resp, 0, sizeof(resp));

    sqlite3_stmt* stmt = nullptr;

    sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);

    const char* sql =
        "INSERT INTO PRODUCTOS "
        "(id_producto, nombre, tipo, cantidad_actual, stock_minimo, "
        "precio_unitario, estrategia_salida, ubicacion_almacen) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        resp.codigo = RESP_ERROR;
        strcpy(resp.mensaje, "Error preparando producto");
        return;
    }

    sqlite3_bind_text(stmt, 1, req.id_producto, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, req.nombre, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, req.tipo, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, req.cantidad_actual);
    sqlite3_bind_int(stmt, 5, req.stock_minimo);
    sqlite3_bind_double(stmt, 6, req.precio_unitario);
    sqlite3_bind_text(stmt, 7, req.estrategia_salida, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, req.ubicacion_almacen, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        resp.codigo = RESP_ERROR;
        strcpy(resp.mensaje, "Producto duplicado o invalido");
        return;
    }

    sqlite3_finalize(stmt);

    if (strcmp(req.tipo, "fragil") == 0) {
        const char* sql_f =
            "INSERT INTO PRODUCTOS_FRAGILES "
            "(id_producto, coste_embalaje, instrucciones) "
            "VALUES (?, ?, ?);";

        if (sqlite3_prepare_v2(db, sql_f, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, req.id_producto, -1, SQLITE_STATIC);
            sqlite3_bind_double(stmt, 2, req.coste_embalaje);
            sqlite3_bind_text(stmt, 3, req.instrucciones, -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    if (strcmp(req.tipo, "inflamable") == 0) {
        const char* sql_i =
            "INSERT INTO PRODUCTOS_INFLAMABLES "
            "(id_producto, nivel_riesgo, zona_almacenamiento) "
            "VALUES (?, ?, ?);";

        if (sqlite3_prepare_v2(db, sql_i, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, req.id_producto, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, req.nivel_riesgo);
            sqlite3_bind_text(stmt, 3, req.zona_almacenamiento, -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    if (strcmp(req.tipo, "perecedero") == 0) {
        const char* sql_p =
            "INSERT INTO PRODUCTOS_PERECEDEROS "
            "(id_producto, fecha_caducidad, temperatura_max) "
            "VALUES (?, ?, ?);";

        if (sqlite3_prepare_v2(db, sql_p, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, req.id_producto, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, req.fecha_caducidad, -1, SQLITE_STATIC);
            sqlite3_bind_double(stmt, 3, req.temperatura_max);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    resp.codigo = RESP_OK;
    strcpy(resp.mensaje, "Producto creado correctamente");
}

/* ── BAJA LOGICA DE PRODUCTO ── */
void db_baja_producto(const BajaProductoRequest& req, OpProductoResponse& resp) {
    std::lock_guard<std::mutex> lock(g_db_mutex);
    memset(&resp, 0, sizeof(resp));

    const char* sql =
        "UPDATE PRODUCTOS SET activo = 0 WHERE id_producto = ? AND activo = 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        resp.codigo = RESP_ERROR;
        copiar(resp.mensaje, sizeof(resp.mensaje), "Error interno del servidor");
        return;
    }
    sqlite3_bind_text(stmt, 1, req.id_producto, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE && sqlite3_changes(db) > 0) {
        resp.codigo = RESP_OK;
        snprintf(resp.mensaje, sizeof(resp.mensaje),
                 "Producto '%s' dado de baja", req.id_producto);
    } else {
        resp.codigo = RESP_ERROR;
        copiar(resp.mensaje, sizeof(resp.mensaje),
               "Producto no encontrado o ya dado de baja");
    }
}

/* ── EDICION DE PRODUCTO (campos base) ── */
void db_editar_producto(const EditarProductoRequest& req, OpProductoResponse& resp) {
    std::lock_guard<std::mutex> lock(g_db_mutex);
    memset(&resp, 0, sizeof(resp));

    if (strlen(req.nombre) == 0) {
        resp.codigo = RESP_ERROR;
        copiar(resp.mensaje, sizeof(resp.mensaje), "El nombre es obligatorio");
        return;
    }
    if (req.precio_unitario <= 0) {
        resp.codigo = RESP_ERROR;
        copiar(resp.mensaje, sizeof(resp.mensaje), "Precio invalido");
        return;
    }

    const char* sql =
        "UPDATE PRODUCTOS SET nombre = ?, stock_minimo = ?, precio_unitario = ?, "
        "estrategia_salida = ?, ubicacion_almacen = ? "
        "WHERE id_producto = ? AND activo = 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        resp.codigo = RESP_ERROR;
        copiar(resp.mensaje, sizeof(resp.mensaje), "Error interno del servidor");
        return;
    }
    sqlite3_bind_text  (stmt, 1, req.nombre,            -1, SQLITE_STATIC);
    sqlite3_bind_int   (stmt, 2, req.stock_minimo);
    sqlite3_bind_double(stmt, 3, req.precio_unitario);
    sqlite3_bind_text  (stmt, 4, req.estrategia_salida, -1, SQLITE_STATIC);
    sqlite3_bind_text  (stmt, 5, req.ubicacion_almacen, -1, SQLITE_STATIC);
    sqlite3_bind_text  (stmt, 6, req.id_producto,       -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE && sqlite3_changes(db) > 0) {
        resp.codigo = RESP_OK;
        snprintf(resp.mensaje, sizeof(resp.mensaje),
                 "Producto '%s' actualizado", req.id_producto);
    } else if (rc == SQLITE_DONE) {
        resp.codigo = RESP_ERROR;
        copiar(resp.mensaje, sizeof(resp.mensaje),
               "Producto no encontrado o dado de baja");
    } else {
        resp.codigo = RESP_ERROR;
        copiar(resp.mensaje, sizeof(resp.mensaje), "No se pudo actualizar (datos invalidos)");
    }
}

/* ── ALERTAS: stock bajo + caducidad ── */
int db_alertas(std::vector<AlertaItem>& out) {
    std::lock_guard<std::mutex> lock(g_db_mutex);
    out.clear();

    /* 1) Stock bajo / sin stock */
    const char* sql_stock =
        "SELECT id_producto, nombre, cantidad_actual, stock_minimo "
        "FROM PRODUCTOS "
        "WHERE activo = 1 AND cantidad_actual < stock_minimo "
        "ORDER BY cantidad_actual ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql_stock, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            AlertaItem it;
            memset(&it, 0, sizeof(it));
            copiar(it.id_producto, sizeof(it.id_producto), reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
            copiar(it.nombre,      sizeof(it.nombre),      reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
            it.stock        = sqlite3_column_int(stmt, 2);
            it.stock_minimo = sqlite3_column_int(stmt, 3);
            copiar(it.tipo_alerta, sizeof(it.tipo_alerta),
                   it.stock <= 0 ? "SIN_STOCK" : "STOCK_BAJO");
            snprintf(it.detalle, sizeof(it.detalle), "min %d", it.stock_minimo);
            out.push_back(it);
        }
        sqlite3_finalize(stmt);
    }

    /* 2) Caducidad de perecederos (caducados o que caducan en <= 30 dias) */
    const char* sql_cad =
        "SELECT p.id_producto, p.nombre, p.cantidad_actual, p.stock_minimo, "
        "       pe.fecha_caducidad, "
        "       CAST(julianday(pe.fecha_caducidad) - julianday('now') AS INTEGER) "
        "FROM PRODUCTOS p "
        "JOIN PRODUCTOS_PERECEDEROS pe ON pe.id_producto = p.id_producto "
        "WHERE p.activo = 1 "
        "  AND julianday(pe.fecha_caducidad) - julianday('now') <= 30 "
        "ORDER BY pe.fecha_caducidad ASC;";
    if (sqlite3_prepare_v2(db, sql_cad, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            AlertaItem it;
            memset(&it, 0, sizeof(it));
            copiar(it.id_producto, sizeof(it.id_producto), reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
            copiar(it.nombre,      sizeof(it.nombre),      reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
            it.stock        = sqlite3_column_int(stmt, 2);
            it.stock_minimo = sqlite3_column_int(stmt, 3);
            const char* fecha = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            int dias = sqlite3_column_int(stmt, 5);
            copiar(it.tipo_alerta, sizeof(it.tipo_alerta),
                   dias < 0 ? "CADUCADO" : "CADUCA_PRONTO");
            snprintf(it.detalle, sizeof(it.detalle), "%s", fecha ? fecha : "-");
            out.push_back(it);
        }
        sqlite3_finalize(stmt);
    }

    return (int)out.size();
}

/* ── LISTADO DE PROVEEDORES ── */
int db_proveedores(std::vector<ProveedorItem>& out) {
    std::lock_guard<std::mutex> lock(g_db_mutex);
    out.clear();

    const char* sql =
        "SELECT id_proveedor, nombre, COALESCE(contacto,''), COALESCE(telefono,'') "
        "FROM PROVEEDORES WHERE activo = 1 ORDER BY id_proveedor;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ProveedorItem it;
        memset(&it, 0, sizeof(it));
        it.id_proveedor = sqlite3_column_int(stmt, 0);
        copiar(it.nombre,   sizeof(it.nombre),   reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        copiar(it.contacto, sizeof(it.contacto), reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
        copiar(it.telefono, sizeof(it.telefono), reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
        out.push_back(it);
    }
    sqlite3_finalize(stmt);
    return (int)out.size();
}

/* ── LISTADO DE PEDIDOS DE REPOSICION PENDIENTES ── */
int db_pedidos(std::vector<PedidoItem>& out) {
    std::lock_guard<std::mutex> lock(g_db_mutex);
    out.clear();

    /* Todos los pedidos: pendientes primero, luego recibidos y cancelados. */
    const char* sql =
        "SELECT pr.id_pedido, pr.id_producto, COALESCE(pv.nombre,'(sin proveedor)'), "
        "       pr.cantidad, pr.fecha, pr.estado "
        "FROM PEDIDOS_REPOSICION pr "
        "LEFT JOIN PROVEEDORES pv ON pv.id_proveedor = pr.id_proveedor "
        "ORDER BY CASE pr.estado WHEN 'pendiente' THEN 0 "
        "                        WHEN 'recibido'  THEN 1 ELSE 2 END, "
        "         pr.id_pedido DESC LIMIT 500;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PedidoItem it;
        memset(&it, 0, sizeof(it));
        it.id_pedido = sqlite3_column_int(stmt, 0);
        copiar(it.id_producto, sizeof(it.id_producto), reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        copiar(it.proveedor,   sizeof(it.proveedor),   reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
        it.cantidad = sqlite3_column_int(stmt, 3);
        copiar(it.fecha,  sizeof(it.fecha),  reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
        copiar(it.estado, sizeof(it.estado), reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
        out.push_back(it);
    }
    sqlite3_finalize(stmt);
    return (int)out.size();
}


