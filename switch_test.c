#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

int main()
{
	int fd,i;
	unsigned int num;

	fd = open("/dev/switch", O_RDONLY);

	if ( fd < 0)
	{
		printf("open devices error\n");
		return -1;
	}
	for(i=0;i<32;i++)
	{
		if(read(fd,&num,sizeof(num))<0)
			{
				perror("read error ");
				exit(-1);
			}
		printf("switch :%#010x\n",num);
		sleep(3);
	}
	close(fd);
	return 0;
}

