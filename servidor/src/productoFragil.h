#pragma once

#include "producto.h"

/* =========================================================
 * productoFragil.h  —  Producto de tipo fragil
 *
 * Atributos adicionales:
 *   coste_embalaje   coste extra por unidad por el embalaje
 *   instrucciones    texto con indicaciones de manipulacion
 *
 * El coste de almacenamiento incluye el embalaje por unidad.
 * ========================================================= */

class ProductoFragil : public Producto
{

private:
    double coste_embalaje;
    std::string instrucciones;

public:
    /* Constructor */
    ProductoFragil(const std::string &id,
                   const std::string &nombre,
                   int stock_actual,
                   int stock_minimo,
                   double precio_unitario,
                   const std::string &estrategia_salida,
                   const std::string &ubicacion,
                   double coste_embalaje,
                   const std::string &instrucciones);

    /* Coste = stock_actual * (precio_unitario + coste_embalaje) */
    double calcularCosteAlmacenamiento() const override;

    /* Etiqueta del tipo para mostrar en consola */
    std::string getTipoEtiqueta() const override;

    /* Getters propios */
    double getCosteEmbalaje() const;
    const std::string &getInstrucciones() const;
};