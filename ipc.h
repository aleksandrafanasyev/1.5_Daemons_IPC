#define MQUEUE_NAME "/my_mqueue"
#define MQUEUE_MAXMSG 10 // max 10

enum dtype{ARRAY, INTEGER, STRUCT};

struct mystruct{
	int a;
	int b;
	int c;
};

union data{
	char array[5];
	int integer;
	struct mystruct stct;
};

struct ipc{
	enum dtype ipc_type;
	union data ipc_data;
};



