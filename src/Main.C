/******************************************************************************************[Main.C]
MiniSat -- Copyright (c) 2003-2005, Niklas Een, Niklas Sorensson
PMSat -- Copyright (c) 2006-2007, Luís Gil

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include <mpi.h>
#include "Solver.h"
#include <cstdio>
#include <cctype>
#include <ctime>
#include <vector>
#include <unistd.h>
#include <csignal>
#include <zlib.h>
#include <math.h>
#include "Global.h"
#include "Sort.h"
#include <string>

#include "OccurVar.h"
#include "Statistics.h"
#include "Messages.h"
#include "Assumptions.h"
#include "arg_parser.h"
#include "LearntsDB.h"

using namespace std;

#define JOB_TAG 1
#define RESULT_TAG 2
#define LEARNT_TAG 3
#define MODEL_TAG 4

#define FEW_FIRST 'f'  
#define MANY_FIRST 'm'
#define SEQUENTIAL 's'
#define RANDOM 'r'
#define LOCAL 'l'

#define MORE_OCCURRENCES 'o'
#define BIGGER_CLAUSES 'b'

#define FILENAME_SIZE 50
#define LEARNTS_MAX_SIZE 20
#define LEARNTS_MAX_AMOUNT 50
#define ASSUMPS_CPU_RATIO 3

//functions to calculate the amount of variables to assume
#define	CALC_EQUAL(t) ( ceil( log2((double) (t)) ) )
#define	CALC_PROGR(n,t) ( ceil( (n) * ((double) (t)) / 2 ) )

//max size of a line in the configuration file
#define MAX_LINE_SIZE 60

MPI_Datatype typeResult;

//=================================================================================================
// BCNF Parser:

#define CHUNK_LIMIT 1048576

static bool parse_BCNF(cchar* filename, Solver& S, vec<OccurVar> & table, char varChoiceMode)
{
    FILE*   in = fopen(filename, "rb");
    
    if (in == NULL) fprintf(stderr, "ERROR! Could not open file: %s\n", filename), exit(1);

    char    header[16];
    fread(header, 1, 16, in);
    if (strncmp(header, "BCNF", 4) != 0) fprintf(stderr, "ERROR! Not a BCNF file: %s\n", filename), MPI_Finalize(), exit(1);
      
    if (*(int*)(header+4) != 0x01020304) fprintf(stderr, "ERROR! BCNF file in unsupported byte-order: %s\n", filename), MPI_Finalize(), exit(1);

    int      n_vars    = *(int*)(header+ 8);
    //int    n_clauses = *(int*)(header+12);
    int*     buf       = xmalloc<int>(CHUNK_LIMIT);
    int      buf_sz;
    vec<Lit> c;

    for (int i = 0; i < n_vars; i++) S.newVar();

    //make the table of occurrences grow
    table.growTo(n_vars);

    for(;;){
        int n = fread(&buf_sz, 4, 1, in);
        if (n != 1) break;
        assert(buf_sz <= CHUNK_LIMIT);
        fread(buf, 4, buf_sz, in);

        int* p = buf;
        while (*p != -1){
            int size = *p++;
            c.clear();
            c.growTo(size);
            for (int i = 0; i < size; i++)
                c[i] = toLit(p[i]);
            p += size;

	//sets the occurrences of clause's literals in the table of occurrences
	    for(int i = 0; i < c.size(); i++){
		// increase the negative counter according to the method chosen to count the occurrences of the variables
		if(sign(c[i])) {
			if(varChoiceMode == MORE_OCCURRENCES) table[var(c[i])].incNegatives();
			else table[var(c[i])].incNegatives(c.size());
		}
		//increase the positive counter
		else {
			if(varChoiceMode == MORE_OCCURRENCES) table[var(c[i])].incPositives(); 
			else table[var(c[i])].incPositives(c.size());
		}
	    }

            S.addClause(c);     // Add clause.
            if (!S.okay()){
                xfree(buf); fclose(in);
                return false; }
        }
    }

    xfree(buf);
    fclose(in);

    S.simplifyDB();
    return S.okay();
}


//=================================================================================================
// DIMACS Parser:

class StreamBuffer {
    gzFile  in;
    char    buf[CHUNK_LIMIT];
    int     pos;
    int     size;

    void assureLookahead() {
        if (pos >= size) {
            pos  = 0;
            size = gzread(in, buf, sizeof(buf)); } }

public:
    StreamBuffer(gzFile i) : in(i), pos(0), size(0) {
        assureLookahead(); }

    int  operator *  () { return (pos >= size) ? EOF : buf[pos]; }
    void operator ++ () { pos++; assureLookahead(); }
};

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

template<class B>
static void skipWhitespace(B& in) {
    while ((*in >= 9 && *in <= 13) || *in == 32)
        ++in; }

template<class B>
static void skipLine(B& in) {
    for (;;){
        if (*in == EOF) return;
        if (*in == '\n') { ++in; return; }
        ++in; } }

template<class B>
static int parseInt(B& in) {
    int     val = 0;
    bool    neg = false;
    skipWhitespace(in);
    if      (*in == '-') neg = true, ++in;
    else if (*in == '+') ++in;
    if (*in < '0' || *in > '9') fprintf(stderr, "PARSE ERROR! Unexpected char: %c\n", *in), exit(3);
    while (*in >= '0' && *in <= '9')
        val = val*10 + (*in - '0'),
        ++in;
    return neg ? -val : val; }

template<class B>
static void readClause(B& in, Solver& S, vec<Lit>& lits, vec<OccurVar> & table) {
    int     parsed_lit, var;
    lits.clear();
    for (;;){
        parsed_lit = parseInt(in);
        if (parsed_lit == 0) break;
        var = abs(parsed_lit)-1;

	/* resets the size of the table if the variable's id is greater than its size */ 
	if(var >= table.size()) 
		table.growTo(var+1);
	
        while (var >= S.nVars()) S.newVar();
        lits.push( (parsed_lit > 0) ? Lit(var) : ~Lit(var) );
    }
}

template<class B>
static void parse_DIMACS_main(B& in, Solver& S, vec<OccurVar> & table, char varChoiceMode) {
    vec<Lit> lits;
    int i;
    for (;;){
        skipWhitespace(in);
        if (*in == EOF)
            break;
        else if (*in == 'c' || *in == 'p')
            skipLine(in);
        else{
            readClause(in, S, lits, table);

	    for(i = 0; i < lits.size(); i++){
		// increase the negative counter according to the method chosen to count the occurrences of the variables
		if(sign(lits[i])) {
			if(varChoiceMode == MORE_OCCURRENCES) table[var(lits[i])].incNegatives();
			else table[var(lits[i])].incNegatives(lits.size());
		}
		//increase the positive counter
		else {
			if(varChoiceMode == MORE_OCCURRENCES) table[var(lits[i])].incPositives(); 
			else table[var(lits[i])].incPositives(lits.size());
		}
	    }

            S.addClause(lits);
	    lits.clear(true);
	}
    }
}

// Inserts problem into solver.
//
static void parse_DIMACS(gzFile input_stream, Solver& S, vec<OccurVar>& table, char varChoiceMode){
    StreamBuffer in(input_stream);
    parse_DIMACS_main(in, S, table, varChoiceMode); }

//=================================================================================================

void printStats(SolverStats& stats)
{
    double  cpu_time = cpuTime();
    int64   mem_used = memUsed();
    reportf("restarts              : %"I64_fmt"\n", stats.starts);
    reportf("conflicts             : %-12"I64_fmt"   (%.0f /sec)\n", stats.conflicts   , stats.conflicts   /cpu_time);
    reportf("decisions             : %-12"I64_fmt"   (%.0f /sec)\n", stats.decisions   , stats.decisions   /cpu_time);
    reportf("propagations          : %-12"I64_fmt"   (%.0f /sec)\n", stats.propagations, stats.propagations/cpu_time);
    reportf("conflict literals     : %-12"I64_fmt"   (%4.2f %% deleted)\n", stats.tot_literals, (stats.max_literals - stats.tot_literals)*100 / (double)stats.max_literals);
    if (mem_used != 0) reportf("Memory used           : %.2f MB\n", mem_used / 1048576.0);
    reportf("CPU time              : %g s\n", cpu_time);
}

//global variable to the solver
Solver* solver;

/* interruption handler to catch CTRL-C and stop all processes. */
static void SIGINT_handler(int signum) {
      reportf("*** INTERRUPTED ***\n");
      MPI_Abort(MPI_COMM_WORLD, 1);
      MPI_Finalize();
      exit(1);
}

/*prints the usage.*/

void usage(char * progName){
	reportf("USAGE: mpirun -np number-of-CPUs %s [options] input-file [output-file]\n",progName);
	reportf("Options:\n\n");
	reportf("  -h, --help  display this help and exit\n\n");
	reportf("  -v, --verbose  enable the verbose mode\n\n");

	reportf("  -n <value>, --number-of-vars  the number of variables to assume\n\n");
	reportf("  -m <arg>, --mode  assumptions generation / search mode:\n");
	reportf("\t l - local execution without assumptions just with 1 CPU\n");
	reportf("\t Progressive mode has <arg>:\n");
	reportf("\t f - start from the assumptions with few literals\n");
	reportf("\t m - start from the assumptions with many literals\n");
	reportf("\t Equal mode has <arg>:\n");
	reportf("\t r - test the assumptions randomly\n");
	reportf("\t s - test the assumptions sequentialy\n\n");

	reportf("  -f <file>, --config-file  read a given configuration file.\n\n");
	reportf("  -g <file>, --generate-config  generate a configuration file and exit. The program is able to work without a configuration file\n\n");
	reportf("  The following options may be set in the configuration file (command line arguments override them):\n\n");
	reportf("  -c, --conflicts  detect and delete assumptions with conflicts\n\n");
	reportf("  -l, --learnts  enable the share of learnt clauses\n\n");
	reportf("  -z <value>, --learnts-max-size  set the max size of the learnt clauses to share (default is %d)\n\n",LEARNTS_MAX_SIZE);
	reportf("  -t <value>, --learnts-max-amount  set the max amount of learnt clauses to share (default is %d)\n\n",LEARNTS_MAX_AMOUNT);
	reportf("  -r, --remove-learnts  remove all the learnt clauses after each solve call\n");
	reportf("                If its share is enabled they are sent before removal\n");
	reportf("                By default the learnt clauses are kept\n\n");
	reportf("  -a <value>, --assumps-cpus-ratio  set the ratio between the number of assumptions to solve and the worker CPUs (default is %d)\n",ASSUMPS_CPU_RATIO);
	reportf("      It is used in the automatic calculation of the number of literals and mode\n\n");
	reportf("  -s <arg>, --selection  methods to select the variables to assume with <arg>:\n");
	reportf("\t o - variables with more occurrences(default)\n");
	reportf("\t b - variables in the biggest clauses\n\n");

	reportf("  input-file: may be either in plain/gzipped DIMACS format or in BCNF\n\n");
	reportf("  output-file: the file where the result is written\n");
}


/*
* Read the configuration file and fill the opts structure.
* Returns 1 if there was a problem with the file or 0 in case of success.
*/

int readConfig(Options & opts, const char *filename){
char arg[MAX_LINE_SIZE], * value;
FILE * fp;
	fp = fopen(filename,"r");
	if(!fp) return 1;
	do{
		fgets(arg,MAX_LINE_SIZE,fp);
		if(feof(fp)) break;
		if(arg[0] != '#' && arg[0] != '\n'){
			value = strstr(arg,"=");
			value[0] = '\0';
			value++;
			value[strlen(value)-1] = '\0';
			if(!strcmp(arg,"LEARNTS_MAX_SIZE"))
				opts.learntsMaxSize = atoi(value); 
			if(!strcmp(arg,"LEARNTS_MAX_AMOUNT"))
				opts.maxLearnts = atoi(value); 
			if(!strcmp(arg,"VARIABLE_SELECTION"))
				opts.varChoiceMode = !strcmp(value,"more_occurrences") ? MORE_OCCURRENCES : BIGGER_CLAUSES ;
			if(!strcmp(arg,"ASSUMPS_CPU_RATIO"))
				opts.assumpsCpuRatio = atoi(value); 
			if(!strcmp(arg,"CONFLICTS"))
				opts.conflicts = !strcmp(value,"true") ? true : false ;
			if(!strcmp(arg,"SHARE_LEARNTS"))
				opts.shareLearnts = !strcmp(value,"true") ? true : false ;
			if(!strcmp(arg,"REMOVE_LEARNTS"))
				opts.removeLearnts = !strcmp(value,"true") ? true : false ;
		}
	}while(1);
	
	fclose(fp);

return 0;
}

/*
* Generates a config file.
* Returns 1 in case of error with the file or 0 in case of success.
*/

int generateConfigFile(const char * filename){
FILE * fp = fopen(filename,"w");
if(!fp) return 1;

 fputs("#keep comments in separate lines\n",fp);
 fputs("#Do not insert spaces !\n\n",fp);

 fputs("#max size of learnt clauses\n",fp);
 fputs("LEARNTS_MAX_SIZE=20\n\n",fp);

 fputs("#share learnt clauses ?\n",fp);
 fputs("SHARE_LEARNTS=false\n\n",fp);

 fputs("#remove learnt clauses after each solve?\n",fp);
 fputs("REMOVE_LEARNTS=false\n\n",fp);

 fputs("#share conflics ?\n",fp);
 fputs("CONFLICTS=false\n\n",fp);

 fputs("#max amount of learnt clauses to send\n",fp);
 fputs("LEARNTS_MAX_AMOUNT=30\n\n",fp);

 fputs("#Ratio between the number of assumptions.\n",fp);
 fputs("#and the amount of CPUs.\n",fp);
 fputs("#Used in automatic calculations of\n",fp);
 fputs("#variables to assume and search mode\n",fp);
 fputs("ASSUMPS_CPU_RATIO=3\n\n",fp);

 fputs("#how select the variables to assume:\n",fp);
 fputs("#can be more_occurrences or bigger_clauses\n",fp);
 fputs("VARIABLE_SELECTION=more_occurrences\n",fp);

fclose(fp);
return 0;
}

/* Tests a problem for satisfiability, spliting it in sub problems and sending them to different CPUs.
* Receives the options of the program, the most popular variables, the 
* number of cpus where the program will run and the object to store the statistics of the execution.
*/

bool test4SAT(Options & opts, vec<OccurVar> & mostUsed, int cpus, Statistics & timec){

MPI_Status status;
AssumptionsMaker *gen = NULL; //assumptions generator
int workerNumber = 1, received = 0, *hyps = 0, i, *learnts, learntsSize, flag;
Result response[1];
vec<int> conflictList;
LearntsDB * db = NULL;

if(opts.shareLearnts)
	db = new LearntsDB(cpus, opts.maxLearnts * (opts.learntsMaxSize + 1) );

switch(opts.searchMode){
	case RANDOM: gen = new Random(opts.nVars, mostUsed); break;
	case SEQUENTIAL: gen = new Sequential(opts.nVars, mostUsed); break;
	case FEW_FIRST: gen = new FewFirst(opts.nVars, mostUsed); break;
	case MANY_FIRST: gen = new MoreFirst(opts.nVars, mostUsed); break;
	default: reportf("ERROR! %c is an invalid mode\n",opts.searchMode); return false; 
}

timec.finishMeasureInit(); 

if(opts.verbose) 
	reportf("Sending assumptions to try...\n\n");
do{
	timec.startMeasureMasterTime();

	/*sends requests while all processors are not busy*/
        hyps = gen->nextAssumption();

        MPI_Send(hyps, opts.nVars, MPI_INT, workerNumber, JOB_TAG, MPI_COMM_WORLD);

        workerNumber = (workerNumber + 1) % cpus;

	//increase the time that master spent working for that worker
	timec.finishMeasureMasterTime(workerNumber);

}while(workerNumber != 0 && gen->moreAssumps2Try());

do{
	timec.startMeasureMasterTime();

	//waits for an answer
	MPI_Probe(MPI_ANY_SOURCE, RESULT_TAG, MPI_COMM_WORLD, &status);
	workerNumber = status.MPI_SOURCE; 
	
	/*receives all the messages from the worker (usually should be only one) possibly containing conflictuous literals */
	do{
	        MPI_Recv(response, 1, typeResult, workerNumber, RESULT_TAG, MPI_COMM_WORLD, &status);

		/*if present, adds the conflicting literals to conflictList*/
		for(i = 0; i < response[0].conflictSize; i++){
			conflictList.push(response[0].conflict[i]);
		}
	}while(response[0].moreMsgs);

        timec.incCpuTime(workerNumber, response[0].cpuTime);

        if(response[0].result == 1) {
		timec.finishMeasureMasterTime(workerNumber);
		if(opts.verbose) 
			reportf("CPU %d found the solution !\n",workerNumber);
		break;
	}
        received++;

	/*receives a message with learnt clauses, FROM ANYONE, when the option is active and there is data to receive*/

	if(opts.shareLearnts){
		MPI_Iprobe(MPI_ANY_SOURCE, LEARNT_TAG, MPI_COMM_WORLD, &flag, &status);
		if(flag){
			MPI_Get_count(&status, MPI_INT, &learntsSize);
			if(opts.verbose) 
				reportf("Master is receiving learnt clauses, with about %d literals, from CPU %d.\n", learntsSize, status.MPI_SOURCE);
			MPI_Recv(db->learntsFrom[status.MPI_SOURCE], learntsSize, MPI_INT, status.MPI_SOURCE, LEARNT_TAG, MPI_COMM_WORLD, &status);
			db->addLearnts(status.MPI_SOURCE, learntsSize);
			timec.increaseSent(status.MPI_SOURCE);
		}
	}

	/* removes the assumptions that contain the conflict literals */

	if(opts.conflicts && conflictList.size() > 0){
		int rem = gen->removeConflicts(conflictList);
		if(opts.verbose) 
			reportf("Number of literals in conflict: %d.\nNumber of removed assumptions: %d.\n",conflictList.size(),rem);
		timec.increaseErased(rem);
		conflictList.clear(true);
	}

        if(opts.verbose) reportf("CPU %d reported UNSAT!\n", workerNumber);

        /* is there more work to send ? */

        if( gen->moreAssumps2Try() ){

		/*sends learnt clauses, to the same worker, if the share mode is selected, and there is data to send*/
		if(opts.shareLearnts){
			learnts = db->getLearnts(workerNumber, learntsSize);
			if(learnts != NULL){ 
				if(opts.verbose) 
					reportf("Master is sending learnt clauses to CPU %d...\n",workerNumber);
                       		MPI_Send(learnts, learntsSize, MPI_INT, workerNumber, LEARNT_TAG, MPI_COMM_WORLD);
				timec.increaseReceived(workerNumber);
			}
		}
		/*sends more work*/
                hyps = gen->nextAssumption();
                MPI_Send(hyps, opts.nVars, MPI_INT, workerNumber, JOB_TAG, MPI_COMM_WORLD);
		if(opts.verbose) reportf("Sending another assumption to be tryed...\n");
        }
	if(opts.verbose) printf("\n");

	timec.finishMeasureMasterTime(workerNumber);
}while(received != gen->getLimit());

return response[0].result == 1;
}


/*Writes the model of the formula to a file. 
Returns 0 if no error occurred, else return -1.*/

int writeModel(bool isSAT, Solver &S, const char * fileName){
FILE * res;
int i;
	res = fopen(fileName, "wb");
	if(res == NULL) return -1;
	if(isSAT){
		fprintf(res, "SAT\n");
	        for (i = 0; i < S.nVars(); i++)
			if (S.model[i] != l_Undef)
               			fprintf(res, "%s%s%d", (i==0)?"":" ", (S.model[i]==l_True)?"":"-", i+1);
		fprintf(res, " 0\n");
	}
	else{
		fprintf(res,"UNSAT\n");
	}
	fclose(res);
	return 0;
}

/*Writes the model of the formula, stored in one array, to a file. 
Returns 0 if no error occurred, else return -1.*/

int writeArrayModel(int * model, int size, const char * fileName){
FILE * res;
int i;
	res = fopen(fileName, "wb");
	if(res == NULL) return -1;
	fprintf(res, "SAT\n");
        for (i = 0; i < size; i++)
        	fprintf(res, "%d ", model[i]);
	fprintf(res, "0\n");
	fclose(res);
	return 0;
}

//=================================================================================================
// Main:

int main(int argc, char** argv){

    Solver  S;

    /*default options given to the program*/
    Options opts = {ASSUMPS_CPU_RATIO, 0, LEARNTS_MAX_AMOUNT, LEARNTS_MAX_SIZE, false, false, false, false, RANDOM, MORE_OCCURRENCES}; 

    //output of the solver, existence of output file, mode and number of vars to assume 
    bool result, outputFile = false, mode = false, numberOfVars = false; 

    int code = 0; // option's code, number of literals of the assumptions 

    int i, j; // auxiliar variables 

    vec<OccurVar> tableOccurs, mostUsed; // set of variables and their occurrences.
    vec<Lit> lit_hyp; //vector of assumed literals

    int error, cpus, rank, flag; //MPI variables for error, number of cpus, id of the process and flag for pending message
    MPI_Status status;

    int *hyps, *learnts, *model, learntsSize, modelSize;   //arrays of data to hypothesis, learnt clauses and model, size of the learnts and model arrays
    Result response[1];  // result sent by the worker 

    /*** for the structure ***/
    MPI_Datatype arrayOfTypes[2];
    int arrayOfBlockLengths[2];
    MPI_Aint displacements[2], extent;

    char *timeFile, *xmlFile; 	//file with the measured times and xml to be parsed automatically
    char const *inFileName = 0; 
    char const *outFileName = 0; //names of the input, output and model files.

    Statistics timeStats; // object to measure time spent and other statistics

    //lets measure the wall time 
    timeStats.startMeasureWallTime();

/* HELP, I NEED SOMEBODY, HELP ! */

if (argc == 1 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))
	usage(argv[0]),
        exit(1);

    error = MPI_Init(&argc, &argv);

    if(error != MPI_SUCCESS) {
	MPI_Abort(MPI_COMM_WORLD, 2);
	MPI_Finalize();
	exit(2);
    }

    MPI_Comm_size(MPI_COMM_WORLD, &cpus);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    timeStats.setCPUS(cpus);

   /*---- SETTING THE STRUCTURE OF THE MESSAGE ----*/

   arrayOfTypes[0] = MPI_INT;
   displacements[0] = 0;
   arrayOfBlockLengths[0] = 3 + MAX_CONFLICTS;

   MPI_Type_extent(MPI_INT,&extent);

   arrayOfTypes[1] = MPI_DOUBLE;
   displacements[1] = (3 + MAX_CONFLICTS) * extent;
   arrayOfBlockLengths[1] = 1;

   MPI_Type_struct(2, arrayOfBlockLengths, displacements, arrayOfTypes, &typeResult);
   MPI_Type_commit(&typeResult);

    /*-------- PARSING THE ARGUMENTS ---------*/

   const Arg_parser::Option options[] = {
	{'h', "help", Arg_parser::no},
	{'v', "verbose", Arg_parser::no},
	{'m', "mode", Arg_parser::yes},      
	{'n', "number-of-vars" , Arg_parser::yes}, 
	{'s', "selection", Arg_parser::yes},
	{'l', "share-learnts"  , Arg_parser::no},
	{'c', "conflicts" , Arg_parser::no},
	{'z', "learnts-max-size", Arg_parser::yes },
	{'t', "learnts-max-amount", Arg_parser::yes},
	{'r', "remove-learnts", Arg_parser::no },
	{'f', "config-file", Arg_parser::yes },
	{'g', "generate-config", Arg_parser::yes },
	{'a', "assumps-cpu-ratio", Arg_parser::yes},
	{0, 0, Arg_parser::no }
	};  

   Arg_parser parser(argc, argv, options);
    
    if(parser.error().size()) {
	usage(argv[0]);
	MPI_Abort(MPI_COMM_WORLD, 2);
	MPI_Finalize();
	exit(2);
    }

 /*--	reads the configuration file from a given dir or from executable's local dir  	--*/

    for(i = 0; i < parser.arguments(); i++) {
	code = parser.code(i);
	if(code == 'f'){
   	    if( readConfig(opts, parser.argument(i).c_str()) ) 
	       reportf("ERROR! configuration file %s not found!\nSetting default values...\n",parser.argument(i).c_str());
	    break;
	}

	if(code == 'g') {
		generateConfigFile(parser.argument(i).c_str()); 
		MPI_Finalize();
		exit(2);
	}
    }

/* some of the options given in the command line may overwrite the same given in the configuration file  */

    /*----------- READS THE ARGUMENTS  ------------*/	

    for(i = 0; i < parser.arguments(); i++) {
	
	code = parser.code(i);

	if(!code){
		if(inFileName == NULL) inFileName = parser.argument(i).c_str();
		else {outFileName = parser.argument(i).c_str(); outputFile = true;}
	} 
	else 
	switch(code){
		case 'v' : opts.verbose = true; break;
		case 'm' : mode = true; opts.searchMode = (parser.argument(i).c_str())[0]; break;
		case 'n' : numberOfVars = true; opts.nVars = atoi(parser.argument(i).c_str()); break;
		case 's' : opts.varChoiceMode = (parser.argument(i).c_str())[0]; break;
		case 'l' : opts.shareLearnts = true; break;
		case 'c' : opts.conflicts = true; break;
		case 'z' : opts.learntsMaxSize = atoi(parser.argument(i).c_str()); break; 
		case 't' : opts.maxLearnts = atoi(parser.argument(i).c_str()); break;
		case 'r' : opts.removeLearnts = true; break;
		case 'g' : break;  //ignore
		case 'f' : break; //configuration file already read
		case 'a' : opts.assumpsCpuRatio = atoi(parser.argument(i).c_str()); break;
		case 'h' :
		default  :	usage(argv[0]);
				MPI_Abort(MPI_COMM_WORLD, 2);
				MPI_Finalize();
				exit(2);
	}
    }

/*check if the given arguments are valid*/

if(opts.searchMode != LOCAL && opts.searchMode != RANDOM && opts.searchMode != SEQUENTIAL && opts.searchMode != FEW_FIRST && opts.searchMode != MANY_FIRST 
		|| opts.varChoiceMode != MORE_OCCURRENCES && opts.varChoiceMode != BIGGER_CLAUSES){
			usage(argv[0]);
			MPI_Abort(MPI_COMM_WORLD, 2);
			MPI_Finalize();
			exit(2);
	}


/* It reads the file and fills the tableOccurs with the number of occurrences of each variable */

    if (strcmp(&inFileName[strlen(inFileName)-5], ".bcnf") == 0)
        parse_BCNF(inFileName, S, tableOccurs, opts.varChoiceMode);
    else{
        gzFile in = gzopen(inFileName, "rb");
        if (in == NULL){
        	fprintf(stderr, "ERROR! Could not open file: %s\n", inFileName);
		MPI_Abort(MPI_COMM_WORLD, 2);
       		MPI_Finalize();
           	exit(2);
	}
        parse_DIMACS(in, S, tableOccurs, opts.varChoiceMode);
        gzclose(in);
    }
    
    /* IF THE PROBLEM IS UNSAT ... */
    if (!S.okay()){
        if (outputFile && rank == 0) 
		if(writeModel(false, S, outFileName))
			reportf("ERROR! Cannot write output to file!\n");
	
        reportf("Trivial problem\n");
        reportf("UNSATISFIABLE\n");
    	MPI_Type_free(&typeResult);	 
        MPI_Finalize();
        exit(20);
    }

//ERROR HANDLING
/* if the number of variables to assume is bigger than the variables of the formula... */

    if(opts.nVars > tableOccurs.size()){
	if(!rank) reportf("ERROR! Number of literals to assume is bigger than number of variables in formula !\n");
    	MPI_Type_free(&typeResult);	 
        MPI_Finalize();
        exit(2);
    }

//if somebody made a mistake and invoked the program without calling mpirun and gave a search mode different than local,
//it is switched to local and a message is printed 

if(cpus == 1 && opts.searchMode != LOCAL) {
	opts.searchMode = LOCAL;
	reportf("ERROR! The number of CPUs is 1 but the selected mode is not LOCAL!\n");
	reportf("The search mode was set to LOCAL and the execution will continue in this machine.\nTo abort hit CTRL+C.\n");
}

//and the converse...
if(cpus > 1 && opts.searchMode == LOCAL){
	opts.searchMode = RANDOM;
	reportf("ERROR! The number of CPUs is greater than 1 but the selected mode is LOCAL!\n");
	reportf("The search mode was changed and the execution will continue.\nTo abort hit CTRL+C.\n");
}
 
/* Automatic opts.nVars and opts.searchMode calculation when they are not specified */

if(opts.searchMode != LOCAL){

    if(!mode && !numberOfVars) {
		opts.nVars = (int) CALC_EQUAL( opts.assumpsCpuRatio * (cpus-1) ); 
		opts.searchMode = RANDOM;
    }

    if(mode && !numberOfVars) {
	if(opts.searchMode == RANDOM || opts.searchMode == SEQUENTIAL) 
		opts.nVars = (int) CALC_EQUAL(opts.assumpsCpuRatio * (cpus-1)); 
	else 
		opts.nVars = (int) CALC_PROGR(opts.assumpsCpuRatio, cpus-1);
    }

    if(!mode && numberOfVars) {
	/*if the ratio 2^opts.nVars / (cpus-1) is at most the assumps CPU ratio, select the random mode */

	if( pow((double)2, opts.nVars) <= opts.assumpsCpuRatio * (cpus-1) ) opts.searchMode = RANDOM;

	/*else choose progressive mode starting from the assumptions with more literals*/

	else opts.searchMode = MANY_FIRST;

    }
} // if ! LOCAL

/* Generates the name of the file for the time measures with the format:
   input file + number of cpus (master + workers) + search option + number of literals to assume + variable's choice method + conflicts + learnts*/
   
    timeFile = (char *) malloc(strlen(inFileName) + FILENAME_SIZE);
    xmlFile  = (char *) malloc(strlen(inFileName) + FILENAME_SIZE);
    
    if(opts.shareLearnts){
sprintf(timeFile,"%s-%d-%c-%d-%c%s%s-l-z%d-t%d.time",inFileName,cpus,opts.searchMode,opts.nVars,opts.varChoiceMode, opts.conflicts ? "-c" : "", opts.removeLearnts ? "-r" : "" , opts.learntsMaxSize,opts.maxLearnts);
sprintf(xmlFile,"%s-%d-%c-%d-%c%s%s-l-z%d-t%d.xml",inFileName,cpus,opts.searchMode,opts.nVars,opts.varChoiceMode, opts.conflicts?"-c":"",opts.removeLearnts ? "-r": "" ,opts.learntsMaxSize,opts.maxLearnts);
    }
    else {    
    	sprintf(timeFile,"%s-%d-%c-%d-%c%s%s.time",inFileName,cpus,opts.searchMode,opts.nVars,opts.varChoiceMode, opts.conflicts ? "-c": "", opts.removeLearnts ? "-r":"" );
    	sprintf(xmlFile,"%s-%d-%c-%d-%c%s%s.xml",inFileName,cpus,opts.searchMode,opts.nVars,opts.varChoiceMode, opts.conflicts ? "-c": "", opts.removeLearnts ? "-r" : "");
}
    S.verbosity = 0; /* NO VERBOSITY */
    solver = &S;
    signal(SIGINT,SIGINT_handler);
    signal(SIGHUP,SIGINT_handler); 


/* SPECIAL SEQUENCIAL MODE, RUNNING LOCALLY, JUST TO MEASURE THE ABSENCE OF DELAYS FROM COMMUNICATION */

 if(opts.searchMode == LOCAL){
	timeStats.finishMeasureInit(); //initialization has ended
	timeStats.startMeasure(); //measuring the solve time
        if(opts.verbose) S.verbosity = 1; 

    	result = S.solve();

	timeStats.incCpuTime(0,timeStats.finishMeasure());
	timeStats.startMeasure(); //measuring the finalization time

	if(outputFile) writeModel(result, S, outFileName);
    	reportf(result ? "\nSATISFIABLE\n" : "\nUNSATISFIABLE\n");

	timeStats.finishMeasureFinal(); //finalization has ended
        timeStats.finishMeasureWallTime();
	timeStats.write2file(false, timeFile, opts);
	timeStats.write2xml(false,xmlFile,opts);
 }

/* PARALLEL MODE */

 else{

    if(rank == 0){ /*I'm master*/

	/* indexing and sorting variables's table of occurrences */
   
	    for(i = 0; i < tableOccurs.size(); i++){
    	    	tableOccurs[i].setId(i);
	    }
	    sort(tableOccurs);
	    /* copying tableOccurs[size - opts.nVars], ... ,[size-1] to mostUsed[0], ... ,[opts.nVars-1]
	    to keep the order of the variables */

	    for(i = tableOccurs.size() - opts.nVars, j = 0; i < tableOccurs.size(); i++, j++ ) {
        	//pushing into mostUsed the vars by the same order of appearance in tableOccurs
		mostUsed.push(tableOccurs[i]);
		if(opts.verbose){
			if(opts.varChoiceMode == MORE_OCCURRENCES) 
			reportf("Var %d occurs %d times.\n",mostUsed[j].getVar(),mostUsed[j].totalOccurs());
			else
			reportf("Var %d occurs in clauses with total length of %d.\n",mostUsed[j].getVar(),mostUsed[j].totalOccurs());
		}
    	    }

	//initialization ends inside the function

        result = test4SAT(opts, mostUsed, cpus, timeStats);
	
	timeStats.startMeasure(); //measuring the finalization time
	
    	reportf(result ? "SATISFIABLE\n" : "UNSATISFIABLE\n");

	//receives the message with the model and writes it to the output file
	if(outputFile) {

		if(result){
			MPI_Probe(MPI_ANY_SOURCE, MODEL_TAG, MPI_COMM_WORLD, &status);
			MPI_Get_count(&status, MPI_INT, &modelSize);
			model = (int *) malloc(sizeof(int) * modelSize);
			MPI_Recv(model, modelSize, MPI_INT, status.MPI_SOURCE, MODEL_TAG, MPI_COMM_WORLD, &status);
			if(writeArrayModel(model, modelSize, outFileName))
				reportf("ERROR! Cannot write output to file!\n");
		}
		else {
			if(writeModel(false, S, outFileName))
				reportf("ERROR! Cannot write output to file!\n");
		}

	}

	timeStats.finishMeasureFinal(); //finalization has ended
        timeStats.finishMeasureWallTime();

	timeStats.write2file(true,timeFile,opts); // WRITES THE TIMES TO THE FILE
	timeStats.write2xml(true,xmlFile,opts); // WRITES THE TIMES TO XML 

	MPI_Abort(MPI_COMM_WORLD, result ? 10 : 20);

    }/*end if rank == 0 : master*/

    else{	/* I'm a worker */
        hyps = (int *) malloc(sizeof(int) * opts.nVars);
	learnts = (int *) malloc( sizeof(int) * (opts.maxLearnts * (opts.learntsMaxSize + 1) ) );

		while(1) {
			timeStats.startMeasure();//start measuring the solve time
			MPI_Recv(hyps, opts.nVars, MPI_INT, 0, JOB_TAG, MPI_COMM_WORLD, &status);

			//creates the literals with the correct polarity 
			for(i = 0 ; i < opts.nVars; i++){
				if(hyps[i] == 0) break; /*reached the end of vector - only in PROGRESSIVE mode !!*/
				if(hyps[i] > 0) lit_hyp.push( Lit(hyps[i] - 1) );
				else lit_hyp.push( ~Lit(abs(hyps[i]) - 1) );
				hyps[i] = abs(hyps[i])-1;
			}

			learntsSize = 0;
			/* are there learnt clauses to receive ? */
			if(opts.shareLearnts){
				MPI_Iprobe(0, LEARNT_TAG, MPI_COMM_WORLD, &flag, &status);
				if(flag){
					MPI_Get_count(&status, MPI_INT, &learntsSize);
					MPI_Recv(learnts, learntsSize, MPI_INT, status.MPI_SOURCE, LEARNT_TAG, MPI_COMM_WORLD, &status);
				}
			}
			
			/*adds the clauses to the solver database (if any) and runs the solver*/

			if(learntsSize) 
				S.addLearnts(learnts, learntsSize);

	    		response[0].result = S.solve(lit_hyp) ? 1 : 0;

			/* if SAT, sends the model to the master */
			if(response[0].result){
				//send result ...
				response[0].moreMsgs = 0;
				response[0].cpuTime = timeStats.finishMeasure();
				MPI_Send(response, 1, typeResult, 0, RESULT_TAG, MPI_COMM_WORLD);
				//... and message with the model
				if(outputFile){
					model = (int *) malloc(sizeof(int) * S.nVars());
	        			for (i = 0, j = 0; i < S.nVars(); i++){
						if (S.model[i] != l_Undef)
               						model[j++] = (S.model[i]==l_True) ? (i+1) : -(i+1);
					}
				// j contains the size of the array
				MPI_Send(model, j, MPI_INT, 0, MODEL_TAG, MPI_COMM_WORLD);
				}
				continue;
			}

			/* Do we need to share learnt clauses ? let's put it here to measure the time consumed.
			   Sends the learnt clauses if they exist */

			if(opts.shareLearnts){
				S.getLearnts(opts.maxLearnts, opts.learntsMaxSize, learnts, learntsSize);
				if(learntsSize)
					MPI_Send(learnts, learntsSize, MPI_INT, 0, LEARNT_TAG, MPI_COMM_WORLD);
			}

			if(opts.removeLearnts) S.dellAllLearnts();

			/* Creates the conflict vector and sends it over several messages if necessary */
			if(opts.conflicts && S.conflict.size() > 0){
				j = 0;
				for(i = 0; i < S.conflict.size(); i++){
					//send the inverse of the literal
					response[0].conflict[j] = sign(S.conflict[i]) ? (var(S.conflict[i])+1) : -(var(S.conflict[i])+1);
					j++;

					/* if the array is full or we reached the end of the list of conflicts*/
					if( (i + 1) == S.conflict.size() || j == MAX_CONFLICTS ) {
						response[0].conflictSize = j;
						j = 0;
						/*if we reached the end of the list of conflicts we send the time measure and the 
						indication that there are no more messages*/
						if((i + 1) == S.conflict.size()){
							response[0].moreMsgs = 0;
						} 
						else{
							response[0].moreMsgs = 1;
						}
						response[0].cpuTime = timeStats.finishMeasure();
						MPI_Send(response, 1, typeResult, 0, RESULT_TAG, MPI_COMM_WORLD);
					}//if j
				}//for
			}//if CONFLICT
			else{
				response[0].conflictSize = 0;
				response[0].moreMsgs = 0;
				response[0].cpuTime = timeStats.finishMeasure();
				MPI_Send(response, 1, typeResult, 0, RESULT_TAG, MPI_COMM_WORLD);
			}
			
			lit_hyp.clear(true);
		}//while 1
    } /* else worker */
 } /* else of PARALLEL MODE*/

    MPI_Type_free(&typeResult);	 
    MPI_Finalize();

    exit(S.okay() ? 10 : 20);     // (faster than "return", which will invoke the destructor for 'Solver')
}
