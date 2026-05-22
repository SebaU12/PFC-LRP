#pragma once
#include "estado.h"
#include <string>

// Serializa el estado completo de una solución LRP a un archivo JSON.
// El archivo resultante es consumido por el visualizador HTML.
//
// Parámetros:
//   estado  — solución a exportar (puede tener no_asignados)
//   ruta    — path del archivo de salida, ej: "solucion.json"
//
// Lanza std::runtime_error si no puede abrir el archivo.
void exportar_solucion_json(const EstadoLRP &estado, const std::string &ruta);
