//
//  blast.c
//  
//
// Created by Rahim on 5/13/18.
// Main thread starts all other threads,

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/timeb.h>

// length of one line of the database line and input line
#define MAX_LINE_LEN 10
#define MAX_DATABASE_LINES 1000
#define MAX_THREADS 128

char query[MAX_LINE_LEN];
char** database = NULL;
char** hooks = NULL;
const char* teststring = "ATGGCATAG";
int g_index = 0;
int g_dbsize = 0;
// The max number of top seqeunces displayed to the user.
unsigned int g_displayTopSequences = 3;
unsigned int g_numOfThreads = 1;
unsigned g_debug = 0;

struct TopMatch {
    // when m_sequence is an empty string indicates the end of the array of top matches.
    char m_sequence[MAX_LINE_LEN];
    int  m_score; 
    int  m_match[9];
    int  m_inputstart;
};
struct TopMatch g_topMatches[MAX_DATABASE_LINES];


struct ThreadArgs {
    int m_tid;
    // DB Lines, the end is inclusive where we specify 4, we include the 4th item.
    unsigned int m_startIndex;
    unsigned int m_endIndex;
};


int compare_topmatches(const void *lhs, const void *rhs){
    struct TopMatch *lhstm = (struct TopMatch*)lhs; 
    struct TopMatch *rhstm = (struct TopMatch*)rhs;

    if (lhstm->m_score < rhstm->m_score)
        return 1; 
    else if (lhstm->m_score > rhstm->m_score)
        return -1; 

        return 0; 
} 



struct TopMatch threadMatches[MAX_DATABASE_LINES][MAX_THREADS];

// hook = input line broken in 3 letters. Catch = DB Sequence broken into 3 letters.

int score (char* hook, char* catch, int* match) {
    int accumulator = 0;
    
    for (int j = 0; j < 3 && catch[j]; j++){
      if (tolower(hook[j]) == tolower(catch[j])){
	accumulator ++;
	match[j] = 1; 
      }   
      else {
	accumulator --;
      }
    }
    return accumulator;
}

//
// Determine score of a sequence 
// @hooks array of hooks
// @idxhook next hook to check for match.
// @sequence from the database
// @catchposition position with seqence to search for the matches
// @tid thread id used for storage of thread specific results
// @index location within thread specific storage to store results. 
//
int sequenceScore (char** hooks, unsigned idxhook, char* sequence, unsigned catchposition, int tid, int index ){
    if (g_debug) printf("The index of the hook is %u and the catchposition is %u\n", idxhook, catchposition);
    int accumulator = 0;
    
    for (int i = idxhook; i < 3; i++){
        if (g_debug) printf("Checking Hook %s\n", hooks[i]);

        accumulator = accumulator + score ( hooks[i], &sequence[catchposition], &threadMatches[index][tid].m_match[catchposition]);
        
        if (catchposition >= 6)
            break;
        catchposition = catchposition + 3;
        
    }
    
    if (catchposition < 6)
      accumulator = accumulator + ((6 - catchposition) * - 1);
    threadMatches[index][tid].m_score = accumulator;
    strcpy (threadMatches[index][tid].m_sequence, sequence);
    threadMatches[index][tid].m_inputstart = (idxhook*3);
    return accumulator;
}

void printTopMatches (int TotalTopMatches) {
    for (int i = 0; i < TotalTopMatches &&  i < g_displayTopSequences; i++){
        int jj = 0;
        while (!g_topMatches[i].m_match[jj]) {
            jj++;
        }
        if (g_topMatches[i].m_inputstart < jj)
            printf("%*c", (jj - g_topMatches[i].m_inputstart), ' ');
        //printf ("The value of j is %d\n", jj);
        // printf ("The value of the input match is %d\n", g_topMatches[i].m_inputstart);
        printf("%s\n", query);
        if (g_topMatches[i].m_inputstart > jj)
            printf("%*c", (g_topMatches[i].m_inputstart - jj), ' ');
        for (int j =0; j < 9; j++){
            //when top match is 1
            if (g_topMatches[i].m_match[j]){
                printf("|");
            }
            else
                printf(" ");
        }
        printf("\n");
        if (g_topMatches[i].m_inputstart > jj)
            printf("%*c", (g_topMatches[i].m_inputstart - jj), ' ');
        printf("%s\n", g_topMatches[i].m_sequence);
        printf("The score is: %d\n", g_topMatches[i].m_score);
    }
}

// The database has n lines each a c-string null terminated. The end
// of the database is denoted by a null line. 
char** createDatabase (void) {
  
    FILE *pToFile = fopen("database1000.txt", "r");
    
    char input[MAX_LINE_LEN + 1];
    
    // creating a 2d db array
    char**  database = malloc(MAX_DATABASE_LINES*sizeof(char*));
    while(fgets (input, MAX_LINE_LEN + 1, pToFile)) {
        if (g_debug) printf("String lenght of the input is %lu\n", strlen(input));
        input[strlen(input)-1] = '\0';
        if (g_debug) printf("DB Line: %s\n",input);
        database[g_dbsize]=malloc(strlen(input)+1);
        strcpy(database[g_dbsize], input);
        g_dbsize ++;
    }
    // Add null line to denote end of database.
    database[g_dbsize] = NULL;
    
    fclose(pToFile);

    return database;
}

void destroyDatabase (char** database) {
    int i = 0; 
    while (i < MAX_DATABASE_LINES) {
        if (database[i] != NULL) {
            free(database[i]); 
        }
        else
            break;
        i++;
    }
}

int process(int argc, char ** argv) {
    int c;
    while ((c = getopt (argc, argv, "m:t:")) != -1)
    {
        switch (c)
        {
                // number of matches
            case 'm':
                g_displayTopSequences = atoi (optarg);
                break;
                // number of threads the user wants to use, if they don't enter it the default is 1 thread.
            case 't':
                g_numOfThreads = atoi (optarg);
                break;
            case '?':
            default:
                if (g_debug) printf("Usage: blast [-mt]\n");
            return 1;
        }
    }
    return 0;
}

// this function runs for a single thread.
void *thread_fptr(void *args){
    struct ThreadArgs *t_ptr = args;
    if (g_debug) printf("Thread: %d startIndex: %d stopIndex: %d\n", t_ptr->m_tid, t_ptr->m_startIndex, t_ptr->m_endIndex);
    int index = 0;
    for (int i = t_ptr->m_startIndex; i < t_ptr->m_endIndex; i++) {
    
        if (g_debug) printf("Line: %s\n",database[i]);
        int score = -9;
        for (int j = 0; j <3; j++){
            if (g_debug) printf("database = %s hook = %s\n", database[i], hooks[j]);
            // The potential position of the hook in the DB Sequence for this line
            char* position = strcasestr(database[i], hooks[j]);
            if (NULL != position){
                int location = position-database[i];
                //assert(location%3 == 0);
                score = sequenceScore ( hooks, j, database[i], location, t_ptr->m_tid, index);
                index++;
                break;
            }
            
        }
        if (g_debug) printf("Sequence = %s and the score is %d\n", database[i], score);
    }
    
    threadMatches[index][t_ptr->m_tid].m_sequence[0] = '\0';

    return NULL;
    
}

// Two memory allocations, database and hooks. Get rid of them later.
int main(int argc, char * argv[]) {
    
    struct timeb start_time;
    ftime(&start_time);
    if (process(argc, argv))
        return -1;
    if (g_debug) printf("Executing with %d threads and displaying %d top matches\n", g_numOfThreads, g_displayTopSequences);
    
    // Initializing the m_match datamember in the topMatch struct to 0. 
    for (int i = 0; i < 5; i++){
        //for (int j =0; j < 9; j++)
        // threadMatches[i].m_match[j] = 0;
    } 

    //if (g_debug) printf ("Enter the DNA Sequence: ");
    //scanf  ("%s", query);
    //if (g_debug) printf ("You entered: %s\n", query);
      strcpy(query, teststring);
    

    
    
        
    hooks = malloc(sizeof(char*)*3);
    int j = 0;
    
    for (int i = 0; i < strlen(query); i=i+3){
        hooks[j] = malloc(4);
        hooks[j][0] = query[i];
        hooks[j][1] = query[i+1];
        hooks[j][2] = query[i+2];
        hooks[j][3] = '\0';
        if (g_debug) printf("Hooks: %s\n", hooks[j]);
        j++;
    }
   
    database = createDatabase();
    struct ThreadArgs t_args;
    t_args.m_tid = 0;
    t_args.m_startIndex = 0;
    t_args.m_endIndex = 8;
    pthread_t thread;
    int create_result, join_result;
    // allocate memory to a pointer to threadargs.
    struct ThreadArgs *argsArray = malloc(g_numOfThreads*sizeof(struct ThreadArgs));
    
    // allocate memory to a pointer to threadargs.
    pthread_t *pthreadArray = malloc(g_numOfThreads*sizeof(pthread_t));
    
    // No of DB lines assigned to a thread
    int no_of_dblines_assigned = g_dbsize / g_numOfThreads;
    
    // storing the threadargs for the number of threads the user requests.
    for (int i = 0; i < g_numOfThreads; i++){
        argsArray[i].m_tid = i;
        argsArray[i].m_startIndex = (i*no_of_dblines_assigned);
        argsArray[i].m_endIndex = (((i+1)*no_of_dblines_assigned)-1);
        
        // create modifies the thread, hence we pass &thread, the address (pointer)
        create_result = pthread_create(&pthreadArray[i], NULL, thread_fptr, &argsArray[i]);
        if (create_result){
            if (g_debug) printf("pthread create failed %d %s\n", create_result, strerror(create_result));
            return -1;
        }
        
    }
    
    for (int i = 0; i < g_numOfThreads; i++ ) {
   
        // join needs to read only the threads, hence we pass the thread as a value (it does not need to  modify).
        join_result = pthread_join(pthreadArray[i], NULL);
        
        if (join_result){
            if (g_debug) printf("pthread join failed %d %s\n", join_result, strerror(join_result));
            return -1;
        }
        
        if (g_debug) printf("Join on thread %d.\n", i);
    }
    
    int TotalTopMatches = 0;
    
    for (int i = 0; i < g_numOfThreads; i++){
        int j = 0;
        while (threadMatches[j][i].m_sequence[0] != '\0'){
            g_topMatches[TotalTopMatches] = threadMatches[j][i];
            j++, TotalTopMatches++;
        }
        
    }
    
    qsort (g_topMatches, TotalTopMatches, sizeof(struct TopMatch), &compare_topmatches);
    printTopMatches (TotalTopMatches);
    
    destroyDatabase(database);

    for (int j=0; j < 3; j++){
        free(hooks[j]);
    }
    free (hooks); 
   
    struct timeb end_time;
    ftime(&end_time);
    
    unsigned long elapsed_msec;
    
    elapsed_msec = (end_time.time - start_time.time)*1000;
    elapsed_msec = elapsed_msec + (end_time.millitm - start_time.millitm);
    
    
    printf("The elapsed time is %lu msec\n", elapsed_msec);
    
    return 0;
}




