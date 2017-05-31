#include <log4cxx/logger.h>
#include <KrisLibrary/Logger.h>
#include "RigidRobot2DCSpace.h"
#include "CSpaceHelpers.h"
#include "EdgePlannerHelpers.h"
#include <GLdraw/GL.h>
#include <math/angle.h>
#include <math/random.h>
#include <math/sample.h>

RigidRobot2DCSpace::RigidRobot2DCSpace()
{
  SetAngleWeight(0.3);
  SetDomain(domain.bmin,domain.bmax);
  visibilityEpsilon = 0.01;
  domain.bmin.set(0,0);
  domain.bmax.set(1,1);
}

void RigidRobot2DCSpace::InitConstraints()
{
  SetDomain(domain.bmin,domain.bmax);
  for(int i=0;i<obstacles.NumObstacles();i++) {
    char buf[64];
    sprintf(buf,"%s[%d]",obstacles.ObstacleTypeName(i),obstacles.ObstacleIndex(i));
    CSpace::AddConstraint(buf,new Geometric2DObstacleFreeSet(obstacles.Obstacle(i),robot,false));
  }
}


void RigidRobot2DCSpace::DrawWorkspaceGL() const
{
  //blank out background (light yellow)
  glColor3f(1,1,0.5);
  glBegin(GL_QUADS);
  glVertex2f(domain.bmin.x,domain.bmin.y);
  glVertex2f(domain.bmax.x,domain.bmin.y);
  glVertex2f(domain.bmax.x,domain.bmax.y);
  glVertex2f(domain.bmin.x,domain.bmax.y);
  glEnd();

  //draw obstacles (dark grey)
  glColor3f(0.2,0.2,0.2);
  obstacles.DrawGL();
}

void RigidRobot2DCSpace::DrawRobotGL(const Config& x) const
{
  RigidTransform2D T;
  T.t.set(x(0),x(1));
  T.R.setRotate(x(2));
  Geometric2DCollection temp = robot;
  temp.Transform(T);

  //draw robot greyish blue
  temp.DrawGL();
  //draw outline
  glPushAttrib(GL_CURRENT_BIT | GL_LINE_BIT);
  glLineWidth(1.0);
  glColor3f(0,0,0);
  temp.DrawOutlinesGL();
  glPopAttrib();

}

void RigidRobot2DCSpace::DrawGL(const Config& x) const
{
  DrawWorkspaceGL();

  glColor3f(0.3,0.3,0.7);
  DrawRobotGL(x);
}

EdgePlanner* RigidRobot2DCSpace::PathChecker(const Config& a,const Config& b)
{
  return new EpsilonEdgeChecker(this,a,b,visibilityEpsilon);
}

EdgePlanner* RigidRobot2DCSpace::PathChecker(const Config& a,const Config& b,int obstacle)
{
  CSpace* space = new SubsetConstraintCSpace(this,obstacle);
  return new EdgePlannerWithCSpaceContainer(space,new EpsilonEdgeChecker(space,a,b,visibilityEpsilon));
}

void RigidRobot2DCSpace::Properties(PropertyMap& map)
{
  SE2CSpace::Properties(map);
}
