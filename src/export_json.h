// Exporta un EstadoLRP a JSON para el visualizador HTML.
// Lanza std::runtime_error si no puede abrir el archivo de salida.
#pragma once
#include "estado.h"
#include <string>

void exportar_solucion_json(const EstadoLRP &estado, const std::string &ruta);
