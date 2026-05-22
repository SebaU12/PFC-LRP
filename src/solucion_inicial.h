#pragma once
#include "estado.h"

// Helper: demanda total de una ruta (usado también por operadores)
double demanda_ruta_externa(const Ruta &r, const InstanciaLRP &datos);

// All-Open Dummy Initialization + Greedy Insertion.
// Abre todos los depósitos, luego inserta cada cliente en la
// posición de menor costo marginal (respetando capacidad Q).
EstadoLRP generar_solucion_inicial(const InstanciaLRP &inst, const Matriz &m);
