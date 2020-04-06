// Author:      Tim Kuelker
// Date:        September 1, 2019
// Course:      CMPSCI 4760 - Operating Systems
// Description: This program gets launched when the user passes in the [-L] option
// to follow links as well as directories.


#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

void handle_root(const char * );
void links_depth_first(const char * , int );


int main(int argc, char *argv[]) {
	int indent = atol(argv[1]);

	fprintf(stderr,"\nDepth-First-Search will now start following Symbolic Links:\n\n");
        
	handle_root(argv[0]);
        links_depth_first(argv[0], indent);

	return 0;
}


void handle_root(const char *root){
    DIR *dir;
    struct dirent *entry;
    struct stat sb;

    if ((dir = opendir(root)) == NULL)
        return;

    lstat(root, &sb);

    fprintf(stderr, "[%s]\n", root);
    closedir(dir);
}


void links_depth_first(const char *root, int indent) {
    DIR *dir;
    struct dirent *entry;
    struct stat sb;

    if ((dir = opendir(root)) == NULL)
        return;

    while ((entry = readdir(dir)) != NULL) {
        char path[1024];

        snprintf(path, sizeof(path), "%s/%s", root, entry->d_name);

        lstat(path, &sb);

        switch (sb.st_mode & S_IFMT) {

            case S_IFDIR:
                if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) // || strcmp(entry->d_name, ".git") == 0)
                    continue;

                fprintf(stderr, "%*s[%s]\n", indent, "", entry->d_name);

                links_depth_first(path, indent + indent);
                break;

            case S_IFLNK:
                if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) // || strcmp(entry->d_name, ".git") == 0)
                    continue;

                fprintf(stderr, "%*s{%s}\n", indent, "", entry->d_name);

		// This recursive call allows links to be followed
                links_depth_first(path, indent + indent);
                break;

            case S_IFREG:
                fprintf(stderr, "%*s- %s\n", indent, "", entry->d_name);
                break;

            default:
                fprintf(stderr, "%*s*%s*\n", indent, "", entry->d_name);
        }
    }
    closedir(dir);
}


