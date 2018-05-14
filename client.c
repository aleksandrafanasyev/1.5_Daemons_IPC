#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <string.h>
#include "ipc.h"

//max size for input line
#define SBUF_MAX 256
//handler messege queue for IPC 
static mqd_t mqueue_des; 
static struct mq_attr mqueue_attr;

static int ifile; //input file stream 

static int exit_stat = EXIT_SUCCESS;

//help message
static const char * const prog_usage = 
	"Usage: client {options}\n"
	"-f INPUTFILE file name for input data\n"
	"-h print this information\n"

static volatile sig_atomic_t term_flag;//0 - work, 1 - term proc

static void sig_handl(int sig) //SIGTERM and SIGINT handler
{
//	printf("#########################\n");
	term_flag = 1;
}


static void errMsg(const char * msg)
{
  	fprintf(stderr, "Error: %s. errno = %d. err discription: %s\n", msg, errno, strerror(errno));
	exit_stat = EXIT_FAILURE;
	return;
}

static void errExit(const char * msg)
{
	//deallocate resource
	if (mq_close(mqueue_des) == -1)
		errMsg("mq_close");
	
	exit(EXIT_FAILURE);
}

int main(int argc, char * argv[])
{	
	char * ifile_name;
	struct ipc qmsg;
	int input_flag;
	int opt;
	char  sbuf[SBUF_MAX];
	while((opt = getopt(argc, argv, ":f")) != -1){
		switch(opt){
		case 'f'://input from file
			ifile_name = optarg;
			input_flag = 1;
			break;
		case 'h'://help
			printf(prog_usage);
			exit(EXIT_SUCCESS);
		case ':'://missing argument
			printf("Error: missing argument\n");
			printf(prog_usage);
			exit(EXIT_FAILURE);
		case '?'://unreconnized option
			printf("Error: unrecognized option\n");
			printf(prog_usage);
			exit(EXIT_FAILURE);
		default:
			printf("Error: getopt\n");
			exit(EXIT_FAILURE);
		}
	}
	//open messege queue
	mqueue_attr.mq_maxmsg = MQUEUE_MAXMSG;
	mqueue_attr.mq_msgsize = sizeof(struct ipc);
	if ((mqueue_des = mq_open(MQUEUE_NAME, O_RDWR, S_IRUSR|S_IWUSR, &mqueue_attr)) == -1)
		  errExit("mq_open");
	//set new handler for SIGTERM and SIGINT	
	struct sigaction act;
	sigemptyset(&act.sa_mask);
	act.sa_handler = &sig_handl;
	act.sa_flags = 0;
	if (sigaction(SIGTERM, &act, NULL) == -1)
		errExit("sigaction");
	if (sigaction(SIGINT, &act, NULL) == -1)
		errExit("sigaction");

	//input from file
	if (input_flag == 1){
		if ((ifile = fopen(ifile_name, "r")) == NULL)
			errExit("fopen");
		char *ch, *nstr;
		long nline = 0; // line number in input file
		while ((fgets(sbuf, SBUF_MAX, ifile) != NULL) && (term_flag == 0)){
			nstr = sbuf;
			nline++;
			ch = strtok(nstr, " :");			
			if (ch == NULL){
				fprintf(stderr, "Input file error in line %ld\n", nline);
				continue;
			}else if (strcmp(ch, "array") == 0){//read array type from file
				if (strlen(nstr) < 5){
					fprintf(stderr, "Input file error in line %ld\n", nline);
					continue;
				}
				memcpy(qmsg.ipc_data.array, nstr, 5);
				qmsg.ipc_type = ARRAY;
			}else if (strcmp(ch, "integer") == 0){//read integer type from file
				if (sscanf(nstr, "%d", &(qmsg.ipc_data.integer)) == EOF){
					fprintf(stderr, "Input file error in line %ld\n", nline);
					continue;
				}
				qmsg.ipc_type = INT;
			}else if (strcmp(ch, "struct") == 0){//read struct type from file
				int tmp;
				tmp = sscanf(nstr, "%d%d%d", &(qmsg.ipc_data.stct.a), &(qmsg.ipc_data.stct.b), &(qmsg.ipc_data.stct.c));
				if (tmp == EOF || tmp != 3){
					fprintf(stderr, "Input file error in line %ld\n", nline);
					continue;
				}
				qmsg.ipc_type = STRUCT;
			}else{//unknown type
				fprintf(stderr, "Input file error in line %ld\n", nline);
				continue;
			}
			//send mesage
			if (mq_send(mqueue_des, (char*) &dmsg, sizeof(struct ipc), 0) == -1)
				  errExit("mq_send");
		}// while

		//close input file 
		if (fclose(ifile) == EOF)
			errMsg("fclose");
	}else{     //input from console
		
		while (term_flag == 0){
			int tpe;
			printf("Введите тип данных(0 - char[5], 1 - integer, 2 - struct):\n");
			if (fgets(sbuf, SBUF_MAX, stdin) == 0){
				printf("Ошибка ввода\n");
				continue;
			}
			if (sscanf(sbuf, "%1d", &tpe) == EOF){
				printf("Ошибка ввода\n");
				continue;
			}
			switch (tpe){
			case 0:// char[5]
				printf("Введите данные (char[5]):\n");
				if (fgets(sbuf, SBUF_MAX, stdin) == 0){
					printf("Ошибка ввода\n");
					continue;
				}
				if (sscanf(sbuf, "%5c", qmsg.ipc_data.array) == EOF){
					printf("Ошибка ввода\n");
					continue;
				}
				qmsg.ipc_type = ARRAY;
				break;
			case 1:// integer
				
				
				break;
			case 2:// struct

				break;
			default:// wrong type
				printf("Не верный тип\n");
				continue;
			}
			//send mesage
			if (mq_send(mqueue_des, (char*) &dmsg, sizeof(struct ipc), 0) == -1)
				  errExit("mq_send");
		}// while (term_flag == 0)
	}// if-else (input_flag)

	//deallocate resource and exit
	if (mq_close(mqueue_des) == -1)
		errMsg("mq_close");

	exit(exit_stat);
}
