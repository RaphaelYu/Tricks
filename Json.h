/*
 * Json.h
 *
 *  Created on: May 25, 2015
 *      Author: raphael
 */

#ifndef JSON_H_
#define JSON_H_


#include <stdlib.h>

#ifdef __cplusplus
extern "C"{
#endif

struct InputStream{
	size_t nbr;
	size_t cursor;
	char buf[];
};

typedef struct String{
	size_t len;
	const char * content;
}String;
typedef enum {
	NONE=-1,
	STRINGLIKE=0,
	DIGITLIKE,
	OBJECTBGN,
	OBJECTEND,
	ARRAYBGN,
	ARRAYEND,
	ENDALL
}SymbolValue;

typedef struct ValueGeneral{
	SymbolValue sym;
	String *value;
	struct {
		size_t leng;
		String * * name;
	} * names;

}ValueGeneral;

typedef int (*converser)(void *,const ValueGeneral *);

int toParse(const char * buf,size_t len, converser  cb,void * instance);

#ifdef __cplusplus
}
#endif




#endif /* JSON_H_ */
