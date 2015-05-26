/*
 * Mediator.cc
 *
 *  Created on: 2015-4-24
 *      Author: Administrator
 */

#include "Mediator.h"
#include <sys/time.h>
int DefaultWait=20*1000*1000;

namespace Tasklet{
void CompeteOr1::Init(){
	mOpr=new MediatorOprBundle(*this);
	cOpr=new ClientOprBundle(*this);
}
CancelPoint::CancelPoint(){
	pthread_cond_init(&cond,NULL);
	pthread_mutex_init(&mutex,NULL);
}
CancelPoint::~CancelPoint(){
	pthread_mutex_unlock(&mutex);
}

void CancelPoint::InitWaitPoint(){
	pthread_mutex_lock(&mutex);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL);
}
int CancelPoint::Standby(){
	struct ::timespec ts;
	struct ::timeval tp;
	gettimeofday(&tp,NULL);
	ts.tv_sec=tp.tv_sec;
	ts.tv_nsec=tp.tv_usec*1000+DefaultWait;
	return pthread_cond_timedwait(&cond,&mutex,&ts);
}
}
