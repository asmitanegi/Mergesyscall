#include <asm/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "xhw1.h"
#ifndef __NR_xmergesort
#error xmergesort system call not defined
#endif

int main(int argc,  char * const argv[])
{
	int rc;
	//void *dummy = (void *) argv[1];
	//void *dummy2 = (void *) argv[2];
	//void* dummy3 = (void*) argv[3];
	int options = 0;
	int flag = 0;
	int i = 0;
	//int a_flag = 0;
	//int i_flag = 0;
	//int t_flag = 0;
	//int d_flag = 0;
//	printf("\n xhw1.c  main ARG1 ------------> %s \n ARG2 ----------------------> %s  ARG3 ------------------> %s \n", (char*)dummy, (char*)dummy2, (char*)dummy3);
	rc = 0;
	while ((options = getopt (argc, argv, "uatid")) != -1)
		switch (options)
		{
			case 'u':
				flag |= 0x01;
				break;
			case 'a':
				flag |= 0x02;
				break;
			case 'i':
				flag |= 0x04;
				break;
			case 't':
				flag |= 0x10;
				break;
			case 'd':
				flag |= 0x20;
				break;
			default:
				fprintf (stderr, "Unknown option `-%c' Only [u,a,i,t,d] allowed.\n", options);
				exit(EXIT_FAILURE);
				abort ();
		}
	//printf("FLAGS ARE : %d \n", flag);
	if((flag & 3) == 3){
		printf("Can not send [-u] and [-a] as input options together");
		return 0;
	}	
	//printf("NEW OPTIONS: %s, %s, %s, %s \n",argv[0], argv[1], argv[2], argv[3]);
	int index = 0;
	for (index = optind; index < argc; index++)
		printf ("Non-option argument %s\n", argv[index]);
	/*       	if(optind + 2 != argc-1){
			printf("The input arguments must be exactly 3 ie. file1, file2, output file");	
			}*/
	struct param* input = (struct param*) malloc(sizeof(struct param));
	input->flag = flag;
	i = 0;
	//	printf("FFFFFFFFFFFFFFFFFFFFFffff %d  %d  %d", optind, i, argc);
	input->files = malloc((argc-optind + 1)* sizeof(char *));
	while(optind + i < argc-1){
		input->files[i] =  argv[optind + i];
		//printf("~~~~~~~~~~~~~~~~ %s ", argv[optind + i]);
		i++;	
	}
	input->output_file = argv[optind + i];
	input->files[i] = NULL;
	input->count = (unsigned int*)malloc(sizeof(unsigned int));
	if((flag & 0x01) != 0 && (flag & 0x02) != 0){
		printf("Can not process flag -u and -a at the same time, please give a different cobination of flags");
		free(input->count);
		return -1;
	}
	
	//	printf("!!!!!!!!!!!!!! argc = %d     optind =  %d   i = %d\n", argc, optind, i);
	rc = syscall(329, input);//, argv[optind], argv[optind+1],argv[optind+2]);
	if (rc == 0){
		printf("syscall returned %d\n", rc);
		if((flag & 0x20) != 0)
			printf("Number of Records read are %d", *(input->count));
	}
	else{

		printf("syscall returned %d (errno=%d)\n", rc, errno);
		switch (errno) {
			case 2:
				printf("ERROR : NO SUCH FILE OR DIRECTORY\n");
				break;
			case 22:
				printf("ERROR : INVALID ARGUMENTS\n");
				break;
			case 9:
				printf("ERROR : BAD FILE NAME\n");
				break;
			case 125:
				printf("ERROR : ONE OR MORE OF THE FILE IS NOT SORTED\n");
				break;
			case 12:
				printf("ERROR : UNABLE TO ALLOCATE MEMORY \n");
				break;
		}		
	}
	free(input->count);
	exit(rc);
}
