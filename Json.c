/*
 * Json.c
 *
 *  Created on: May 25, 2015
 *      Author: raphael
 */

#include "Json.h"
#include <assert.h>
#include <string.h>
#ifdef __cplusplus
extern "C"{
#endif

#define STK_SIZE 64

typedef struct context_of_state{
	unsigned char state;
	unsigned char trace;

}__attribute__((aligned(16)))context_of_state;

typedef struct json_cursor{
	void * cbInst;
	converser cb;
	struct{
		const char * content;
		size_t leng,ofs;
	} input;
	struct {
		size_t top;
		String  body[STK_SIZE*2];

	}stringStack;
	struct {
		size_t cursor;
		String *names[STK_SIZE];;
	}nameList;
	struct {
		int top;
		context_of_state body[STK_SIZE];
	} ctxtStack;
	struct {
		context_of_state * current ,* prev;
	}parse;
	struct {
		String * curString; //workstring

	}ValueArea;

	ValueGeneral value;
}json_cursor;




static json_cursor * beginParse(const char *, size_t leng );



enum{
	E_COMMA,
	E_QUOTA,
	E_DIGIT,
	E_MINUS,
	E_LEFTP,
	E_RIGHTP,
	E_LEFTB,
	E_RIGHTB,
	E_ALPHABET,
	E_BLANK,
	E_EOF
};



#define ASCII_S 128
typedef struct alphabets{
	unsigned char tbl[256];
}alphabets;

static alphabets genTable;

static void init_table()__attribute__((constructor(1100)));

void init_table(){
	int i,max;
	for (i=0;i<256;i++)
		genTable.tbl[i]=E_BLANK;
	genTable.tbl[',']=E_COMMA;
	genTable.tbl['"']=E_QUOTA;
	genTable.tbl['-']=E_MINUS;
	genTable.tbl['{']=E_LEFTP;
	genTable.tbl['}']=E_RIGHTP;
	genTable.tbl['[']=E_LEFTB;
	genTable.tbl[']']=E_RIGHTB;

	i='0';
	max='9'+1;
	for (;i<max;i++)
		genTable.tbl[i]=E_DIGIT;
	i='a';
	max='z'+1;

	for (;i<max;i++)
		genTable.tbl[i]=E_ALPHABET;

	i='A';
	max='Z'+1;
	for (;i<max;i++)
		genTable.tbl[i]=E_ALPHABET;


}


enum {

	/*S_OBJECT=0,
	S_ARRAY,*/
	S_VALUEINOBJECT=0,
	S_VALUEINARRAY,
	S_STRING,
	S_NUMBER,
	S_NOPENDING,
	S_DUMMY
};

/*
 * P for array bracket [ ]
 * B for object bracket {}
 */

enum{
	E_REENT=-1,
	E_UNAV=-2,
	E_UNACCEPT=-3,
	E_UNKNOWN=-4,
	E_BAD=-5,
	E_USER=-6
};

/*
static int ActingOnObject(int evt, json_cursor *);
static int EntObject(json_cursor *);
static int ReentObject(json_cursor *);
static int ExitObject(json_cursor *);


static int ActingOnArray(int evt, json_cursor *);
static int EnterArray(json_cursor *);
static int ReentArray(json_cursor *);
static int ExitArray(json_cursor *);
*/

static int ActingOnString(int evt, json_cursor *);
static int EnterString(json_cursor *);
static int ExitString(json_cursor *);

static int ActingOnNumber(int evt,json_cursor *c);
static int EnterNumber(json_cursor *);
static int ExitNumber(json_cursor *);

static int ActingVIO(int evt,json_cursor *c);
static int EnterVIO(json_cursor *);
static int ReentVIO(json_cursor *);
static int ExitVIO(json_cursor *);

static int ActingVIA(int evt,json_cursor *c);
static int EnterVIA(json_cursor *);
static int ReentVIA(json_cursor *);
static int ExitVIA(json_cursor *);

static int ReentFail(json_cursor *p){
	return E_REENT;
}

typedef int (*action_t)(int ,json_cursor *c);
typedef int (*enter_t)(json_cursor *);
typedef int (*leave_t)(json_cursor *);


typedef struct state_entity{
	action_t act;
	enter_t entry,reent;
	leave_t exit;
}state_entity;

static state_entity state_table[S_NUMBER+1]={
		//{ActingOnObject,EntObject,ReentObject,ExitObject},
		//{ActingOnArray,EnterArray,ReentArray,ExitArray},
		{ActingVIO,EnterVIO,ReentVIO,ExitVIO},
		{ActingVIA,EnterVIA,ReentVIA,ExitVIA},
		{ActingOnString,EnterString,ReentFail,ExitString},
		{ActingOnNumber, EnterNumber,ReentFail,ExitNumber}
};

#define PUSH(stk,eptr, max) do {\
	assert((stk).top<max);\
	eptr=((stk).body+stk.top++);\
}while(0)

#define POP(stk) (stk).top>0?(stk).top--:0

#define TOP(stk) (stk).top>0?(stk).body+(stk).top-1:NULL


#define MOVE(i) (i).input.ofs++

#define REP(cur_ptr) cur_ptr->cb(cur_ptr->cbInst,&cur_ptr->value)
enum{
	VIO_NAME_WAITING,
	VIO_VALUE_WAITING,
	VIO_READY
};

static int event_on_state[E_ALPHABET+1]={
		E_UNKNOWN,S_STRING,S_NUMBER,S_NUMBER,S_VALUEINARRAY,E_UNACCEPT,
		S_VALUEINOBJECT,S_NUMBER,E_UNACCEPT
};

static int treatEventAsValue(int evt,  json_cursor * cur_ptr){
	assert(evt<=E_EOF);
	int rc=cur_ptr->parse.current->state;
	if (evt==E_EOF)
		rc=E_BAD;
	else
		rc=event_on_state[evt];
	return rc;
}

static int treateObjectValue(json_cursor * cur_ptr,SymbolValue sym){

	cur_ptr->value.sym=sym;
	cur_ptr->value.value=cur_ptr->ValueArea.curString;
	int rc=REP(cur_ptr)?S_VALUEINOBJECT:E_USER;
	POP(cur_ptr->stringStack);
	POP(cur_ptr->stringStack);
	return rc;
}

static int EnterVIO(json_cursor * cur_ptr){
	MOVE(*cur_ptr);
	PUSH(cur_ptr->ctxtStack,cur_ptr->parse.current,STK_SIZE);
	cur_ptr->parse.current->state=S_VALUEINOBJECT;
	cur_ptr->parse.current->trace=VIO_NAME_WAITING;
	cur_ptr->value.sym=OBJECTBGN;
	if (REP(cur_ptr))
		return S_VALUEINOBJECT;
	return E_USER;
}

static int ExitVIO(json_cursor * cur_ptr){
	MOVE(*cur_ptr);
	cur_ptr->parse.prev=NULL;
	cur_ptr->value.sym=OBJECTEND;
	if (REP(cur_ptr))
		return E_USER;

	cur_ptr->nameList.cursor--;
	POP(cur_ptr->ctxtStack);
	context_of_state * cp=TOP(cur_ptr->ctxtStack);
	return NULL==cp?S_DUMMY:cp->state;
}


static int ReentVIO(json_cursor * cur_ptr){
	context_of_state * lst=cur_ptr->parse.prev;

	POP(cur_ptr->ctxtStack);
	context_of_state * cPtr=cur_ptr->parse.current=TOP(cur_ptr->ctxtStack);
	int rc;

	switch (cPtr->state){
	case VIO_NAME_WAITING:
		rc=cPtr->state;
		assert((lst->state==S_STRING)&&
				(cur_ptr->nameList.cursor<STK_SIZE));
		cur_ptr->nameList.names[cur_ptr->nameList.cursor++]=
				cur_ptr->ValueArea.curString;

		break;
	case VIO_VALUE_WAITING:
		if (NULL!=lst){
			switch (lst->state){
			case S_STRING:
				rc=treateObjectValue(cur_ptr,STRINGLIKE);
				break;
			case S_NUMBER:
				rc=treateObjectValue(cur_ptr,DIGITLIKE);
				break;
			}
		}
		cur_ptr->parse.current->trace=VIO_NAME_WAITING;
		cur_ptr->nameList.cursor--;
		break;

	default:
		rc=E_UNAV;
		break;
	}
	return rc;
}


static int ActingVIO(int evt,json_cursor * cur_ptr){

	context_of_state * cPtr=cur_ptr->parse.current;


	int rc;
	if (evt==E_BLANK)
	{
		MOVE(*cur_ptr);
		return cPtr->state;
	}

	switch (cPtr->trace){
	case VIO_NAME_WAITING:
		if (E_QUOTA==evt){
			rc=S_STRING;
		}else if (E_RIGHTB==evt){
			rc=S_DUMMY;

		}else{
			rc=E_UNACCEPT;
		}
		break;
	case VIO_VALUE_WAITING:
		rc=treatEventAsValue(evt,cur_ptr);
		break;
	default:
		rc=E_UNAV;
		break;

	}
	return rc;
}

//Array State;
enum{
	VIAREAD,
	VIADILE, //Dilemma
	VIAEND
};

static int EnterVIA(json_cursor * cur_ptr)
{
	MOVE(*cur_ptr);
	PUSH(cur_ptr->ctxtStack,cur_ptr->parse.current,STK_SIZE);
	context_of_state * cp=cur_ptr->parse.current;
	cp->state=S_VALUEINARRAY;
	cp->trace=VIAREAD;
	return S_VALUEINARRAY;
}

static int ReentVIA(json_cursor * cur_ptr){
	context_of_state * lst=cur_ptr->parse.prev;
	POP(cur_ptr->ctxtStack);
	context_of_state * cp=cur_ptr->parse.current=TOP(cur_ptr->ctxtStack);
	cp->trace=VIADILE;
	if ((NULL!=lst)&&((lst->state==S_STRING)||lst->state==S_NUMBER)){
		if (lst->state==S_STRING)
			cur_ptr->value.sym=STRINGLIKE;
		else
			cur_ptr->value.sym=DIGITLIKE;
		cur_ptr->value.value=cur_ptr->ValueArea.curString;
		REP(cur_ptr);
		POP(cur_ptr->stringStack);
	}
	return S_VALUEINARRAY;
}

static int ExitVIA(json_cursor * cur_ptr){
	MOVE(*cur_ptr);
	cur_ptr->value.sym=ARRAYEND;
	REP(cur_ptr);
	POP(cur_ptr->ctxtStack);
	context_of_state * cp=TOP(cur_ptr->ctxtStack);
	return NULL==cp?S_DUMMY:cp->state;
}
static int ActingVIA(int evt,json_cursor *cur_ptr){

	context_of_state * cp =cur_ptr->parse.current;
	int rc=cp->state;
	if (evt==E_BLANK) {
		MOVE(*cur_ptr);
		return rc;
	}
	if (cp->trace==VIAREAD){
		rc=treatEventAsValue(evt,cur_ptr);
	}else if (cp->trace==VIADILE){
		if (evt==E_COMMA){
			cp->trace=VIAREAD;
		}else if (evt==E_LEFTP){
			cp->trace=VIAEND;
			rc=S_DUMMY;
		}else
		{
			rc=E_UNACCEPT;
		}
	}
	return rc;

}
void InitNumberOrString(json_cursor * cur_ptr){
	PUSH(cur_ptr->ctxtStack,cur_ptr->parse.current,STK_SIZE);

	PUSH(cur_ptr->stringStack,cur_ptr->ValueArea.curString,STK_SIZE*2);
	MOVE(*cur_ptr);
	cur_ptr->ValueArea.curString->len=0;
	cur_ptr->ValueArea.curString->content=cur_ptr->input.content+cur_ptr->input.ofs;
}

int AcingOnstring(int evt,json_cursor *cur_ptr){
	if (E_QUOTA == evt){
		return S_DUMMY;
	}else{
		cur_ptr->ValueArea.curString->len++;
		return S_STRING;
	}
}

int EnterString(json_cursor * cur_ptr){
	InitNumberOrString(cur_ptr);
	cur_ptr->parse.current->state=S_STRING;
	cur_ptr->parse.current->trace=VIO_NAME_WAITING;
	return S_STRING;

}

int ExitString(json_cursor * cur_ptr){
	MOVE(*cur_ptr);
	cur_ptr->parse.prev=cur_ptr->parse.current;
	POP(cur_ptr->ctxtStack);
	context_of_state * temp=TOP(cur_ptr->ctxtStack);
	return NULL==temp?S_DUMMY:temp->state;
}

int ActingOnString(int evt,json_cursor * cur_ptr){
	int rc=cur_ptr->parse.current->state;
	if (evt==E_QUOTA)
		rc=S_DUMMY;
	else
		cur_ptr->ValueArea.curString->len++;
	return rc;
}



int EnterNumber(json_cursor * cur_ptr){
	InitNumberOrString(cur_ptr);
	cur_ptr->parse.current->state=S_NUMBER;
	cur_ptr->parse.current->trace=0;
	cur_ptr->ValueArea.curString->len=1;
	MOVE(*cur_ptr);
	return S_NUMBER;

}

int ExitNumber(json_cursor * cur_ptr){

	cur_ptr->parse.prev=cur_ptr->parse.current;
	POP(cur_ptr->ctxtStack);
	context_of_state * temp=TOP(cur_ptr->ctxtStack);
	return NULL==temp?S_DUMMY:temp->state;
}

int ActingOnNumber(int evt,json_cursor * cur_ptr){
	int rc=cur_ptr->parse.current->state;
	if (evt==E_COMMA)
		rc=S_DUMMY;
	else
		cur_ptr->ValueArea.curString->len++;
	return rc;
}

json_cursor * beginParse(const char * buf,  size_t leng){

	json_cursor *cur=malloc(sizeof(json_cursor));
	memset(cur,0,sizeof(json_cursor));
	cur->cbInst=NULL;
	cur->cb=NULL;
	cur->input.content=buf;
	cur->input.leng=leng;
	cur->input.ofs=0;
	return cur;

}



int toParse(const char * buf, size_t leng, converser cb, void * instance){
	json_cursor * cur=beginParse(buf,leng);
	cur->cbInst=instance;
	cur->cb=cb;
	register unsigned char alph;
	register char inbyte;
	int isBlank=1;
	for (;(cur->input.leng>cur->input.ofs)&&isBlank;cur->input.ofs++){
		inbyte=cur->input.content[cur->input.ofs];
		alph=genTable.tbl[inbyte];
		isBlank=alph==E_BLANK;
	}

	int curState,prev;
	curState=prev=event_on_state[alph];
	if (prev<0)
		return prev;
	PUSH(cur->ctxtStack,cur->parse.current,STK_SIZE);
	state_entity *sptr;
	sptr=state_table+curState;
	sptr->entry(cur);
	for (;(curState>=0)&&(cur->input.leng>cur->input.ofs);){
		prev=curState;
		sptr=state_table+curState;
		inbyte=cur->input.content[cur->input.ofs];
		alph=genTable.tbl[inbyte];
		curState=sptr->act(alph,cur);
		if (prev!=curState){
			if (curState==S_DUMMY){
				sptr=state_table+prev;
				curState=sptr->exit(cur);
				if (S_DUMMY!=curState){
					sptr=state_table+curState;
					sptr->reent(cur);
				}else {
					break;
				}
			}else {
				sptr=state_table+curState;
				sptr->reent(cur);
			}
		}

	}
	//It is for end with no more pendings
	if (curState>=0){
		cur->stringStack.body[0].content=cur->input.content;
		cur->stringStack.body[0].len=cur->input.leng-cur->input.ofs;
		cur->value.sym=ENDALL;
		cur->value.value=cur->stringStack.body;
		REP(cur);
	}
#ifdef TEST
	else{
		abort();
	}
#endif
	free(cur);
	return curState;

}


#ifdef __cplusplus
}
#endif



