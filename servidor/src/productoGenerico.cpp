#include "productoGenerico.h"

/* =========================================================
 * productoGenerico.cpp  —  Implementacion de ProductoGenerico
 * ========================================================= */

/* ---------------------------------------------------------
 * Constructor
 *
 * Llama al constructor de la clase base pasando "generico"
 * como tipo. No tiene atributos propios que inicializar.
 * --------------------------------------------------------- */
ProductoGenerico::ProductoGenerico(const std::string &id,
                                   const std::string &nombre,
                                   int stock_actual,
                                   int stock_minimo,
                                   double precio_unitario,
                                   const std::string &estrategia_salida,
                                   const std::string &ubicacion)
    : Producto(id,
               nombre,
               "generico",
               stock_actual,
               stock_minimo,
               precio_unitario,
               estrategia_salida,
               ubicacion)
{
    /* Sin inicializacion adicional */
}

/* ---------------------------------------------------------
 * calcularCosteAlmacenamiento
 *
 * Coste base: simplemente el numero de unidades en stock
 * multiplicado por el precio unitario del producto.
 * --------------------------------------------------------- */
double ProductoGenerico::calcularCosteAlmacenamiento() const
{

    double coste = stock_actual * precio_unitario;

    return coste;
}

/* ---------------------------------------------------------
 * getTipoEtiqueta
 * --------------------------------------------------------- */
std::string ProductoGenerico::getTipoEtiqueta() const
{
    return "[GENERICO]";
}