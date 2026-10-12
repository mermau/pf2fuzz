#include "FuzzCircuit.h"
#include <cstdio>
// Test del modello fisico: punto di lavoro, distorsione al variare di FUZZ, CPU
#include <chrono>
using namespace pf2;
int main(){ const double fs=192000;
 { FuzzCircuit c; c.prepare(fs); c.setControls(0.7f,0.5f);
   for(int i=0;i<fs;i++) c.process(0.0);
   printf("DC: Q1 base %.3f coll %.3f | Q2 base %.3f coll %.3f | nodo S %.3f\n",
     c.nodeVoltage(FuzzCircuit::B1),c.nodeVoltage(FuzzCircuit::K1),c.nodeVoltage(FuzzCircuit::B2),c.nodeVoltage(FuzzCircuit::K2),c.nodeVoltage(FuzzCircuit::S)); }
 for(double amp:{0.005,0.02,0.1,0.3}){ printf("amp %.3f V:",amp);
  for(double fz:{0.0,0.5,1.0}){ FuzzCircuit c; c.prepare(fs); c.setControls((float)fz,1.f);
   double acc=0,re=0,im=0; int n=(int)fs,cnt=0; double f=220; bool bad=false;
   for(int i=0;i<n;i++){ double y=c.process(amp*std::sin(2*kPi*f*i/fs)); if(!std::isfinite(y)) bad=true; if(i>=n/2){acc+=y*y;cnt++;re+=y*std::cos(2*kPi*f*i/fs);im+=y*std::sin(2*kPi*f*i/fs);} }
   double rms=std::sqrt(acc/cnt), fr=std::sqrt(2.)*std::hypot(re,im)/cnt; printf("  fz%.1f: %.4fV thd%4.0f%%%s",fz,rms,100*std::sqrt(std::max(0.,rms*rms-fr*fr))/std::max(fr,1e-12),bad?" NaN":"");}
  printf("\n");}
 FuzzCircuit c; c.prepare(fs); auto t0=std::chrono::steady_clock::now(); volatile double s=0;
 for(int i=0;i<fs*10;i++) s=s+c.process(0.2*std::sin(i*0.0072));
 double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();
 printf("CPU: %.1f%% di un core per canale (4x a 48k)\n",sec*10);
}
