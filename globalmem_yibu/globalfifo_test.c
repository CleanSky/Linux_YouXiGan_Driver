/*
 *globalfifo异步通知测试文件
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>

void input_handler(int signum)
{
  printf("receive a signal from globalfifo,signalnum:%d\n",signum);
}

main()
{
  int fd, oflags;
  fd = open("/dev/globalfifo", O_RDWR, S_IRUSR | S_IWUSR);
  if (fd !=  - 1) {
    signal(SIGIO, input_handler); 
    fcntl(fd, F_SETOWN, getpid());
    oflags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, oflags | FASYNC);
    while(1) {
    	sleep(100);
    }
  } else {
    printf("device open failure\n");
  }
}
