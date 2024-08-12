#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <errno.h>
#include <syslog.h>

// Function to display usage
void usage(const char *program_name) {
    syslog(LOG_USER, "Usage: %s <file_path> <text_string>", program_name);
    exit(EXIT_FAILURE);
}

// Function to create directory hierarchy
int create_directory(const char *path) {
    struct stat st = {0};

    if (stat(path, &st) == -1) {
        if (mkdir(path, 0700) == -1) {
            syslog(LOG_ERR, "mkdir: %s", strerror(errno));
            return -1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    // Open connection to syslog
    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);

    if (argc != 3) {
        usage(argv[0]);
    }

    const char *file_path = argv[1];
    const char *text_string = argv[2];

    // Duplicate the file path to avoid modifying the original
    char *file_path_dup = strdup(file_path);
    if (file_path_dup == NULL) {
        syslog(LOG_ERR, "strdup: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Extract the directory path from the file path
    char *dir_path = dirname(file_path_dup);

    // Create the entire directory hierarchy if it does not exist
    if (create_directory(dir_path) == -1) {
        syslog(LOG_ERR, "Error: Could not create directory path %s", dir_path);
        free(file_path_dup);
        exit(EXIT_FAILURE);
    }

    // Attempt to create or overwrite the file and write the text string to it
    FILE *file = fopen(file_path, "w");
    if (file == NULL) {
        syslog(LOG_ERR, "fopen: %s", strerror(errno));
        free(file_path_dup);
        exit(EXIT_FAILURE);
    }

    syslog(LOG_DEBUG, "Writing %s to %s\n", text_string, file_path);
    if (fprintf(file, "%s\n", text_string) < 0) {
        syslog(LOG_ERR, "fprintf: %s", strerror(errno));
        fclose(file);
        free(file_path_dup);
        exit(EXIT_FAILURE);
    }

    fclose(file);
    free(file_path_dup);

    // Close connection to syslog
    closelog();

    // Exit with success code
    return EXIT_SUCCESS;
}
