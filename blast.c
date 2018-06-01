//
//  blast.c
//  
//
//  Created by Rahim on 5/13/18.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>

// length of one line of the database line and input line
#define MAX_LINE_LEN 10
#define MAX_DATABASE_LINES 1000
#define MAX_THREADS 128

char query[MAX_LINE_LEN];
char** database = NULL;
char** hooks = NULL;
const char* teststring = "AAAAACTAA";
int g_index = 0;
// The max number of top seqeunces displayed to the user.
unsigned int g_displayTopSequences = 3;
unsigned int g_numOfThreads = 1;

struct TopMatch {
    // when m_sequence is an empty string indicates the end of the array of top matches.
    char m_sequence[MAX_LINE_LEN];
    int  m_score; 
    int  m_match[9]; 
};

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



struct TopMatch topMatches[MAX_DATABASE_LINES];//[MAX_THREADS];

// hook = input line broken in 3 letters. Catch = DB Sequence broken into 3 letters.

int score (char* hook, char* catch, int* match) {
    int accumulator = 0;
    
    for (int j = 0; j <3; j++){
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

int sequenceScore (char** hooks, unsigned idxhook, char* sequence, unsigned catchposition ){
    printf("The index of the hook is %u and the catchposition is %u\n", idxhook, catchposition);
    int accumulator = 0;
    
    for (int i = idxhook; i < 3; i++){
        printf("Checking Hook %s\n", hooks[i]);

        accumulator = accumulator + score ( hooks[i], &sequence[catchposition], &topMatches[g_index].m_match[catchposition]);
        
        if (catchposition == 6)
            break;
        catchposition = catchposition + 3;
        
    }
    
    accumulator = accumulator + ((6 - catchposition) * - 1);
    topMatches[g_index].m_score = accumulator; 
    strcpy (topMatches[g_index].m_sequence, sequence); 
    g_index++; 
    return accumulator;
}


void printTopMatches () {
    for (int i = 0; i < g_index; i++){
        printf("Input:       %s\n", query);
        printf("             ");
        for (int j =0; j < 9; j++){
            //when top match is 1
            if (topMatches[i].m_match[j]){
                printf("|"); 
            }
            else
                printf(" "); 
        }
        printf("\n"); 
        printf("DB Sequence: %s\n", topMatches[i].m_sequence);
        printf("The score is: %d\n", topMatches[i].m_score);  
    }
}

char** createDatabase (void) {

  
    FILE *pToFile = fopen("database.txt", "r");
    int line = 0;
    char input[MAX_LINE_LEN + 1];
    
    // creating a 2d db array
    char**  database = malloc(MAX_DATABASE_LINES*sizeof(char*));
    while(fgets (input, MAX_LINE_LEN + 1, pToFile)) {
        printf("String lenght of the input is %lu\n", strlen(input));
        input[strlen(input)-1] = '\0';
        printf("DB Line: %s\n",input);
        database[line]=malloc(strlen(input)+1);
        strcpy(database[line], input);
        line ++;
    }
    database[line] = NULL;
    
    
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
            case 't':
                g_numOfThreads = atoi (optarg);
                break;
            case '?':
            default:
                printf("Usage: blast [-mt]\n");
            return 1;
        }
    }
    return 0;
}

void *thread_ptr(void *args){
    struct ThreadArgs *t_ptr = args;
    printf("Thread: %d startIndex: %d stopIndex: %d\n", t_ptr->m_tid, t_ptr->m_startIndex, t_ptr->m_endIndex);
    
    for (int i = t_ptr->m_startIndex; i < t_ptr->m_endIndex; i++) {
    
        printf("Line: %s\n",database[i]);
        int score = -9;
        for (int j = 0; j <3; j++){
            printf("database = %s hook = %s\n", database[i], hooks[j]);
            // The potential position of the hook in the DB Sequence for this line
            char* position = strcasestr(database[i], hooks[j]);
            if (NULL != position){
                int location = position-database[i];
                assert(location%3 == 0);
                score = sequenceScore ( hooks, j, database[i], location );
                
                break;
            }
            
        }
        
        printf("Sequence = %s and the score is %d\n", database[i], score);
    }

    return NULL;
    
}

// Two memory allocations, database and hooks. Get rid of them later.
int main(int argc, char * argv[]) {
    
    if (process(argc, argv))
        return -1;
    printf("Executing with %d threads and displaying %d top matches\n", g_numOfThreads, g_displayTopSequences);
    
    // Initializing the m_match datamember in the topMatch struct to 0. 
    for (int i = 0; i < 5; i++){
        for (int j =0; j < 9; j++)
         topMatches[i].m_match[j] = 0;  
    } 

    //printf ("Enter the DNA Sequence: ");
    //scanf  ("%s", query);
    //printf ("You entered: %s\n", query);
      strcpy(query, teststring);
    
    
    
    
        
    hooks = malloc(sizeof(char*)*3);
    int j = 0;
    
    for (int i = 0; i < strlen(query); i=i+3){
        hooks[j] = malloc(4);
        hooks[j][0] = query[i];
        hooks[j][1] = query[i+1];
        hooks[j][2] = query[i+2];
        hooks[j][3] = '\0';
        printf("Hooks: %s\n", hooks[j]);
        j++;
    }

    database = createDatabase();
    struct ThreadArgs t_args;
    t_args.m_tid = 0;
    t_args.m_startIndex = 0;
    t_args.m_endIndex = 8;
    thread_ptr(&t_args);
    qsort (topMatches, g_index, sizeof(struct TopMatch), &compare_topmatches);
    printTopMatches ();
    
    destroyDatabase(database);

    for (int j=0; j < 3; j++){
        free(hooks[j]);
    }
    free (hooks); 
   
    
    return 0;
}




