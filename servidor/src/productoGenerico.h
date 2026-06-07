#pragma once

#include "producto.h"

/* =========================================================
 * producto_generico.h  —  Producto de tipo generico
 *
 * No tiene atributos adicionales respecto a la clase base.
 * El coste de almacenamiento es el basico: stock * precio.
 * ========================================================= */

class ProductoGenerico : public Producto
{

public:
    /* Constructor */
    ProductoGenerico(const std::string &id,
                     const std::string &nombre,
                     int stock_actual,
                     int stock_minimo,
                     double precio_unitario,
                     const std::string &estrategia_salida,
                     const std::string &ubicacion);

    /* Coste de almacenamiento: stock_actual * precio_unitario */
    double calcularCosteAlmacenamiento() const override;

    /* Etiqueta del tipo para mostrar en consola */
    std::string getTipoEtiqueta() const override;
};