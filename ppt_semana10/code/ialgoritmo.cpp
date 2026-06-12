// algoritmo.h
struct ResultadoAlgoritmo {
  EstadoLRP mejor_estado;
  double    costo_inicial, costo_final;
  int       iteraciones;
  vector<pair<int,double>> historial_mejoras;
};
class IAlgoritmo {
public:
  virtual ResultadoAlgoritmo
    ejecutar(const EstadoLRP&) = 0;
  virtual string nombre() const = 0;
};
