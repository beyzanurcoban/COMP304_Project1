#include<stdio.h>
#include<sys/types.h>
#include <unistd.h>

int main() {
     pid_t mainProcessID = getpid();
  
     pid_t pid;
	   int level = 0;
	   int i;
  
     for(i=0; i<3; i++) {
		      pid = fork();
	    }

	   return 0;

}
