#include <iostream>
#include <string>
#include <stdlib.h>
#include <sstream>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <experimental/filesystem>
#include <vector>
#include <algorithm>
#include <unordered_map>

using namespace std;
namespace fs = std::experimental::filesystem::v1;

void printenv(string env){
    const char* temp = &env[0];
    char* res = getenv(temp);
    if(res){
        cout<<getenv(temp)<<endl;
    }
}

int main(){
    string input;
    vector<string> functions;
    for(auto& x : fs::directory_iterator("bin/")){
    	string temp = x.path();
    	functions.push_back(temp.substr(4));
    }
    bool end = true;
    istringstream cmd_full; // cmd_full : one line of command
    int line_cnt = 0;
    unordered_map<int, int> number_pipe; // map the line number to STDIN_FILENO it should use
    int origin_stdin = dup(STDIN_FILENO);
    while(true){
    	if(end){
    		line_cnt++;
    		dup2(origin_stdin, STDIN_FILENO); // if one line of command is fully executed, set STDIN back
			cout<<"% ";
			getline(cin, input);
			if(input==""){
				line_cnt--;
				continue;
			}
			if(cin.eof() || input=="exit")break;
			cmd_full.clear();
			cmd_full.str(input);
		}
		if(number_pipe.find(line_cnt)!=number_pipe.end()){
			dup2(number_pipe[line_cnt], STDIN_FILENO);
		}
		string cmd_1;
		cmd_full>>cmd_1;
		if(cmd_1=="printenv"){
			string cmd_2;
			cmd_full>>cmd_2;
			printenv(cmd_2);
			end = true;
		}
		else if(cmd_1=="setenv"){
			string cmd_2, cmd_3;
			cmd_full>>cmd_2>>cmd_3;
			const char* temp_1 = &cmd_2[0];
			const char* temp_2 = &cmd_3[0];
			setenv(temp_1, temp_2, 1);
			end = true;
		}
		else if(find(functions.begin(), functions.end(), cmd_1)!=functions.end()){
			vector<string> param_temp{cmd_1};
			string temp;
			while(cmd_full>>temp && temp!="" && temp[0]!='|' && temp!=">" && temp[0]!='!'){
				param_temp.push_back(temp);
			}
			vector<const char*> param;
			const char* cmd_name = &cmd_1[0];
			for(int i=0; i<param_temp.size(); i++){
				param.push_back(param_temp[i].c_str());
			}
			param.push_back(NULL);
			if(temp==">"){
				cmd_full>>temp;
				const char* fname = &temp[0];
				pid_t pid = fork();
				if(pid<0){
					fprintf(stderr, "Fork Failed");
					exit(-1);
				}
				else if(pid==0){
					int fd = open(fname, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
					if(fd==-1){
						fprintf(stderr, "Open Failed");
						exit(-1);
					}
					dup2(fd, STDOUT_FILENO);
					close(fd);
					execvp(cmd_name, const_cast<char* const *>(param.data()));
				}
				else{
					wait(NULL);
				}
				end = true;
			}
			else if(temp=="|"){
				int fd[2];
				if(pipe(fd)==-1){
					fprintf(stderr, "Pipe Failed");
					exit(-1);
				}
				pid_t pid = fork();
				if(pid<0){
					fprintf(stderr, "Fork Failed");
					exit(-1);
				}
				else if(pid==0){
					close(fd[0]);
					dup2(fd[1], STDOUT_FILENO);
					close(fd[1]);
					execvp(cmd_name, const_cast<char* const *>(param.data()));
				}
				else{
					close(fd[1]);
					dup2(fd[0], STDIN_FILENO);
					close(fd[0]);
					wait(NULL);
				}
				end = false;
			}
			else if(temp[0]=='|' || temp[0]=='!'){
				int target_line = line_cnt + stoi(temp.substr(1));
				int fd[2];
				if(pipe(fd)==-1){
					fprintf(stderr, "Pipe Failed");
					exit(-1);
				}
				pid_t pid = fork();
				if(pid<0){
					fprintf(stderr, "Fork Failed");
					exit(-1);
				}
				else if(pid==0){ //if target_line was piped previously, read everything in that pipe and output to new pipe
					close(fd[0]);
					if(number_pipe.find(target_line)!=number_pipe.end()){
						string buf;
						int temp_stdin = dup(STDIN_FILENO), temp_stdout = dup(STDOUT_FILENO);
						dup2(number_pipe[target_line], STDIN_FILENO);
						dup2(fd[1], STDOUT_FILENO);
						while(getline(cin, buf))cout<<buf<<endl;
						dup2(temp_stdin, STDIN_FILENO);
						dup2(temp_stdout, STDOUT_FILENO);
					}
					dup2(fd[1], STDOUT_FILENO);	// execute the command and output to pipe
					if(temp[0]=='!')dup2(fd[1], STDERR_FILENO);
					close(fd[1]);
					execvp(cmd_name, const_cast<char* const *>(param.data()));
				}
				else{
					close(fd[1]);	//save the fd to read from pipe
					number_pipe[target_line] = dup(fd[0]);
					close(fd[0]);
					wait(NULL);
				}
				end = true;
			}
			else{
				pid_t pid = fork();
				if(pid<0){
					fprintf(stderr, "Fork Failed");
					exit(-1);
				}
				else if(pid==0){
					if(execvp(cmd_name, const_cast<char* const *>(param.data()))==-1){
						perror("execl error");
						exit(-1);
					}
				}
				else{
					wait(NULL);
				}
				end = true;
			}
		}
		else{
			string next;
			cmd_full>>next;
			if(next=="|")end = false;
			else end = true;
			cout<<"Unknown command: ["<<cmd_1<<"]."<<endl;
		}
    }
    return 0;

}