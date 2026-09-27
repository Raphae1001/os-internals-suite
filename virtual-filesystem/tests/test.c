#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"

/*
============ CONSTANTS ============
You may change these constants to test different scenarios.
*/
#define SIZE_OF_FILE 10000
#define NUM_FILES 32
#define MAX_ALLOWED_NUM_FILES 32

/*
============ HELPERS ============
Please do not modify these functions.
*/
void create_disk(const char *disk_path)
{
    // if "disk" file exists, remove it
    if(access(disk_path, F_OK) == 0)
    {
        if(remove(disk_path) == 0)
        {
            printf("Removed existing disk file.\n");
        }
        else
        {
            perror("Failed to remove existing disk file");
            exit(1);
        }
    }
    int retval = fs_format(disk_path);
    if(retval < 0)
    {
        fprintf(stderr, "Test failed: call to fs_format failed: %s\n", disk_path);
        exit(1);
    }
}

int safe_string_compare(const char *s1, const char *s2, int max_length)
{
    if(s1 == NULL || s2 == NULL)
    {
        return -1; // indicate error
    }
    int first_has_null = 0, second_has_null = 0;
    for(int i = 0; i < max_length; i++)
    {
        if(s1[i] == '\0')
        {
            first_has_null = 1;
        }
        if(s2[i] == '\0')
        {
            second_has_null = 1;
        }
    }
    if(first_has_null)
    {
        if(!second_has_null)
        {
            return -1; // s1 is shorter than s2
        }
    }
    else
    {
        if(second_has_null)
        {
            return -1; // s2 is shorter than s1
        }
    }
    if(first_has_null && second_has_null)
    {
        return strcmp(s1, s2); // both strings are null-terminated
    }
    return memcmp(s1, s2, max_length);
}

void check_files_are_created(char expected_filenames[NUM_FILES][MAX_FILENAME])
{
    // check files list
    char list_of_files[NUM_FILES][MAX_FILENAME];
    if(fs_list(list_of_files, NUM_FILES) < 0)
    {
        fprintf(stderr, "Test failed: fs_list failed to list files.\n");
        exit(1);
    }   

    for(int i = 0; i < NUM_FILES; i++)
    {
        if(safe_string_compare(list_of_files[i], expected_filenames[i], MAX_FILENAME) != 0)
        {
            fprintf(stderr, "File %s not found in the filesystem.\n", expected_filenames[i]);
            printf("Expected %d-th file: %s\n", i, expected_filenames[i]);
            printf("Got %d-th file: %s\n", i, list_of_files[i]);
            exit(1);
        }
    }
    printf("All files have been created successfully.\n");
}

void init_filenames(char filenames_array[NUM_FILES][MAX_FILENAME])
{
    for(int i = 0; i < NUM_FILES; i++)
    {
        memset(filenames_array[i], 0, MAX_FILENAME);
        snprintf(filenames_array[i], MAX_FILENAME, "file_%d.txt", i);
    }
}


/*
============ MAIN FUNCTION ============
This is the main function that runs the test.
*/
int main(int argc, char *argv[])
{
    if(NUM_FILES > MAX_ALLOWED_NUM_FILES)
    {
        fprintf(stderr, "NUM_FILES (%d) cannot be greater than %d.\n", NUM_FILES, MAX_ALLOWED_NUM_FILES);
        exit(1);
    }

    create_disk("disk");

    char filenames[NUM_FILES][MAX_FILENAME];
    init_filenames(filenames);

    if(fs_mount("disk") < 0)
    {
        fprintf(stderr, "Test failed: call to fs_mount failed.\n");
        exit(1);
    }

    // init buffers
    char contents[NUM_FILES][SIZE_OF_FILE];
    for(int i = 0; i < NUM_FILES; i++)
    {
        // intialize `contents[i]`
        for(int j = 0; j < SIZE_OF_FILE; j++)
        {
            contents[i][j] = '0' + i % NUM_FILES;
        }
        contents[i][SIZE_OF_FILE - 1] = '\0'; // null-terminate the string
    }

    for(int i = 0; i < NUM_FILES; i++)
    {
        int retval;

        if((retval = fs_create(filenames[i])) < 0)
        {
            fprintf(stderr, "Test failed: fs_create failed (%d) for file %s\n", retval, filenames[i]);
            exit(1);
        }

        if((retval = fs_write(filenames[i], contents[i], SIZE_OF_FILE)) < 0)
        {
            fprintf(stderr, "Test failed: fs_write failed (%d) for file %s\n", retval, filenames[i]);
            exit(1);
        }
    }
    printf("All files written successfully.\n");

    check_files_are_created(filenames);

    char buff[SIZE_OF_FILE];
    for(int i = 0; i < NUM_FILES; i++)
    {
        int retval = fs_read(filenames[i], buff, SIZE_OF_FILE);
        if(retval < 0)
        {
            fprintf(stderr, "Test failed: fs_read failed (%d) to read file %s\n", retval, filenames[i]);
            exit(1);
        }
        if(retval != SIZE_OF_FILE)
        {
            fprintf(stderr, "Test failed: fs_read did not read the expected number of bytes for file %s (expected %d, got %d)\n", filenames[i], SIZE_OF_FILE, retval);
            exit(1);
        }
        if(memcmp(buff, contents[i], SIZE_OF_FILE) != 0)
        {
            fprintf(stderr, "Test failed for filename %s\n", filenames[i]);
            printf("Expected: %s\n", contents[i]);
            printf("Got: %s\n", buff);
            exit(1);
        }
    }
    
    printf("All files read successfully.\n");
    printf("Success!\n");

    fs_unmount();

    return 0;
}