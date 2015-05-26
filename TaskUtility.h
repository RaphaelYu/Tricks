/*
 * TaskUtility.h
 *
 *  Created on: 2015-4-29
 *      Author: Administrator
 */

#ifndef TASKUTILITY_H_
#define TASKUTILITY_H_
#include "Mediator.h"

namespace Tasklet{
struct Universal{
	enum{
		SUCCESS=0,
		NEG1=-1
	};

};
struct ScopedValve{
	enum{
		OFF,
		ON
	};
	char &valve;
	ScopedValve(char & v):
		valve(v)
	{}
	~ScopedValve(){valve=ON;}
	bool isOn(){return valve==ON;}

};
class ScopedClient{
	ClientOr1 * cli;
public:
	ScopedClient(ClientOr1 *o):
		cli(o)
	{}
	~ScopedClient(){
		delete cli;
	}
	inline ClientOr1 & operator *(){return *cli;}
	inline ClientOr1 * operator ->(){return cli;}

};

struct QueueState{
	enum{
		EMPTY=-1,
		SUCCESS=0,
		FULL=1,
	};
};

template<class T,int  max>
struct DistReqQueue{
	typedef T value_type;
	typedef T * pointer_type;
	typedef T & reference_type;
	typedef const T * const_pointer_type;
	typedef const T & const_reference_type;
	volatile unsigned char Hdr, Tail;
	pointer_type Queue[max];
	DistReqQueue():
		Hdr(0),
		Tail(0)
	{
		for (int i=0;i<max;Queue[i++]=NULL);
	}
	int Enqueue(pointer_type  ap){
		unsigned char Next=(Tail+1)%max;
		if ((Next==Hdr)&&(Queue[Tail]!=NULL)) return QueueState::FULL;
		Queue[Tail]=ap;
		if (Next!=Hdr) Tail=Next;
		return QueueState::SUCCESS;
	}
	int Dequeue(pointer_type *ap){
		if (Hdr==Tail&&Queue[Tail]==NULL) return QueueState::EMPTY;
		*ap=Queue[Hdr];
		Queue[Hdr]=NULL;
		Hdr=(Hdr+1)%max;
		return QueueState::SUCCESS;
	}
	//only receiver works
	bool isEmpty(){
	  return Hdr==Tail&&Queue[Tail]==NULL;

	}
};


}



#endif /* TASKUTILITY_H_ */
