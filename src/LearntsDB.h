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


#ifndef LEARNTS_DB
#define LEARNTS_DB

#include "Global.h"

/*
Class to implement a database to save the learnt clauses created by the solver and sent by the workers to the master.
The database has the following behaviour:

- Records the last array of learnt clauses sent by each worker in vector learntsFrom;
- Records the size of each of the previous arrays in the vector learntsSize;
- Logs the workers that received each array of learnt clause in vector receivers;

The number of the worker CPU is used to index the previous arrays.

The main concern is send to each worker only new information.

- When the master receives an array of clauses, replaces the previous one of the same worker by the new and 
  clears the list of receivers.
- When is needed to choose an array to send to a given worker, we choose an array from a different worker,
  after confirming that the worker is not in the list of receivers of that array.


*/

class LearntsDB {

private:

	int currentPos; // for round robin
	vec<int> learntsSize; //number of literals and separators on the i-th array in learntsFrom vector
	vec< vec<int> > receivers; //workers that already received the i-th set of learnt clauses from vector learntsFrom

public:

	vec<int*> learntsFrom; // to rewrite the buffers during execution of the master

	/*Constructor*/

	LearntsDB(int nCpus, int bufferSize);

	/* Add learnt clauses by a certain CPU. */

	void addLearnts(int fromCpu, int learntSize);

	/* Gets a learnt clause to a certain cpu. This clause was registered by other cpu different from the receiver.
	   May return NULL if all learnt clauses were already retrieved by the receiver or if the database is empty. */

	int* getLearnts(int toCpu, int & learntSize);

};

#endif
