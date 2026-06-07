#include "productoInflamable.h"
#include <stdexcept>

/* =========================================================
 * productoInflamable.cpp  —  Implementacion de ProductoInflamable
 * ========================================================= */

/* ---------------------------------------------------------
 * Constructor
 *
 * Llama al constructor base y valida que el nivel de riesgo
 * este entre 1 y 3, tal como define el schema de la BD.
 * --------------------------------------------------------- */
ProductoInflamable::ProductoInflamable(const std::string &id,
                                       const std::string &nombre,
                                       int stock_actual,
                                       int stock_minimo,
                                       double precio_unitario,
                                       const std::string &estrategia_salida,
                                       const std::string &ubicacion,
                                       int nivel_riesgo,
                                       const std::string &zona_almacenamiento)
    : Producto(id,
               nombre,
               "inflamable",
               stock_actual,
               stock_minimo,
               precio_unitario,
               estrategia_salida,
               ubicacion)
{
    if (nivel_riesgo < 1 || nivel_riesgo > 3)
    {
        throw std::invalid_argument("El nivel de riesgo debe ser 1, 2 o 3");
    }

    if (zona_almacenamiento.empty())
    {
        throw std::invalid_argument("La zona de almacenamiento es obligatoria para inflamables");
    }

    this->nivel_riesgo = nivel_riesgo;
    this->zona_almacenamiento = zona_almacenamiento;
}

/* ---------------------------------------------------------
 * calcularCosteAlmacenamiento
 *
 * Los productos inflamables tienen un recargo por peligrosidad.
 * A mayor nivel de riesgo, mayor coste de almacenamiento:
 *
 *   Nivel 1  ->  recargo del 10%
 *   Nivel 2  ->  recargo del 20%
 *   Nivel 3  ->  recargo del 30%
 *
 * Formula: stock_actual * precio_unitario * (1 + nivel_riesgo * 0.10)
 * --------------------------------------------------------- */
double ProductoInflamable::calcularCosteAlmacenamiento() const
{

    double recargo = nivel_riesgo * 0.10;

    double factor = 1.0 + recargo;

    double coste = stock_actual * precio_unitario * factor;

    return coste;
}

/* ---------------------------------------------------------
 * getTipoEtiqueta
 * --------------------------------------------------------- */
std::string ProductoInflamable::getTipoEtiqueta() const
{
    return "[INFLAMABLE]";
}

/* ---------------------------------------------------------
 * Getters
 * --------------------------------------------------------- */

int ProductoInflamable::getNivelRiesgo() const
{
    return nivel_riesgo;
}

const std::string &ProductoInflamable::getZonaAlmacenamiento() const
{
    return zona_almacenamiento;
}