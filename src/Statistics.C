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



#include "Statistics.h"

/*to measure the used time since the begining of the program
fills the parameters with user time and system time.
*/

void Statistics::getTime(double & dest){
    struct rusage resources; // used resources
	getrusage(RUSAGE_SELF, &resources);
	dest = (double) resources.ru_utime.tv_sec + 1.e-6 * (double) resources.ru_utime.tv_usec + 
	       (double) resources.ru_stime.tv_sec + 1.e-6 * (double) resources.ru_stime.tv_usec;
}	

	/*sets the number of cpus and initializes the stats data structure*/

	void Statistics::setCPUS(int n){
		int i;
		nWorkers = n-1 ;
		stats.growTo(n); 
		for(i = 0; i < stats.size(); i++) {
			stats[i].masterTime = 0;
			stats[i].workerTime = 0; 
			stats[i].nSolveCalls = 0;
			stats[i].sentDB = 0;
			stats[i].receivedDB = 0;
		}
	}

	/* increases the number of erased assmptions  */

	void Statistics::increaseErased(int n){
		erasedAssumps += n;
	}

	/*To start measure the time. To be called just before a send, receive or solve().*/

 	void Statistics::startMeasure(){ 
		getTime(init); 
	}

	/*to finish one measure*/

	double Statistics::finishMeasure(){ 
		getTime(end);
		return end - init;
	}

/*----- functions that call the finishMeasure method, to be used with the startMeasure -----*/

	void Statistics::finishMeasureInit(){
		initializationTime = finishMeasure();
	}

	void Statistics::finishMeasureFinal(){
		finalizationTime = finishMeasure();
	}

/* ------------------------------------------------------ */
	/*to measure the idle time of workers*/
	void Statistics::startMeasureMasterTime(){
		getTime(initMaster);
	}

	/*increases the idle time of a given worker*/

	void Statistics::finishMeasureMasterTime(int worker){
		getTime(endMaster);
		stats[worker].masterTime += endMaster - initMaster;
	}

	/*sets the cpu time of a given worker */

	void Statistics::incCpuTime(int worker, double newTime){
		stats[worker].workerTime += newTime;
		stats[worker].nSolveCalls++;
	}

	/* increases the number of databases (with learnt clauses) received from the master*/

	void Statistics::increaseReceived(int worker){
		stats[worker].receivedDB++;
	}

	/* increases the number of databases (with learnt clauses) sent by the worker*/

	void Statistics::increaseSent(int worker){
		stats[worker].sentDB++;
	}

	/*to measure the wall time*/

	void Statistics::startMeasureWallTime(){
		struct timeval tp;
		gettimeofday(&tp, NULL);
		wall0 = (double)tp.tv_sec+(1.e-6)*tp.tv_usec;
	}

	void Statistics::finishMeasureWallTime(){
		struct timeval tp;
		gettimeofday(&tp, NULL);
		wall1 = (double)tp.tv_sec+(1.e-6)*tp.tv_usec;
	}

	/*calculates the total time spent by the computation*/

	double Statistics::calcTotalTime(bool parallel){
		int i; 
		double total = initializationTime + finalizationTime, max = 0;
		if(!parallel) {
			total += stats[0].workerTime;
		}
		else{
			//lets calculate the maximum of the execution times of the workers.
			for(i = 1; i < stats.size(); i++){
				if( max < (stats[i].workerTime + stats[i].masterTime) )
					max = stats[i].workerTime + stats[i].masterTime; 
			}
			total += max;
		}
	return total;
	}

	/*writes all the times to a file, or just one if it runs on local mode */
		
	int Statistics::write2file(bool parallel, char * fileName, Options & opts){
	     FILE * res;
		int i;
		res = fopen(fileName, "wb");
		if(res == NULL) return -1;
		fprintf(res,"Master initialization time: %f secs\n\n",initializationTime);

		if(parallel){
			fprintf(res,"Workers: %d\nVariables to be assumed: %d\n",nWorkers,opts.nVars);
			fprintf(res,"Search mode: %c\nVariable's selection mode: %c\n",opts.searchMode,opts.varChoiceMode);

			if(opts.conflicts) 
				fprintf(res,"Erased assumptions: %d\n",erasedAssumps);
			if(opts.shareLearnts) {
				fprintf(res, "Learnt max amount: %d\nLearnts max size: %d\n", opts.maxLearnts, opts.learntsMaxSize);
		    	}

			if(opts.removeLearnts) 
				fprintf(res, "All learnts were removed after each solve() call.\n");

			for(i = 1; i < stats.size(); i++){
	fprintf(res,"\nWorker %d:\nsolve() was executed %d times\nTotal time spent by worker: %lf secs\n",i,stats[i].nSolveCalls,stats[i].workerTime);
	   			fprintf(res,"Total time spent by master with this worker: %lf secs\n",stats[i].masterTime);
				if(opts.shareLearnts) fprintf(res,"Databases received: %d\nDatabases sent: %d\n",stats[i].receivedDB, stats[i].sentDB);
			}
		}
		else{
			fprintf(res,"Solve time: %lf secs\n",stats[0].workerTime);
		}
		fprintf(res,"\nMaster finalization time: %lf secs\n",finalizationTime);
		fprintf(res,"\nTotal CPU time: %lf secs\n",calcTotalTime(parallel));
		fprintf(res,"\nTotal wall time: %lf secs\n",wall1-wall0);
		fclose(res);
	return 0;
	}

/*writes a xml file with the statistics*/

	int Statistics::write2xml(bool parallel, char * fileName, Options & opts){
	     FILE * res;
	     int i;
		res = fopen(fileName, "wb");
		if(res == NULL) return -1;
	        fprintf(res, "<Statistics>\n");
		fprintf(res, "<InitializationTime>\n %f \n</InitializationTime>\n", initializationTime);
		if(parallel){
	        	fprintf(res, "<NumberOfWorkers>\n %d \n</NumberOfWorkers>\n",nWorkers);
			fprintf(res, "<NumberOfVariables>\n %d\n </NumberOfVariables>\n",opts.nVars);
		        fprintf(res, "<SearchMode>\n %c \n</SearchMode>\n",opts.searchMode);
			if(opts.conflicts)
		        	fprintf(res, "<ErasedAssumptions>\n %d \n</ErasedAssumptions>\n",erasedAssumps);
			fprintf(res,"<RemoveLearnts>\n %s\n</RemoveLearnts>\n",opts.removeLearnts ? "true": "false" );

			for(i = 1; i < stats.size(); i++){
				fprintf(res,"<Runtime worker=\"%d\">\n",i);
				fprintf(res,"<NumberOfExecutions>\n %d \n</NumberOfExecutions>\n",stats[i].nSolveCalls);
				fprintf(res,"<MasterTime>\n %f \n</MasterTime>\n",stats[i].masterTime);
				fprintf(res,"<WorkerTime>\n %f \n</WorkerTime>\n",stats[i].workerTime);
				if(opts.shareLearnts){
					fprintf(res,"<DBSent>\n %d\n</DBSent>\n",stats[i].sentDB);
					fprintf(res,"<DBReceived>\n %d\n</DBReceived>\n",stats[i].receivedDB);
				}
				fprintf(res,"</Runtime>\n");
			}
		}
		else{
			fprintf(res,"<RunTime>\n %lf \n</RunTime>\n",stats[0].workerTime);
		}
		fprintf(res,"<FinalizationTime>\n %f \n</FinalizationTime>\n",finalizationTime);
	        fprintf(res, "<MaxTime>\n %f \n</MaxTime>\n",calcTotalTime(parallel));
		fprintf(res, "<WallTime>\n %f \n</WallTime>\n",wall1-wall0);
 	        fprintf(res, "</Statistics>\n");
		fclose(res);
		return 0;
	}

