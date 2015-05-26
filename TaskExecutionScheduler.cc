/*
 * TaskExecutionScheduler.cc
 *
 *  Created on: 2015-4-29
 *      Author: Administrator
 */

#include "TaskExecutionScheduler.h"
#include <assert.h>
#include <unistd.h>
namespace{
::useconds_t interval=20*1000;
struct TWOVAL{
  enum{
    NO=0,
    YES
  };
};

struct SchedState{
  enum{
    WORKING,
    EMPTY
  };
};
}
namespace Tasklet{

/*
 * Register part:
 */
TaskExecScheduler * TaskExecScheduler::Inst=NULL;
TaskExecScheduler::RegisterBus::RegisterBus(MediatorOr1 *optr ):
    Mo(optr),
	Last(pthread_self()),
	Current(Last),
    Idles(MaxIDLE)
{}
inline void TaskExecScheduler::RegisterBus::ResetIdle(){
  Idles=MaxIDLE;
}
int TaskExecScheduler::RegisterMe(uunit_t Prior){

	assert(0==State.NONEREG);
	if (!EntRepo.canDo()) return Universal::NEG1;

	RegisterFunc funcR(*EntRepo,Prior,pthread_self(),RBus->Current);


	if (funcR.hasSame()) return Universal::NEG1;

	ScopedClient sc(RBus->Mo->getClientOr1());
	int rc=(*sc).Attempt(funcR);
	if (rc==Universal::SUCCESS)
		return funcR.ret;
	return rc;
}

bool TaskExecScheduler::RegisterFunc::hasSame(){
	std::size_t i;
	for (i=0;i<Repo.size()&&Repo[i]->Name!=m;i++);
	return i!=Repo.size();
}

void TaskExecScheduler::RegisterFunc::operator()(){

	if (hasSame()){
		ret=Universal::NEG1;
	}else{
	  current=m;
		Repo.push_back(new  EntryCtrlBlock(Prior));
	}
}





TaskExecScheduler::TaskExecScheduler():
		TickCounter(0),
		Runtimes(max),
		RBus(NULL),
		schedId(pthread_self())
		{}



int TaskExecScheduler::doInit(){
	RBus=new RegisterBus(MediatorOr1::Instance(&EntRepo));
	int rc=pthread_create(&schedId,NULL,Schedule,this);
	State.ENVIR=1;
	for (;State.SCHEDULE==0;sched_yield());
	return rc;
}

int TaskExecScheduler::Init(){
	TaskExecScheduler * sp=new TaskExecScheduler;
	return sp->doInit();
}

void * TaskExecScheduler::Schedule(void * v){
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
	TaskExecScheduler * ts=(TaskExecScheduler *) v;
	ts->State.SCHEDULE=1;
	for(;SchedulerState::EXIT!=ts->Scheduling();usleep(interval));
	pthread_exit(NULL);


}
void TaskExecScheduler::FinalizeReg(){
	State.NONEREG=1;
}


void TaskExecScheduler::ScheduleRegister(){
  if (NULL!=RBus){
    if ((State.NONEREG==1)&&(NULL!=RBus)){
      if (RBus->Last==RBus->Current){
        RBus->Idles--;
      }else {
        RBus->Idles=RegisterBus::MaxIDLE;
      }
      if (RBus->Idles==0){
        RBus->Mo->Stop();
        delete RBus;
        RBus=NULL;
      }
    }
    RBus->Last=RBus->Current;
  }

}

bool TaskExecScheduler::EntryRepo::canDo(){
  return Entries.size()<max;
}

bool TaskExecScheduler::EntryRepo::SeekMe(std::size_t &r,
  unit_t & now)
{
  std::size_t j;
  now=0;
  bool ret=false;
  pthread_t myName=pthread_self();
  unit_t cur;
 
  for (j=0;j<Entries.size()&&Entries[j]->Name!=myName;j++){
   cur=Entries[j]->sender.Ticket;
   if (cur>now)     now=cur;
  }
  ret=j<Entries.size();
  if (!ret) return ret;
  r=j;
  for (;j<Entries.size();j++){
    cur=Entries[j]->sender.Ticket;
    if (cur>now)     now=cur;
  }
  
  return ret;
}

void TaskExecScheduler::Cleanup_Handler(void * v)
{
  RunningJob * rInst=(RunningJob *)v;
  RuntimeJob * job=const_cast<RuntimeJob *>(rInst->recver.jpoi);
  if (rInst->recver.state==RunningJob::recvChannel::ONDUTY)
    rInst->recver.state=RunningJob::recvChannel::CANCEL;
  if (job->JobState!=RuntimeJob::ACCOMP){
    job->JobState=RuntimeJob::CANCEL;
    job->Act->Report(job->tid,EventCode::CANCEL,job->res);
  }

}

void * TaskExecScheduler::RunningTask(void * v){
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
  RunningJob * rInst=(RunningJob *)v;
  pthread_cleanup_push(Cleanup_Handler,v);

 
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
  for (sched_yield();(rInst->sender.state!=RunningJob::sndChannel::EMPTY)
  &&(rInst->sender.jpoi!=NULL);sched_yield()){
  
    if (rInst->sender.jpoi!=rInst->recver.jpoi){
      rInst->recver.state=RunningJob::recvChannel::ONDUTY;
      RuntimeJob * jp=
      const_cast<RuntimeJob*>(rInst->recver.jpoi=rInst->sender.jpoi);
      jp->JobState=RuntimeJob::DOING;
      rInst->start=Inst->Now();
      rInst->recver.jpoi->DoJob();
      rInst->recver.state=RunningJob::recvChannel::FREE;
      jp->JobState=RuntimeJob::ACCOMP;
    }

  }
  rInst->recver.state=RunningJob::recvChannel::END;
  pthread_cleanup_pop(0);

  pthread_exit(NULL);
  return NULL;

}

int TaskExecScheduler::RunTask(RuntimeCB &cb){
  return pthread_create(&cb.SelfId,NULL,RunningTask,cb.Cur);
}
TaskExecScheduler::RunningJob::RunningJob(RuntimeJob * job):
  recver(job),
  sender(job)
  {}
  
void TaskExecScheduler::RunningJob::Reset(RuntimeJob *job){
  sender.jpoi=job;
  sender.state=sndChannel::WAIT;
  recver.jpoi=NULL;
  recver.state=recvChannel::FREE;
}

int TaskExecScheduler::RuntimeCB::Schedule(Tick_t & expired,
  Collector &gc){
  int rc=0;
  RuntimeJob *jp;
  pthread_t * pid;
  if (Cur!=NULL){
    switch(Cur->recver.state){
    case RunningJob::recvChannel::FREE:
      if (bus.Dequeue(&jp)==QueueState::EMPTY){
        
        Cur->sender.state=RunningJob::sndChannel::EMPTY;
      }else {
        Cur->sender.jpoi=jp;
      }
      break;
    case RunningJob::recvChannel::ONDUTY:
      expired=Inst->Now()-Cur->start;

      break;
    case RunningJob::recvChannel::CANCEL:
    case RunningJob::recvChannel::END:
      pid=new pthread_t;
      *pid=SelfId;
      gc.pendings.Enqueue(pid);
      rc=TWOVAL::YES;
      if (bus.Dequeue(&jp)!=QueueState::EMPTY){
        Cur->Reset(jp);
        RunTask(*this);
        rc=3;
      }else{
        delete Cur;
        Cur=NULL;
      }
      break;
    }

  }
  return rc;

}

//Recycle actions;

void * TaskExecScheduler::RecycleThread(void *v){
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
  Collector * col=(Collector *)v;
  pthread_t * pid;
  while (col->pendings.Dequeue(&pid)!=QueueState::EMPTY){
    pthread_join(*pid,NULL);
    delete pid;
  }
  pthread_attr_destroy(&col->attr);
  pthread_exit(NULL);
  col->state=0;
  return NULL;
}

int TaskExecScheduler::DoRecycle()
{
  pthread_attr_init(&GarbageCollector.attr);
  pthread_attr_setdetachstate(&GarbageCollector.attr,
      PTHREAD_CREATE_DETACHED);
  return pthread_create(&GarbageCollector.pid,
      &GarbageCollector.attr,RecycleThread,&GarbageCollector);


}
taskid TaskExecScheduler::EntryCtrlBlock::genId(){
  taskid ret=rawid;
  rawid=(rawid+1)%20000000;
  return ret;
}

int TaskExecScheduler::DoScheduleEntry(AsyncCallRequest *req,taskid &id){
  unit_t cTick;
  std::size_t myid;
  int rc=RetCode::INVALID;
  if (!EntRepo.SeekMe(myid,cTick)) return rc;

  RuntimeJob * rj=new RuntimeJob(req->Exc);

  EntryCtrlBlock * cb=(*EntRepo)[myid];
  rc=RetCode::OK;
  if (cb->sender.Ticket>=0){
    if (QueueState::SUCCESS!=cb->queue.Enqueue(rj)){
      rc=RetCode::BUSY;
    }else{
      id=rj->tid=cb->genId();
      cb->taskTbl.insert(std::make_pair(id,rj));
    }
  }else {
    cTick=cb->sender.Ticket=cTick+cb->recver.Prior;
    EntryVector & ent=*EntRepo;
    id=rj->tid=cb->genId();
    cb->taskTbl.insert(std::make_pair(id,rj));
    rj->EntId=myid;
    std::size_t i;
    for (i=0;i!=ent.size();sched_yield()){
      for(i=0;i<(ent.size());i++){
        if (ent[i]->sender.Ticket<cTick){
          break;
        }else if ((ent[i]->sender.Ticket==cTick)&&(i<myid)){
          break;
        }
      }
    }
    for (i=0;(i<Runtimes.size())&&(Runtimes[i].Cur!=NULL);i++);
    if (i<Runtimes.size()){
      Runtimes[i].Cur=new RunningJob(rj);
      if (RunTask(Runtimes[i])!=0)
        rc=RetCode::UNKNOWN;

    }else {
      cb->queue.Enqueue(rj);
    }
  }
  std::map<taskid,RuntimeJob *>::iterator  it;
  for (it=cb->taskTbl.begin();it!=cb->taskTbl.end();)
  {
    rj=it->second;
    if (rj->JobState>RuntimeJob::DOING)
      cb->taskTbl.erase(it++);
    
  }
  return rc;

}


void TaskExecScheduler::ScheduleEntry(){
  std::size_t i;
  EntryVector & e=*EntRepo;
  unit_t ticket;
  for (i=0;i<e.size();i++){
    if (!e[i]->queue.isEmpty()){
      
      ticket=e[i]->recver.Ticket;
      std::list<std::size_t>::iterator it;
      for (it=SchedBuf.senders.begin();
          (it!=SchedBuf.senders.end())&&(ticket<e[*it]->recver.Ticket);
          it++);
      SchedBuf.senders.insert(it,i);
    }
  }
  return;

}

void TaskExecScheduler::ScheduleEntity(){
  Tick_t t;
  for (std::size_t i=0;i<Runtimes.size();i++){
    switch (Runtimes[i].Schedule(t,GarbageCollector)){
    case 0:
      SchedBuf.Cand.push_back(i);
      break;
    case 3:
      SchedBuf.bestCand.push_back(i);
      break;
    case 2:
      SchedBuf.GoodCand.push_back(i);
      break;
    default:
      SchedBuf.NotGood.push_back(i);
    }
  }
}

int TaskExecScheduler::Scheduling(){
  ScheduleEntity();
  ScheduleEntry();
  DoRecycle();
  return 0;


}


void TaskExecScheduler::Migrate(){

}

void TaskExecScheduler::RuntimeJob::DoJob()
{
  Act->DoThis(res);
  Act->Report(tid,EventCode::EXEC,res);
  JobState=ACCOMP;
}

taskid TaskExecScheduler::tryAction(AsyncCallRequest *req){
  taskid ret=-1;
  if (DoScheduleEntry(req,ret)==0)return ret;
  return -1;
}

void TaskExecScheduler::Cancel(taskid id){
  std::size_t i;
  unit_t now;
  if (EntRepo.SeekMe(i,now)){
    EntryCtrlBlock * ecb=EntRepo.Entries[i];
    std::map<taskid,RuntimeJob *>::iterator it=ecb->taskTbl.find(id);
    if (it!=ecb->taskTbl.end()){
      it ->second->ExtBanner=1;
      ecb->taskTbl.erase(id);
    }
  }
}


}


