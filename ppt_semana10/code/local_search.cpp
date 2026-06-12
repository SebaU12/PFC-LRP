// Estructura interna de la LS
struct RutasExplicitas {
  vector<vector<Ruta>> rutas; // [dep][ruta][pos]
  double costo;
};
// LS1=inter-deposito; LS2=solo intra
// Loop hasta optimo local (3 vecindades):
double LS1(CromosomaLRP&,
           vector<SplitResultado>&,
           const Matriz&,
           const InstanciaLRP&);
