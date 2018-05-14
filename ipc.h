#define MQUEUE_NAME "/my_mqueue"
#define MQUEUE_MAXMSG 64

enum dtype{ ARRAY, INT, STRUCT};

struct mystruct{
	int a;
	int b;
	int c;
};

union data{
	char[5] array;
	int integer;
	mystruct stct;
};

struct ipc{
	dtype ipc_type;
	data ipc_data;
};



