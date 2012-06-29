    #include <stdio.h>    
    #include <stdlib.h>    
    #include <unistd.h>    
    #include <fcntl.h>    
    #include <signal.h>    
    #include <sys/ioctl.h>  
    #include <linux/joystick.h>
     
   
    int main(int argc, char **argv)    
    {    
         /*事件结构体*/
         struct js_event event;  
             int fd;
             /*打开设备*/
         fd = open("/dev/input/js0", O_RDWR);  
           
         int i=0,j=0; 
           
         if(fd != -1)    
         {    
         printf("open device successfully\n"); 
              while(1){
            /*从内核读取事件*/
            j=read(fd,&event,sizeof(struct js_event));
           if(j<0)
              {
             printf("read buffer error!!!\n"); 
              break;
               }
           
             /*根据读出来的event 输出相应的按键
                 type=1为ev_key事件
                    type=2为ev_abs事件*/
         if(event.type==1 && event.value==1){
          switch(event.number){
                 case 0: printf("press 1\n");break;
                 case 1: printf("press 2\n");break;
                 case 2: printf("press 3\n");break;
                 case 3: printf("press 4\n");break;
                 case 4: printf("press left 1\n");break;
                 case 5: printf("press right 1\n");break;
                 case 6: printf("press left 2\n");break;
                 case 7: printf("press right 2\n");break;
                 case 8: printf("press select \n");break;
                 case 9: printf("press start\n");break;
                 default : ;
            }
          
      } 
        else if(event.type==2){
           switch(event.value){
             case 0x7fff: if(event.number==0)printf("press right\n");
                        else if(event.number==1)printf("press down\n");
                      break;
             case 0xffff8001 : if(event.number==0)printf("press left\n");
                        else if(event.number==1)printf("press up\n");
                      break;
             default : ;
            }
        }


            sleep(1);
              i++;
           }
         }    
         else    
             printf("Could not open device!\n");    
         return 0;    
    }    
 
