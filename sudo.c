/*
 * sodu.c
 *
 *  Created on: Nov 23, 2018
 *      Author: yoram
 *
 *      Sudoku and sudoku-killer solver.
 *      inout file format:

<sboard>
.  .  .  .  .  .  .  .  .
.  .  .  .  .  .  .  5  .
.  .  .  .  .  .  .  .  .
.  .  .  .  6  .  .  .  9
.  .  .  .  .  .  .  .  .
.  1  .  .  .  .  .  .  .
.  .  .  .  .  .  .  .  .
.  .  1  .  .  .  .  .  .
.  .  .  .  .  .  .  .  .
</sboard>

<kboard>
 1  1  2  2  3  3  4  4  5
 1  6  2  8  3 10 10  .  5
12  6  7  8  9  9 10 11  5
12  6  7  7  . 15 15 11  .
13 14 14 16 16 15 17 11 18
13  . 19 16 21 22 17 23 18
20 20 19 21 21 22 24 23 23
20 20  . 25 25 25 24 23 23
26 26 26 26 25 27 27 28 28
<kboard>

<cage>
1,	15,	3
2, 	19,	3
3,	16,	3
4,	3,	2
5,	19,	3
6,	15,	3
7,	16,	3
8,	4,	2
9,	10,	2
10,	18,	3
11,	12,	3
12,	7,	2
13,	16,	2
14,	10,	2
15,	17,	3
16,	20,	3
17,	7,	2
18,	8,	2
19,	6,	2
20,	23,	4
21,	9,	3
22,	7,	2
23,	26,	5
24,	12,	2
25,	25,	4
26,	21,	4
27,	12,	2
28,	10,	2
</cage>
* 
 <sboard> describes the sudoku board
 <kboard> describes the killer sudoku cages (numbers indicate cage ID)
 <cage>   descibed the cages (ID, sum, number of digits/cells)

*
*       usage: sudokur -f inFile [-k] -l[1/2]

	-k - for killer sudoku.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sudo.h"
#include "../nfrmt/nfrmt.h"

char *CageB = "<cage>";
char *CageE = "</cage>";
char *kBoardB = "<kboard>";
char *kBoardE = "</kboard>";
char *sBoardB = "<sboard>";
char *sBoardE = "</sboard>";


static _cage 	**cageInd = NULL; // allow direct access to cage information
_addR           sBlks [3][3];
long long int   iter = 0, stack = 0, deepest = 0; // for statistic printouts
int	        loging = 0;
int		sKiller = 0;

/********************************************************************************************
*	Utility functions
*********************************************************************************************/

// return number of set bits in word
char countSetBits(unsigned short int num)
{
	char count = 0;

	while (num)
	{
		if (num & 1)
			count++;
		num >>= 1;
	}
	return count;
}

// get next available value from given bitmask
char getVal(unsigned short int bits)
{
	char i;

	for (i = 0; i < 10; i++)
		if (isBitSet(i, bits))
			break;

	return (i < 10 ? i : 0);
}

int readInput(FILE *fp, char *kBoard, char *sBoard)
{
	char line [100];
	char *tok;
	int s;

	while ((fgets(line, sizeof(line) - 1, fp)))
	{
		if ((tok = strstr(line, CageB)) && sKiller)
		{
			if (!(s = readCage(fp)))
				break;
		}
		else if ((tok = strstr(line, kBoardB)) && sKiller)
		{
			if (!(s = readBoard(fp, kBoard, kBoardE)))
				break;
		}
		else if ((tok = strstr(line, sBoardB)))
		{
			if (!(s = readBoard(fp, sBoard, sBoardE)))
				break;
		}
	}

	return s;
}

// Read a raw of numbers and update the relevant raw pon board
// It also validate that each cage ID is valid
int readLineB(char *board, char *line)
{
	char *token;
	int i = 0;

	token = strtok(line, " ");

	while ((token != NULL) && (i < 9))
	{
		board[i] = atoi(token);
		i++;
		token = strtok(NULL, " ");
	}

	if (i == 9)
		return 1;

	return 0;
}

int readBoard(FILE *fp, char *board, char *token)
{
	int i;
	char *tok;
	char line[100];

	i = 0;
	while ((fgets(line, sizeof(line) - 1, fp)) && (i < 9))
	{
		if ((tok = strstr(line, token)))
		{
			break;
		}
		else
		{
			if (readLineB(&board[i * 9], line))
				i++;
			else
			{
				break;
			}
		}
	}

	if (i < 9)
	{
		printf("failed to read board %s\n", token);
		return 0;
	}

	return 1;
}

/********************************************************************************************
*	Sudo-Killer related functions
*********************************************************************************************/
int nextNumbers(int *digits, const int num)
{
	int i, j = 0, k;
	int found = 0;

	// get current counter
	i = num - 1;
	while (!found && (i > -1))
	{
		digits[i]++;
		if (digits[i] > (9 - j))
		{
			i--;
			j++;
		}
		else
		{
			for (i++; i < num; i++)
				digits[i] = digits[i - 1] + 1;

			found = 1;
		}
	}

	return found;
}

int calcSum(const int *digits, const int num)
{
	int i, s = 0;

	for (i = 0; i < num; i++)
		s += digits[i];

	return s;
}


// This function will go through all sudoku cells which are part of a given cage
// and update their bitmask according to cage bitmask.
void updateCage(_brdR *b, int cId)
{
	int i, r, c;
	_cage *cPtr;
	_addR *cellPtr;
	_comb *combPtr;
	unsigned short *bitMask;
	char *score;
	unsigned short cBits = 0;

	cPtr = cageInd[cId];
	bitMask = &(b->bitMask[0][0]);
	score = &(b->score[0][0]);

	if (cPtr != NULL)
	{
		// first constract the cage remaining digits
		combPtr = cPtr->ptr;
		while (combPtr)
		{
			cBits |= combPtr->remainingDigits;
			combPtr = combPtr->next;
		}

		// now go through all sudo board cells belonging to this cage and uodate their bitmask and scores
		cellPtr = cPtr->cellPtr;
		if (cellPtr != NULL)
		{
			for (i = 0; i < cPtr->nCells; i++)
			{
				r = cellPtr[i].raw;
				c = cellPtr[i].col;

				if (score[r * 9 + c] > 0)
				{
					bitMask[r * 9 + c] &= cBits;
					score[r*9+c] = countSetBits(bitMask[r*9+c]);
				}
			}
		}
	}
}

// release all allocated RAM
void releaseMem()
{
	_comb *combPtr, *rlPtr;
	_cage *cagePtr;
	int i, j;

	for (i = 0; i < INDEXS; i++)
	{
		cagePtr = cageInd[i];

		if (cagePtr)
		{
			combPtr = cagePtr->ptr;
			while (combPtr)
			{
				rlPtr = combPtr;
				combPtr = combPtr->next;
				free(rlPtr);
			}
			free(cagePtr);
		}
	}
	free(cageInd);
}

// call once to initialize the cage index array
_cage **cageIndInit()
{

	_cage **cPtr;

	// we allocate 82 to cover 1-81 indexes
	cPtr = malloc(sizeof(cageInd) * INDEXS);
	if (cPtr != NULL)
		memset(cPtr, 0, sizeof(cageInd) * INDEXS);

	return cPtr;
}

// Add new sun combination
_comb *addComb(short int cID, unsigned short digits)
{
	_comb *nPtr, *ptr;

	nPtr = malloc(sizeof(_comb));

	if (nPtr != NULL)
	{
		nPtr->allDigits = digits;
		nPtr->remainingDigits = digits;
		nPtr->next = NULL;

		if (cageInd[cID]->ptr)
		{
			ptr = cageInd[cID]->ptr;
			while (ptr->next != NULL)
				ptr = ptr->next;

			ptr->next = nPtr;
		}
		else
			cageInd[cID]->ptr = nPtr;
	}

	return nPtr;
}

// convert array of digits to a Mask
unsigned short genBMask(const int *digits, const int num)
{
	int i;
	unsigned short mask = 0;

	for (i = 0; i < num; i++)
		mask = setBit(digits[i], mask);

	return mask;
}

// find all digit combinations that sums to given sum
int brk(const int cID, const int sum, const int num)
{
	int i, j = 0, keepSearching = 1;
	int digits[NUMBERS];
	unsigned short dMask;

	// inint original numbers
	for (i = 0; i < num; i++)
		digits[i] = i + 1;

	// start scanning all possible combinations
	// for each num numbers check if sun is what we are looking for
	// if yes, add this combination to resultArr

	do
	{
		if (calcSum(digits, num) == sum)
		{
			dMask = genBMask(digits, num);
			if (addComb(cID, dMask) == NULL)
			{
				j = -1;
				break;
			}
			j++;
		}
	} while (nextNumbers(digits, num));
	return j;
}

// Being called every time new cage is found
_cage *cageCreate(short int id, short int sum, short int nCells)
{
	_cage *cPtr;
	_addR *cellPtr;
	int i;

	if ((id >= INDEXS) || (id < 1))
	{
		printf("Invalid cage ID [%d]\n", id);
		return NULL;
	}

	// if cage has already been created - ignore this call
	if (cageInd[id] != NULL)
		return cageInd[id];

	// allocate ram for cell back pointers
	if ((cellPtr = calloc(nCells, sizeof(_addR))) == NULL)
	{
		printf("failed to allocate memory inside cageCreate\n");
		return NULL;
	}
	else
	{
		// inint pointers with -1 we will need that later
		for (i = 0; i < nCells; i++)
		{
			cellPtr[i].col = -1;
			cellPtr[i].raw = -1;
		}
	}

	cPtr = calloc(1, sizeof(_cage));
	if (cPtr != NULL)
	{
		cPtr->sum = sum;
		cPtr->nCells = nCells;
		cPtr->cellPtr = cellPtr;
		cageInd[id] = cPtr;

		if (brk(id, sum, nCells) < 0)
			return NULL;
	}

	return cPtr;
}



int readCage(FILE *fp)
{
	char line[100];
	char *tok;
	int cageNum, cageSum, nCells;

	while ((fgets(line, sizeof(line) - 1, fp)))
	{
		tok = strstr(line, CageE);
		if (!tok)
		{
			tok = strtok(line, ",");
			if (tok)
			{
				cageNum = atoi(tok);
				tok = strtok(NULL, ",");
				cageSum = atoi(tok);
				tok = strtok(NULL, ",");
				nCells = atoi(tok);
				if (!cageCreate(cageNum, cageSum, nCells))
				{
					printf("failed to add new cage ID [%d]\n", cageNum);
					return 0;
				}
			}
		}
		else
			break;
	}
	return 1;
}

// individual cell with cage ID surounded by cells from other cages is invali
int validateCageId(char *board, int r, int c)
{
	int cId;
	int rm, rp, cm, cp;
	int s = 0;

	cId = board[r * 9 + c];

	// no need to check for cage ID 0
	if (cId == 0)
		return 1;

	rm = (r > 0 ? r - 1 : r);
	rp = (r < 8 ? r + 1 : r);
	cm = (c > 0 ? c - 1 : c);
	cp = (c < 8 ? c + 1 : c);

	if ((rm != r) && (board[rm * 9 + c] == cId))
		s = 1;
	if ((rp != r) && (board[rp * 9 + c] == cId))
		s = 1;
	if ((cm != c) && (board[r * 9 + cm] == cId))
		s = 1;
	if ((cp != c) && (board[r * 9 + cp] == cId))
		s = 1;

	return s;
}

// cage backpointers are two bit masks holding the numbers of raws as columns of cells
// belonging to a particular cage.
void updateBackPointers(int cId, int r, int c)
{
	_cage *cPtr;
	int i;

	// check that input cId is in frange, otherwise ignore
	if ((cId < 1) || (cId >= INDEXS))
		return;

	cPtr = cageInd[cId];

	if (cPtr != NULL)
	{
		i = 0;

		// advance to first empty address slot
		while (i < cPtr->nCells)
		{
			if (cPtr->cellPtr[i].raw >= 0)
				i++;
			else
				break;
		}

		if (i < cPtr->nCells)
		{
			cPtr->cellPtr[i].raw = r;
			cPtr->cellPtr[i].col = c;
		}
		else
			printf("Failed to update back pointer. All slots are full. [cId: %d] [raw: %d] [col: %d]\n", cId, r, c);
	}
	else
		printf("Error updating back pointers [cId: %d] [raw: %d] [col: %d]\n", cId, r, c);
}

int initKiller(char *board)
{
	char line[100];
	int i, j, failure = 0;
	char *tok;

	// Validate input board.
	// validateCageId will also update each cage with its backpointers to board cells
	for (i = 0; i < 9 && !failure; i++)
	{
		for (j = 0; j < 9 && !failure; j++)
			if (validateCageId(board, i, j))
			{
				//update cage backpointers
				updateBackPointers(board[i*9+j], i, j);
			}
			else
			{
				failure = 1;
				if (loging)
					printf("failed to update cage %d, in cell [%d, %d]\n", board[i*9+j], i, j);
			}

	}

	return (!failure);
}

// Set Combo Bits.
// Update all combo bits according to given cell value.
void setCageComboBits (int cId, int val)
{
	_cage *cPtr;
	_comb *combPtr;

	cPtr = cageInd[cId];

	if (cPtr)
	{
		combPtr = cPtr->ptr;

		while (combPtr)
		{
			if (isBitSet(val, combPtr->remainingDigits))
			{
				combPtr->remainingDigits = clearBit (val, combPtr->remainingDigits);
			}
			else
			{
				combPtr->remainingDigits = 0;
			}
			combPtr = combPtr->next;			
		}
	}
}

// set digit combination change flag and reset all combination digits
void resetCombChange()
{
	int i;
	_cage *cPtr;
	_comb *combPtr;

	for (i = 1; i < INDEXS; i++)
	{
		if ((cPtr = cageInd[i]) != NULL)
		{
			combPtr = cPtr->ptr;

			while (combPtr)
			{
				combPtr->remainingDigits = combPtr->allDigits;			
				combPtr = combPtr->next;
			}
		}
	}
}

/********************************************************************************************
*Sudo related functions
*********************************************************************************************/

// reset all cages after returning from failed solvBoard()
void restoreCageStates(const _brdR *sBoard)
{
	int i, j;
	char *kBoard;

	kBoard = sBoard->kBoard;

	if (!kBoard)
		return;

	// reset all combinations digits in all cages
	resetCombChange();

	for (i = 0; i < 9; i++)
	{
		for (j = 0; j < 9; j++)
		{
			if (sBoard->score[i][j] < 0)
				setCageComboBits(kBoard[i * 9 + j], sBoard->board[i][j]);
		}
	}
}

void printBoard (char b[9][9])
{
	int i,j;

	for (i = 0; i < 9; i++)
	{
		for (j = 0; j < 9; j++)
			printf("%d ", b[i][j]);

		printf("\n");
	}
}

void printStat(_brdR *b, int r, int c, int v)
{
	switch (loging)
	{
	case 2:
		printBoard(b->board);
	case 1:
		printf("Iterations: %I64d, (stk:%I64d), (recd:%I64d), (r:%d), (c:%d), (v:%d) Non empty cells: %d\n", iter, stack, deepest, r, c, v, b->cells);
	default:
		break;
	}
}



// Once we set a cell with nre value, all cells on same raw, col must be updated
// so that this value is not a valid value for them
void updateLines (_brdR *b, const int r, const int c, const char v)
{
	int i;

	for (i = 0; i < 9; i++)
	{
		b->bitMask[r][i] = clearBit(v, b->bitMask[r][i]);
		b->bitMask[i][c] = clearBit(v, b->bitMask[i][c]);

		if (b->score[r][i] > 0)
			b->score[r][i] = countSetBits(b->bitMask[r][i]);
		if (b->score[i][c] > 0)
			b->score[i][c] = countSetBits(b->bitMask[i][c]);
	}
}

// Once we set a cell with nre value, all cells on same block must be updated
// so that this value is not a valid value for them
void updateBlock (_brdR *b, const int r, const int c, const char v)
{
	int i1, i2, j1, j2;
	_addR vertex = sBlks[r/3][c/3];

	i2 = vertex.raw + 3;
	j2 = vertex.col + 3;

	for (i1 = vertex.raw; i1 < i2; i1++)
		for (j1 = vertex.col; j1 < j2; j1++)
			if (b->score[i1][j1] > 0)
			{
				b->bitMask[i1][j1] = clearBit(v, b->bitMask[i1][j1]);
				b->score[i1][j1] = countSetBits(b->bitMask[i1][j1]);
			}
}


//set new value in a given cell
int setNewValue (_brdR *b, int r, int c, char v)
{
	int cId;

	if (sKiller)
	{
		if (!b->kBoard)
			return 0;
		else
			cId = b->kBoard[r * 9 + c];
	}

	iter++;
	// check that selected location is free, otherwise return error
	if ((b->board[r][c] == 0) && (isBitSet(v, b->bitMask[r][c])))
	{
		b->board[r][c] = v;
		b->score[r][c] = -1;

		updateLines(b, r, c, v);
		updateBlock(b, r, c, v);

		if (sKiller)
		{
			// first update cage information based on goven set value
			setCageComboBits(cId, v);

			// Then update back all relevant sudoku cells
			updateCage(b, cId);
		}

		b->cells++;

		if (loging)
			printStat(b, r, c, v);
		return 1;
	}
	else
		return 0;
}

// find the first non set cell location
// maxOpt indicates max value options allowed for the selected cell
int findFirstNonSet (_brdR *b, int *r, int *c, int maxOpt)
{
	int i, j, score;

	// if board is complete no next cell for update
	if (b->cells == 81)
		return 0;

	for (i = 0; i < 9; i++)
		for (j = 0; j < 9; j++)
		{
			score = b->score[i][j];
			if(score > -1)
			{
				if ((score > 0) && (score <= maxOpt))
				{
					*r = i;
					*c = j;
					return 1;
				}
				else if (score == 0)
					// dead-end
					return 0;
			}
		}

	return 0;
}


int initSudo(FILE *fp, _brdR *pBlk, char *sBoard)
{
	int i, j;
	_addR vert;

	for (i=0; i<3; i++)
		for(j=0; j<3; j++)
		{
			vert.raw = i*3;
			vert.col = j*3;
			sBlks[i][j] = vert;
		}

	for (i=0; i < 9; i++)
		for (j = 0; j < 9; j++)
		{
			pBlk->bitMask[i][j] = 0x3fe;
			pBlk->score[i][j] = 9;
		}

	// fill board with initial values
	// read these values from stdin
	// no imput validation !!!
	for (i = 0; i < 9; i++)
	{
		for (j = 0; j < 9; j++)
		{
			if (sBoard[i*9+j] > 0)
			{
				if (!setNewValue(pBlk, i, j, sBoard[i*9+j]))
				{
					printf("Invalid board, can't add value: %d in (r:%d) (c:%d)\n", sBoard[i * 9 + j], i + 1, j + 1);
					return 0;
				}
			}
		}
	}

	return 1;
}


// this is the recursive function that is used to solve the board
// the function will update the given board only if it managed to sove it.
// otherwise it is using a local copy to try and fing solution using recursion calls
int solveBoard (_brdR *board, const int raw, const int col, const char value)
{
	int r , c;
	_brdR *b;
	int changed;
	char v;
	unsigned short bitmask;

	stack++;
	deepest = (deepest > stack? deepest: stack);

	// create local copy of board and sent it to solvBoard()
	if ((b = malloc(sizeof(_brdR))) == NULL)
		return 0;
	memcpy(b, board, sizeof(_brdR));

	//  if we call solveBoard with Value > 0 we need first to change the input board to have this value
	// as final value for this recursion
	if (value > 0)
	{
		if (!setNewValue(b, raw, col, value))
		{
			printf("fatal Error!\n");
			return 0;
		}
	}

	
	// try to solve board assuming we now might have new cells with single possible value option
	changed = 1;
	while (changed && (b->cells < 81))
	{
		changed = findFirstNonSet(b, &r, &c, 1);
		if	(changed)
			changed = setNewValue(b, r, c, getVal(b->bitMask[r][c]));
	}

	if (b->cells == 81)
	{
		// found solution - exit recursion
		memcpy(board, b, sizeof(_brdR));
		free(b);
		stack--;
		return 1;
	}


	// if we get here, it means we could not find any other cells with single value option.
	// now scan the board for all cells with more value options
	if (findFirstNonSet(b, &r, &c, 9))
	{
		bitmask = b->bitMask[r][c];
		while ((v = getVal(bitmask)))
		{
			// recursive call !!
			if (!solveBoard(b, r, c, v))
			{
				bitmask = clearBit(v, bitmask);
				if (sKiller)
				{
					restoreCageStates(b);
				}
			}
			else
			{
				// recursive call solved the board. we need to updated the input board and return
				memcpy(board, b, sizeof(_brdR));
				free(b);
				stack--;
				return 1;
			}
		}
	}

	// if we get here it means we did not find solution for this recursive branch
	free(b);
	stack--;

	return 0;
}



int main (int argc, char **argv)
{
	char  opt, *fname = NULL, str[31];
	FILE *fp = NULL;
	int stat = 1;
	char kBoard[9][9], sBoard[9][9];
	_brdR *pBlk;

	// get commad line input
	if (argc > 1)
	{
		while ((opt = getopt(argc, argv, "f:l:k")) > 0)
			switch (opt)
			{
			case 'f':
				fname = optarg;
				fp = fopen(fname, "r");
				break;
			case 'l':
				loging = atoi(optarg);
				break;
			case 'k':
				sKiller = 1;
				break;
			default:
				printf("wrong input\n");
				return stat;
			}
		}

	if (!fp)
	{
		printf("error opening file %s, error: %s\n Change to stdin\n", fname, strerror(errno));
		fp = stdin;
	}

	// allocate Sudoku board record
	if ((pBlk = calloc(1, sizeof(_brdR))) == NULL)
		return stat;

	// inint cage index array
	if ((cageInd = cageIndInit()) == NULL)
	{
		printf("failed to inintialize cage index array\n");
		return stat;
	}

	if (!readInput(fp, &kBoard[0][0], &sBoard[0][0]))
	{
		printf("failed to read input file\n");
		return stat;
	}

	if (sKiller)
	{
		if (!initKiller(&kBoard[0][0]))
		{
			printf("failed to initialize sudoku-killer board\n");
			return stat;
		}
		pBlk->kBoard = &kBoard[0][0];
	}

	if (!initSudo(fp, pBlk, &sBoard[0][0]))
	{
		printf("failed to initialize sudoku board\n");
		return stat;
	}

	// after initilizing both killer and sudo board
	// we need to update bitmasks on all cages before we go to solveboard
	if (sKiller)
	{
		int i;

		for (i = 1; i < INDEXS; i++)
			updateCage(pBlk, i);
	}

	printf("Input board:\n");
	printBoard(pBlk->board);
	printf("\n");

	iter=0;
	if ((stat = !solveBoard(pBlk, 0, 0, 0)))
		printf("failed to find solution\n");

	printBoard(pBlk->board);
	printf("Iterations: %s (recd:%I64d)\n", frmt(str, 30, "20", INT, &iter), deepest);
	free(pBlk);

	if (fp != stdin)
		fclose(fp);

	if (sKiller)
		releaseMem();

	return stat;
}
