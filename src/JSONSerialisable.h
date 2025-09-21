/*
 * JSONSerialisable.h
 *
 *  Created on: 22 Apr 2024
 *      Author: jondurrant
 */

#ifndef SRC_JSONSERIALISABLE_H_
#define SRC_JSONSERIALISABLE_H_

class JSONSerialisable {
public:
	JSONSerialisable();
	virtual ~JSONSerialisable();


	virtual char * json() = 0;
};

#endif /* SRC_JSONSERIALISABLE_H_ */
