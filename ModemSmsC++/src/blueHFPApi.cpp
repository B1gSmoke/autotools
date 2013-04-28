/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * lib.c
 * Copyright (C) jjx 2012 <jianxin.jin@asianux.com>
 *
 * phone-daemon is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * phone-daemon is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>      /*标准输入输出定义*/
#include <stdlib.h>     /*标准函数库定义*/
#include <unistd.h>     /*Unix 标准函数定义*/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>      /*文件控制定义*/
#include <termios.h>    /*PPSIX 终端控制定义*/
#include <errno.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include <iostream>
#include <sstream>

using namespace std;

#include "unicode.h"
#include "blueHFPApi.h"
#include "blueutils.h"

//#define FALSE  -1
//#define TRUE   0
#define BUF_LEN 1024
#define CHINA_SET_MAX 8

//static  int serial_fd;
//static gboolean lock = TRUE;
//static char *center = "8613010331500";

static int read_id= 0;
static int delete_id = 0;
static int sim_id = 0;
static int process_id = 0;
static int sms_id = 0;
static GQueue *smsQueue;
static gboolean send_sms = FALSE;

typedef struct
{
    int serial_fd;
    gboolean lock;
    char *centerNumber;
    HFPCallback hfpCallback;
} BlueHfp;

BlueHfp *blueHfp = NULL;

int speed_arr[] = { B38400, B19200, B9600, B4800, B2400, B1200, B300,
                    B38400, B19200, B9600, B4800, B2400, B1200, B300,
                  };

int name_arr[] = {38400,  19200,  9600,  4800,  2400,  1200,  300, 38400,
                  19200,  9600, 4800, 2400, 1200,  300,115200,
                 };

void *serial_read_data(gpointer data);

string blue_hfp_utf8_tounicode(const string& content);
string blue_hfp_get_center_number(const string& num);
string blue_hfp_get_address_number(const string& num);

char *blue_hfp_process_string(char *msg);

gboolean blue_hfp_read_sms_timeout(gpointer data);
gboolean blue_hfp_send_sms_timeout(gpointer data);
gboolean blue_hfp_delete_sms_timeout(gpointer data);
gboolean blue_hfp_delete_sim_sms(gpointer data);
gboolean blue_hfp_get_at_zpas(gpointer data);
gboolean blue_hfp_process_string_timeout(gpointer data);


int initBlueHFP(unsigned int  heartbeatInterval)
{
    g_type_init();

    blueHfp = (BlueHfp *)malloc(sizeof(BlueHfp *)*1);

    smsQueue = g_queue_new();

    blueHfp->centerNumber = "8613010331500";

    blueHfp->serial_fd = openDev("/dev/ttyUSB1");

    if(blueHfp->serial_fd == -1)
    {
        exit(0);
    }

    set_speed(blueHfp->serial_fd,115200);

    if (set_Parity(blueHfp->serial_fd,8,1,'N') == FALSE)  {
        printf("Set Parity Error\n");
        exit (0);
    }

    g_thread_create(serial_read_data,(void *)blueHfp->serial_fd,NULL,NULL);

    char *commond = "AT+CSCA?\r";
    int ret = strlen(commond);

    if(ret == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("write success\n");
    }

    /*设置短信发送格式 0为pdu 1为英文字符*/
    commond = "AT+CMGF=0\r";
    ret = strlen(commond);

    if(ret == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("write AT+CMGF=0 success\n");
    }

    commond = "AT+ZCSQ=5\r";
    ret = strlen(commond);

    if(ret == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("write AT+ZCSQ=5 success\n");
    }

    commond = "AT+CSQ\r";
    ret = strlen(commond);

    if(ret == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("write AT+CSQ=0 success\n");
    }

    read_id = g_timeout_add_seconds(5,blue_hfp_read_sms_timeout,NULL);


    blueHfp->lock = FALSE;

    return HFP_SUCCESS;
}

int openDev(const char *dev)
{
    int	fd = open( dev,O_RDWR|O_NOCTTY );

    if (-1 == fd)
    {
        perror("Can't Open Serial Port");
        return -1;
    }
    else
        return fd;
}


void set_speed(int fd, int speed)
{
    int   i;
    int   status;
    struct termios   Opt;

    tcgetattr(fd, &Opt);
    for ( i= 0;  i < sizeof(speed_arr) / sizeof(int);  i++) {
        if  (speed == name_arr[i]) {
            tcflush(fd, TCIOFLUSH);
            cfsetispeed(&Opt, speed_arr[i]);
            cfsetospeed(&Opt, speed_arr[i]);
            status = tcsetattr(fd, TCSANOW, &Opt);
            if  (status != 0) {
                perror("tcsetattr fd1");
                return;
            }
            tcflush(fd,TCIOFLUSH);
        }
    }
}

int set_Parity(int fd,int databits,int stopbits,int parity)
{

    struct termios options;
    if  ( tcgetattr( fd,&options)  !=  0) {
        perror("SetupSerial 1");
        return(FALSE);
    }
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CLOCAL | CREAD;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    switch (databits) /*设置数据位数*/
    {
    case 7:
        options.c_cflag |= CS7;
        break;
    case 8:
        options.c_cflag |= CS8;
        break;
    default:
        fprintf(stderr,"Unsupported data size\n");
        return (FALSE);
    }
    switch (parity)
    {
    case 'n':
    case 'N':
        options.c_cflag &= ~PARENB;   /* Clear parity enable */
        options.c_iflag &= ~INPCK;     /* Enable parity checking */
        break;
    case 'o':
    case 'O':
        options.c_cflag |= (PARODD | PARENB); /* 设置为奇效验*/
        options.c_iflag |= INPCK;             /* Disnable parity checking */
        break;
    case 'e':
    case 'E':
        options.c_cflag |= PARENB;     /* Enable parity */
        options.c_cflag &= ~PARODD;   /* 转换为偶效验*/
        options.c_iflag |= INPCK;       /* Disnable parity checking */
        break;
    case 'S':
    case 's':  /*as no parity*/
        options.c_cflag &= ~PARENB;
        options.c_cflag &= ~CSTOPB;
        break;
    default:
        fprintf(stderr,"Unsupported parity\n");
        return (FALSE);
    }
    /* 设置停止位*/
    switch (stopbits)
    {
    case 1:
        options.c_cflag &= ~CSTOPB;
        break;
    case 2:
        options.c_cflag |= CSTOPB;
        break;
    default:
        fprintf(stderr,"Unsupported stop bits\n");
        return (FALSE);
    }
    /* Set input parity option */
    if (parity != 'n')
        options.c_iflag |= INPCK;
    //tcflush(fd,TCIFLUSH);
    tcflush(fd, TCIOFLUSH);
    //options.c_cc[VTIME] = 150; /* 设置超时15 seconds*/
    options.c_cc[VTIME] = 10;
    options.c_cc[VMIN] = 0; /* Update the options and do it NOW */
    if (tcsetattr(fd,TCSANOW,&options) != 0)
    {
        perror("SetupSerial 3");
        return (FALSE);
    }
    return (TRUE);

}

int unInitBlueHFP()
{
    close(blueHfp->serial_fd);
    free(blueHfp);
    blueHfp = NULL;
}

int dial(const char* number)
{
    blueHfp->lock = FALSE;
    char *commond = (char *)malloc(sizeof(char *)*128);
    memset(commond,0,strlen(commond));

    sprintf(commond,"ATD%s;\r",number);

    int size = strlen(commond);

    if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("make call success\n");
    }

    printf("%s\n",commond);

    free(commond);
    commond = NULL;
    blueHfp->lock = TRUE;
}

int dialExtNumber(const char* number)
{
    blueHfp->lock = FALSE;
    char *commond = (char *)malloc(sizeof(char *)*128);
    memset(commond,0,strlen(commond));

    sprintf(commond,"AT+VTS=%s\r",number);

    int size = strlen(commond);

    if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("dial extension success\n");
    }

    printf("%s\n",commond);

    free(commond);
    commond = NULL;
    blueHfp->lock = TRUE;


}

int answerCall()
{
    char *commond = "ATA\r";
    int size = strlen(commond);

    if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("%s\n",commond);
    }

}

int rejectCall()
{
    char *commond = "AT+CHUP\r";
    int size = strlen(commond);

    if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("%s\n",commond);
    }
}

int handupCall()
{
    char *commond = "AT+CHUP\r";
    int size = strlen(commond);

    if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("%s\n",commond);
    }

}

int sendSM(const string& content,const string& number)
{
    cout<<"C++****************************************C++"<<endl;
    string addressNumber = blue_hfp_get_address_number(number);

    //cout<<"addressNumber="<<addressNumber<<endl;
    string centerNumber = blue_hfp_get_center_number(string(blueHfp->centerNumber));
//	cout<<"centerNumber="<<centerNumber<<endl;

    string msg = blue_hfp_utf8_tounicode(content);

	//sprintf(commond,"%s%s000800%s\x1A",centerNumber,addressNumber,msg);
	string commond = centerNumber+addressNumber+"000800"+msg+"\x1A";

	char commond1[64];

	sprintf(commond1,"AT+CMGS=%d\r",(addressNumber.length()+6+msg.length())/2);


   	blueHfp->lock = FALSE;

    int ret =  strlen(commond1);
    if(ret == write(blueHfp->serial_fd,commond1,ret))
    {
        printf("commond1 success\n");
    }


	{
		ostringstream  ss;
		ss << ((addressNumber.length()+6+msg.length())/2);
		string commond1("AT+CMGS=");
		commond1+=ss.str();
		commond1 +="\r";
	}

    ret = commond.length();
    if(ret == write(blueHfp->serial_fd,commond.c_str(),ret))
    {
        printf("commond success\n");
    }

	cout<<"commond1= "<<commond1<<endl;
	cout<<commond<<endl;

    blueHfp->lock = TRUE;


}

int deletesendSM(unsigned short choice)
{
}

int readSM(unsigned short choice)
{
}

string blue_hfp_get_center_number(const string& num)
{
    string number = blue_utils_switch_number(num);
    string size = blue_utils_number_to16(("91"+number).length()/2);

    return string(size+"91"+number);
}


string blue_hfp_get_address_number(const string& num)
{
    string str = blue_utils_switch_number(num);
    string size = blue_utils_number_to16(str.length());

    return string("1100"+size+"91"+str);
}

string blue_hfp_utf8_tounicode(const string& content)
{
	int n=0,  unicode = 0;

	string message;

    for(int i=0; i<content.length(); NULL)
    {
        const char *str = content.substr(i,1).c_str();

        if(str[0] >=0 && str[0]<=127)
        {
            n = UTF8toUnicode((unsigned char *)str,&unicode);
            if(unicode<255)
            {

                char str[16] ;
                sprintf(str,"00%x",unicode);
                if(strlen(str) == 3)
                {
                    char tmp[16];
                    sprintf(tmp,"0%s",str);
					message+=tmp;
                }
                else
                {
					message+=str;                  
                }
            }
            else
            {
                char str[16] ;
                sprintf(str,"%x",unicode);
				message+=str;
            }

            i++;
        }
        else
        {
			const char *str = content.substr(i,3).c_str();

            n = UTF8toUnicode((unsigned char *)str,&unicode);
            if(unicode<255)
            {
                char str[16];
                sprintf(str,"00%x",unicode);
                message+=str;
            }
            else
            {
                char str[16];
                sprintf(str,"%x",unicode);
				message+=str;
            }

            i+=3;
        }
    }

	string str = blue_utils_number_to16(message.length()/2);

	message.insert(0,str);

//    cout<<message<<endl;

    return message;
}

char *blue_hfp_unicode_utf8(char *content)
{
}

void *serial_read_data(gpointer data)
{
    fd_set rfd;
    int ret;
    struct timeval timeout;
    char buf[BUF_LEN];

    while(1)
    {
        /*        if(lock)
                {
        			*/
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        FD_ZERO(&rfd);
        FD_SET(blueHfp->serial_fd, &rfd);
        ret = select(blueHfp->serial_fd+1, &rfd, NULL, NULL, &timeout);

        if(FD_ISSET(blueHfp->serial_fd, &rfd))
        {
            do
            {
                memset(buf, '\0', BUF_LEN);
                ret = read(blueHfp->serial_fd, buf, BUF_LEN);
                if(ret > 0)
                {
                    char *str = strdup(buf);
                    printf("%s\n",str);
                    blue_hfp_process_string(str);
                }
            } while(ret > 0);
        }
//        }
    }
}

char *blue_hfp_process_string(char *msg)
{
    printf("%s\n",msg);

    char *p = NULL;

    if(p = strstr(msg,"+CSCA"))
    {
        if(p !=NULL)
        {
//			printf("=========%s\n",p);
            char *center = blue_utils_get_center_number(msg);
            blueHfp->centerNumber = strdup(center);
//			printf("%s\n",center);
        }
    }
    else if(p = strstr(msg,"STOPRING"))
    {
        printf("hangup\n");

        if(p !=NULL)
        {
            if(blueHfp->hfpCallback)
            {
                (blueHfp->hfpCallback)(PHONESTATUS_EVENT,NULL,CALLINACTIVE);
            }
        }
    }
    else if(p = strstr(msg,"HANGUP"))
    {
        if(p !=NULL)
        {
            if(blueHfp->hfpCallback)
            {
                (blueHfp->hfpCallback)(PHONESTATUS_EVENT,NULL,CALLINACTIVE);
            }
        }
    }
    else if(p = strstr(msg,"NO CARRIER"))
    {
        if(p !=NULL)
        {
            if(blueHfp->hfpCallback)
            {
                (blueHfp->hfpCallback)(PHONESTATUS_EVENT,NULL,CALLINACTIVE);
            }
        }

    }
    else if(p = strstr(msg,"+CLIP"))
    {
        if(p = strstr(msg,"+CLIP"))
        {
            char *ringNumber = blue_utils_get_ring_number(msg);

            if(blueHfp->hfpCallback)
            {
                (blueHfp->hfpCallback)(CALLERNUMBER_EVENT,(void*)CALLACTIVE,(unsigned long )ringNumber);
            }
        }
    }
    else if(p = strstr(msg,"CMTI"))
    {
        if(p = strstr(msg,"CMTI"))
        {
            char *index = blue_utils_notify_sms(msg);
            if(index !=NULL)
            {
                char *commond = (char *)malloc(sizeof(char *)*512);
                memset(commond,'\0',strlen(commond));

                sprintf(commond,"AT+CMGR=%s\r",index);

                blueHfp->lock = FALSE;

                int size = strlen(commond);

                if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
                {
                    printf("make call success\n");
                }
                printf("%s\n",commond);

                free(commond);
                commond = NULL;
                blueHfp->lock = TRUE;

                sim_id = g_timeout_add_seconds(3,blue_hfp_delete_sim_sms,index);
            }
        }
    }
    else if(p = strstr(msg,"ANSWER"))
    {
        printf("answer\n");

        if(blueHfp->hfpCallback)
        {
            (blueHfp->hfpCallback)(PHONESTATUS_EVENT,NULL,CALLACTIVE);
        }
    }
    else if(p = strstr(msg,"+CMGL"))
    {
        printf("message\n");
        if(p !=NULL)
        {
            /*char *str = strdup(msg);
            printf("***********%s\n",msg);
            char *tmp= blue_utils_get_sms_msg(str);
            if(tmp !=NULL)
            {
                if(blueHfp->hfpCallback)
                {
                    (blueHfp->hfpCallback)(SMMT_EVENT,NULL,tmp);
                }
            }
            */

            if(process_id == 0)
            {
                //process_id = g_timeout_add_seconds(3,G_CALLBACK((void *)blue_hfp_process_string_timeout),NULL);
                process_id = g_timeout_add_seconds(3,blue_hfp_process_string_timeout,NULL);
            }

            g_queue_push_tail(smsQueue,msg);
        }
    }
    else if(p = strstr(msg,"CMGR"))
    {
        if(p !=NULL)
        {
            char *str = strdup(msg);
            /*            printf("***********%s\n",msg);
                        char *tmp= blue_utils_get_sms_msg(str);

                        if(tmp !=NULL)
                        {
                            if(blueHfp->hfpCallback)
                            {
                                (blueHfp->hfpCallback)(SMMT_EVENT,NULL,tmp);
                            }
                        }
            			*/

            if(process_id == 0)
            {
                process_id = g_timeout_add_seconds(3,blue_hfp_process_string_timeout,NULL);
            }

            g_queue_push_tail(smsQueue,msg);

        }
    }
    else if(p = strstr(msg,"CSQ"))
    {
        if(p !=NULL)
        {
            char *tmp = blue_utils_get_signal(msg);
            printf("singal = %s\n",tmp);
            if(blueHfp->hfpCallback)
            {
                (blueHfp->hfpCallback)(SINGAL_EVENT,NULL,(unsigned long)tmp);
            }
        }
    }
    else if(p = strstr(msg,"+CMGS:"))
    {
        if(p !=NULL)
        {
            if(blueHfp->hfpCallback)
            {
                (blueHfp->hfpCallback)(SMS_SUCCESS,NULL,NULL);
                if(sms_id !=0)
                {
                    g_source_remove(sms_id);
                    sms_id = 0;
                }

                send_sms = FALSE;
            }
        }
    }
}

void setMsgCallBack(HFPCallback pFunc,void *data)
{
    blueHfp->hfpCallback = pFunc;

    /*	char *str = "+CSCA: \"+8613010331500\",145";

    	char *tmp = blue_utils_get_center_number(str);
    	printf("%s\n",tmp);

    	if(blueHfp->hfpCallback)
    	{
    		(blueHfp->hfpCallback)(RING_EVENT,CALLACTIVE,tmp);
    	}

    	char *str = "+CLIP: \"18661018857\",128,,,,0";

    	char *tmp = blue_utils_get_ring_number(str);
    	printf("%s\n",tmp);

    */

}

gboolean blue_hfp_send_sms_timeout(gpointer data)
{
    if(sms_id !=0)
    {
        g_source_remove(sms_id);
        sms_id = 0;
    }

    if(blueHfp->hfpCallback)
    {
        (blueHfp->hfpCallback)(SMS_FAILED,NULL,NULL);
    }

    send_sms = FALSE;

    return FALSE;
}

gboolean blue_hfp_read_sms_timeout(gpointer data)
{
    printf("read sms success\n");
    if(read_id !=0)
    {
        g_source_remove(read_id);
        read_id = 0;
    }

    blueHfp->lock = FALSE;

    char *commond = "AT+CMGL=4\r";

    int size = strlen(commond);

    if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("read sms  success\n");
    }

    printf("%s\n",commond);

    blueHfp->lock = TRUE;


    delete_id = g_timeout_add_seconds(10,blue_hfp_delete_sms_timeout,NULL);

    printf("***********************\n");

    return FALSE;
}

gboolean blue_hfp_delete_sms_timeout(gpointer data)
{
    printf("delete sms timeout\n");
    if(delete_id !=0)
    {
        g_source_remove(delete_id);
        delete_id = 0;
    }

    blueHfp->lock = FALSE;

    char *commond = "AT+CMGD=1,3\r";

    int size = strlen(commond);

    if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("read sms  success\n");
    }

    printf("%s\n",commond);

    blueHfp->lock = TRUE;

    return FALSE;
}

gboolean blue_hfp_delete_sim_sms(gpointer data)
{
    if(sim_id !=0)
    {
        g_source_remove(sim_id);
        sim_id = 0;
    }

    char *commond = (char *)malloc(sizeof(char *)*512);

    memset(commond,'\0',strlen(commond));

    sprintf(commond,"AT+CMGD=%s\r",(char *)data);
    blueHfp->lock = FALSE;

    int size = strlen(commond);
    if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
        printf("delete sms for sim\n");
    }
    printf("%s\n",commond);

    free(commond);
    commond = NULL;

    blueHfp->lock = TRUE;

	return FALSE;
}

gboolean blue_hfp_get_at_zpas(gpointer data)
{
    char *commond = "AT+ZPAS?\r";
    int ret = strlen(commond);

    if(ret == write(blueHfp->serial_fd,commond,strlen(commond)))
    {
    }

    return TRUE;
}


gboolean blue_hfp_process_string_timeout(gpointer data)
{
    if(g_queue_is_empty(smsQueue))
    {
        printf("smsQueue is empty\n");
        g_source_remove(process_id);
        process_id = 0;

        char *commond = "AT+CMGD=1,3\r";

        int size = strlen(commond);

        if(size == write(blueHfp->serial_fd,commond,strlen(commond)))
        {
            printf("read sms  success\n");
        }

    }
    else
    {

        /*printf("-----------------------\n");
        gpointer str= g_queue_pop_head(smsQueue);

        //char *tmp= blue_utils_get_sms_msg((char *)str);

        printf("*********************%s\n",tmp);

        if(tmp !=NULL)
        {
            if(blueHfp->hfpCallback)
            {
                (blueHfp->hfpCallback)(SMMT_EVENT,NULL,(int)tmp);
            }
        }
		*/
    }



    return TRUE;
}

void test()
{
}
