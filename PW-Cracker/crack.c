//This program brute-forces a given password hash by trying all possible
//passwords of a given length.
//
//Usage:
//crack <threads> <keysize> <target>
//
//Where <threads> is the number of threads to use, <keysize> is the maximum
//password length to search, and <target> is the target password hash.
//
//For example:
//
//./crack 1 5 na3C5487Wz4zw
//
//Should return the password 'apple'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  
#include <pthread.h>
#include <crypt.h>


#define USAGE "crack <threads> <keysize> <target> <enable special characters>(optional)"

// global variables
char found = 0;   // Determines whether the password has been found
char enableSpecialChars = 0; // Enable flag for special characters

struct PassData {
  int threadCount;
  int curThread; 
  int keysize;
  char* target; 
  char salt[3];
};

void* cracker(void* args);
void checkCombinations(char start, int length, char* target, char* salt, char begin, char end);

int main(int argc, char* argv[]) {

    if (argc != 4) {
        if(argc == 5) {
          enableSpecialChars = 1;
        }
        else {
          printf("Error: Improper Command Line Arguments\nProper Use:\n%s\n", USAGE);
          exit(EXIT_FAILURE);
        }
    }
    

    // Three arguments from command line:
    int thread_count;
    int keysize;
    char* target = argv[3];

    // Error checking:
    char* endptr;
    thread_count = (int)strtol(argv[1], &endptr, 10);
    if (endptr == argv[1]) {
        printf("Error: No valid digits found from argument <threads>\nProper Use:\n%s\n", USAGE);
        exit(EXIT_FAILURE);
    }
    else if (*endptr != '\0') {
        printf("Invalid character encountered from argument <threads> : %c\nProper Use:\n%s\n", *endptr, USAGE);
        exit(EXIT_FAILURE);
    }
    else if (thread_count <= 0) {
        printf("Error: Zero or Negative value found from argument <threads> (Must be a positive integer)\n");
        exit(EXIT_FAILURE);
    }

    keysize = (int)strtol(argv[2], &endptr, 10);
    if (endptr == argv[2]) {
        printf("Error: No valid digits found from argument <keysize>\nProper Use:\n%s\n", USAGE);
        exit(EXIT_FAILURE);
    }
    else if (*endptr != '\0') {
        printf("Error: Invalid character encountered from argument <keysize> : %c\nProper Use:\n%s\n", *endptr, USAGE);
        exit(EXIT_FAILURE);
    }
    else if (keysize > 8) {
        printf("Error: Keysize value exceeds maximum allowed (8)\n");
        exit(EXIT_FAILURE);
    }
    else if (keysize <= 0) {
        printf("Error: Keysize value zero or negative\n");
        exit(EXIT_FAILURE);
    }

    if (target == NULL) {
        printf("Error: NULL argument found from argument <target>\n");
        exit(EXIT_FAILURE);
    }

    char salt[3];
    memcpy(salt, target, 2);
    salt[2] = '\0';

    pthread_t threads[thread_count];
    struct PassData thread_data[thread_count];

    // Load data into each struct and launch its respective thread
    for(int i = 0; i < thread_count; i++) {
      thread_data[i].threadCount = thread_count; 
      thread_data[i].curThread = i;
      thread_data[i].keysize = keysize;
      thread_data[i].target = target;
      strcpy(thread_data[i].salt, salt);

      pthread_create(&threads[i], NULL, cracker, (void*)&thread_data[i]);

    }

    // Wait for all threads to finish executing
    for(int i = 0; i < thread_count; i++) {
      pthread_join(threads[i], NULL);
    }

    return 0;
}


void* cracker(void* args) {
  struct PassData* data = (struct PassData*)args;
  
  char start;
  char end;
  int range;

  if(enableSpecialChars != 0) {
    start = '!';
    end = '~';
    range = 94;
  }
  else {
    start = 'a';
    end = 'z';
    range = 26;
  }
  
  // For loop will iterate through every letter a particular thread needs to handle ]
  // Letters are split based on thread number and increment by total number of threads 
  // This ensures no overlap while also allowing # of threads that do not evenly divide 26
  for(int cindex = data->curThread; cindex < range; cindex += data->threadCount) {
    checkCombinations(start + cindex, data->keysize, data->target, data->salt, start, end);
    if(found != 0) {
      break;
    }
  }
}


void checkCombinations(char start, int length, char* target, char* salt, char begin, char end) {
  char curCombination[length];
  curCombination[0] = start;    // Set starting letter 

  // Set all other letters to 'a'
  for(int i = 1; i < length; i++) {
    curCombination[i] = begin;
  }
  // Iterate through all possible combinations from index 1 -> length-1 
  while(1) {
    struct crypt_data cdata;
    cdata.initialized = 0;

    char *hash = crypt_r(curCombination, salt, &cdata);
    if(strcmp(hash, target) == 0) {
      printf("Found match: %s\n", curCombination);
      found = 1;
      return;
    }
    // Observe from other threads if anything has been found
    // Very useful to ensure program returns quickly when all special characters are enabled
    if(found == 1) {
      return;
    }

    int position = length - 1; // Start at the last letter
    while(position >= 0) {
      if (curCombination[position] < end) {
        curCombination[position]++;
        break;
      }
      // Move onto next index and change that up to z 
      else {
        curCombination[position] = begin;
        position--; 
      }
    }
    if(position < 1) {  // Do not generate any further combinations
      printf("Found no match in %c\n", start);
      break; 
    }
  }
}