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

#include "Assumptions.h"

/*---	Constructors of the classes	---*/

AssumptionsMaker::AssumptionsMaker() :
   nAssumps(0), limit(0) {}


//this definition is needed to the compiler know that the vtable is to be placed in the object file of this source code.

void AssumptionsMaker::makeAllAssumps(vec<OccurVar> & mostUsed){
}


/*
removes from the list of all assumptions those wich contain the literals from the vector of conflicts
returns the number of removed assumptions.
*/

int AssumptionsMaker::removeConflicts(vec<int> & conflicts){
int i, j, totalOccurs, totalErased, *assump;

list<int*>::iterator iter = allAssumps.begin();

totalErased = 0;

/*for each assumption not tested*/
	while (iter != allAssumps.end()){
		totalOccurs = 0;
		assump = *iter;
		/*for each literal of the conflicts array */
		for(i = 0; i < conflicts.size(); i++){

			/*Lets check whether the i-th conflict literal is in the assumption.
			The comparison between assump[j] and 0 is needed beacause most of the 
			assumptions for progressive mode do not use all array and their end is indicated with 0*/

			for(j = 0; j < nAssumps && assump[j] != 0; j++) 
				if(assump[j] == conflicts[i]) {
					totalOccurs++;
					break;
				}
			
		}
		/*if all the conflictuos literals are in the assumption*/
		if(totalOccurs == conflicts.size()) {
			iter = allAssumps.erase(iter);
			totalErased++;
		}
		else iter++;
	}
limit -= totalErased;
return totalErased;
}

/*returns the next assumption of the assumptions list*/

int* AssumptionsMaker::nextAssumption(){
	int * res = allAssumps.front();


	allAssumps.pop_front();
return res;
}


/*indicates whether the assumptions list is empty*/

bool AssumptionsMaker::moreAssumps2Try(){
	return !allAssumps.empty();
}

/*--- 	Constructors and methods of the subclasses    ---*/

//the superclass constructor should always be the first instruction in the body of the subclass constructor.

Equal::Equal(int nVars) :
    AssumptionsMaker() {
    limit = (long) pow(2,(double) nVars);
    nAssumps = nVars;
}

/*Methods to fill an array with the literals of an assumption encoded as an integer.*/

void Equal::makeEqualHyp(int *array, int value, vec<OccurVar> & mostUsed){
int  j;
        for(j = 0; j < nAssumps; j++){
                array[j] = ((value >> j) & 0x1) ? (mostUsed[j].getVar() + 1) : -(mostUsed[j].getVar() + 1) ;
        }
}

Progressive::Progressive(int nVars) :
    AssumptionsMaker() {
    limit = (long) (2 * nVars);
    nAssumps = nVars;
}

/* In progressive mode the assumptions may have a variable amount of literals. 
In that case the end is marked with the value 0  */

void Progressive::makeProgressiveHyp(int *array, int value, vec<OccurVar> & mostUsed){
int j;
       	for(j = 0; j < value; j++){
               	array[j] = mostUsed[j].positiveMax() ? (mostUsed[j].getVar()+1): -(mostUsed[j].getVar()+1);
       	}
        if(j != nAssumps){
       	        array[j] = mostUsed[j].positiveMax() ? -(mostUsed[j].getVar()+1): (mostUsed[j].getVar()+1);
               	j++;
                if(j < nAssumps) array[j] = 0;
       	}

}

/*Constructors and methods of the modes*/

Sequential::Sequential(int nVars, vec<OccurVar> & mostUsed):
    Equal(nVars) {

    startValue = 0;

/* We make the number encoded by the signals of the vars (false -> 0, true -> 1) from which we start the search.
Less significant digit corresponds to the variable with the minimal number of occurrences.
Most significant digit to the var with max number of occurrences. */

    for(int j = 0; j < mostUsed.size(); j++)
		if(mostUsed[j].positiveMax()) startValue += (long) pow((double)2,j); 

    makeAllAssumps(mostUsed);
}


Random::Random(int nVars, vec<OccurVar> & mostUsed) : Equal(nVars) {
	makeAllAssumps(mostUsed);
}


/* Methods to fill the list allAssumps with all the assumptions.
   The constructor invokes this method. */


void Random::makeAllAssumps(vec<OccurVar> & mostUsed){
int i, j, tmp, *toSearch, * newAssump;

srand(time(NULL)); 
// lets create an array with all the encodings of the assumptions in integers and mix them

    toSearch = (int *) malloc(sizeof(int) * limit);

    for(i = 0; i < limit; i++){
	    toSearch[i] = i;
    }

    for(i = 0; i < limit; i++){
            j = rand() % limit;
            tmp = toSearch[j];
            toSearch[j] = toSearch[i];
            toSearch[i] = tmp;
    }

//lets add all the assumptions to the list

    for(i = 0; i < limit; i++){
	newAssump = (int *) malloc(sizeof(int) * nAssumps);
	makeEqualHyp(newAssump, toSearch[i], mostUsed);
	allAssumps.push_back(newAssump);
    }

    free(toSearch);
}

void Sequential::makeAllAssumps(vec<OccurVar> & mostUsed){
int *newAssump;
long i;

    i = startValue;
    do{
	newAssump = (int *) malloc(sizeof(int) * nAssumps);
	makeEqualHyp(newAssump, i, mostUsed);
	allAssumps.push_back(newAssump);
	i = (i + 1) % limit;
    }while(i != startValue);
}


FewFirst::FewFirst(int nVars, vec<OccurVar> & mostUsed) :
    Progressive(nVars) {
	makeAllAssumps(mostUsed);
}

void FewFirst::makeAllAssumps(vec<OccurVar> & mostUsed){
int i, *newAssump, *copy;

	for(i = 1; i <= nAssumps; i++){
		newAssump = (int *) malloc(sizeof(int) * nAssumps);
		makeProgressiveHyp(newAssump, i, mostUsed);
		allAssumps.push_back(newAssump);
		copy = (int *) malloc(sizeof(int) * nAssumps);
		memcpy(copy, newAssump, nAssumps * sizeof(int));
		copy[0] = -copy[0];
		allAssumps.push_back(copy);
	}
}


MoreFirst::MoreFirst(int nVars, vec<OccurVar> & mostUsed) :
    Progressive(nVars) {
	makeAllAssumps(mostUsed);
}


void MoreFirst::makeAllAssumps(vec<OccurVar> & mostUsed){
int i, *newAssump, *copy;

	for(i = nAssumps; i >= 1; i--){
		newAssump = (int *) malloc(sizeof(int) * nAssumps);
		makeProgressiveHyp(newAssump, i, mostUsed);
		allAssumps.push_back(newAssump);
		copy = (int *) malloc(sizeof(int) * nAssumps);
		memcpy(copy, newAssump, nAssumps * sizeof(int));
		copy[0] = -copy[0];
		allAssumps.push_back(copy);
	}	
}


