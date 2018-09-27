#include <errno.h>
#include <fcntl.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <fstream>
//#include <ugpio/ugpio.h>

using namespace std;

enum mystate {excessive, normal, low, toolow, alarmcancel, toolowtoolong, hightoolong ,hold };
enum type {  error, trace };
enum logstate {  recorded, empty, unrecorded , null};
int writelog(type currtype , logstate currlogstate);
int writeoutput (mystate currstate, const float data);

//int gpio_direction_output(unsigned int gpio, int value);
//int gpio_direction_output(unsigned int gpio, int value);
int set_interface_attribs(int fd, int speed)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        //printf("Error from tcgetattr: %s\n", strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);

    tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         /* 8-bit characters */
    tty.c_cflag &= ~PARENB;     /* no parity bit */
    tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
    tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

    /* setup for non-canonical mode */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    /* fetch bytes as they become available */
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        //printf("Error from tcsetattr: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

void set_mincount(int fd, int mcount)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
       // printf("Error tcgetattr: %s\n", strerror(errno));
        return;
    }

    tty.c_cc[VMIN] = mcount ? 1 : 0;
    tty.c_cc[VTIME] = 5;        /* half second timer */

    //if (tcsetattr(fd, TCSANOW, &tty) < 0)
        //printf("Error tcsetattr: %s\n", strerror(errno));
}

int open_serial(const char * portname)
{
    const char *port = portname;
    int fd;
    // to access serial port on linux, you need to open file.
    fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        //printf("Error, cannot receive data from %s: %s\n", portname, strerror(errno));
        return -1;
    }
    /*baudrate 9600, 8 bits, no parity, 1 stop bit */
    set_interface_attribs(fd, B9600);
    //set_mincount(fd, 0);                /* set to pure timed read */
    return fd;
}

/*
*   Read data from serial port into buffer array.  fd is int returned from open_serial.
*//*
int convert(unsigned char* buffer){
	int i = 0;	
	const char copybuff[4];

	for(i = 0; i<4 ; i++){
	copybuff[i] = buffer[i];
	}	
	unsigned int number = atoi(copybuff);
	return number;
}*/
int read_serial(int fd, char*  buffer)
{
	
        int rdlen;
        rdlen = read(fd, buffer, sizeof(buffer) - 1);
        if (rdlen > 0) {
            //buffer[rdlen] = 0;
            //printf("%s\n",buffer);
	
	/*int index = 0;
	char curr = buffer[index];
	int number =0;
	
	while(index<3){
		if (curr=='\0'){
			return number;
		}
		number*=10;
		unsigned int value = curr-'0';
		number+=value;
		index++;
		curr = buffer[index];
	}*/

	//const char* buffer = reinterpret_cast<const char*>(buffer);
	int number = atoi(buffer);
	return number;
			
        } else if (rdlen < 0) {
            //printf("Error from read: %d: %s\n", rdlen, strerror(errno));
            return -1;
        }
}


int main() {
    int fd;
    const char *port_name = "/dev/ttyS1";
    fd = open_serial(port_name);
   
    char buf[100];
    /*for(int i =0;i<100;i++){
	buf[i]='0';
    }*/
    //printf("data:%s\n", buf);

while (1){ 

    int read = read_serial(fd,buf);//moved from outside into the loop
	printf("%d\n",read);
    mystate currmystate = normal;
    int DangerTimeCounter = 0;

    float data = (float)read;
    float maximum=30;
    float lowlevel1 = 20;
    float lowlevel2 = 15;
    bool hightoolongAlarm = false;
    bool highAlarm = false;
    bool lowAlarm = false;
    bool toolowAlarm = false;
    bool toolowtoolongAlarm = false;
    bool hasanAlarm = false;  //related to statetokill
    bool alarmchange = false;   //already in one alarm state, enter another alarm state rather than normal directly, this ->true, and do sth
 
   // if(/*input empty*/){
     //   writelog (error,empty);
    //}
        
        
    if(data>=maximum&&!highAlarm){ ///////////////what if u had and maintain this state???
        if(!hasanAlarm){
            hasanAlarm = true;
            currmystate = excessive;
        }
        else {  //if has an alarm already
            currmystate = alarmcancel;
        }
    }
    
    if(data>=maximum&&highAlarm){
        currmystate = hold;
    }
    
    if(data<maximum&&data>lowlevel1){
        //...
        if(hasanAlarm){  //already sent some alarm but back to normal now
            currmystate = alarmcancel;
            //in this case , state abnormal to normal, need output
            
            if( writeoutput(normal,data)==-1){
                writelog(error, unrecorded);
            }
            else{
                writelog(trace, recorded);
            }
        }
        else{////staynormal
            currmystate = hold;
        }
    }
    
    if((data<=lowlevel1&&data>lowlevel2)&&!lowAlarm){
        //...
        if(!hasanAlarm){
            hasanAlarm = true;
            currmystate = low;
        }
        else{
            currmystate = alarmcancel;
        }
    }
    
    if((data<=lowlevel1&&data>lowlevel2)&&lowAlarm){
        currmystate = hold;
    }
    
    if(data<=lowlevel2&&!toolowAlarm){
        //...
        if(!hasanAlarm){
            hasanAlarm = true;
            currmystate = toolow;
        }
        else{
            currmystate = alarmcancel;
        }
    }
    
    if((data<=lowlevel1&&data>lowlevel2)&&toolowAlarm){
        currmystate = hold;//no operation
    }
    
    ///////////////////////////////////////////
    switch (currmystate) {
            
            writelog(trace, null); //////////into switch
            
        case excessive:////toolong case
            highAlarm = true;
            if(writeoutput(excessive, data)==-1){
                writelog(error, unrecorded);   //////////////msg sending failed
            }
            else{
                writelog(trace, recorded);
            }
            DangerTimeCounter++;
            if(DangerTimeCounter==3600){   //3s per data, 3600 times, which is 3 hour ,we call it too long, new alarm frequency should be applied
              //  writeoutput(hightoolong,data);
                hightoolongAlarm = true;
                if(writeoutput(hightoolong,data)==-1){
                    writelog(error, unrecorded);
                }
                else{
                    writelog(trace, recorded);
                }
                DangerTimeCounter = 0 ; // reset timecounter, recount timesuntill 3hrs again, resend the msg
            }
          
            break;
            
        case normal:
           
            //..probablly nothing to do
            break;
            
        case low:
            lowAlarm = true;
            if( writeoutput(low,data)==-1){
                writelog(error, unrecorded);
            }
            else{
                writelog(trace, recorded);
            }
            break;
            
        case toolow:////toolong case
            toolowAlarm = true;
            if(writeoutput( toolow,data)==-1){
                writelog(error,unrecorded);
            }
            else{
                writelog(trace, recorded);
            }
            DangerTimeCounter++;
            if(DangerTimeCounter==3600){
               // writeoutput( toolowtoolong,data);
                toolowtoolongAlarm = true;
                if(writeoutput( toolowtoolong,data)==-1){
                    writelog(error, unrecorded);
                }
                else{
                    writelog(trace, recorded);
                }
                DangerTimeCounter = 0;
            }
            
            break;
            
        case alarmcancel:

            highAlarm = false;
            lowAlarm = false;
            toolowAlarm = false;
            DangerTimeCounter = 0;  //no longer stay in the same state, reset
            if(writeoutput(alarmcancel,data)==-1){
                writelog(error, unrecorded);
            }
            else{
                writelog(trace, recorded);
            }
            break;
            

        default:
            break;
    }
    
    
}//end of while loop    
    return 0;
}


int writelog(type currtype, logstate currlogstate){
    time_t currtime = time(0);   //get time now
    struct tm * now = localtime(&currtime);
    
    ofstream outfile;
    outfile.open("logfile.txt");
    if(!outfile.is_open())
        return -1;
    
    switch (currtype) {
        case error:
            if(currlogstate==empty){
                outfile << "TIME:[" << now->tm_mday << "/" <<  now->tm_mon+1 << " " << now->tm_hour << ":" << now->tm_min << ":" << now->tm_sec <<"] ";
                outfile << "<<" << currtype << ">> :" << "SENSOR FAILED TO GET DATA FROM SENSOR. NO DATA BEING ANALYZE..." << endl;
            }
            else{ //unrecorded case
                outfile << "TIME:[" << now->tm_mday << "/" <<  now->tm_mon+1 << " " << now->tm_hour << ":" << now->tm_min << ":" << now->tm_sec <<"] ";
                outfile << "<<" << currtype << ">> :" << "FAILED TO OPEN THE OUTPUT FILE." << endl;
            }
            break;
        
        case trace:
            if(currlogstate==null){  //INTO SWITCH
                outfile << "TIME:[" << now->tm_mday << "/" <<  now->tm_mon+1 << " " << now->tm_hour << ":" << now->tm_min << ":" << now->tm_sec <<"] ";
                outfile << "<<" << currtype << ">> :" <<  "SWITCHING TO ANALYZE CRRENT DATA......" << endl;
            }
            else{  //recorded
                outfile << "TIME:[" << now->tm_mday << "/" <<  now->tm_mon+1 << " " << now->tm_hour << ":" << now->tm_min << ":" << now->tm_sec <<"] ";
                outfile << "<<" << currtype << ">> :" << "REMINDING MESSAGE SUCCESSFULLY RECORDED TO OUTPUT FILE." << endl;
            }
            break;
            

        default:
            break;
    }
    outfile.close();
    return 0;
    
}

int writeoutput(mystate currstate, const float data){
    time_t currtime = time(0);   //get time now
    struct tm * now = localtime(&currtime);
    
    ofstream outfile;
    outfile.open("output.txt");
    if(!outfile.is_open())
        return -1;
    
    switch (currstate) {
        case alarmcancel:
            outfile << "TIME:[" << now->tm_mday << "/" <<  now->tm_mon+1 << " " << now->tm_hour << ":" << now->tm_min << ":" << now->tm_sec <<"] ";
            outfile << "CURRENT MOISTURE IS:"<< data << "%" << endl;
            outfile << "CURRENT MOISTURE LEVEL HAS CHANGED." << endl;
            break;
            
        case excessive:
            outfile << "TIME:[" << now->tm_mday << "/" <<  now->tm_mon+1 << " " << now->tm_hour << ":" << now->tm_min << ":" << now->tm_sec <<"] ";
            outfile << "CURRENT MOISTURE IS:"<< data << "%" << endl;
            outfile << "CURRENT MOISTURE LEVEL IS QUITE HIGH...PLEASE CONSIDER REMOVING SOME WATER!" << endl;
			gpio_direction_output(2, 1);
            break;
            
        case low:
            outfile << "TIME:[" << now->tm_mday << "/" <<  now->tm_mon+1 << " " << now->tm_hour << ":" << now->tm_min << ":" << now->tm_sec <<"] ";
            outfile << "CURRENT MOISTURE IS:"<< data << "%" << endl;
            outfile << "CURRENT MOISTURE LEVEL IS QUITE LOW...PLEASE REMEMBER TO WATER THE PLANT!"<< endl;
            break;
            
        case toolow:
            outfile << "TIME:[" << now->tm_mday << "/" <<  now->tm_mon+1 << " " << now->tm_hour << ":" << now->tm_min << ":" << now->tm_sec <<"] ";
            outfile << "CURRENT MOISTURE IS:"<< data << "%" << endl;
            outfile << "CURRENT MOISTURE LEVEL IS TOOOO LOW...PLEASE WATER ME ASAP!!" <<endl;
            gpio_direction_output(1, 1);
            break;
            
        case toolowtoolong:
            outfile << "TIME:[" << now->tm_mday << "/" <<  now->tm_mon+1 << " " << now->tm_hour << ":" << now->tm_min << ":" << now->tm_sec <<"] ";
            outfile << "CURRENT MOISTURE IS:"<< data << "%" << endl;
            outfile <<  "MOISTURE LEVEL MAINTAINED TOO LOW FOR 3 HOURS(OR ANOTHER 3)...PLEASE WATER THE PLANT NOW!!!IF U DON'T HOPE TO WITHER IT."<< endl;
            break;
            
        case hightoolong:
            outfile << "TIME:[" << now->tm_mday << "/" <<  now->tm_mon+1 << " " << now->tm_hour << ":" << now->tm_min << ":" << now->tm_sec <<"] ";
            outfile << "CURRENT MOISTURE IS:"<< data << "%" << endl;
            outfile << "MOISTURE LEVEL MAINTAINED HIGH FOR 3 HOURS(OR ANOTHER 3)...PLEASE REMOVE SOME WATER NOW!!!IF U DON'T HOPE TO DROWN IT."<< endl;
            break;
            
        case normal:
            outfile << "TIME:[" << now->tm_mday << "/" <<  now->tm_mon+1 << " " << now->tm_hour << ":" << now->tm_min << ":" << now->tm_sec <<"] ";
            outfile << "CURRENT MOISTURE IS:"<< data << "%" << endl;
            outfile << "MOISTURE LEVEL IS CURRENTLY BACK TO NORMAL STATE... GOOD JOB:D" << endl;
            gpio_direction_output(1, 0);
            gpio_direction_output(2,0);
            break;
            
        case hold:
            outfile << "TIME:[" << now->tm_mday << "/" <<  now->tm_mon+1 << " " << now->tm_hour << ":" << now->tm_min << ":" << now->tm_sec <<"] ";
            outfile << "CURRENT MOISTURE IS:"<< data << "%" << endl;
            outfile << "MOISTURE LEVEL STATE DID NOT CHANGE.== " << endl;
            break;
            
        default:
            break;
    }
    outfile.close();
    return 0;
	/*Giving a value of 0 normally forces a LOW on the GPIO pin, whereas a value
	of 1 sets the GPIO to HIGH. But note, that if the GPIO is configured as
	'active low' then the logic is reversed.
	*/
    
}

