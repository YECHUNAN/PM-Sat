/*PMSat -- Copyright (c) 2006-2007, Luís Gil

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.*/


#ifndef MESSAGES_H
#define MESSAGES_H

#define MAX_CONFLICTS 20

/*message types used in communication by the worker to indicate the format of the solve*/

typedef struct {
	int result; 		       // true or false
	int conflict[MAX_CONFLICTS];  //conflict vector
	int conflictSize; 	     // conflict size (number of literals in the array)
	int moreMsgs;  		    // are there more messages to send ?  
	double cpuTime; 	   // total cpu time spent by one worker, since the end of initialization  
}Result;

/* struct to store the relevant options and values */

typedef struct {
	int assumpsCpuRatio;	//ratio between the number of assumptions to test and the number of CPUs
	int nVars;		//number of variables to assume
	int maxLearnts; 	//max number of learnt clauses to share
	int learntsMaxSize;	//max size (amount of literals) of learnt clauses to share
	bool conflicts;		//should share conflicts ?
	bool shareLearnts;	//should share learnt clauses ?
	bool removeLearnts;	//should remove learnt clauses after solve ?
	bool verbose;		//enable verbose mode ?
	char searchMode;	//type of search mode
	char varChoiceMode;	//type of mode to select the variables to assume
} Options;

#endif
