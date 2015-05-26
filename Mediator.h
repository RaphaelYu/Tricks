/*
 * Mediator.h
 *
 *  Created on: 2015-4-24
 *      Author: Administrator
 */

#ifndef MEDIATOR_H_
#define MEDIATOR_H_
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

namespace Tasklet{
//Multi-thread task Structure
class MediatorOprBundle;
class ClientOprBundle;
//Shared part for clien and server
class CompeteOr1{
	friend class MediatorOprBundle;
	friend class ClientOprBundle;
protected:
	pthread_t DefaultId; //Thread is used for the uuid
	int servState;
	volatile pthread_t ChanServ; // Server thread id;
	volatile pthread_t ChanClient;
	inline void ReleaseCli(){
		ChanClient=DefaultId;
	}
public:
	MediatorOprBundle * mOpr;
	ClientOprBundle * cOpr;
	enum {
		BUSY=-1,
		OK=0,
		WAITINIT=1
	};

	CompeteOr1():
		DefaultId(pthread_self()),
		servState(OK),
		ChanServ(DefaultId),
		ChanClient(DefaultId),
		mOpr(NULL),
		cOpr(NULL)
	{}
	void Init();
	inline bool isFree(){ return (ChanServ==ChanClient)&&(ChanServ==DefaultId);}
	inline bool hasWork(){ return (ChanServ!=ChanClient)&&(ChanServ==DefaultId);}
	inline bool canRetrieve(){ return (ChanServ!=ChanClient)&&(ChanClient==DefaultId);}
	inline bool isReady(pthread_t myid){return (ChanServ==myid);}
	inline int whyNot(){return servState;}
	inline bool notBusy(){return servState!=BUSY;}

};
class MediatorOprBundle{
	friend class MediatorOr1;
	CompeteOr1 & ref;
protected:
	inline void eval(){ref.ChanServ=ref.ChanClient;}
	inline void stop(){pthread_cancel(ref.DefaultId);}
	inline pthread_t * getDefault(){return &ref.DefaultId;}
	inline void setBusy(){ref.servState=CompeteOr1::BUSY;}
	inline void setFree(){ref.servState=CompeteOr1::OK;}
	inline int getServState(){return ref.servState;}
public:
	MediatorOprBundle(CompeteOr1 & r):ref(r){}
	void Finalize();
};

class ClientOprBundle{
	friend class ClientOr1;
	CompeteOr1 & ref;
protected:
	inline void setClient(pthread_t p){ref.ChanClient=p;}
	inline void Release(){ref.ReleaseCli();}
public:
	ClientOprBundle(CompeteOr1 & r):ref(r){}

};




class CancelPoint{
	pthread_mutex_t mutex;
	pthread_cond_t cond;

public:
	CancelPoint();
	~CancelPoint();
	void InitWaitPoint();
	int Standby();
};
class ClientOr1{
	CompeteOr1 * Channels;
	pthread_t LocalThread;
public:

	ClientOr1(CompeteOr1 * p):
		Channels(p),
		LocalThread(pthread_self())
	{}
	template<class T>
	int Attempt(T & Func){
		for (;;){
			for (;!Channels->isFree();sched_yield());
			Channels->cOpr->setClient(LocalThread);
			if (Channels->isReady(LocalThread))
				break;
			if (Channels->whyNot()==CompeteOr1::BUSY)
				return CompeteOr1::BUSY;
			sched_yield();
		}
		Func();
		Channels->cOpr->Release();
		return CompeteOr1::OK;

	}

};
// Mediate multiple clients
class MediatorOr1{
	int CurState;
	CompeteOr1 * Channels;

	CancelPoint *cptr;
	MediatorOr1():
		CurState(CompeteOr1::WAITINIT),
		Channels(NULL),
		cptr(NULL)
	{

	}
	inline void Init(){
			cptr=new CancelPoint();
			cptr->InitWaitPoint();
	}
	inline void InitChan(){
		Channels=new CompeteOr1();
		Channels->Init();
	}
	template<class T>
	struct mediator_pair{
		MediatorOr1 * MO;
		T * Mediator;
		mediator_pair(MediatorOr1 *mo, T * me):
			MO(mo),
			Mediator(me)
		{}
	};
	template<class T>
	inline int doMediate(T * Mediator){
		if (Mediator->canDo()){
			Channels->mOpr->setFree();
		}else{
			return Channels->mOpr->getServState();
		}

		for (;(!Channels->isFree())&&(Channels->notBusy());){
			if (Channels->hasWork()){
				if (Mediator->canDo()){
					Channels->mOpr->eval();
				}else {
					Channels->mOpr->setBusy();
				}
			}else if (Channels->canRetrieve()){
				Channels->mOpr->eval();
			}
			sched_yield();
		}
		return Channels->mOpr->getServState();

	}

public:
		// not thread safe ,it seems not necessary;
	// it can be done in InitFase;
	template<class T>
	static void * MediatorRun(void *p){
		mediator_pair<T> * mp=(mediator_pair<T> *)p;
		MediatorOr1 * me=mp->MO;
		me->Init();

		//struct ::timespec ts,tp;
		me->CurState=CompeteOr1::OK;
		for (;;){
			me->doMediate<T>(mp->Mediator);
			me->cptr->Standby();
		}
		pthread_exit(NULL);
		return NULL;



	}
	template<class T>
	static MediatorOr1 * Instance(T * exec){
		MediatorOr1 * mPtr=new MediatorOr1;
		mPtr->InitChan();

		pthread_create(mPtr->Channels->mOpr->getDefault(),NULL,MediatorRun<T>,(void *)mPtr);
		for (;mPtr->CurState==CompeteOr1::WAITINIT;sched_yield());
		return mPtr;

	}
	ClientOr1 *getClientOr1(){
		return new ClientOr1(Channels);
	}
	void Stop(){
		Channels->mOpr->stop();
	}



};



}

#endif /* MEDIATOR_H_ */
