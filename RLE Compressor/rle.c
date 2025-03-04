#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define USAGE "rle <input file> <output file> <compression length> <mode>\n\
                \ninput file: the file to compress/decompress\n\
                \noutput file: the result of the operation\n\
                \ncompression length: the base size of candidate runs\n\
                \nmode: specifies whether to compress or decompress- if mode=0, then compress the input file, if mode=1 then decompress the input file\n"

int main(int argc, char* argv[]) {

    // Error check the passed inputs
    if (argc != 5) {
        printf("Error: Improper Command Line Arguments\nProper Use:\n%s", USAGE);
        exit(EXIT_FAILURE);
    }
    int file_in = open(argv[1], O_RDONLY);
    if (file_in == -1) {
        perror("Error Encountered with Input File: " );
        exit(EXIT_FAILURE);
    }
    int file_out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

    int run_length = atoi(argv[3]);
    if (run_length < 1) {
        printf("Error: Zero or Non-Positive Value for Run-Length");
        exit(EXIT_FAILURE);
    }
    int mode = atoi(argv[4]);
    if (!(mode == 0 || mode == 1)) {
        printf("Error: Invalid Selection for Mode");
        exit(EXIT_FAILURE);
    }

// Main Logic Variables:
    char pattern1[run_length];
    char pattern2[run_length];
    char pattern1_bytes; 
    char pattern2_bytes;
    char equivalent = 1;
    unsigned char count = 1;
    int read_stat;
    int write_stat;
    int write_stat2;
    int file_stat;
    int file_stat2;

//Compression:
    if(mode == 0) {
        // Read in first K bytes
        pattern1_bytes = read(file_in, pattern1, run_length);
        if (pattern1_bytes == -1) {
            perror("Error when Parsing Input File : ");
            exit(EXIT_FAILURE);
        }
        // Iterate through following K bytes so long as they are not end of file
        while((pattern2_bytes = read(file_in, pattern2, run_length)) > 0) {
            for (int i = 0; i < pattern1_bytes; i++) {
                if (!(pattern1[i] == pattern2[i])) {
                    equivalent = 0;
                    break;
                }
            }
            if (equivalent == 1 && count != 0xFF) {
                count++;
            }
            else {
                write_stat = write(file_out, &count, 1); // Write the counter to the compression file
                write_stat2 = write(file_out, pattern1, pattern1_bytes);
                if ((write_stat == -1) || (write_stat2 == -1)) {
                    perror("Error when Writing to Output File : " );
                    exit(EXIT_FAILURE);
                }
                // Make sure pattern2 is now the pattern to be compared to 
                // (Set pattern1 equal to pattern2)
                for (int i = 0; i < run_length; i++) {
                    pattern1[i] = pattern2[i];
                }
                pattern1_bytes = pattern2_bytes;
                count = 1;
                equivalent = 1;
            }
        }
        // Avoid truncating the final pattern (if while condition stops being true it will not be written out)
        write_stat = write(file_out, &count, 1);
        write_stat2 = write(file_out, pattern1, pattern1_bytes); // Base it off the previous bytes read in
        if ((write_stat == -1) || (write_stat2 == -1)) {
            perror("Error when Writing to Output File : " );
            exit(EXIT_FAILURE);
        }
    }
    else {
//Decompression:
        int instances = 0;
        while((read_stat = read(file_in, &instances, 1)) > 0) { // Read the compression value each iteration through 
            read_stat = read(file_in, pattern1, run_length); // Obtain the pattern to repeat 
            if (read_stat == -1) {
                perror("Error when Parsing Input File : ");
                exit(EXIT_FAILURE);
            }

            for(int i = 0; i < instances; i++) {
                write_stat = write(file_out, pattern1, read_stat);
                if (write_stat == -1) {
                    perror("Error when Writing to Output File : " );
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
//End:
    // Close files 
    file_stat = close(file_in);
    file_stat2 = close(file_out);
    if ((file_stat == -1) || (file_stat2 == -1)) {
        perror("Error closing files : " );
        exit(EXIT_FAILURE);
    }
    return 0;

}