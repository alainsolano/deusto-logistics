#include "producto_perecedero.h"

/* =========================================================
 * producto_perecedero.cpp  —  Implementacion de ProductoPerecedero
 * ========================================================= */


/* ---------------------------------------------------------
 * Constructor
 *
 * Llama al constructor base y asigna los atributos propios
 * del producto perecedero.
 * --------------------------------------------------------- */
ProductoPerecedero::ProductoPerecedero(const std::string& id,
                                       const std::string& nombre,
                                       int stock_actual,
                                       int stock_minimo,
                                       double precio_unitario,
                                       const std::string& estrategia_salida,
                                       const std::string& ubicacion,
                                       const std::string& fecha_caducidad,
                                       double temperatura_max)
    : Producto(id,
               nombre,
               "perecedero",
               stock_actual,
               stock_minimo,
               precio_unitario,
               estrategia_salida,
               ubicacion)
{
    this->fecha_caducidad = fecha_caducidad;
    this->temperatura_max = temperatura_max;
}


/* ---------------------------------------------------------
 * calcularCosteAlmacenamiento
 *
 * Los productos perecederos necesitan condiciones especiales
 * de conservacion (frio, humedad controlada, etc.).
 * Esto se traduce en un recargo del 20% sobre el coste base.
 *
 * Formula: stock_actual * precio_unitario * 1.20
 * --------------------------------------------------------- */
double ProductoPerecedero::calcularCosteAlmacenamiento() const {

    double coste_base    = stock_actual * precio_unitario;

    double recargo       = coste_base * 0.20;

    double coste_total   = coste_base + recargo;

    return coste_total;
}


/* ---------------------------------------------------------
 * getTipoEtiqueta
 * --------------------------------------------------------- */
std::string ProductoPerecedero::getTipoEtiqueta() const {
    return "[PERECEDERO]";
}


/* ---------------------------------------------------------
 * Getters
 * --------------------------------------------------------- */

const std::string& ProductoPerecedero::getFechaCaducidad() const {
    return fecha_caducidad;
}

double ProductoPerecedero::getTemperaturaMax() const {
    return temperatura_max;
}
