// r4_analysis.cpp - post-processing driver: volume-weighted Tavg, Tmax location,
// 30 probes with consistent samplers, FVM outer-loop stats, field exports.
#include "thermal3d/FemSolver3D.h"
#include "thermal3d/FdmSolver3D.h"
#include "thermal3d/FvmSolver3D.h"
#include "thermal3d/Verification3D.h"
#include "thermal3d/Mesh3D.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <iostream>
using namespace thermatrix; using namespace thermatrix::t3d;

static std::string OUT = ".";
struct Mom { double vol=0; };

// ---- samplers ----
static double sample_fdm_tri(const StructuredGrid3D& g, const std::vector<double>& T, const Point3D& p) {
    double fx=(p.x-g.origin.x)/g.dx-0.5, fy=(p.y-g.origin.y)/g.dy-0.5, fz=(p.z-g.origin.z)/g.dz-0.5;
    int i0=(int)std::floor(fx), j0=(int)std::floor(fy), k0=(int)std::floor(fz);
    double tx=fx-i0, ty=fy-j0, tz=fz-k0, sw=0, sT=0;
    for(int a=0;a<2;++a)for(int b=0;b<2;++b)for(int c=0;c<2;++c){
        int i=i0+a,j=j0+b,k=k0+c; if(i<0||j<0||k<0||i>=g.nx||j>=g.ny||k>=g.nz) continue;
        int id=g.flat_idx(i,j,k); if(!g.is_solid[id]) continue;
        double w=(a?tx:1-tx)*(b?ty:1-ty)*(c?tz:1-tz); sw+=w; sT+=w*T[g.cell_to_dof[id]]; }
    if(sw>1e-12) return sT/sw;
    return Verification3D::sample_fdm(g,T,p); // nearest solid cell fallback
}
struct FvmAdj { std::vector<std::vector<int>> nb; std::vector<Point3D> cen; std::vector<std::array<Point3D,2>> bb; };
static FvmAdj build_adj(const FemMesh& m){
    FvmAdj A; int ne=m.n_elements(); A.nb.assign(ne,{}); A.cen.resize(ne); A.bb.resize(ne);
    std::map<std::array<int,3>,int> f;
    static const int fi[4][3]={{1,2,3},{0,2,3},{0,1,3},{0,1,2}};
    for(int e=0;e<ne;++e){ auto&v=m.elements[e].v; Point3D c{0,0,0}; Point3D lo{1e9,1e9,1e9},hi{-1e9,-1e9,-1e9};
        for(int a=0;a<4;++a){auto&p=m.nodes[v[a]]; c.x+=p.x/4;c.y+=p.y/4;c.z+=p.z/4;
            lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);lo.z=std::min(lo.z,p.z);
            hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);hi.z=std::max(hi.z,p.z);}
        A.cen[e]=c; A.bb[e]={lo,hi};
        for(int q=0;q<4;++q){ std::array<int,3> k={v[fi[q][0]],v[fi[q][1]],v[fi[q][2]]}; std::sort(k.begin(),k.end());
            auto it=f.find(k); if(it==f.end()) f[k]=e; else {A.nb[e].push_back(it->second);A.nb[it->second].push_back(e);} } }
    return A;
}
static bool locate(const FemMesh& m,const FvmAdj&A,const Point3D&p,double*lam,int&e_out){
    const double tol=1e-9;
    for(int e=0;e<m.n_elements();++e){ auto&bb=A.bb[e]; if(p.x<bb[0].x-tol||p.x>bb[1].x+tol||p.y<bb[0].y-tol||p.y>bb[1].y+tol||p.z<bb[0].z-tol||p.z>bb[1].z+tol) continue;
        auto&v=m.elements[e].v; const Point3D&p0=m.nodes[v[0]],&p1=m.nodes[v[1]],&p2=m.nodes[v[2]],&p3=m.nodes[v[3]];
        auto vol=[](const Point3D&a,const Point3D&b,const Point3D&c,const Point3D&d){
            double ax=a.x-d.x,ay=a.y-d.y,az=a.z-d.z,bx=b.x-d.x,by=b.y-d.y,bz=b.z-d.z,cx=c.x-d.x,cy=c.y-d.y,cz=c.z-d.z;
            return (ax*(by*cz-bz*cy)+ay*(bz*cx-bx*cz)+az*(bx*cy-by*cx))/6.0;};
        double V=vol(p0,p1,p2,p3); if(std::abs(V)<1e-18) continue;
        double l0=vol(p,p1,p2,p3)/V,l1=vol(p0,p,p2,p3)/V,l2=vol(p0,p1,p,p3)/V,l3=vol(p0,p1,p2,p)/V;
        if(l0>=-1e-7&&l1>=-1e-7&&l2>=-1e-7&&l3>=-1e-7){lam[0]=l0;lam[1]=l1;lam[2]=l2;lam[3]=l3;e_out=e;return true;} }
    return false;
}
static double sample_fem_b(const FemMesh&m,const FvmAdj&A,const std::vector<double>&T,const Point3D&p,bool&ok){
    double l[4]; int e; ok=locate(m,A,p,l,e); if(!ok) return Verification3D::sample_fem(m,T,p);
    auto&v=m.elements[e].v; return l[0]*T[v[0]]+l[1]*T[v[1]]+l[2]*T[v[2]]+l[3]*T[v[3]]; }
// FVM: cell value + LSQ gradient from face neighbours
static double sample_fvm_g(const FemMesh&m,const FvmAdj&A,const std::vector<double>&T,const Point3D&p,bool&ok){
    double l[4]; int e; ok=locate(m,A,p,l,e); if(!ok) return Verification3D::sample_fvm(m,T,p);
    double a[3][3]={{0}},b[3]={0};
    for(int n:A.nb[e]){ double d[3]={A.cen[n].x-A.cen[e].x,A.cen[n].y-A.cen[e].y,A.cen[n].z-A.cen[e].z};
        double w=1.0/(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]); double dT=T[n]-T[e];
        for(int i=0;i<3;++i){b[i]+=w*d[i]*dT; for(int j=0;j<3;++j)a[i][j]+=w*d[i]*d[j];}}
    double det=a[0][0]*(a[1][1]*a[2][2]-a[1][2]*a[2][1])-a[0][1]*(a[1][0]*a[2][2]-a[1][2]*a[2][0])+a[0][2]*(a[1][0]*a[2][1]-a[1][1]*a[2][0]);
    if(A.nb[e].size()<3||std::abs(det)<1e-40) return T[e];
    double g[3]; // Cramer
    auto D=[&](int c){double M[3][3];for(int i=0;i<3;++i)for(int j=0;j<3;++j)M[i][j]=(j==c)?b[i]:a[i][j];
        return M[0][0]*(M[1][1]*M[2][2]-M[1][2]*M[2][1])-M[0][1]*(M[1][0]*M[2][2]-M[1][2]*M[2][0])+M[0][2]*(M[1][0]*M[2][1]-M[1][1]*M[2][0]);};
    for(int c=0;c<3;++c) g[c]=D(c)/det;
    double r=T[e]+g[0]*(p.x-A.cen[e].x)+g[1]*(p.y-A.cen[e].y)+g[2]*(p.z-A.cen[e].z);
    double lo=T[e],hi=T[e]; for(int n:A.nb[e]){lo=std::min(lo,T[n]);hi=std::max(hi,T[n]);}
    return std::min(hi,std::max(lo,r)); // bounded (no over/undershoot)
}

int main(int argc,char**argv){
    if(argc>1) OUT=argv[1];
    std::ofstream sum(OUT+"/analysis_summary.csv"), prb(OUT+"/probe_values.csv"), fvmq(OUT+"/fvm_outer_stats.csv"), mq(OUT+"/mesh_quality.csv");
    sum<<"solver,level,case,n_dof,iterations,rel_res,Tmax,xm,ym,zm,Tmin,Tavg_vw,Tavg_native,Tsrc,Q_conv,Q_bal_rel,locate_fail\n";
    prb<<"solver,level,case,probe,x,y,z,T\n";
    fvmq<<"level,case,outer_loops,last_dT_max,history\n";
    mq<<"level,n_tets,vol_total_m3,min_vol,max_vol,vol_ratio,mean_aspect,max_aspect\n";
    auto probes=Verification3D::get_standard_probes();
    SolverParams sp; sp.rel_tol=1e-10; sp.max_iter=100000; sp.method="PCG-IC0";
    ThermalProblem prob; HeatSinkGeometry geom;
    struct L{const char*name;const char*file;MeshResolution r;bool grid;};
    std::vector<L> levels={{"Coarse","data/heatsink_coarse.msh",MeshResolution::Coarse,true},{"Medium","data/heatsink_medium.msh",MeshResolution::Medium,true},{"Fine","data/heatsink_fine.msh",MeshResolution::Fine,true},{"Finer","data/heatsink_finer.msh",MeshResolution::Fine,false}};
    for(auto&lv:levels){
        FemMesh mesh=load_msh_file(lv.file); FvmAdj adj=build_adj(mesh);
        { double mn=1e9,mx=0,sa=0,ma=0,sv=0; for(auto&e:mesh.elements){mn=std::min(mn,e.volume);mx=std::max(mx,e.volume);sv+=e.volume;
            double emin=1e9,emax=0; for(int a=0;a<4;++a)for(int b=a+1;b<4;++b){auto&p=mesh.nodes[e.v[a]];auto&q=mesh.nodes[e.v[b]];
                double d=std::sqrt((p.x-q.x)*(p.x-q.x)+(p.y-q.y)*(p.y-q.y)+(p.z-q.z)*(p.z-q.z));emin=std::min(emin,d);emax=std::max(emax,d);}
            sa+=emax/emin; ma=std::max(ma,emax/emin);}
          mq<<lv.name<<","<<mesh.n_elements()<<","<<sv<<","<<mn<<","<<mx<<","<<mx/mn<<","<<sa/mesh.n_elements()<<","<<ma<<"\n"; }
        StructuredGrid3D grid; if(lv.grid) grid=generate_structured_grid(geom,lv.r);
        for(int cs=0;cs<2;++cs){
            ConvCase cc=cs==0?ConvCase::A:ConvCase::B; const char*cl=cs==0?"A":"B";
            for(int s=0;s<3;++s){
                if(!lv.grid && s==1) continue; // no FDM at "Finer"
                if(!lv.grid && s==2) continue; // finer reference: FEM only
                SolverResult r; const char*sn=s==0?"FEM":s==1?"FDM":"FVM"; int fail=0;
                FemSolver3D fem(prob); FdmSolver3D fdm(prob); FvmSolver3D fvm(prob);
                if(s==0) r=fem.solve(mesh,cc,sp); else if(s==1) r=fdm.solve(grid,cc,sp); else r=fvm.solve(mesh,cc,sp);
                if(!r.success){std::cerr<<"FAIL "<<sn<<" "<<lv.name<<"\n";}
                double Tmax=-1e9,xm=0,ym=0,zm=0,vw=0,V=0;
                if(s==0){ for(int i=0;i<mesh.n_nodes();++i) if(r.T[i]>Tmax){Tmax=r.T[i];xm=mesh.nodes[i].x;ym=mesh.nodes[i].y;zm=mesh.nodes[i].z;}
                    for(auto&e:mesh.elements){double t=0;for(int a=0;a<4;++a)t+=r.T[e.v[a]]/4;vw+=t*e.volume;V+=e.volume;} }
                else if(s==2){ for(int e=0;e<mesh.n_elements();++e){ if(r.T[e]>Tmax){Tmax=r.T[e];xm=adj.cen[e].x;ym=adj.cen[e].y;zm=adj.cen[e].z;}
                    vw+=r.T[e]*mesh.elements[e].volume;V+=mesh.elements[e].volume;} }
                else { for(int d=0;d<grid.n_solid_cells();++d){ if(r.T[d]>Tmax){Tmax=r.T[d];auto c=grid.dof_to_coord[d];auto p=grid.cell_center(c.i,c.j,c.k);xm=p.x;ym=p.y;zm=p.z;}
                    vw+=r.T[d];V+=1;} }
                vw/=V;
                for(size_t pi=0;pi<probes.size();++pi){ bool ok=true; double T;
                    if(s==0) T=sample_fem_b(mesh,adj,r.T,probes[pi],ok); else if(s==2) T=sample_fvm_g(mesh,adj,r.T,probes[pi],ok); else T=sample_fdm_tri(grid,r.T,probes[pi]);
                    if(!ok) ++fail;
                    prb<<sn<<","<<lv.name<<","<<cl<<","<<pi<<","<<probes[pi].x<<","<<probes[pi].y<<","<<probes[pi].z<<","<<T<<"\n"; }
                char buf[512]; snprintf(buf,512,"%s,%s,%s,%d,%d,%.3e,%.6f,%.5f,%.5f,%.5f,%.6f,%.6f,%.6f,%.6f,%.6f,%.3e,%d\n",sn,lv.name,cl,r.n_dof,r.iterations,r.final_rel_res,Tmax,xm,ym,zm,r.T_min,vw,r.T_avg,r.T_src,r.Q_conv,r.Q_balance_rel,fail);
                sum<<buf; sum.flush();
                if(s==2){ auto&h=fvm.outer_history(); fvmq<<lv.name<<","<<cl<<","<<h.size()<<","<<(h.empty()?0:h.back())<<",\""; for(size_t i=0;i<h.size();++i)fvmq<<(i?" ":"")<<h[i]; fvmq<<"\"\n"; }
                // field export for Fine/Case A
                if(std::string(lv.name)=="Fine" && cs==0){
                    std::ofstream f(OUT+"/field_"+sn+"_fine_A.csv"); f<<"idx,x,y,z,T\n";
                    if(s==0) for(int i=0;i<mesh.n_nodes();++i) f<<i<<","<<mesh.nodes[i].x<<","<<mesh.nodes[i].y<<","<<mesh.nodes[i].z<<","<<r.T[i]<<"\n";
                    else if(s==2) for(int e=0;e<mesh.n_elements();++e) f<<e<<","<<adj.cen[e].x<<","<<adj.cen[e].y<<","<<adj.cen[e].z<<","<<r.T[e]<<"\n";
                    else for(int d=0;d<grid.n_solid_cells();++d){auto c=grid.dof_to_coord[d];auto p=grid.cell_center(c.i,c.j,c.k);f<<d<<","<<p.x<<","<<p.y<<","<<p.z<<","<<r.T[d]<<"\n";}
                    // cut planes: x=0.030 (y-z) and z=0.002 (x-y)
                    auto plane=[&](const std::string&tag,int which){ std::ofstream g(OUT+"/plane_"+tag+"_"+sn+"_fine_A.csv"); g<<"u,v,T\n";
                        int nu=which==0?481:241, nv=which==0?233:241; double du=which==0?0.060/(nu-1):0.060/(nu-1), dv=which==0?0.029/(nv-1):0.060/(nv-1);
                        for(int a=0;a<nu;++a)for(int b=0;b<nv;++b){ Point3D p= which==0? Point3D{0.0301,a*du,b*dv}:Point3D{a*du,b*dv,0.0021};
                            if(!geom.is_in_solid(p.x,p.y,p.z)){g<<a*du<<","<<b*dv<<",nan\n";continue;}
                            bool ok; double T= s==0?sample_fem_b(mesh,adj,r.T,p,ok): s==2?sample_fvm_g(mesh,adj,r.T,p,ok): sample_fdm_tri(grid,r.T,p);
                            g<<a*du<<","<<b*dv<<","<<T<<"\n"; } };
                    plane("x30",0); plane("z2",1);
                }
                std::cout<<sn<<" "<<lv.name<<" "<<cl<<" Tmax="<<Tmax<<" Tavg_vw="<<vw<<" fail="<<fail<<std::endl;
            }
        }
    }
    // mesh topology export for surface plots (Fine)
    { FemMesh mesh=load_msh_file("data/heatsink_fine.msh"); std::ofstream f(OUT+"/surface_tris_fine.csv"); f<<"n0,n1,n2,bc\n";
      for(auto&b:mesh.boundary_faces) f<<b.v[0]<<","<<b.v[1]<<","<<b.v[2]<<","<<(int)b.bc_type<<"\n";
      std::ofstream g(OUT+"/tets_fine.csv"); g<<"n0,n1,n2,n3\n"; for(auto&e:mesh.elements) g<<e.v[0]<<","<<e.v[1]<<","<<e.v[2]<<","<<e.v[3]<<"\n"; }
    return 0; }
