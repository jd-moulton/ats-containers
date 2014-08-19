/*
Utility functions for Vegetation.

Author: Ethan Coon (ecoon@lanl.gov)
        Chonggang Xu (cxu@lanl.gov)

Licencse: BSD
*/


#ifndef ATS_BGC_VEG_HH_
#define ATS_BGC_VEG_HH_

namespace Amanzi {
namespace BGC {

// Functor for calculating QSat.
class QSat {
 public:
  QSat();

  void operator()(double tleafk, double pressure,
                  double* es, double* esdT, double* qs, double* qsdT);

 private:
  double a0, a1, a2, a3, a4, a5, a6, a7, a8;
  double b0, b1, b2, b3, b4, b5, b6, b7, b8;
  double c0, c1, c2, c3, c4, c5, c6, c7, c8;
  double d0, d1, d2, d3, d4, d5, d6, d7, d8;
};

// Determine time, in minutes of a given day of the year at a given latitude.
double DayLength(double lat, int doy);

// Limit the highest temp?
double HighTLim(double tleaf);

// This function calculate the net photosynthetic rate based on Farquhar
// model, with updated leaf temperature based on energy balances
void Photosynthesis(double PARi, double LUE, double LER, double pressure, double windv,
                    double tair, double relh, double CO2a, double mp, double Vcmax25,
                    double* A, double* tleaf, double* Resp);

} // namespace
} // namespace

#endif

