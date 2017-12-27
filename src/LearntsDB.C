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

#include "LearntsDB.h"

	/*Constructor*/

	LearntsDB::LearntsDB(int nCpus, int bufferSize){
		int i;
		currentPos = 1;
		learntsFrom.growTo(nCpus);
		learntsSize.growTo(nCpus);
		for(i = 0; i < nCpus; i++) {
			learntsFrom[i] = (int *) malloc(sizeof(int) * bufferSize);
			learntsSize[i] = 0;
		}
		receivers.growTo(nCpus);
	}

	/* Add info about a set of learnt clauses provided by a certain CPU. */

	void LearntsDB::addLearnts(int fromCpu, int learntSize){
		learntsSize[fromCpu] = learntSize;
		receivers[fromCpu].clear(true);
	}

	/* Gets a learnt clause to a certain cpu. This clause was registered by other cpu different from the receiver.
	   May return NULL if all learnt clauses were already retrieved by the receiver. */

	int* LearntsDB::getLearnts(int toCpu, int & learntSize){
		int i, j;
		learntSize = 0;
		//for each list of receivers 
		i = currentPos; 
		do{
			//do not search the receive's list or those without learnt clauses.
			if(i != toCpu && learntsSize[i] > 0){
				//if the cpu doesn't belong to the list, give him the clauses.
				for(j = 0; j < receivers[i].size(); j++){
					if(receivers[i][j] == toCpu) break;
				}
				//true when the list is empty or the end was reached
				if(j == receivers[i].size()){
					receivers[i].push(toCpu);
					learntSize = learntsSize[i];
					currentPos = (currentPos + 1) % learntsFrom.size();
					if(!currentPos) currentPos++;
					return learntsFrom[i];
				}
			}
			i = (i + 1) % learntsFrom.size();
			if(!i) i++;
		}while(i != currentPos);

		currentPos = (currentPos + 1) % learntsFrom.size();
		if(!currentPos) currentPos++;

	return NULL; //there's nothing to send
	}


