#include "db_manager.h"
#include <sqlite3.h>
#include <iostream>

static sqlite3* db = nullptr;

bool db_init(const char* ruta) {
    if (sqlite3_open(ruta, &db) != SQLITE_OK) {
        std::cerr << "Error abriendo BD: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    std::cout << "[BD] Archivo abierto: " << ruta << std::endl;

    const char* sql =
        "CREATE TABLE IF NOT EXISTS PRODUCTOS ("
        "  id_producto TEXT PRIMARY KEY,"
        "  nombre TEXT NOT NULL,"
        "  cantidad_actual INTEGER NOT NULL DEFAULT 0,"
        "  stock_minimo INTEGER NOT NULL DEFAULT 5"
        ");"
        "CREATE TABLE IF NOT EXISTS HISTORIAL_MOVIMIENTOS ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  id_producto TEXT NOT NULL,"
        "  tipo_operacion TEXT NOT NULL,"
        "  cantidad INTEGER NOT NULL,"
        "  stock_resultante INTEGER NOT NULL,"
        "  timestamp INTEGER NOT NULL,"
        "  id_operario TEXT NOT NULL"
        ");";

    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "[BD] Error creando tablas: " << err << std::endl;
        sqlite3_free(err);
        return false;
    }

    std::cout << "[BD] Tablas creadas correctamente." << std::endl;
    return true;
}

bool db_registrar_movimiento(const MovimientoStock& mov, int stock_resultante) {
    const char* sql =
        "INSERT INTO HISTORIAL_MOVIMIENTOS "
        "(id_producto, tipo_operacion, cantidad, stock_resultante, timestamp, id_operario) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text (stmt, 1, mov.id_producto, -1, SQLITE_STATIC);
    sqlite3_bind_text (stmt, 2, mov.tipo_op == 'E' ? "entrada" : "salida", -1, SQLITE_STATIC);
    sqlite3_bind_int  (stmt, 3, mov.cantidad);
    sqlite3_bind_int  (stmt, 4, stock_resultante);
    sqlite3_bind_int64(stmt, 5, mov.timestamp);
    sqlite3_bind_text (stmt, 6, mov.id_operario, -1, SQLITE_STATIC);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) {
        std::cerr << "[BD] Error insertando movimiento: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_finalize(stmt);
    return ok;
}

void db_close() {
    if (db) sqlite3_close(db);
}