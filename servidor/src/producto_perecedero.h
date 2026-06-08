#pragma once

#include "producto.h"

/* =========================================================
 * producto_perecedero.h  —  Producto de tipo perecedero
 *
 * Atributos adicionales:
 *   fecha_caducidad   fecha limite de uso (formato YYYY-MM-DD)
 *   temperatura_max   temperatura maxima de almacenamiento en C
 *
 * El coste de almacenamiento tiene un recargo del 20% por
 * los requisitos de refrigeracion o conservacion especial.
 * ========================================================= */

class ProductoPerecedero : public Producto {

private:

    std::string fecha_caducidad;
    double      temperatura_max;

public:

    /* Constructor */
    ProductoPerecedero(const std::string& id,
                       const std::string& nombre,
                       int stock_actual,
                       int stock_minimo,
                       double precio_unitario,
                       const std::string& estrategia_salida,
                       const std::string& ubicacion,
                       const std::string& fecha_caducidad,
                       double temperatura_max);

    /* Coste = stock_actual * precio_unitario * 1.20 */
    double calcularCosteAlmacenamiento() const override;

    /* Etiqueta del tipo para mostrar en consola */
    std::string getTipoEtiqueta() const override;

    /* Getters propios */
    const std::string& getFechaCaducidad() const;
    double             getTemperaturaMax() const;
};
