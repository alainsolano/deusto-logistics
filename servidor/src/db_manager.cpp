#include "db_manager.h"
#include <iostream>
#include <sqlite3.h>
#include <cstring>

static sqlite3* db = nullptr;

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

int db_obtener_stock(const char* id_producto) {
    const char* sql =
        "SELECT cantidad_actual FROM PRODUCTOS WHERE id_producto = ?;";

    sqlite3_stmt* stmt = nullptr;
    int stock = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, id_producto, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stock = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return stock;
}

bool db_init(const char* ruta) {
    if (sqlite3_open(ruta, &db) != SQLITE_OK) {
        std::cerr << "Error abriendo BD: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    std::cout << "[BD] Archivo abierto: " << ruta << std::endl;
    return true;
}

bool db_registrar_movimiento(const MovimientoStock& mov, int& stock_resultante) {
    int id_usuario = obtener_id_usuario(mov.id_operario);

    if (id_usuario == -1) {
        std::cerr << "[BD] Usuario no encontrado: " << mov.id_operario << std::endl;
        return false;
    }

    int stock_actual = db_obtener_stock(mov.id_producto);

    if (mov.tipo_op == 'E') {
        stock_resultante = stock_actual + mov.cantidad;
    } else if (mov.tipo_op == 'S') {
        if (stock_actual < mov.cantidad) {
            std::cerr << "[BD] Stock insuficiente para " << mov.id_producto << std::endl;
            return false;
        }
        stock_resultante = stock_actual - mov.cantidad;
    } else {
        std::cerr << "[BD] Tipo de operacion no valido." << std::endl;
        return false;
    }

    const char* sql_update =
        "UPDATE PRODUCTOS SET cantidad_actual = ? WHERE id_producto = ?;";

    sqlite3_stmt* stmt_update = nullptr;

    if (sqlite3_prepare_v2(db, sql_update, -1, &stmt_update, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt_update, 1, stock_resultante);
    sqlite3_bind_text(stmt_update, 2, mov.id_producto, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt_update) != SQLITE_DONE) {
        sqlite3_finalize(stmt_update);
        return false;
    }

    sqlite3_finalize(stmt_update);

    const char* sql_insert =
        "INSERT INTO HISTORIAL_MOVIMIENTOS "
        "(id_producto, id_usuario, tipo_operacion, cantidad, stock_resultante, origen) "
        "VALUES (?, ?, ?, ?, ?, 'manual');";

    sqlite3_stmt* stmt_insert = nullptr;

    if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt_insert, nullptr) != SQLITE_OK) {
        return false;
    }

    const char* tipo_operacion = (mov.tipo_op == 'E') ? "entrada" : "salida";

    sqlite3_bind_text(stmt_insert, 1, mov.id_producto, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt_insert, 2, id_usuario);
    sqlite3_bind_text(stmt_insert, 3, tipo_operacion, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt_insert, 4, mov.cantidad);
    sqlite3_bind_int(stmt_insert, 5, stock_resultante);

    bool ok = sqlite3_step(stmt_insert) == SQLITE_DONE;

    if (!ok) {
        std::cerr << "[BD] Error insertando movimiento: "
                  << sqlite3_errmsg(db) << std::endl;
    }

    sqlite3_finalize(stmt_insert);
    return ok;
}

void db_close() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}


