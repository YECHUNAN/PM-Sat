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



#ifndef OCCUR_VAR_H
#define OCCUR_VAR_H

#include "SolverTypes.h"

class OccurVar {
private:
	int id;        // number of the variable
	unsigned int negatives; // number of negative literals
	unsigned int positives; // number of positive literals
public:
	OccurVar() : id(0), negatives(0), positives(0) {}
	int getVar() { return id; }
	//total number of occurrences of the variable
	unsigned int totalOccurs() { return negatives + positives; }
	//to set the id of the variable
	void setId(int i){ id = i; }
	//increase the number of positive occurrences
	void incPositives (int value = 1) { positives += value; }
	//increase the number of negative occurrences
	void incNegatives (int value = 1) { negatives += value; }
	//does the positive literal occur more times than the negative ?
	bool positiveMax () { return negatives > positives ? false : true;  }
	//to compare by the total amount of occurrences
	friend	bool operator< (OccurVar & p, OccurVar & q) { return  p.totalOccurs() < q.totalOccurs(); }
};


#endif
