#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <vector>
#include <string>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main () {
    int stdin_cpy = dup(STDIN_FILENO);
    int stdout_cpy = dup(STDOUT_FILENO);
    int fd[2];
    vector<int> pids;
    string input;
    vector<char*> directory; // used later for cd commands
    char* cwd = get_current_dir_name();
    directory.push_back(cwd);
    while (true) {

        /* setting up terminal line */
        char* name = getenv("USER");
        cwd = get_current_dir_name();
        time_t curr_time = time(NULL);
        string time = ctime(&curr_time);
        time = time.substr(0, time.find_last_of(' '));        
        time = time.substr(time.find_first_of(' ') + 1);
        time = time.erase(time.find_first_of(' '), 1);
        cout << RED << time << ' ' << name << ':'  << cwd << NC << "$ ";
        free(cwd);
        /*--------------------------*/

        /* check for background process     */
        for (long unsigned int i = 0; i < pids.size(); i++) {
            int wstatus = 0;
            waitpid(pids.at(i), &wstatus, WNOHANG);
            if (WIFSIGNALED(wstatus)) {
                pids.erase(pids.begin() + i);
            }
        }
        /*----------------------------------*/

        getline(cin, input);
        char* _cwd = nullptr;
        if (input == "exit") {  // print exit message and break out of infinite loop
            if (directory.size() == 0) {
                free(_cwd);
            }
            for (unsigned long int i = 0; i < directory.size(); i++) { // before we exit, have to deallocate memory
                free(directory.at(i));
            }
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        Tokenizer token(input);
        if (token.hasError()) {  // continue to next prompt if input had an error
            continue;
        }
        
        // // print out every command token-by-token on individual lines
        // // prints to cerr to avoid influencing autograder
        // for (auto cmd : token.commands) {
        //     for (auto str : cmd->args) {
        //         cerr << "|" << str << "| ";
        //     }
        //     if (cmd->hasInput()) {
        //         cerr << "in< " << cmd->in_file << " ";
        //     }
        //     if (cmd->hasOutput()) {
        //         cerr << "out> " << cmd->out_file << " ";
        //     }
        //     cerr << endl;
        // }

        // individual commands are stored in token.commands[i]->args[j]
        // ex: commands[0] = args{ls, -l} 
        // commands[1] = args{grep, shell.cpp}

        /*---------- if command involves a cd -------------*/
        if (token.commands[0]->args[0] == "cd") {
            if (token.commands[0]->args[1] == "-") { // cd -
                if (directory.size() == 1) { // if the first command is cd -
                    cout << "cd: OLDPWD not set\n";
                    continue;
                }
                _cwd = get_current_dir_name();
                chdir(directory.back());
                directory.push_back(_cwd);
            }
            else if (token.commands[0]->args[1] == "~" || token.commands[0]->args.size() == 1) { // cd ~ and cd
                _cwd = get_current_dir_name();
                chdir(getenv("HOME"));
                directory.push_back(_cwd);
            }
            else{ // cd anything else
                _cwd = get_current_dir_name();
                directory.push_back(_cwd);
                chdir(token.commands[0]->args[1].c_str());               
            }
            continue;
        }
        /*-------------------------------------------------*/

        for (unsigned long int i = 0; i < token.commands.size(); i++) {
            //create a pipe
            pipe(fd);
            int pid = fork();
            if (pid == 0) {  // child
                if (i < token.commands.size() - 1) {
                    dup2(fd[1], STDOUT_FILENO); // redirect output to write end of pipe
                    close(fd[0]);
                }
                char **cmd = new char*[token.commands[i]->args.size()+1];

                for (unsigned long int j = 0; j < token.commands[i]->args.size(); j++) {
                    cmd[j] = new char[token.commands[i]->args[j].size()];
                    memcpy(cmd[j], token.commands[i]->args[j].c_str(), token.commands[i]->args[j].size());
                }
                cmd[token.commands[i]->args.size()] = nullptr;

                // IO redirection
                if (token.commands[i]->hasOutput()) {
                    int output_file = open(const_cast<char*>((token.commands[i]->out_file).c_str()), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR, S_IRGRP | S_IROTH);
                    dup2(output_file, STDOUT_FILENO);
                }
                if (token.commands[i]->hasInput()) {
                    int input_file = open(const_cast<char*>((token.commands[i]->in_file).c_str()), O_RDONLY, S_IWUSR | S_IRUSR);
                    dup2(input_file, STDIN_FILENO);
                }

                if (execvp(cmd[0], cmd) < 0) {    // execute token.commands[i]
                    perror("exec failed");
                    return 0;
                } 
            }
            
            else {  // parent
                // background process check
                if (token.commands[i]->isBackground()) {
                    pids.push_back(pid);
                    continue;
                }
                // update fd table of parent process
                dup2(fd[0], STDIN_FILENO); // redirects input to read end of pipe
                close(fd[1]); // close write end
                waitpid(pid, NULL, 0);
            }
        }
        dup2(stdin_cpy, 0);
    }
    // restore the stdin and stdout
    dup2(stdout_cpy, 1);
    close(stdin_cpy);
    close(stdout_cpy);
}
