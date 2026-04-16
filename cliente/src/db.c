#include "db.h"
#include "config.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

static sqlite3 *db = NULL;

static int db_obtener_id_usuario(const char *nombre_usuario) {
    const char *sql = "SELECT id_usuario FROM USUARIOS WHERE nombre_usuario = ?;";
    sqlite3_stmt *stmt = NULL;
    int id_usuario = -1;

    if (db == NULL) {
        return -1;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, nombre_usuario, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id_usuario = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return id_usuario;
}

int db_inicializar(void) {
    if (sqlite3_open(g_config.ruta_db, &db) != SQLITE_OK) {
        printf("[ERROR] No se pudo abrir la base de datos: %s\n", g_config.ruta_db);
        return -1;
    }

    return 0;
}

int db_obtener_stock(const char *id_producto, int *stock, int *entradas, int *salidas) {
    const char *sql =
        "SELECT "
        "COALESCE(SUM(CASE WHEN tipo_operacion='entrada' THEN cantidad ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN tipo_operacion='salida' THEN cantidad ELSE 0 END), 0) "
        "FROM HISTORIAL_MOVIMIENTOS "
        "WHERE id_producto = ?;";

    sqlite3_stmt *stmt = NULL;
    int ent = 0;
    int sal = 0;

    if (db == NULL) {
        return -1;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -2;
    }

    sqlite3_bind_text(stmt, 1, id_producto, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        ent = sqlite3_column_int(stmt, 0);
        sal = sqlite3_column_int(stmt, 1);
    }

    sqlite3_finalize(stmt);

    *entradas = ent;
    *salidas = sal;
    *stock = ent - sal;

    return 0;
}

int db_insertar_movimiento(const char *id_producto, int cantidad, char tipo_op, const char *id_operario) {
    const char *sql =
        "INSERT INTO HISTORIAL_MOVIMIENTOS "
        "(id_producto, id_usuario, tipo_operacion, cantidad, stock_resultante, origen) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt = NULL;
    int id_usuario;
    int stock_actual = 0;
    int entradas = 0;
    int salidas = 0;
    int stock_resultante;
    const char *tipo_operacion = NULL;

    if (db == NULL) {
        return -1;
    }

    id_usuario = db_obtener_id_usuario(id_operario);
    if (id_usuario == -1) {
        return -2;
    }

    if (db_obtener_stock(id_producto, &stock_actual, &entradas, &salidas) != 0) {
        return -3;
    }

    if (tipo_op == 'E') {
        tipo_operacion = "entrada";
        stock_resultante = stock_actual + cantidad;
    } else if (tipo_op == 'S') {
        tipo_operacion = "salida";
        stock_resultante = stock_actual - cantidad;
    } else {
        return -4;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -5;
    }

    sqlite3_bind_text(stmt, 1, id_producto, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, id_usuario);
    sqlite3_bind_text(stmt, 3, tipo_operacion, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, cantidad);
    sqlite3_bind_int(stmt, 5, stock_resultante);
    sqlite3_bind_text(stmt, 6, "manual", -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return -6;
    }

    sqlite3_finalize(stmt);
    return 0;
}