#include <iostream>
#include <string>
#include <sched.h>
#include <unistd.h>
#include <cstring>
//#include <sys/wait.h>
#include <wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/mman.h>

/// CONSTANTS ///
#define STACK_SIZE 8192
#define DIR_PERMISSIONS 0755
#define SUCCESS 0
#define UNMOUNT 0
#define REMOVE_FILES 1
#define FAILURE -1

/// PATHS ///
#define PROC_PATH "/proc"
#define FS_PATH "/sys/fs"
#define C_GROUP_PATH "/sys/fs/cgroup"
#define P_IDS_PATH "/sys/fs/cgroup/pids"
#define PID_MAX_FILEPATH "/sys/fs/cgroup/pids/pids.max"
#define C_GROUP_PROC_FILEPATH "/sys/fs/cgroup/pids/cgroup.procs"
#define NOTIFY_RELEASE_FILEPATH "/sys/fs/cgroup/pids/notify_on_release"

/// COMMANDS ///
#define REMOVE_COMMAND "rm -r "

/// ERROR MESSAGES ///
#define SYSTEM_ERROR "system error: "
#define ALLOCATION_ERROR_MSG "Memory Allocation Failure"
#define CHROOT_ERROR_MSG "Root Change Failure"
#define HOST_NAME_ERROR_MSG "Change Host Name Failure"
#define CHDIR_ERROR_MSG "Directory Change Failure"
#define MOUNT_ERROR_MSG "Mount Failure"
#define UMOUNT_ERROR_MSG "Umount Failure"
#define MKDIR_ERROR_MSG "Folder Creation Failure"
#define CONTAINER_PROGRAM_ERROR_MSG "Container Program Failure"
#define CLONE_ERROR_MSG "Clone Failure"




/**
 * Function which prints a relevant message when system failure occurs
 */

void CreateErrorMessage(const std::string& msg)
{
    std::cerr << SYSTEM_ERROR << msg << std::endl;
}


/**
 * This function removes all the files which created in pids folder and removes all the folders that
 * created by the Child function.
 */
void RemoveManager(const char *BaseFileSystemPath, const char *ExtraPath, int command) {
    std::string BasePath = BaseFileSystemPath;
    std::string ExtPath = ExtraPath;
    std::string AllPath = BasePath + ExtPath;
    if (command == UNMOUNT) {
        const char *unmountPath = AllPath.c_str();
        if (umount(unmountPath) != SUCCESS) {
            CreateErrorMessage(UMOUNT_ERROR_MSG);
            exit(1);
        }
    }
    if (command == REMOVE_FILES) {
        std::string removeCommandString = REMOVE_COMMAND + AllPath;
        const char *removeFilesCommand = removeCommandString.c_str();
        system(removeFilesCommand);
    }

}


/**
 * This function sends relevant directories and flags for the Umount func and the remove of the files and folders
 */
void RemovePath(char **argv) {

    // unmount
    RemoveManager(argv[2], PROC_PATH, UNMOUNT);

    // remove directories and files
    RemoveManager(argv[2],FS_PATH,REMOVE_FILES);
}



/**
 * This function creates three files in the pids folders and writes to the files relevant information
 */

void CreateFiles(char **argv) {
    std::ofstream PidMax(PID_MAX_FILEPATH, std::ofstream::out);
    PidMax << argv[3];
    PidMax.close();
    chmod(PID_MAX_FILEPATH, DIR_PERMISSIONS);

    std::ofstream CGroup(C_GROUP_PROC_FILEPATH, std::ofstream::out);
    int Pid = getpid();
    auto PidStr = std::to_string(Pid);
    CGroup << PidStr.c_str();
    CGroup.close();
    chmod(C_GROUP_PROC_FILEPATH, DIR_PERMISSIONS);

    std::ofstream Release(NOTIFY_RELEASE_FILEPATH, std::ofstream::out);
    Release << '1';
    Release.close();
    chmod(NOTIFY_RELEASE_FILEPATH, DIR_PERMISSIONS);
}


/**
 * This function creates new folders in the new file system
 */

void CreateDirectories(char **argv) {
    // creates all the folders
    if (mkdir(FS_PATH, DIR_PERMISSIONS) != SUCCESS ||
        mkdir(C_GROUP_PATH, DIR_PERMISSIONS) != SUCCESS  ||
        mkdir(P_IDS_PATH, DIR_PERMISSIONS) != SUCCESS ) {
            CreateErrorMessage(MKDIR_ERROR_MSG);
            exit(1);
    }
}

/**
 * This is the main function for creating the Container. the function changes the host name, the file system
 * and creates new process by calling execvp function
 */

int Child(void *arguments) {
    // cast the arguments
    char **ChildArguments = (char **) arguments;
    const char *NewHostName = ChildArguments[1];
    char *root = ChildArguments[2];

    // assign new host name
    if (sethostname(NewHostName, strlen(NewHostName)) != SUCCESS) {
        CreateErrorMessage(HOST_NAME_ERROR_MSG);
        exit(1);
    }

    // change the file system root
    if (chroot(root) != SUCCESS) {
        CreateErrorMessage(CHROOT_ERROR_MSG);
        exit(1);
    }

    // move to root folder
    if (chdir("/") != SUCCESS) {
        CreateErrorMessage(CHDIR_ERROR_MSG);
        exit(1);
    }

    // mount
    if (mount("proc", "/proc", "proc", 0, nullptr) != SUCCESS) {
        CreateErrorMessage(MOUNT_ERROR_MSG);
        exit(1);
    }

    // create new directories for CGroup
    CreateDirectories(ChildArguments);

    // creates and writes into files
    CreateFiles(ChildArguments);

    // run the program
    char * arr[] = {ChildArguments[4],*(ChildArguments + 5), nullptr};

    if (execvp(ChildArguments[4], arr) != SUCCESS)
    {
        CreateErrorMessage(CONTAINER_PROGRAM_ERROR_MSG);
        exit(1);
    }
    return 0;
}



int main(int argc, char *argv[]) {
    void *stack = malloc(STACK_SIZE);
    if (stack == nullptr) {
        CreateErrorMessage(ALLOCATION_ERROR_MSG);
        exit(1);
    }
    int CloneRetVal = clone(Child, (char *) stack + STACK_SIZE, SIGCHLD | CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS, argv);
    if (CloneRetVal == FAILURE)
    {
        CreateErrorMessage(CLONE_ERROR_MSG);
        exit(1);
    }

    wait(nullptr);

    RemovePath(argv);

    free(stack);

    return SUCCESS;
}

