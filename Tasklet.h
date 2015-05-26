/*
 * TaskLet.h
 *
 *  Created on: 2015-4-21
 *      Author: Administrator
 */

#ifndef TASKLET_H_
#define TASKLET_H_
#include <string>
#define CANCELED
#define UNEXPECT -1
#define DONE 1
namespace Tasklet{
typedef  long taskid;

struct ActionExec{
	
	virtual int DoThis(std::string &)=0;
	virtual void Report(taskid id,int rv,const std::string &)=0;
	virtual ~ActionExec(){};
};


struct ActionDaemon{
	enum{
		CONT=0,
		STOP=1
	};
	virtual int Run()=0;
	virtual ~ActionDaemon(){}
	
};


struct AsyncCallRequest{
	ActionExec * Exc;
	AsyncCallRequest():
		Exc(NULL)
	{}


};

struct DaemonRequest{
	ActionDaemon * Dae;
	DaemonRequest():
		Dae(NULL)
	{}
};

struct DaemonCollector{
	virtual void Report(int evt, ActionDaemon *)=0;
	virtual ~DaemonCollector();
};
struct RetCode{
	enum {
		OK=0,
		ALREADY=-2,
		INVALID=-3,
		BUSY=-4,
		UNKNOWN=-5
	};
};

struct EventCode{
  enum{
    EXEC=0,
    CANCEL=1
  };
};
}

#ifdef __cplusplus
extern "C"{
#endif

struct TaskletAttribute{
	int max;
};


int InitTaskLet(const TaskletAttribute *,Tasklet::DaemonCollector * c);
int RegisterMe(int Prior);
Tasklet::taskid async_call(Tasklet::AsyncCallRequest *task);
Tasklet::taskid start_daemon(Tasklet::DaemonRequest *task);

void cancel_call(Tasklet::taskid id);
void stop_daemon(Tasklet::taskid id);

#ifdef __cplusplus
}
#endif

#endif /* TASKLIST_H_ */
