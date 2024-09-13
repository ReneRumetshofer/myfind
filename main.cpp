#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>

using namespace std;
namespace fs = std::filesystem;

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
    string searchPath = filePaths[0];
    filePaths.erase(filePaths.begin());

    // Check if searchPath is a valid folder, otherwise return with error
    if(!fs::is_directory(searchPath)) {
        cerr << "Error: " << searchPath << " is not a valid path to a directory" << endl;
        return EXIT_FAILURE;
    }

    for(auto it = filePaths.begin(); it != filePaths.end(); it++) {
        // Distinguish between case sensitive and case insensitive search
        string fileNameToSearch = *it;
        if(caseInsensitiveMode) {
            transform(fileNameToSearch.begin(), fileNameToSearch.end(), fileNameToSearch.begin(), ::tolower);
        }

        try {
            // Iterate through all files in the directory and subdirectories
            auto directoryIterator = fs::recursive_directory_iterator(searchPath);
            for (const auto& entry : directoryIterator) {
                // Change behaviour of iterator to mimmick non-recursive iterator
                // This is done to avoid if/else with almost identical content
                if(!recursiveMode) {
                    directoryIterator.disable_recursion_pending();
                }

                // Get the current file name (iteration) and depending on case sensitivity, transform it to lower case
                string currentFileName = entry.path().filename();
                if(caseInsensitiveMode) {
                    transform(currentFileName.begin(), currentFileName.end(), currentFileName.begin(), ::tolower);
                }

                // File match -> print resulting path
                if (entry.is_regular_file() && currentFileName == fileNameToSearch) {
                    std::cout << "File found: " << entry.path() << std::endl;
                }
            }
        } catch (const fs::filesystem_error& err) {
            // Unknown error will be printed
            cerr << "Error: " << err.what() << endl;
        }
    }
}

void printUsage(string programName) {
    cerr << "Usage: " << programName << " [OPTIONS] searchpath file1 file2 ..." << endl;
    cerr << "Options:" << endl;
    cerr << "  -i: interactive mode" << endl;
    cerr << "  -R: recursive mode (includes sub-folders)" << endl;
    cerr << "Arguments:" << endl;
    cerr << "  searchpath: path to search for files" << endl;
    cerr << "  file1, file2, ...: file names to search for" << endl;
}