/*
 * nfrmt.c
 *
 *  Created on: Nov 16, 2018
 *      Author: yoram
 */


#include <stdlib.h>	// for llabs()
#include <math.h>	// for fabs()
#include <string.h>	// for strlen()


#include "nfrmt.h"

typedef struct _frmt
{
	char Parenthesis;		// If 1 desplay negative numbers with Perentasis
	char DecimalR;			// Number of digits right to the decimal point
	char DecimalL;			// Number of digita left to the decimal point
	char RightJ;			// Right Justify?
} frmtR;

int getfrmt (const char *format, frmtR *frmtRec)
{
	int i, len, decR = 0, decL = 0, dec = 0;

	if (format == NULL)
		return 0;

	len = strlen(format);
	if (len && (frmtRec != NULL))
	{
		memset(frmtRec, 0, sizeof (frmt));
		for (i = 0; i < len; i++)
		{
			switch (format[i])
			{
			case '-':
				frmtRec->RightJ = 1;
				break;
			case '(':
				frmtRec->Parenthesis = 1;
				break;
			case ')':
				break;
			case '.':
				dec = 1;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if (dec)
				{
					decR *= 10;
					decR += format[i] - '0';
				}
				else
				{
					decL *= 10;
					decL += format[i] - '0';
				}
				break;
			default:
				return 0;
			}
		}

		frmtRec->DecimalL = decL;
		frmtRec->DecimalR = decR;
		return 1;
	}
	else
		return 0;
}

void strShiftR (char *str, const int moves, const int slen)
{
	int i, j;

	i = (moves < slen? moves: slen);
	j = slen;


	while (j-i)
	{
		j--;
		str[j] = str[j-i];
	}

	for (j = 0; j < i; j++)
		str[j] = '\0';
}

char *frmt (char *trgstr, const int trgstrlen, const char *format, const char type, const void *number)
{
	frmtR frmtRec;
	int i, j, tlen, clen, sigL, mantL, cm, isNeg;
	long long int sigNum, sN;
	double	d;
	char digit;
	char sigNumS [20];		// 20 digits
	char mantS [20];	// 20 digits

	if (!getfrmt(format, &frmtRec))
		return NULL;

	if (trgstr == NULL)
		return NULL;

	// check for negative number
	isNeg = ((type == INT)? (*(long long int *)number < 0): (*(double *)number < 0));

	// check target string length
	tlen = frmtRec.DecimalL + frmtRec.DecimalR + (number < 0? (frmtRec.Parenthesis? 2 : 1) : 0) + (frmtRec.DecimalR? 1 : 0);

	// add commas
	tlen += ((frmtRec.DecimalL -1) / 3);
	if (tlen > trgstrlen-1)
		return NULL;

	// if we get here we can start constructing the target string
	// convert the significant number into string
	sigNum = ((type == INT) ? *(long long int *)number: *(double *)number);	// Get only the significant part of the number
	sigNum = llabs(sigNum);
	sN = sigNum;
	sigL = 0;
	cm = 0;

	while (sN)
	{
		if (cm == 3)					// add comma
		{
			strShiftR(sigNumS, 1, sigL+1);
			sigNumS[0] = ',';
			sigL++;
			cm = 0;
		}
		digit = sN % 10; 					// get last digit
		strShiftR(sigNumS, 1, sigL+1);			// right shift 1 char
		sigNumS[0] = digit + '0';				// convert digit to char
		sN /= 10;
		sigL++;
		cm++;
	}

	// convert the mantissa part to string
	if (type == REAL)
		d = fabs(*(double *)number) - sigNum;
	else
		d = 0;
	mantL = 0;
	while (mantL < frmtRec.DecimalR)
	{
		digit = d * 10;
		mantS[mantL] = digit + '0';
		d *= 10.0;
		d -= digit;
		mantL++;
	}


	clen = tlen - (frmtRec.DecimalL-sigL);

	// if calculated length is bigger than calculated formatted target length check if target string can have the longer string if not return NULL
	if (clen > tlen)
	{
		if (clen > trgstrlen-1)
			return NULL;
	}

	// construct the result string
	i = 0;

	// fill with blanks or zerros if right justify
	if (frmtRec.RightJ)
		for (j = tlen - clen; j > 0; j--)
			trgstr[i++] = ' ';

	if (isNeg)
		trgstr[i++] = (frmtRec.Parenthesis? '(' : '-');

	// insert digits for the significant number
	for (j = 0; j < sigL; j++)
		trgstr[i++] = sigNumS[j];

	// insert mantica is required
	if (frmtRec.DecimalR)
	{
		trgstr[i++] = '.';

		for (j = 0; j < mantL; j++)
			trgstr[i++] = mantS[j];
	}

	// Add right parentasis if required
	if ((isNeg) && (frmtRec.Parenthesis))
		trgstr[i++] = ')';

	// Add EOS to target string
	trgstr[i] = '\0';

	return trgstr;
}



