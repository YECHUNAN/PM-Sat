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



#ifndef STATISTICS_H
#define STATISTICS_H

#include <sys/resource.h>
#include <stdio.h>
#include "Global.h"
#include "Messages.h"
#include <sys/time.h>

typedef struct {
	double masterTime;   // time spent by the master processing the responses of the worker
	double workerTime;    // total cpu time spent by one worker, including communication time 
	int nSolveCalls; // number of times solve() was called and finished
	int sentDB; 	 // number of databases sent to the master 
	int receivedDB;	 // number of received databases from the master
} workerStats;

class Statistics {

private:

	int nWorkers, erasedAssumps; //number of workers, number of assumed variables and number of erased assumptions

	vec<workerStats> stats; // stats for each worker

	double init, end;    //init and end times used for several measures
	double initMaster, endMaster; // init and end times to measure the time spent by master master
	double initializationTime, finalizationTime;
	double wall0, wall1;

	/*to measure the used time since the begining of the program
	fills the parameters with user time and system time. */

	void getTime(double & dest);

public:

	/*constructor and destructor*/
	Statistics() : nWorkers(0), erasedAssumps(0), init(0), end(0)  {}
	~Statistics() {}

	/*sets the number of cpus and initializes the stats data structure*/
	void setCPUS(int n);

	/* increases by n the number of erased assumptions */
	void increaseErased(int n);

	/* increases the number of databases (with learnt clauses) received from the master*/
	void increaseReceived(int worker);

	/* increases the number of databases (with learnt clauses) sent by the worker*/
	void increaseSent(int worker);

	/*To start measure the time. To be called just before a send, receive or solve().*/
	void startMeasure();

	/*to finish measure the cpu time elapsed since the last call on startMeasure()*/
	double finishMeasure(); 

/*----- functions that call the finishMeasure method. To be used with startMeasure() -----*/

	//to calculate the initialization time
	void finishMeasureInit();

	//to calculate the finalization time
	void finishMeasureFinal();


/*----------------------------------------------------------*/

	/*to measure the idle time of workers*/
	void startMeasureMasterTime();

	/*increases the idle time of a given worker*/
	void finishMeasureMasterTime(int worker);

	/*to measure the wall time*/

	void startMeasureWallTime();
	void finishMeasureWallTime();

	/*increases the total computation time of a given worker */
	void incCpuTime(int worker, double newTime);

	/*calculates the total time spent by the computation*/
	double calcTotalTime(bool parallel);

	/*writes all the times to a file, or just one if it runs on local mode */
	int write2file(bool parallel, char * fileName, Options & opts);

	/*... or to a xml file to be parsed to collect the statistics*/
	int write2xml(bool parallel, char * fileName, Options & opts);

};

#endif
