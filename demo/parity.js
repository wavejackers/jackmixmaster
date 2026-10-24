const J=require('./jack-core.js');
const fs=44100,n=Math.floor(fs*6);
const cs=[[0,40,1],[1,90,2],[6,60,0],[3,100,1]];
for(const [src,amt,feel] of cs){
  const L=new Float32Array(n),R=new Float32Array(n);
  for(let i=0;i<n;i++){const t=i/fs;const ph=t%0.5;
    const kick=Math.sin(2*Math.PI*(50+120*Math.exp(-ph*30))*ph)*Math.exp(-ph*9)*0.9;
    const ph2=(t+0.25)%0.5;const hat=Math.sin(2*Math.PI*7000*t)*Math.exp(-ph2*40)*0.25;
    const pad=0.12*Math.sin(2*Math.PI*220*t)*(0.6+0.4*Math.sin(2*Math.PI*0.3*t));
    L[i]=kick+hat+pad;R[i]=kick*0.9+hat*1.1+pad;}
  const c=new J.Compressor();c.prepare(fs);const p=J.defaultParams();p.source=src;p.amount=amt;p.feel=feel;c.setParams(p);
  for(let i=0;i<n;i+=512){const m=Math.min(512,n-i);c.process([L.subarray(i,i+m),R.subarray(i,i+m)],m);}
  let ss=0,pk=0;for(let i=0;i<n;i++){ss+=L[i]*L[i];pk=Math.max(pk,Math.abs(L[i]));}
  c.flushStats();const s=c.stats;
  console.log(`${src} ${amt.toFixed(1)} ${feel} | rms ${Math.sqrt(ss/n).toFixed(5)} pk ${pk.toFixed(5)} gr ${c.currentGrDb().toFixed(4)} thr ${c.currentThresholdDb().toFixed(3)} avgGr ${s.avgGrDb.toFixed(4)} maxGr ${s.maxGrDb.toFixed(4)}`);
}
