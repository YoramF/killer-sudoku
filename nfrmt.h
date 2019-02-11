/*
 * nfrmt.h
 *
 *  Created on: Nov 16, 2018
 *      Author: yoram
 */

#ifndef NFRMT_H_
#define NFRMT_H_

#define INT		1
#define REAL	2

char *frmt (char *trgstr, const int trgstrlen, const char *format, const char type, const void *number);

#endif /* NFRMT_H_ */
