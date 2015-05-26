


#include "Tasklet.h"
#include <pthread.h>
#include "TaskUtility.h"
#include <vector>
#include <map>
#include <set>
#include <list>
#ifdef __cplusplus
extern "C"{
#endif

void ReportException(Tasklet::ActionDaemon *);

#ifdef __cplusplus
}
#endif
namespace Tasklet{
class DaemonSchedule{
  struct DaemonMonitor{
  };
  struct DaemonCB{
    enum{
      READY,
      ONJOB,
      EXIT
    };
    
    pthread_t self;
    ActionDaemon * dp;
    DaemonMonitor mon;
    unsigned State:4;
    DaemonCB(ActionDaemon * d):
    	self(pthread_self()),
		dp(d),
		State(READY)
      {}
    void Cancel();
    
    
  };
  MediatorOr1 * mo;
  std::map<taskid,DaemonCB *> DaemonTbl;
  taskid orig_id;
  
  DaemonSchedule();
  
  ~DaemonSchedule();
  struct FuncInitor{
    std::map<taskid,DaemonCB *> & dtbl;
    DaemonCB * dcb;
    taskid &id,ret;
    FuncInitor(std::map<taskid,DaemonCB *> & t,
    DaemonCB * d, taskid & i):dtbl(t),dcb(d),id(i),ret(-1)
    {}
    void operator()();
    
  };
  struct FuncRem{
    std::map<taskid,DaemonCB *> &tbl;
    taskid id;
    FuncRem(std::map<taskid,DaemonCB *> & t,
      taskid  i):tbl(t),id(i)
      {}
    void operator()();
  };
  static void * DaemonThread(void *);
public:
  static DaemonSchedule * Init();
  int start_daemon(DaemonRequest * task,taskid & id);
  void stop_daemon(taskid);
  bool canDo(){return true;}
  
  
    
};

DaemonSchedule::DaemonSchedule():
  mo(NULL),
  orig_id(0)
  {}
DaemonSchedule::~DaemonSchedule(){
  mo->Stop();
  delete mo;
}
void * DaemonSchedule::DaemonThread(void *v){
  DaemonCB * dcb=(DaemonCB *)(v);
  for (;;){
    dcb->dp->Run();
  }
  pthread_exit(NULL);
  return NULL;
}

void DaemonSchedule::FuncInitor::operator()(){
  pthread_attr_t attrs;
  pthread_attr_init(&attrs);
  pthread_attr_setdetachstate(&attrs,PTHREAD_CREATE_DETACHED);
  if (0==pthread_create(&dcb->self,&attrs,DaemonThread,dcb)){
    ret=id++;
    dtbl.insert(std::make_pair(ret,dcb));

  }
  pthread_attr_destroy(&attrs);
  
}

void DaemonSchedule::FuncRem::operator()(){
  std::map<taskid,DaemonCB *> ::iterator it=tbl.find(id);
  if (it!=tbl.end()){
    it->second->Cancel();
    tbl.erase(it);
  }
}

DaemonSchedule * DaemonSchedule::Init(){
  DaemonSchedule * ds=new DaemonSchedule();
  ds->mo=MediatorOr1::Instance(ds);
  return ds;
 
}
int DaemonSchedule::start_daemon(DaemonRequest * task,taskid &t){
  ScopedClient cli(mo->getClientOr1());
  DaemonCB * dcb=new DaemonCB(task->Dae);
  FuncInitor f(DaemonTbl,dcb,orig_id);
  int rc=cli->Attempt(f);
  t=f.ret;
  return rc;

 
}

void DaemonSchedule::stop_daemon(taskid tid){
  FuncRem f(DaemonTbl,tid);
  ScopedClient cli(mo->getClientOr1());
  cli->Attempt(f);
}

void DaemonSchedule::DaemonCB::Cancel(){
	pthread_cancel(self);
}

}

#ifdef __cplusplus
extern "C"{
#endif
static Tasklet::DaemonSchedule * sch=NULL;
void InitDaemon(){
  sch=Tasklet::DaemonSchedule::Init();
}

Tasklet::taskid start_daemon(Tasklet::DaemonRequest * task){
  Tasklet::taskid id;
  if (0!=sch->start_daemon(task,id)) return -1;
  return id;
}

void stop_daemon(Tasklet::taskid id){
  sch->stop_daemon(id);
}

#ifdef __cplusplus
}
#endif
