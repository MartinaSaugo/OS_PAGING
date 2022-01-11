/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * hugerandom.c
 *
 * 	Tests the VM system by RANDOMLY accessing a large array (sparse) that
 *	cannot fit into memory.
 *
 *	When the VM system assignment is done, your system should be able
 *	to run this successfully.
 */

#include <stdio.h>
#include <stdlib.h>

#define PageSize	4096  
#define NumPages	512  

typedef struct {
	int i;
	int j;
	int old;
} position_t; 

int sparse[NumPages][PageSize];	/* use only the first element in the row */
position_t history[NumPages];	/* keep the history of the randomly accessed positions */

int
main(void)
{
	int i, j, n;

	/* pseudo-random but deterministic seed */
	srandom(11012022);
	printf("Entering the hugerandom program - I will stress test your VM\n");

	/* initialize the matrix with random values */
	for(i=0; i < NumPages; i++){
		for(j=0; j < PageSize; j++){
			sparse[i][j] = 0;
		}
	}

	printf("stage [1] done\n");

	/* increment a random position inside a random page */
	for(n=0; n < NumPages; n++){
		i = random() % NumPages;
		j = random() % PageSize;
		history[n].i = i;
		history[n].j = j;
		// keep the old value
		history[n].old = sparse[i][j];	
		sparse[i][j]++;
	}

	printf("stage [2] done\n");
	
	/* check that the new value is bigger than the old one, this means that it has been incremented */
	for(n=0; n < NumPages; n++){
		i = history[n].i;
		j = history[n].j;
		if(sparse[i][j] <= history[n].old){
			printf("BAD NEWS!!! - your VM mechanism has a bug!\n");
			exit(1);
		}
	}

	printf("stage [3] done\n");

	printf("You passed!\n");

	return 0;
}

