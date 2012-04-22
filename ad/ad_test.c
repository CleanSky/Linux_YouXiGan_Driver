
#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <linux/i2c-dev.h> 
#include <linux/i2c.h> 

static int my16to10(char num)
{	
	int end;

	switch(num){
case '0':
	end = 0;
	break;

case '1':
	end = 1;
	break;

case '2':
	end = 2;
	break;

case '3':
	end = 3;
	break;

case '4':
	end = 4;
	break;

case '5':
	end = 5;
	break;

case '6':
	end = 6;
	break;

case '7':
	end = 7;
	break;

case '8':
	end = 8;
	break;

case '9':
	end = 9;
	break;

case 'a':
	end = 10;
	break;

case 'b':
	end = 11;
	break;

case 'c':
	end = 12;
	break;

case 'd':
	end = 13;
	break;

case 'e':
	end = 14;
	break;

case 'f':
	end = 15;
	break;

default:
	printf("error num:%d\n",num);
	exit( -1 );
	break;

}

	return end;
}

static int value( char *buffer)
{	
	int sum = 0;
	char * p = buffer;
	for( ; *p !='\0';p++)
	{	
		sum = sum*16 + my16to10( *p );
	}	
	return sum;
}

static int gain = 0,mininum = 0;

static void init(int fd)
{
	char buffer[8];

	read (fd, buffer, 3*sizeof(char));
	printf ("Configure register : %02x\n",buffer[2]);
	switch (buffer[2] & 0xc){
	case 0xc:
		mininum = 32768 ; //15SPS
		break;

	case 0x8:
		mininum = 16384 ;//30SPS
		break;
		
	case 0x4:
		mininum = 8192 ;//60SPS
		break;
		
	case 0x0:
		mininum = 2048 ;//240SPS
		break;

	default:
		printf("error");
		exit(-1);
	}
	
	switch (buffer[2] & 0x3){
	case 0x3:
		gain = 8 ; //15SPS
		break;

	case 0x2:
		gain = 4 ;//30SPS
		break;
		
	case 0x1:
		gain = 2 ;//60SPS
		break;
		
	case 0x0:
		gain = 1 ;//240SPS
		break;

	default:
		printf("error");
		exit(-1);
	}
}

static void ad(int fd)
{
	char buffer[8],temp[8];

	read (fd, buffer, 2*sizeof(char));
	printf ("ad value   = %x%x\n",buffer[0], buffer[1]);
		
	sprintf(temp,"%02x%02x",buffer[0],buffer[1]);
	printf("v+ - v- : %f V\n\n",(( short ) value( temp ) * 2.048 /mininum /gain));
}

static void configure(int fd)
{
	char buffer[8];
	
	buffer[0]=0x0c;//15SPS  gain1
	//buffer[0]=0x0f;//15SPS  gain8
	//buffer[0]=0x00;//240SPS  gain1
	write (fd, buffer, 1*sizeof(char));
}

int main(int argc, char *argv[])
{
	int fd;
	char buffer[8], temp[8];

	if((fd = open("/dev/i2c-3", O_RDWR))<0) {
		perror("/dev/i2c-3");
		exit(-1);
	}

	if (ioctl(fd, I2C_SLAVE, 0x48) < 0) {
		perror("ioctl error");
		return -1;
	}

	printf("Press ctrl+c to quit\n\n");
	if(argc < 2) {
		while( 1 ) {
			read (fd, buffer, 2 *sizeof(char));
			
			printf ("ad value   = %x%x;\n",buffer[0], buffer[1]);
			sprintf(temp,"%02x%02x",buffer[0],buffer[1]);
			//printf("ad :%s,:%d\n",temp,( short ) value( temp ));
			printf("v+ - v- : %f V\n\n",(( short ) value( temp ) * 2.048 /32768));
			sleep(1);
		}
	}
	else if(strcmp(argv[1], "2") == 0) {
		// 15SPS  增益1
		read (fd, buffer, 6*sizeof(char));
		printf ("ad value   = %x%x;configure : %02x,other:%x,%x,%x\n",buffer[0], buffer[1],buffer[2],buffer[3],buffer[4],buffer[5]);

		buffer[0]=0x0c;
		write (fd, buffer, 1*sizeof(char));
		
		read (fd, buffer, 6*sizeof(char));
		printf ("ad value   = %x%x;configure : %02x,other:%x,%x,%x\n",buffer[0], buffer[1],buffer[2],buffer[3],buffer[4],buffer[5]);

		// 15SPS  增益8
#if 0 //-
		read (fd, buffer, 6*sizeof(char));
		printf ("ad value   = %x%x;configure : %02x,other:%x,%x,%x\n",buffer[0], buffer[1],buffer[2],buffer[3],buffer[4],buffer[5]);

		buffer[0]=0x0f;
		write (fd, buffer, 1*sizeof(char));
		
		read (fd, buffer, 6*sizeof(char));
		printf ("ad value   = %x%x;configure : %02x,other:%x,%x,%x\n",buffer[0], buffer[1],buffer[2],buffer[3],buffer[4],buffer[5]);

		//sprintf(temp,"%02x%02x",buffer[0],buffer[1]);
		//printf("ad :%s,:%d\n",temp,( short ) value( temp ));
		//printf("v+ - v- : %f V\n\n",(( short ) value( temp ) * 2.048 /32768));
#endif
		sleep(1);
	}
	else if(strcmp(argv[1], "3") == 0) {
		init(fd);
		printf("****mininum:%d,gain:%d****\n\n",mininum,gain);

		configure(fd);
		init(fd);
		printf("****mininum:%d,gain:%d****\n\n",mininum,gain);

		sleep(1);
	
		ad(fd);
	}
	
	close(fd);

	return 0;
}

