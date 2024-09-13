#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include <iostream>
#include <string>
#include <vector>
using namespace std;

void printUsage(string programName);

int main(int argc, char** argv) {
    bool caseInsensitiveMode = false;
    bool recursiveMode = false;

    // Parse options using getopt
    int option;
    while((option = getopt(argc, argv, "iR")) != EOF) {
        switch (option)
        {
        case 'i':
            caseInsensitiveMode = true;
            break;
        case 'R':
            recursiveMode = true;
            break;
        default:
            printUsage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    // If no further arguments (outside of options) are provided -> error as there are no provided file names
    if(optind == argc) {
        cerr << "No file names provided" << endl;
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    // Parse file paths from argv excluding options
    vector<string> filePaths;
    for(int i = optind; i < argc; i++) {
        string currentArgument = argv[i];
        if(currentArgument.at(0) != '-') {
            filePaths.push_back(currentArgument);
        }
    }

    // Print file paths
    for(string filePath : filePaths) {
        cout << filePath << endl;
    }
}

void printUsage(string programName) {
    cerr << "Usage: " << programName << " [OPTION] file1 file2 ..." << endl;
    cerr << "Options:" << endl;
    cerr << "  -i: interactive mode" << endl;
    cerr << "  -R: recursive mode" << endl;
}