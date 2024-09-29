#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <cstring>

const key_t MSG_QUEUE_KEY = ftok("progfile", 65); // Generate unique key
#define MSG_QUEUE_PERMISSIONS 0666
#define MSG_PAYLOAD_MAX_SIZE 1024
#define CHILD_MESSAGE_TYPE 1

using namespace std;
namespace fs = std::filesystem;

typedef struct {
    long messageType;
    char childMessage[MSG_PAYLOAD_MAX_SIZE];
} child_message_t;

int msgQueueId;

void printUsage(string programName);
void searchFile(bool caseInsensitiveMode, bool recursiveMode, string searchPath, string file);
void sendMessageViaQueue(int messageQueueId, const string message);
void cleanupAndExit(int exitCode);

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
            exit(EXIT_FAILURE);
        }
    }

    // If no further arguments (outside of options) are provided -> error as there are no provided file names
    if(optind == argc) {
        cerr << "No file names provided" << endl;
        printUsage(argv[0]);
        exit(EXIT_FAILURE);
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
        exit(EXIT_FAILURE);
    }

    // Create message queue
    msgQueueId = msgget(MSG_QUEUE_KEY, MSG_QUEUE_PERMISSIONS | IPC_CREAT | IPC_EXCL);
    if(msgQueueId == -1) {
        cerr << "Error while creating message queue" << endl;
        exit(EXIT_FAILURE);
    }

    // Fork for each file to search for
    for (auto it = filePaths.begin(); it != filePaths.end(); it++) {
        pid_t pid = fork();

        // Failure while forking
        if (pid == -1) {
            cerr << "Error while forking! Aborting." << endl;
            exit(EXIT_FAILURE);
        }
        // Child process that performs the search
        else if (pid == 0) {
            searchFile(caseInsensitiveMode, recursiveMode, searchPath, *it);
            exit(EXIT_SUCCESS);
        }
    }

    // Receive found paths (via msg queue) and wait for all child processes to finish
    child_message_t receivedMessage;
    while(true) {
        // Receive message with NO_WAIT to avoid blocking
        int msgReceiveSuccess = msgrcv(msgQueueId, &receivedMessage, sizeof(receivedMessage) - sizeof(long), CHILD_MESSAGE_TYPE, IPC_NOWAIT);
        if (msgReceiveSuccess != -1) {
            cout << receivedMessage.childMessage;
        }
        // No message available, but errno is set -> error
        else if(msgReceiveSuccess == -1 && errno != ENOMSG) {
            cerr << "Error while receiving message" << endl;
            cleanupAndExit(EXIT_FAILURE);
        }

        int status;
        pid_t exitedPid = waitpid(-1, &status, WNOHANG); // Wait for any child by using -1
        if (exitedPid == -1) {
            // No more children to wait for -> stop waiting
            if (errno == ECHILD) {
                break;
            }

            // Error occured while waiting
            cerr << "Error while waiting for child processes" << endl;
            cleanupAndExit(EXIT_FAILURE);
        }

        // Check if a child has exited at all (exitedPid > 0) and if so whether it exited successfully
        if (exitedPid > 0 && WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS) {
            cerr << "Child process with PID " << exitedPid << " exited with status " << WEXITSTATUS(status) << endl;
        }
    }

    cleanupAndExit(EXIT_SUCCESS);
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

void searchFile(bool caseInsensitiveMode, bool recursiveMode, string searchPath, string file) {
    // Distinguish between case sensitive and case insensitive search
    string fileNameToSearch = file;
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
                stringstream foundTextStream;
                foundTextStream << "File found: " << entry.path() << endl;
                string foundText = foundTextStream.str();

                // Send foundText via message queue to avoid buffer corruption
                // sendMessageViaQueue(msgQueueId, foundText);
            }
        }
    } catch (const fs::filesystem_error& err) {
        // File system error will be printed
        cerr << "Error: " << err.what() << endl;
    }
}

void sendMessageViaQueue(int messageQueueId, const string message) {
    // Construct message via converting the string to a char array in a safe manner
    // Message must be null terminated, which might not be the case when using strncpy (reaching the limit)
    child_message_t foundTextMessage;
    foundTextMessage.messageType = CHILD_MESSAGE_TYPE;
    strncpy(foundTextMessage.childMessage, message.c_str(), MSG_PAYLOAD_MAX_SIZE);
    foundTextMessage.childMessage[MSG_PAYLOAD_MAX_SIZE - 1] = '\0';

    int msgSendSuccess = msgsnd(msgQueueId, &foundTextMessage, sizeof(foundTextMessage) - sizeof(long), IPC_NOWAIT);
    if (msgSendSuccess == -1) {
        // Msg send failed -> still use buffer as queue cannot be used
        cerr << "Error while sending message" << endl;
        exit(EXIT_FAILURE);
    }
}

// Needed to remove message queue to avoid accumulation
void cleanupAndExit(int exitCode) {
    // Remove message queue
    if(msgctl(msgQueueId, IPC_RMID, NULL) == -1) {
        cerr << "Error while removing message queue" << endl;
    }

    exit(exitCode);
}