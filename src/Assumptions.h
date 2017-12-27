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


#ifndef ASSUMPTIONS_H
#define ASSUMPTIONS_H

#include <cstdlib> 
#include <cmath>
#include <ctime>
#include <list>
#include "Global.h"
#include "OccurVar.h"
using namespace std;

/*Abstract class to make an assumptions generator with virtual methods to create the next assumption 
to test and to report whether there are more assumptions to try*/

class AssumptionsMaker {

    public:
        AssumptionsMaker();
        virtual ~AssumptionsMaker(){};
	
	int removeConflicts(vec<int> & conflicts);
	int* nextAssumption();
        bool moreAssumps2Try();
	int getLimit(){return limit;}
	virtual void makeAllAssumps(vec<OccurVar> & mostUsed);

    protected:
        int nAssumps; 	 /*number of variables to assume*/
	long limit;	 /*total number of different assumptions*/
	bool conflicts;  /*flag to remove assumptions based on conflicts*/
	list<int*> allAssumps; /*list with all assumptions saved in arrays of integers*/
};

/*Class for Equal search method*/

class Equal : public AssumptionsMaker {
	public:
		/*receives the number of variables to assume */
		Equal(int nVars);
		virtual ~Equal(){};

		/*creates an array of literals with polarity from the value and the most popular variables*/
		void makeEqualHyp(int *array, int value, vec<OccurVar> & mostUsed);
};

/*Class for Progressive search method*/

class Progressive : public AssumptionsMaker {
	public:
		/* receives the number of variables to assume */
		Progressive(int nVars);
		virtual ~Progressive(){};

		/* creates an assumption in an array based on the current index of the most used variables's vector */
		void makeProgressiveHyp(int *array, int value, vec<OccurVar> & mostUsed);
};

class Sequential : public Equal {
    public:
	/* receives the number of variables to assume and their ids*/
        Sequential(int nVars, vec<OccurVar> & mostUsed);
        ~Sequential(){};

	void makeAllAssumps(vec<OccurVar> & mostUsed);

    private:
        long startValue;
};

class Random : public Equal {
    public:
	/* receives the number of variables to assume and their id's*/
        Random(int nVars, vec<OccurVar> & mostUsed);
        ~Random(){};

	void makeAllAssumps(vec<OccurVar> & mostUsed);
};

class FewFirst : public Progressive {
    public:		
	/* receives the number of variables to assume */
        FewFirst(int nVars, vec<OccurVar> & mostUsed);
        ~FewFirst(){};

	void makeAllAssumps(vec<OccurVar> & mostUsed);
};

class MoreFirst : public Progressive {
    public:
	/* receives the number of variables to assume */
        MoreFirst(int nVars, vec<OccurVar> & mostUsed);
        ~MoreFirst(){};

	void makeAllAssumps(vec<OccurVar> & mostUsed);
};

#endif

