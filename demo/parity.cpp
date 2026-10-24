#include "../core/JackCore.h"
#include <cstdio>
#include <cmath>
using namespace jack;
static void sig(std::vector<float>& L, std::vector<float>& R, int n, double fs){
  for(int i=0;i<n;i++){ double t=i/fs; double ph=std::fmod(t,0.5);
    double kick=std::sin(2*M_PI*(50+120*std::exp(-ph*30))*ph)*std::exp(-ph*9)*0.9;
    double ph2=std::fmod(t+0.25,0.5); double hat=std::sin(2*M_PI*7000*t)*std::exp(-ph2*40)*0.25;
    double pad=0.12*std::sin(2*M_PI*220*t)*(0.6+0.4*std::sin(2*M_PI*0.3*t));
    L[i]=(float)(kick+hat+pad); R[i]=(float)(kick*0.9+hat*1.1+pad);}
}
int main(){
  const double fs=44100; const int n=(int)(fs*6);
  struct Case{int src;float amt;int feel;bool adv;};
  Case cs[]={{0,40,1,false},{1,90,2,false},{6,60,0,false},{3,100,1,false}};
  for(auto&c:cs){
    std::vector<float>L(n),R(n); sig(L,R,n,fs);
    Compressor comp; comp.prepare(fs); Params p; p.source=(Source)c.src;p.amount=c.amt;p.feel=(Feel)c.feel; comp.setParams(p);
    float* ch[2]={L.data(),R.data()}; double ss=0; float pk=0; double gsum=0;
    for(int i=0;i<n;i+=512){int m=std::min(512,n-i); float* b[2]={ch[0]+i,ch[1]+i}; comp.process(b,2,m); gsum+=comp.currentGrDb();}
    for(int i=0;i<n;i++){ss+=L[i]*L[i];pk=std::max(pk,std::fabs(L[i]));}
    comp.flushStats(); auto&s=comp.getStats();
    printf("%d %.1f %d | rms %.5f pk %.5f gr %.4f thr %.3f avgGr %.4f maxGr %.4f\n",c.src,c.amt,c.feel,std::sqrt(ss/n),pk,comp.currentGrDb(),comp.currentThresholdDb(),s.avgGrDb,s.maxGrDb);
  }
}
