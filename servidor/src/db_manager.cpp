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
        "FROM PRODUCTOS WHERE id_producto = ?;";
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

/* API publica */
bool db_init(const char* ruta) {
    if (sqlite3_open(ruta, &db) != SQLITE_OK) {
        std::cerr << "Error abriendo BD: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    sqlite3_busy_timeout(db, 3000);
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
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

    if (ok) {
        sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
        resp.stock_actual = stock_resultante;
        if (stock_resultante < stock_minimo) {
            resp.codigo = RESP_OK;
            snprintf(resp.mensaje, sizeof(resp.mensaje),
                     "Registrado. AVISO: stock (%d) bajo el minimo (%d)",
                     stock_resultante, stock_minimo);
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

    bool con_filtro = (req.filtro_producto[0] != '\0');
    std::string sql =
        "SELECT h.timestamp, h.id_producto, h.tipo_operacion, h.cantidad, "
        "       h.stock_resultante, COALESCE(u.nombre_usuario,'?') "
        "FROM HISTORIAL_MOVIMIENTOS h "
        "LEFT JOIN USUARIOS u ON u.id_usuario = h.id_usuario ";
    if (con_filtro) sql += "WHERE h.id_producto = ? ";
    sql += "ORDER BY h.id_movimiento DESC LIMIT 500;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    if (con_filtro) {
        sqlite3_bind_text(stmt, 1, req.filtro_producto, -1, SQLITE_STATIC);
    }

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
        "FROM PRODUCTOS ORDER BY id_producto;";
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
