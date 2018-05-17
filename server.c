#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "ipc.h"

#define STR_TIME_SIZE 256 //length for formated string with date and time

//help message
static const char * const prog_usage = 
	"Usage: server {options}\n"
	"-a ARRAYFILE file name for array data\n"
	"-D daemond server\n"
	"-i INTFILE file name fo int data\n"
	"-h print this information\n"
	"-s STRUCTNAME file name for struct data\n";
	
static const char * afile_name = "array.txt";
static const char * ifile_name = "int.txt";
static const char * sfile_name = "struct.txt";

static FILE * afile;//file stream for array type
static FILE * ifile;//file stream for int type
static FILE * sfile;//file stream for struct type
static int daemonize_srv;//0 - console proc, 1 - daemon
static volatile sig_atomic_t term_flag;//0 - work, 1 - term proc
//handler messege queue for IPC 
static mqd_t mqueue_des; 
static struct mq_attr mqueue_attr;

static void sig_handl(int sig) //SIGTERM and SIGINT handler
{
//	printf("#########################\n");
	term_flag = 1;
}

static void errExit(const char * msg)
{
	syslog(LOG_ERR,"Error: %s. errno = %d. err discription: %s", msg, errno, strerror(errno));
	//deallocate resource
	if (mq_close(mqueue_des) == -1)
		syslog(LOG_ERR,"Error: mq_close. errno = %d. err discription: %s", errno, strerror(errno));
	if (mq_unlink(MQUEUE_NAME) == -1)
		syslog(LOG_ERR,"Error: mq_unlink. errno = %d. err discription: %s", errno, strerror(errno));

	if (fclose(afile) == EOF)
		syslog(LOG_ERR,"Error: close file sream for array type. errno = %d. err discription: %s", errno, strerror(errno));
	if (fclose(ifile) == EOF)
		syslog(LOG_ERR,"Error: close file stream for integer type. errno = %d. err discription: %s", errno, strerror(errno));
	if (fclose(sfile) == EOF)
		syslog(LOG_ERR,"Error: close file stream for struct type. errno = %d. err discription: %s", errno, strerror(errno));
	closelog();
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{	
	int opt;
	while ((opt = getopt(argc, argv, ":a:Di:hs:")) != -1){
		switch(opt){
		case 'a'://file name for array data
			afile_name = optarg;
			break;
		case 'D'://daemonize server
			daemonize_srv = 1;
			break;
		case 'i':// file name for int data
			ifile_name = optarg;
			break;
		case 'h'://help
			printf(prog_usage);
			exit(EXIT_SUCCESS);
		case 's'://file name for struct data
			sfile_name = optarg;
			break;
		case ':':// missing argument
			printf("Error: missing argument\n");
			printf(prog_usage);
			exit(EXIT_FAILURE);
		case '?'://unrecognized option
			printf("Error: unrecognized option\n");
			printf(prog_usage);
			exit(EXIT_FAILURE);
		default:
			printf("Error: getopt\n");
			exit(EXIT_FAILURE);
		}
	}
	//open syslog
	if (daemonize_srv == 0){
		openlog("Myserver", LOG_PID|LOG_PERROR, LOG_USER);
	}else{
		openlog("Myserver", LOG_PID, LOG_DAEMON);
	}

	//create messege queue
	mqueue_attr.mq_maxmsg = MQUEUE_MAXMSG;
	mqueue_attr.mq_msgsize = sizeof(struct ipc);
	if ((mqueue_des = mq_open(MQUEUE_NAME, O_CREAT|O_EXCL|O_RDWR, S_IRUSR|S_IWUSR, &mqueue_attr)) == -1)
		  errExit("mq_open");
	
	//create 3 files
	//new record added at the end of file if file already exist
	//set stdio string buf mode (_IOLBF)
	if ((afile = fopen(afile_name, "a")) == NULL)
		  errExit("open file for array type");
	if (setvbuf(afile, NULL, _IOLBF , 0) != 0)
		errExit("setvbuf");
	if ((ifile = fopen(ifile_name, "a")) == NULL)
		  errExit("open file for array type");
	if (setvbuf(ifile, NULL, _IOLBF , 0) != 0)
		errExit("setvbuf");
	if ((sfile = fopen(sfile_name, "a")) == NULL)
		  errExit("open file for array type");
	if (setvbuf(sfile, NULL, _IOLBF , 0) != 0)
		errExit("setvbuf");
	
	//set new handler for SIGTERM and SIGINT	
	struct sigaction act;
	sigemptyset(&act.sa_mask);
	act.sa_handler = &sig_handl;
	act.sa_flags = 0;
	if (sigaction(SIGTERM, &act, NULL) == -1)
		errExit("sigaction");
	if (sigaction(SIGINT, &act, NULL) == -1)
		errExit("sigaction");
	printf("Start server for work mesage queue\n");
	//Daemonize server
	if (daemonize_srv == 1){
		pid_t d_pid;
		d_pid = fork();
		switch(d_pid){
		case -1:
			  errExit("fork");
		case 0:
			  break;
		default:
			  printf("Daemon pid=%d",d_pid);
			  exit(EXIT_SUCCESS);
		}
		
		if (setsid() == -1)
			  errExit("setsid");
		//close STDIN STDOUT STDERR
		int i;
		for (i = 0; i < 3; i++){
			if (close(i) == -1)
				errExit("close");
		}
	}

	//main cycle
	while (term_flag != 1){
		struct ipc qmsg;
		if (mq_receive(mqueue_des,(char *) &qmsg, sizeof(struct ipc), NULL) == -1){
			if (term_flag == 0){ 
				errExit("mq_receive");
			}else{
				break;
			}
		}
		//formated string with localized date and time
		time_t utime;
		struct tm * ptm;
		char strtm[STR_TIME_SIZE];
		if ((utime = time(NULL)) == -1)
			  errExit("time");
		if ((ptm = localtime(&utime)) == NULL)
			  errExit("localtime");
		if (strftime(strtm, STR_TIME_SIZE, "%x,%X", ptm ) == 0)
			  errExit("strftime");
		//write type to the same file
		switch(qmsg.ipc_type){
		case ARRAY:
			if (fprintf(afile, "%s:", strtm) < 0)
				  errExit("fprintf");
			if (fwrite(qmsg.ipc_data.array, 1, 5, afile) != 5)
				  errExit("fwrite");
			if (fputc('\n', afile) == EOF)
				  errExit("fputc");
			if (daemonize_srv == 0)
				  printf("%s:char[5]=%c%c%c%c%c\n",strtm,qmsg.ipc_data.array[0],qmsg.ipc_data.array[1],qmsg.ipc_data.array[2],qmsg.ipc_data.array[3],qmsg.ipc_data.array[4]);
			break;
		case INTEGER:
			if (fprintf(ifile, "%s:%i\n", strtm, qmsg.ipc_data.integer) < 0)
				  errExit("fprintf");
			if (daemonize_srv == 0)
				printf("%s:integer=%i\n",strtm,qmsg.ipc_data.integer);
			break;
		case STRUCT:
			if (fprintf(sfile, "%s:%i %i %i\n", strtm, qmsg.ipc_data.stct.a, qmsg.ipc_data.stct.b, qmsg.ipc_data.stct.c) < 0)
				  errExit("fprintf");
			if (daemonize_srv == 0)
				printf("%s:struct int a=%i, int b=%i, int c=%i\n",strtm,qmsg.ipc_data.stct.a,qmsg.ipc_data.stct.b,qmsg.ipc_data.stct.c);
			break;
		default:
			  errExit("receive bad message type");
		}
	}

	//deallocate resource
	if (mq_close(mqueue_des) == -1)
		syslog(LOG_ERR,"Error: mq_close. errno = %d. err discription: %s", errno, strerror(errno));
	if (mq_unlink(MQUEUE_NAME) == -1)
		syslog(LOG_ERR,"Error: mq_unlink. errno = %d. err discription: %s", errno, strerror(errno));

	if (fclose(afile) == EOF)
		syslog(LOG_ERR,"Error: close file sream for array type. errno = %d. err discription: %s", errno, strerror(errno));
	if (fclose(ifile) == EOF)
		syslog(LOG_ERR,"Error: close file stream for integer type. errno = %d. err discription: %s", errno, strerror(errno));
	if (fclose(sfile) == EOF)
		syslog(LOG_ERR,"Error: close file stream for struct type. errno = %d. err discription: %s", errno, strerror(errno));

	closelog();

	exit(EXIT_SUCCESS);
}
