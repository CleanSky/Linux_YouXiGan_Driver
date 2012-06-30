#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#define CLEAR 1

int main()
{
	int fd = 0, ret = 0, length = 0;
	char buffer[1024];

	fd = open("/dev/globalmem", O_RDWR);

	if(fd < 0){
		printf("Cannot Open '/dev/globalmem'\n");
		close(fd);
		return 0;
	}

	memset(buffer, 0, 1024);
	strcpy(buffer, "Globalmem Test, Clean Sky, Zou Bingsong\n");
	length = strlen(buffer);
	printf("要写入的字符:\n长度=%d\n字符=%s\n", length, buffer);
	ret = lseek(fd, 0, SEEK_SET);
	ret = write(fd, buffer, length);

	memset(buffer, 0, 1024);
	ret = lseek(fd, 0, SEEK_SET);
	ret = read(fd, buffer, length);
	if(ret > 0){
		printf("清除内存前读出的字符:\n长度=%d\n字符=%s\n", ret, buffer);
	}
	
	ret = lseek(fd, 0, SEEK_SET);
	ret = ioctl(fd, CLEAR, 0);
	ret = read(fd, buffer, length);
	printf("清除内存后读出的字符:\n长度=%d\n字符=%s\n", ret, buffer);

	close(fd);

	return 0;
}
