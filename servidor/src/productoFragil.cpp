#include "productoFragil.h"
#include <stdexcept>

/* =========================================================
 * productoFragil.cpp  —  Implementacion de ProductoFragil
 * ========================================================= */

/* ---------------------------------------------------------
 * Constructor
 *
 * Llama al constructor base y luego valida e inicializa
 * los atributos propios del producto fragil.
 * --------------------------------------------------------- */
ProductoFragil::ProductoFragil(const std::string &id,
                               const std::string &nombre,
                               int stock_actual,
                               int stock_minimo,
                               double precio_unitario,
                               const std::string &estrategia_salida,
                               const std::string &ubicacion,
                               double coste_embalaje,
                               const std::string &instrucciones)
    : Producto(id,
               nombre,
               "fragil",
               stock_actual,
               stock_minimo,
               precio_unitario,
               estrategia_salida,
               ubicacion)
{
    if (coste_embalaje < 0)
    {
        throw std::invalid_argument("El coste de embalaje no puede ser negativo");
    }

    this->coste_embalaje = coste_embalaje;
    this->instrucciones = instrucciones;
}

/* ---------------------------------------------------------
 * calcularCosteAlmacenamiento
 *
 * Los productos fragiles requieren embalaje especial.
 * Por cada unidad en stock se suma el coste de embalaje
 * al precio unitario del producto.
 *
 * Formula: stock_actual * (precio_unitario + coste_embalaje)
 * --------------------------------------------------------- */
double ProductoFragil::calcularCosteAlmacenamiento() const
{

    double coste_por_unidad = precio_unitario + coste_embalaje;

    double coste_total = stock_actual * coste_por_unidad;

    return coste_total;
}

/* ---------------------------------------------------------
 * getTipoEtiqueta
 * --------------------------------------------------------- */
std::string ProductoFragil::getTipoEtiqueta() const
{
    return "[FRAGIL]";
}

/* ---------------------------------------------------------
 * Getters
 * --------------------------------------------------------- */

double ProductoFragil::getCosteEmbalaje() const
{
    return coste_embalaje;
}

const std::string &ProductoFragil::getInstrucciones() const
{
    return instrucciones;
}