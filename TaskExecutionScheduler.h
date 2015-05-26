/*
 * TaskExecutionScheduler.h
 *
 *  Created on: 2015-4-29
 *      Author: Administrator
 */

#ifndef TASKEXECUTIONSCHEDULER_H_
#define TASKEXECUTIONSCHEDULER_H_
#include "Tasklet.h"
#include<pthread.h>
#include "TaskUtility.h"
#include <vector>
#include <map>
#include <set>
#include <list>

namespace Tasklet{
/*
 * pthread_t unique -> it can be use to distinguish
 */

typedef unsigned long long Tick_t;
typedef unsigned char uunit_t;
typedef char unit_t;
class TaskExecScheduler{
	static const int max=40;
	
	 // counter of ticktac

	struct MonitorEntry{
		Tick_t RequestTimes,CanceledTimes;
	};

	struct TicketDefault{
		enum{
			None=-1
		};
	};
	struct EntryAdmin{
		enum{
			INSVC
		};
	};
	struct SchedulerState{
		enum{
			EXIT=-1
		};
	};
	struct RuntimeJob{
	  enum{
	    READY,
	    DOING,
	    ACCOMP,
	    CANCEL
	  };
		
		ActionExec * Act;
		std::string res;
		std::size_t EntId;
		taskid tid;
		volatile unsigned JobState:4;
		volatile unsigned ExtBanner:1;
		RuntimeJob(ActionExec * a):
		  Act(a),
		  EntId(std::string::npos),
		  tid(0),
		  JobState(READY),
		  ExtBanner(0)
		  {}
		void DoJob();
		
	};
	struct EntryCtrlBlock{
		pthread_t Name;
		struct sndChannel{
		  volatile unit_t Ticket;
		  sndChannel():
		    Ticket(-1)
		    {}
		} sender;
		struct rcvChannel{
		  enum{
		    FREE,
		    TRUSTY
		  };
		  volatile unit_t Ticket;
		  uunit_t Prior;
		  volatile unsigned state:4;
		  rcvChannel(uunit_t p):
		    Ticket(-1),
			Prior(p),
		    state(FREE)
		    {}
		
		}recver;
		taskid rawid;
		uunit_t PrimPrior;
		volatile unit_t ObjState;
		DistReqQueue<RuntimeJob ,max> queue;
		MonitorEntry Mon;
		std::map<taskid,RuntimeJob *>  taskTbl;
		EntryCtrlBlock(uunit_t p):
			Name(pthread_self()),
			sender(),
			recver(p),
			rawid(0),
			PrimPrior(p),
			ObjState(EntryAdmin::INSVC)
		{}
		RuntimeJob * enter_request(ActionExec * act,taskid &);
		void cancel_request(taskid);
		void table_arrange();
		taskid genId();

	};

	
	struct RunningJob{
	  
		Tick_t start;
		struct recvChannel{
		   RuntimeJob * volatile jpoi;
		  enum {
		    FREE=0,
		    ONDUTY=1,
		    CANCEL=2,
		    END=3
		  };
		  volatile unsigned state:3;
		  recvChannel(RuntimeJob *j):
		    jpoi(NULL),
		    state(FREE)
		    {}
		}recver;
		struct sndChannel{
		   RuntimeJob * volatile jpoi;
		  enum{
		    EMPTY=0,
		    WAIT=1
		  };
		  volatile unsigned state:2;
		  sndChannel(RuntimeJob *j):
		    jpoi(j),
		    state(WAIT)
		    {}
		  
		}sender;
		RunningJob(RuntimeJob * );
		void Reset(RuntimeJob *);


	};
	struct MonitorRuntime{
		Tick_t maxRun;
		std::set<uunit_t> threads; //registered senders
	};
	struct Collector{
	  pthread_t pid;
	  DistReqQueue<pthread_t  ,max +1> pendings;
	  pthread_attr_t attr;
	  unsigned state:1;
	  Collector():
		  pid(pthread_self()),
		  state(0)
	  {}
	} GarbageCollector;
	friend struct RunTimeCB;
	struct RuntimeCB{
	  enum{
	    FREE,
	    BUSY
	  };

		pthread_t SelfId;
		RunningJob * volatile Cur; //Current State;
		DistReqQueue<RuntimeJob,max> bus;

		MonitorRuntime Mon;
		int Schedule(Tick_t &,Collector &);
		RuntimeCB();
		void DoJob(RuntimeJob *j);
	};


	typedef std::vector<EntryCtrlBlock *> EntryVector;
	typedef std::vector<RuntimeCB> RuntimeVector;

	
	struct EntryRepo{
		EntryVector Entries;
		EntryRepo()
		{
		  Entries.reserve(max);
		}
		bool canDo();
		bool SeekMe(std::size_t & r,unit_t &now);
		EntryVector & operator*(){return Entries;}
		

	} EntRepo; //entry Repository
	struct RegisterFunc{
		EntryVector &Repo;
		uunit_t  Prior;
		int ret;
		pthread_t m;
		pthread_t & current;
		RegisterFunc(EntryVector & r, uunit_t p,
				pthread_t mi,pthread_t &c):
			Repo(r),
			Prior(p),
			ret(-1),
			m(mi),
			current(c)
		{}
		void operator()();
		bool hasSame();
	};
	Tick_t TickCounter;
	RuntimeVector Runtimes;
	struct RegisterBus{
	  static const unit_t MaxIDLE=5;
	  MediatorOr1 * Mo;
	  pthread_t Last,Current;
	  unit_t Idles;
	  RegisterBus(MediatorOr1 * o);
	  inline void ResetIdle();
	}  *RBus;
	pthread_t schedId;
	struct ScheduleBuffer{
	  std::list<std::size_t> senders;
	  std::list<std::size_t> bestCand,GoodCand,Cand,NotGood; //sender is the entry, running Jobs
	  ScheduleBuffer(){}
	}SchedBuf;
	union {
	  unit_t Raw;
	  struct {
	    volatile unsigned NONEREG:1;
	    volatile unsigned SCHEDULE:1;// 0 Not ready, 1 ready
	    volatile unsigned ENVIR:1; //0 not ready, 1 ready
	  } State;
	};

	// schedule method
	int Scheduling();



	int DoScheduleEntry(AsyncCallRequest *req,taskid &); // Schedule by self
	void ScheduleEntry();// Schedule by scheduler
	void ScheduleEntity(); // Schedule runnable entities;
	void ScheduleRegister();// Register Open or close
	void Migrate();


	int DoRecycle();
	
	static void * RecycleThread(void *);
	static void * RunningTask( void *); //Execution finished when no job
	static void Cleanup_Handler(void *);
	static void * Schedule(void *);
	static Tick_t Now();
	int RegisterThread(unsigned char p);

	
	TaskExecScheduler();
	~TaskExecScheduler();
	static TaskExecScheduler * Inst;
	int doInit( );
protected:
	static int RunTask(RuntimeCB & cb);
public:
	static int Init();
	void FinalizeReg();
	int RegisterMe(uunit_t Prior);
	taskid tryAction(AsyncCallRequest *req);
	void Cancel(taskid);

};
}



#endif /* TASKEXECUTIONSCHEDULER_H_ */
