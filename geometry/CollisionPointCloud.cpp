#include <log4cxx/logger.h>
#include <KrisLibrary/Logger.h>
#include "CollisionPointCloud.h"
#include <Timer.h>

namespace Geometry {

CollisionPointCloud::CollisionPointCloud()
  :gridResolution(0),grid(3)
{
  currentTransform.setIdentity();
}

CollisionPointCloud::CollisionPointCloud(const Meshing::PointCloud3D& _pc)
  :Meshing::PointCloud3D(_pc),gridResolution(0),grid(3)
{
  currentTransform.setIdentity();
  InitCollisions();
}

CollisionPointCloud::CollisionPointCloud(const CollisionPointCloud& _pc)
  :Meshing::PointCloud3D(_pc),bblocal(_pc.bblocal),currentTransform(_pc.currentTransform),
   gridResolution(_pc.gridResolution),grid(_pc.grid),
   octree(_pc.octree)
{}

void CollisionPointCloud::InitCollisions()
{
  bblocal.minimize();
  grid.buckets.clear();
  octree = NULL;
  if(points.empty()) 
    return;
  Assert(points.size() > 0);
  Timer timer;
  for(size_t i=0;i<points.size();i++)
    bblocal.expand(points[i]);
  //set up the grid
  Real res = gridResolution;
  if(gridResolution <= 0) {
    Vector3 dims = bblocal.bmax-bblocal.bmin;
    Real maxdim = Max(dims.x,dims.y,dims.z);
    Real mindim = Min(dims.x,dims.y,dims.z);
    //default grid size: assume points are evenly distributed on a 2D manifold
    //in space, try to get 50 points per grid cell
    Real vol = dims.x*dims.y*dims.z;
    //h^2 * n = vol
    int ptsPerCell = 50;
    Real h = Pow(vol / points.size() * ptsPerCell, 1.0/2.0);
    if(h > mindim) { 
      //TODO: handle relatively flat point clouds
    }
    res = h;
  }
  grid.hinv.set(1.0/res);
  int validptcount = 0;
  for(size_t i=0;i<points.size();i++) {
    if(IsFinite(points[i].x)) {
      Vector p(3,points[i]);
      GridSubdivision::Index ind;
      grid.PointToIndex(p,ind);
      grid.Insert(ind,&points[i]);
      validptcount++;
    }
  }
  LOG4CXX_INFO(KrisLibrary::logger(),"CollisionPointCloud::InitCollisions: "<<validptcount<<" valid points, res "<<res<<", time "<<timer.ElapsedTime());
  //print stats
  int nmax = 0;
  for(GridSubdivision::HashTable::const_iterator i=grid.buckets.begin();i!=grid.buckets.end();i++)
    nmax = Max(nmax,(int)i->second.size());
  LOG4CXX_INFO(KrisLibrary::logger(),"  "<<grid.buckets.size()<<" nonempty grid buckets, max size "<<nmax<<", avg "<<Real(points.size())/grid.buckets.size());
  timer.Reset();

  //initialize the octree, 10 points per cell, res is minimum cell size
  octree = new OctreePointSet(bblocal,10,res);
  for(size_t i=0;i<points.size();i++) {
    if(IsFinite(points[i].x))
      octree->Add(points[i],(int)i);
  }
  LOG4CXX_INFO(KrisLibrary::logger(),"  octree initialized in time "<<timer.ElapsedTime()<<"s, "<<octree->Size()<<" nodes, depth "<<octree->MaxDepth());
  //TEST: should we fit to points
  octree->FitToPoints();
  LOG4CXX_INFO(KrisLibrary::logger(),"  octree fit to points in time "<<timer.ElapsedTime());
  /*
  //TEST: method 2.  Turns out to be much slower
  timer.Reset();
  octree = new OctreePointSet(bblocal,points.size());
  octree->SplitToResolution(res);
  for(size_t i=0;i<points.size();i++)
    octree->Add(points[i],(int)i);
  octree->Collapse(10);
  LOG4CXX_INFO(KrisLibrary::logger(),"  octree 2 initialized in time "<<timer.ElapsedTime()<<"s, "<<octree->Size());
  */
}

void GetBB(const CollisionPointCloud& pc,Box3D& b)
{
  b.setTransformed(pc.bblocal,pc.currentTransform);
}

static Real gWithinDistanceTestThreshold = 0;
static GeometricPrimitive3D* gWithinDistanceTestObject = NULL;
bool withinDistanceTest(void* obj)
{
  Point3D* p = reinterpret_cast<Point3D*>(obj);
  if(gWithinDistanceTestObject->Distance(*p) <= gWithinDistanceTestThreshold)
    return false;
  return true;
}

bool WithinDistance(const CollisionPointCloud& pc,const GeometricPrimitive3D& g,Real tol)
{
  Box3D bb;
  GetBB(pc,bb);
  //quick reject test
  if(g.Distance(bb) > tol) return false;

  GeometricPrimitive3D glocal = g;
  RigidTransform Tinv;
  Tinv.setInverse(pc.currentTransform);
  glocal.Transform(Tinv);

  AABB3D gbb = glocal.GetAABB();  
  gbb.setIntersection(pc.bblocal);

  //octree overlap method
  vector<Vector3> points;
  vector<int> ids;
  pc.octree->BoxQuery(gbb.bmin-Vector3(tol),gbb.bmax+Vector3(tol),points,ids);
  for(size_t i=0;i<points.size();i++) 
    if(glocal.Distance(points[i]) <= tol) return true;
  return false;

  /*
  //grid enumeration method
  GridSubdivision::Index imin,imax;
  pc.grid.PointToIndex(Vector(3,gbb.bmin),imin);
  pc.grid.PointToIndex(Vector(3,gbb.bmax),imax);
  int numCells = (imax[0]-imin[0]+1)*(imax[1]-imin[1]+1)*(imax[2]-imin[2]+1);
  if(numCells > (int)pc.points.size()) {
    //test all points, linearly
    for(size_t i=0;i<pc.points.size();i++)
      if(glocal.Distance(pc.points[i]) <= tol) return true;
    return false;
  }
  else {
    gWithinDistanceTestThreshold = tol;
    gWithinDistanceTestObject = &glocal;
    bool collisionFree = pc.grid.IndexQuery(imin,imax,withinDistanceTest);
    return !collisionFree;
  }
  */
}

static Real gDistanceTestValue = 0;
static GeometricPrimitive3D* gDistanceTestObject = NULL;
bool distanceTest(void* obj)
{
  Point3D* p = reinterpret_cast<Point3D*>(obj);
  gDistanceTestValue = Min(gDistanceTestValue,gDistanceTestObject->Distance(*p));
  return true;
}

Real Distance(const CollisionPointCloud& pc,const GeometricPrimitive3D& g)
{
  GeometricPrimitive3D glocal = g;
  RigidTransform Tinv;
  Tinv.setInverse(pc.currentTransform);
  glocal.Transform(Tinv);

  /*
  AABB3D gbb = glocal.GetAABB();
  gDistanceTestValue = Inf;
  gDistanceTestObject = &glocal;
  pc.grid.BoxQuery(Vector(3,gbb.bmin),Vector(3,gbb.bmax),distanceTest);
  return gDistanceTestValue;
  */
  //test all points, linearly
  Real dmax = Inf;
  for(size_t i=0;i<pc.points.size();i++)
    dmax = Min(dmax,glocal.Distance(pc.points[i]));
  return dmax;
}

static Real gNearbyTestThreshold = 0;
static GeometricPrimitive3D* gNearbyTestObject = NULL;
static std::vector<Point3D*> gNearbyTestResults;
static size_t gNearbyTestBranch = 0;
bool nearbyTest(void* obj)
{
  Point3D* p = reinterpret_cast<Point3D*>(obj);
  if(gNearbyTestObject->Distance(*p) <= gNearbyTestThreshold)
    gNearbyTestResults.push_back(p);
  if(gNearbyTestResults.size() >= gNearbyTestBranch) return false;
  return true;
}

void NearbyPoints(const CollisionPointCloud& pc,const GeometricPrimitive3D& g,Real tol,std::vector<int>& pointIds,size_t maxContacts)
{
  Box3D bb;
  GetBB(pc,bb);
  //quick reject test
  if(g.Distance(bb) > tol) return;

  GeometricPrimitive3D glocal = g;
  RigidTransform Tinv;
  Tinv.setInverse(pc.currentTransform);
  glocal.Transform(Tinv);

  AABB3D gbb = glocal.GetAABB();
  gbb.setIntersection(pc.bblocal);

  //octree overlap method
  vector<Vector3> points;
  vector<int> ids;
  pc.octree->BoxQuery(gbb.bmin-Vector3(tol),gbb.bmax+Vector3(tol),points,ids);
  for(size_t i=0;i<points.size();i++) 
    if(glocal.Distance(points[i]) <= tol) {
      pointIds.push_back(ids[i]);
      if(pointIds.size()>=maxContacts) return;
    }
  return;

  /*
  //grid enumeration method
  GridSubdivision::Index imin,imax;
  pc.grid.PointToIndex(Vector(3,gbb.bmin),imin);
  pc.grid.PointToIndex(Vector(3,gbb.bmax),imax);
  int numCells = (imax[0]-imin[0]+1)*(imax[1]-imin[1]+1)*(imax[2]-imin[2]+1);
  if(numCells > (int)pc.points.size()) {
    LOG4CXX_INFO(KrisLibrary::logger(),"Testing all points\n");
    //test all points, linearly
    for(size_t i=0;i<pc.points.size();i++)
      if(glocal.Distance(pc.points[i]) <= tol) {
	pointIds.push_back(int(i));
	if(pointIds.size()>=maxContacts) return;
      }
  }
  else {
    LOG4CXX_INFO(KrisLibrary::logger(),"Testing points in BoxQuery\n");
    gNearbyTestThreshold = tol;
    gNearbyTestResults.resize(0);
    gNearbyTestObject = &glocal;
    gNearbyTestBranch = maxContacts;
    pc.grid.BoxQuery(Vector(3,gbb.bmin),Vector(3,gbb.bmax),nearbyTest);
    pointIds.resize(gNearbyTestResults.size());
    for(size_t i=0;i<points.size();i++)
      pointIds[i] = gNearbyTestResults[i] - &pc.points[0];
  }
  */
}

int RayCast(const CollisionPointCloud& pc,Real rad,const Ray3D& r,Vector3& pt)
{
  Ray3D rlocal;
  pc.currentTransform.mulInverse(r.source,rlocal.source);
  pc.currentTransform.R.mulTranspose(r.direction,rlocal.direction);
  int res = RayCastLocal(pc,rad,rlocal,pt);
  if(res >= 0) {
    pt = pc.currentTransform*pt;
  }
  return res;
}

int RayCastLocal(const CollisionPointCloud& pc,Real rad,const Ray3D& r,Vector3& pt)
{
  Real tmin=0,tmax=Inf;
  if(!((const Line3D&)r).intersects(pc.bblocal,tmin,tmax)) return -1;

  int value = pc.octree->RayCast(r,rad);
  if(value >= 0)
    pt = pc.points[value];
  return value;

  /*
  Real closest = Inf;
  int closestpt = -1;
  Sphere3D s;
  s.radius = margin;
  if(margin < 1e-3) {
    //grid rasterization method
    Segment3D slocal;
    r.eval(tmin,slocal.a);
    r.eval(tmax,slocal.b);
    //normalize to grid resolution
    slocal.a.x /= pc.grid.h[0];
    slocal.a.y /= pc.grid.h[1];
    slocal.a.z /= pc.grid.h[2];
    slocal.b.x /= pc.grid.h[0];
    slocal.b.y /= pc.grid.h[1];
    slocal.b.z /= pc.grid.h[2];
    vector<IntTriple> cells;
    Meshing::GetSegmentCells(slocal,cells);
    GridSubdivision::Index ind;
    ind.resize(3);
    for(size_t i=0;i<cells.size();i++) {
      ind[0] = cells[i].a;
      ind[1] = cells[i].b;
      ind[2] = cells[i].c;
      //const list<void*>* objs = pc.grid.GetObjectSet(ind);
      //if(!objs) continue;
      GridSubdivision::HashTable::const_iterator objs = pc.grid.buckets.find(ind);
      if(objs != pc.grid.buckets.end()) {
	for(list<void*>::const_iterator k=objs->second.begin();k!=objs->second.end();k++) {
	  Vector3* p = reinterpret_cast<Vector3*>(*k);
	  s.center = *p;
	  Real tmin,tmax;
	  if(s.intersects(r,&tmin,&tmax)) {
	    if(tmax >= 0 && tmin < closest) {
	      closest = Max(tmin,0.0);
	      closestpt = (p - &pc.points[0]);
	    }
	  }
	}
      }
    }
  }
  else {
    //brute force method
    for(size_t i=0;i<pc.points.size();i++) {
      s.center = pc.points[i];
      Real tmin,tmax;
      if(s.intersects(r,&tmin,&tmax)) {
	if(tmax >= 0 && tmin < closest) {
	  closest = Max(tmin,0.0);
	  closestpt = i;
	}
      }
    }
  }
  pt = r.eval(closest);
  return closestpt;
  */
}

} //namespace Geometry
