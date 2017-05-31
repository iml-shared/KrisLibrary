#include <log4cxx/logger.h>
#include <KrisLibrary/Logger.h>
#include "GLDisplayList.h"
#include "GL.h"
#include <errors.h>
#include <stdio.h>

using namespace GLDraw;

static int gNumDisplayLists = 0;

GLDisplayList::GLDisplayList(int _count)
  :count(_count)
{}

GLDisplayList::~GLDisplayList()
{
  erase();
}


bool GLDisplayList::isCompiled() const 
{
  return id != NULL;
}

void GLDisplayList::beginCompile(int index)
{
  if(id == NULL) {
    id = new int;
    *id = glGenLists(count);
    gNumDisplayLists += count;
    if(gNumDisplayLists > 3000)
      LOG4CXX_WARN(KrisLibrary::logger(),"Warning, compiling new OpenGL display list id "<<*id<<", total number "<<gNumDisplayLists);
  }
  glNewList(*id+index,GL_COMPILE);
}

void GLDisplayList::endCompile()
{
  if(id == NULL) return;
  //LOG4CXX_INFO(KrisLibrary::logger(),"End compile,  list "<<*id);
  glEndList();
}

void GLDisplayList::call(int index) const
{
  if(id == NULL) return;
  //LOG4CXX_INFO(KrisLibrary::logger(),"Calling list "<<*id+index);
  glCallList(*id+index);
}

void GLDisplayList::callAll() const
{
  if(id == NULL) return;
  for(int i=0;i<count;i++)
    glCallList(*id+i);
}

void GLDisplayList::erase()
{
  if(id && id.getRefCount()==1) {
    //LOG4CXX_INFO(KrisLibrary::logger(),"Erasing OpenGL display list "<<*id);
    glDeleteLists(*id,count);
    gNumDisplayLists -= count;
  }
  //else if(id)
    //LOG4CXX_INFO(KrisLibrary::logger(),"Not yet erasing OpenGL display list "<<*id<<" has ref count "<<id.getRefCount());

  id=NULL;
}
