#include "EdgePlanner.h"
#include "EdgePlannerHelpers.h"
#include "InterpolatorHelpers.h"
#include <errors.h>
using namespace std;

EdgeChecker::EdgeChecker(CSpace* _space,const SmartPointer<Interpolator>& _path)
:space(_space),path(_path)
{}

EdgeChecker::EdgeChecker(CSpace* _space,const Config& a,const Config& b)
:space(_space),path(new CSpaceInterpolator(_space,a,b))
{}


TrueEdgeChecker::TrueEdgeChecker(CSpace* _space,const SmartPointer<Interpolator>& _path)
  :EdgeChecker(_space,path)
{}

TrueEdgeChecker::TrueEdgeChecker(CSpace* _space,const Config& x,const Config& y)
  :EdgeChecker(_space,x,y)
{}


FalseEdgeChecker::FalseEdgeChecker(CSpace* _space,const SmartPointer<Interpolator>& _path)
  :EdgeChecker(_space,path)
{}

FalseEdgeChecker::FalseEdgeChecker(CSpace* _space,const Config& x,const Config& y)
  :EdgeChecker(_space,x,y)
{}


EndpointEdgeChecker::EndpointEdgeChecker(CSpace* _space,const SmartPointer<Interpolator>& _path)
  :EdgeChecker(_space,path)
{}

EndpointEdgeChecker::EndpointEdgeChecker(CSpace* _space,const Config& x,const Config& y)
  :EdgeChecker(_space,x,y)
{}

bool EndpointEdgeChecker::IsVisible() { return space->IsFeasible(path->End()); }

PiggybackEdgePlanner::PiggybackEdgePlanner(SmartPointer<EdgePlanner> _e)
:EdgeChecker(_e->Space(),NULL),e(_e)
{
  EdgeChecker* ec = dynamic_cast<EdgeChecker*>(&*e);
  if(ec)
    path = ec->path;
}

PiggybackEdgePlanner::PiggybackEdgePlanner(CSpace* _space,const SmartPointer<Interpolator>& _path,SmartPointer<EdgePlanner> _e)
  :EdgeChecker(_space,_path),e(_e)
{}

PiggybackEdgePlanner::PiggybackEdgePlanner(CSpace* _space,const Config& _a,const Config& _b,SmartPointer<EdgePlanner> _e)
  :EdgeChecker(_space,_a,_b),e(_e)
{}

EdgePlanner* PiggybackEdgePlanner::Copy() const
{
  return new PiggybackEdgePlanner(space,path,e);
}

EdgePlanner* PiggybackEdgePlanner::ReverseCopy() const
{
  if(path)
   return new PiggybackEdgePlanner(space,new ReverseInterpolator(path),e->ReverseCopy());
 else
  return new PiggybackEdgePlanner(e->ReverseCopy());
}


void PiggybackEdgePlanner::Eval(Real u,Config& x)
{
  if(path) EdgeChecker::Eval(u,x);
  else e->Eval(u,x);
}

Real PiggybackEdgePlanner::Length() const
{
  if(path) return EdgeChecker::Length();
  else return e->Length();
}

const Config& PiggybackEdgePlanner::Start() const
{
  if(path) return EdgeChecker::Start();
  else return e->Start();
}
const Config& PiggybackEdgePlanner::End() const
{
  if(path) return EdgeChecker::End();
  else return e->End();
}

CSpace* PiggybackEdgePlanner::Space() const
{
  if(space) return space;
  return e->Space();
}

IncrementalizedEdgePlanner::IncrementalizedEdgePlanner(const SmartPointer<EdgePlanner>& e)
:PiggybackEdgePlanner(e),checked(false),visible(false)
{}

Real IncrementalizedEdgePlanner::Priority() const { return (checked ? 0.0 : e->Length()); }
bool IncrementalizedEdgePlanner::Plan() { if(!checked) visible = e->IsVisible(); checked=true; return false; }
bool IncrementalizedEdgePlanner::Done() const { return checked; }
bool IncrementalizedEdgePlanner::Failed() const { return checked && !visible; }
EdgePlanner* IncrementalizedEdgePlanner::Copy() const {
  IncrementalizedEdgePlanner *ie = new IncrementalizedEdgePlanner(e);
  ie->checked = checked;
  ie->visible = visible;
  return ie;
}
EdgePlanner* IncrementalizedEdgePlanner::ReverseCopy() const
{
  IncrementalizedEdgePlanner *ie = new IncrementalizedEdgePlanner(e->ReverseCopy());
  ie->checked = checked;
  ie->visible = visible;
  return ie;
}

EpsilonEdgeChecker::EpsilonEdgeChecker(CSpace* _space,const Config& _a,const Config& _b,Real _epsilon)
  :EdgeChecker(_space,_a,_b),epsilon(_epsilon)
{
  foundInfeasible = false;
  dist = Length();
  depth = 0;
  segs = 1;
  if(dist < 0) {
    fprintf(stderr,"EpsilonEdgeChecker: Warning, path has negative length?\n");
  }
}

EpsilonEdgeChecker::EpsilonEdgeChecker(CSpace* _space,const SmartPointer<Interpolator>& path,Real _epsilon)
  :EdgeChecker(_space,path),epsilon(_epsilon)
{
  foundInfeasible = false;
  dist = Length();
  depth = 0;
  segs = 1;
  if(dist < 0) {
    fprintf(stderr,"EpsilonEdgeChecker: Warning, path has negative length?\n");
  }
}


Real Log2(Real r) { return Log(r)*Log2e; }

bool EpsilonEdgeChecker::IsVisible()
{
  if(foundInfeasible) return false;
  while(dist > epsilon) {
    depth++;
    segs *= 2;
    dist *= Half;
    Real du2 = 2.0 / (Real)segs;
    Real u = du2*Half;
    for(int k=1;k<segs;k+=2,u+=du2) {
      path->Eval(u,m);
      if(!space->IsFeasible(m)) {
	foundInfeasible = true;
	return false;
      }
    }
  }
  return true;
}


EdgePlanner* EpsilonEdgeChecker::Copy() const
{
  EpsilonEdgeChecker* p=new EpsilonEdgeChecker(space,path,epsilon);
  p->depth=depth;
  p->segs=segs;
  p->dist=dist;
  p->foundInfeasible = foundInfeasible;
  return p;
}

EdgePlanner* EpsilonEdgeChecker::ReverseCopy() const
{
  EpsilonEdgeChecker* p=new EpsilonEdgeChecker(space,new ReverseInterpolator(path),epsilon);
  p->depth=depth;
  p->segs=segs;
  p->dist=dist;
  p->foundInfeasible = foundInfeasible;
  return p;
}

Real EpsilonEdgeChecker::Priority() const { return dist; }

bool EpsilonEdgeChecker::Plan() 
{
  if(foundInfeasible || dist <= epsilon) return false;
  depth++;
  segs *= 2;
  dist *= Half;
  Real du2 = 2.0 / (Real)segs;
  Real u = du2*Half;
  for(int k=1;k<segs;k+=2,u+=du2) {
    path->Eval(u,m);
    if(!space->IsFeasible(m)) {
      dist = 0;
      foundInfeasible=true;
      return false;
    }
  }
  return true;
}

bool EpsilonEdgeChecker::Done() const { return dist <= epsilon; }

bool EpsilonEdgeChecker::Failed() const { return foundInfeasible; }  






ObstacleDistanceEdgeChecker::ObstacleDistanceEdgeChecker(CSpace* _space,const Config& _a,const Config& _b)
  :EdgeChecker(_space,_a,_b)
{}

ObstacleDistanceEdgeChecker::ObstacleDistanceEdgeChecker(CSpace* _space,const SmartPointer<Interpolator>& path)
  :EdgeChecker(_space,path)
{}

bool ObstacleDistanceEdgeChecker::IsVisible()
{
  const Config& a = path->Start();
  const Config& b = path->End();
  return CheckVisibility(path->ParamStart(),path->ParamStart(),a,b,space->ObstacleDistance(a),space->ObstacleDistance(b));
}

bool ObstacleDistanceEdgeChecker::CheckVisibility(Real ua,Real ub,const Config& a,const Config& b,Real da,Real db)
{
  Real dmin = Min(da,db);
  if(dmin <= 0) {
    fprintf(stderr,"ObstacleDistanceEdgeChecker: being used when space doesn't properly implement ObstacleDistance()\n");
    return false;
  }
  if(dmin < Epsilon) {
    cout<<"Warning, da or db is close to zero"<<endl;
    return false;
  }
  Real r = space->Distance(a,b);
  Assert(r >= Zero);
  if(dmin > r) return true;
  Config m;
  Real um = (ua+ub)*0.5;
  path->Eval(um,m);
  if(!space->IsFeasible(m)) return false;
  Real ram = space->Distance(a,m);
  Real rbm = space->Distance(b,m);
  Assert(ram < r*0.9 && ram > r*0.1);
  Assert(rbm < r*0.9 && rbm > r*0.1);
  Real dm = space->ObstacleDistance(m);
  Assert(dm >= Zero);
  return CheckVisibility(ua,um,a,m,da,dm)
    && CheckVisibility(um,ub,m,b,dm,db);
}

EdgePlanner* ObstacleDistanceEdgeChecker::Copy() const
{
  ObstacleDistanceEdgeChecker* p=new ObstacleDistanceEdgeChecker(space,path);
  return p;
}

EdgePlanner* ObstacleDistanceEdgeChecker::ReverseCopy() const
{
  ObstacleDistanceEdgeChecker* p=new ObstacleDistanceEdgeChecker(space,new ReverseInterpolator(path));
  return p;
}






BisectionEpsilonEdgePlanner::BisectionEpsilonEdgePlanner(CSpace* _space,const Config& a,const Config& b,Real _epsilon)
  :space(_space),epsilon(_epsilon)
{
  path.push_back(a);
  path.push_back(b);
  Segment s;
  s.prev = path.begin();
  s.length = space->Distance(a,b);
  q.push(s);
}


BisectionEpsilonEdgePlanner::BisectionEpsilonEdgePlanner(CSpace* _space,Real _epsilon)
  :space(_space),epsilon(_epsilon)
{}

Real BisectionEpsilonEdgePlanner::Length() const
{
  Real len = 0.0;
  const Config* prev = &(*path.begin());
  for(list<Config>::const_iterator i=++path.begin();i!=path.end();i++) {
    len += space->Distance(*prev,*i);
    prev = &(*i);
  }
  return len;
}

const Config& BisectionEpsilonEdgePlanner::Start() const { return path.front(); }
const Config& BisectionEpsilonEdgePlanner::End() const { return path.back(); }

bool BisectionEpsilonEdgePlanner::IsVisible()
{
  while(!Done()) {
    if(!Plan()) return false;
  }
  return true;
}

void BisectionEpsilonEdgePlanner::Eval(Real u,Config& x) const
{
  //if(!Done()) cout<<"Warning, edge planner not done!"<<endl;
  if(IsNaN(u) || u < 0 || u > 1) {
    cout<<"Uh... evaluating path outside of [0,1] range"<<endl;
    cout<<"u="<<u<<endl;
    getchar();
  }
  Assert(u >= Zero && u <= One);
  Real dt = One/(Real)(path.size()-1);
  Real t=Zero;
  list<Config>::const_iterator i=path.begin();
  while(t+dt < u) {
    t+=dt;
    i++;
    if(i == path.end()) { cout<<"End of path, u="<<u<<endl; x=path.back(); return; }
  }
  Assert(t<=u);
  if(t==u) { x=*i; }
  else {
    list<Config>::const_iterator n=i; n++;
    if(n != path.end()) {
      space->Interpolate(*i,*n,(u-t)/dt,x);
    }
    else {
      x = *i;
    }
  }
}

EdgePlanner* BisectionEpsilonEdgePlanner::Copy() const
{
  if(path.size() == 2 && q.size()==1) {  //uninitialized
    return new BisectionEpsilonEdgePlanner(space,path.front(),path.back(),epsilon);
  }
  else {
    BisectionEpsilonEdgePlanner* p=new BisectionEpsilonEdgePlanner(space,epsilon);
    p->path = path;
    if(!Done()) {
      cout<<"Warning: making a copy of a bisection edge planner that is not done!"<<endl;
      Segment s;
      s.prev = p->path.begin();
      s.length = space->Distance(path.front(),path.back());
      p->q.push(s);
      Assert(!p->Done());
      //cout<<"Press any key to continue..."<<endl;
      //getchar();
    }
    return p;
  }
}

EdgePlanner* BisectionEpsilonEdgePlanner::ReverseCopy() const
{
  BisectionEpsilonEdgePlanner* p=new BisectionEpsilonEdgePlanner(space,epsilon);
  p->path.resize(path.size());
  reverse_copy(path.begin(),path.end(),p->path.begin());
  return p;
}

Real BisectionEpsilonEdgePlanner::Priority() const
{
  if(q.empty()) return 0;
  return q.top().length;
}

bool BisectionEpsilonEdgePlanner::Plan()
{
  Segment s=q.top(); q.pop();
  list<Config>::iterator a=s.prev, b=a; b++;
  space->Midpoint(*a,*b,x);
  if(!space->IsFeasible(x)) { 
    //printf("Midpoint was not feasible\n");
    s.length=Inf; q.push(s); return false;
  }
  list<Config>::iterator m=path.insert(b,x);

  if(q.size()%100 == 0 &&
     Real(q.size())*epsilon > 4.0*space->Distance(Start(),End())) {
    s.length = Inf;
    q.push(s);
    cout<<"BisectionEpsilonEdgePlanner: Over 4 times as many iterations as needed, quitting."<<endl;
    cout<<"Original length "<<space->Distance(Start(),End())<<", epsilon "<<epsilon<<endl;
    return false;
  }
  //insert the split segments back in the queue
  Real l1=space->Distance(*a,x);
  Real l2=space->Distance(x,*b);
  if(l1 > 0.9*s.length || l2 > 0.9*s.length) {
    //printf("Midpoint exceeded 0.9x segment distance: %g, %g > 0.9*%g\n",l1,l2,s.length);
    //cout<<"a = "<<*a<<endl;
    //cout<<"b = "<<*b<<endl;
    //cout<<"Midpoint "<<x<<endl;
    //getchar();
    s.length = Inf;
    q.push(s);
    return false;
  }
  s.prev = a;
  s.length = l1;
  if(s.length > epsilon) q.push(s);

  s.prev = m;
  s.length = l2;
  if(s.length > epsilon) q.push(s);
  return true;
}

bool BisectionEpsilonEdgePlanner::Plan(Config*& pre,Config*& post)
{
  Segment s=q.top(); q.pop();
  list<Config>::iterator a=s.prev, b=a; b++;
  space->Midpoint(*a,*b,x);

  //in case there's a failure...
  pre = &(*a); 
  post = &(*b);

  if(!space->IsFeasible(x))  { s.length=Inf; q.push(s); return false; }
  list<Config>::iterator m=path.insert(b,x);

  //insert the split segments back in the queue
  Real l1=space->Distance(*a,x);
  Real l2=space->Distance(x,*b);
  if(Abs(l1-l2) > 0.9*s.length) {
    s.length = Inf;
    q.push(s); 
    return false;
  }
  s.prev = a;
  s.length = l1;
  if(s.length > epsilon) q.push(s);

  s.prev = m;
  s.length = l2;
  if(s.length > epsilon) q.push(s);
  return true;
}

bool BisectionEpsilonEdgePlanner::Done() const
{
  return q.empty() || q.top().length <= epsilon || IsInf(q.top().length);
}

bool BisectionEpsilonEdgePlanner::Failed() const
{
  if(q.empty()) return false;
  return IsInf(q.top().length);
}








PathEdgeChecker::PathEdgeChecker(CSpace* _space,const std::vector<SmartPointer<EdgePlanner> >& _path)
:space(_space),path(_path),progress(0),foundInfeasible(false)
{}
void PathEdgeChecker::Eval(Real u,Config& x) const
{
  Real s =Floor(u*path.size());
  int seg = int(s);
  if(seg < 0) x = path.front()->Start();
  else if(seg >= (int)path.size()) x = path.back()->End();
  else
    path[seg]->Eval(s - seg,x);
}
Real PathEdgeChecker::Length() const
{
  Real l = 0;
  for(size_t i=0;i<path.size();i++) l += path[i]->Length();
  return l;
}
const Config& PathEdgeChecker::Start() const { return path.front()->Start(); }
const Config& PathEdgeChecker::End() const { return path.back()->End(); }
bool PathEdgeChecker::IsVisible()
{
  while(progress < path.size()) {
    if(!path[progress]->IsVisible()) {
      foundInfeasible = true;
      return false;
    }
    progress++;
  }
  return true;
}
EdgePlanner* PathEdgeChecker::Copy() const
{
  return new PathEdgeChecker(space,path);
}

EdgePlanner* PathEdgeChecker::ReverseCopy() const
{
  vector<SmartPointer<EdgePlanner> > rpath(path.size());
  for(size_t i=0;i<path.size();i++)
    rpath[path.size()-i-1] = path[i]->ReverseCopy();
  return new PathEdgeChecker(space,rpath);
}

Real PathEdgeChecker::Priority() const
{
  return path.size()-progress;
}
bool PathEdgeChecker::Plan()
{
  if(foundInfeasible) return false;
  if(progress < path.size()) {
    if(!path[progress]->IsVisible()) {
      foundInfeasible = true;
      return false;
    }
    progress++;
  }
  return (progress < path.size());
}
bool PathEdgeChecker::Done() const
{
  return progress >= path.size() || foundInfeasible;
}

bool PathEdgeChecker::Failed() const
{
  return progress < path.size() && foundInfeasible;
}


MultiEdgePlanner::MultiEdgePlanner(CSpace* space,const SmartPointer<Interpolator>& path,const std::vector<SmartPointer<EdgePlanner> >& components)
:PiggybackEdgePlanner(space,path,new PathEdgeChecker(space,components))
{}



EdgePlannerWithCSpaceContainer::EdgePlannerWithCSpaceContainer(const SmartPointer<CSpace>& space,const SmartPointer<EdgePlanner>& e)
  :PiggybackEdgePlanner(e),spacePtr(space)
{
}

EdgePlanner* EdgePlannerWithCSpaceContainer::Copy() const
{
  return new EdgePlannerWithCSpaceContainer(spacePtr,e->Copy());
}

EdgePlanner* EdgePlannerWithCSpaceContainer::ReverseCopy() const
{
  return new EdgePlannerWithCSpaceContainer(spacePtr,e->ReverseCopy());
}

