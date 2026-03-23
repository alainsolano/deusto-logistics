#pragma once
#include "../../compartido/protocolo.h"

bool db_init(const char* ruta);
bool db_registrar_movimiento(const MovimientoStock& mov, int stock_resultante);
void db_close();
