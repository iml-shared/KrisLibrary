#include <log4cxx/logger.h>
#include <KrisLibrary/Logger.h>
#include "EZTrace.h"
#include <errors.h>
using namespace std;

#define MAXBUF 4096

EZTrace::EZTrace()
  :dumpStats(true),dumpTrace(true)
{
  Assert(curTrace == NULL);
  curTrace = &myTrace;
}

EZTrace::~EZTrace()
{
  LOG4CXX_INFO(KrisLibrary::logger(),"Destroying EZTrace object..."<<"\n");
  Assert(curTrace != NULL);
  Assert(curTrace == &myTrace);
  if(dumpTrace) {
    LOG4CXX_INFO(KrisLibrary::logger(),"********* Program execution trace: **********"<<"\n");
    curTrace->DumpTrace(cout);
    LOG4CXX_INFO(KrisLibrary::logger(),"*********************************************"<<"\n");
  }
  if(dumpStats) {
    LOG4CXX_INFO(KrisLibrary::logger(),"********** Function call stats: *************"<<"\n");
    curTrace->DumpStats(cout);
    LOG4CXX_INFO(KrisLibrary::logger(),"*********************************************"<<"\n");
  }
  curTrace = NULL;
}

Trace* EZTrace::curTrace=NULL;

EZCallTrace::EZCallTrace(const char* name)
{
  if(EZTrace::curTrace != NULL) {
    EZTrace::curTrace->Call(name);
    this->name = name;
    this->retval = "void";
  }
}

EZCallTrace::EZCallTrace(const char* name,const char* fmt,...)
{
  if(EZTrace::curTrace != NULL) {
    char buf [MAXBUF];
    va_list args;
    va_start(args, fmt);
#ifdef _WIN32
    _vsnprintf(buf, MAXBUF, fmt, args);
#else
    vsnprintf(buf, MAXBUF, fmt, args);
#endif
    EZTrace::curTrace->Call(name,buf);
    this->name = name;
    this->retval = "void";
  }
}

EZCallTrace::~EZCallTrace()
{
  if(EZTrace::curTrace) 
    EZTrace::curTrace->EndCall(name.c_str(),retval.c_str());
}
