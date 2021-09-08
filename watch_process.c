/**
  ******************************************************************************
  * @file    watch_process.c 
  * @author  
  * @version V1.0
  * @date    2015-12-18
  * @brief   
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/select.h>
#include <stdlib.h>
#include <fcntl.h>  
#include <sys/stat.h>
#include <mqueue.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/ioctl.h>
#include <ctype.h>


#include "../../include/pid_list.h"
#include "../../include/common.h"

#include "libcommon.h"


/* Private define ------------------------------------------------------------*/
#define MAX_WATCH_PROCESS 				20
#define MAX_WP_FILE_SIZE 				(500*1024)
#define DEFAULT_WP_CHECK_INTERVAL 		5

#define PID_NAME_MAX_LEN				64
#define EXEC_CMD_MAX_LEN				64

/* Private typedef -----------------------------------------------------------*/
typedef struct
{
    int valid;
    char process[PID_NAME_MAX_LEN];
    char cmd[EXEC_CMD_MAX_LEN];
} wpcmd_def;

typedef struct
{
    int check_interval;
    int cmd_cnt;
    wpcmd_def wp_cmd[MAX_WATCH_PROCESS];
} wp_def;

mmap_info_s *mmap_info = NULL;

wp_def g_wp;

static char* trans_wp_info(char* result, char* key, char* info)
{
    char* found=NULL;
    //dump_string(_F_, _FU_, _L_, "info=%s;key=%s\n", info, key);
    found = strstr(info, key);

    if(NULL!=found)
    {
        //dump_string(_F_, _FU_, _L_, "res=%s\n", found+strlen(key));
        sscanf(found+strlen(key), "[%[^]]", result);
        //dump_string(_F_, _FU_, _L_, "trans_json result=%s\n", result);
    }

    return found;
}

static int get_watch_info()
{
    FILE* fd=NULL;
    int i;
    int ret = -1;
        
    fd = fopen("/home/app/wp_cmd", "r");
    if(NULL!=fd)
    {
        int size;
        char* ptr = NULL;
        
        size = filesize(fd);

        if(size < MAX_WP_FILE_SIZE)
        {
            char info[512] = {0};
            char* tmpptr = NULL;
            
            ptr = (char*)malloc(size+16);

            if(NULL!=ptr)
            {
                /*将文件设为最开始*/
                fseek(fd, 0, SEEK_SET);
                fread(ptr, size+16, sizeof(char), fd);
                fclose(fd);

                tmpptr = ptr;
                
                if(NULL!=(tmpptr=trans_wp_info(info, "check_interval:", tmpptr)))
                {
                    g_wp.check_interval = atoi(info);
                }
                else
                {
                    g_wp.check_interval = DEFAULT_WP_CHECK_INTERVAL;
                }
                dump_string(_F_, _FU_, _L_, "check_interval=%d\n", g_wp.check_interval);
                
                for(i=0;i<MAX_WATCH_PROCESS;i++)
                {
                    if(NULL==tmpptr)
                    {
                        break;
                    }
                    else
                    {
                        if(NULL!=(tmpptr=trans_wp_info(info, "process:", tmpptr)))
                        {
                            snprintf(g_wp.wp_cmd[i].process, sizeof(g_wp.wp_cmd[i].process), "%s", info);
                            if(NULL!=(tmpptr=trans_wp_info(info, "cmd:", tmpptr)))
                            {
                                snprintf(g_wp.wp_cmd[i].cmd, sizeof(g_wp.wp_cmd[i].cmd), "%s", info);
                                g_wp.wp_cmd[i].valid = 1;
                                g_wp.cmd_cnt ++;
                                dump_string(_F_, _FU_, _L_, "process=%s;cmd=%s;\n", g_wp.wp_cmd[i].process, g_wp.wp_cmd[i].cmd);
                            }
                        }
                    }
                }
                
                free(ptr);
                ret = 1;
            }
        }
    }
    else
    {
        dump_string(_F_, _FU_, _L_, "no /home/app/wp_cmd\n");
    }
    
    return ret;
}

static int check_watch_info(int fd)
{
    int i;
	int ret = -1;

    for(i=0;i<MAX_WATCH_PROCESS;i++)
    {
        if(1==g_wp.wp_cmd[i].valid)
        {
			ret = ioctl(fd, IS_CONTAIN_PID, g_wp.wp_cmd[i].process);
			if (PID_TRUE == ret)
			{
				//dump_string(_F_, _FU_, _L_, ">>pid list contain %s\n", g_wp.wp_cmd[i].process);
			}
			else
			{
				int exe_fun = 1;
				
				dump_string(_F_, _FU_, _L_, "%s crashed!", g_wp.wp_cmd[i].process);
				if(0==strncmp(g_wp.wp_cmd[i].cmd, "reboot", 6))
				{
					char buf_string[512] = {0};
					//(void)system_cmd_withret_timeout("killall recbackup;killall mp4record;", buf_string, sizeof(buf_string), 60);
					(void)system_cmd_withret_timeout("killall mp4record;", buf_string, sizeof(buf_string), 60);
					(void)system_cmd_withret_timeout("/home/app/localbin/reboot_by_watchdog", buf_string, sizeof(buf_string), 0);
					//(void)system_cmd_withret_timeout("umount -l /tmp/cifs", buf_string, sizeof(buf_string), 60);
					//sleep(1);
					//(void)system_cmd_withret_timeout("reboot -f", buf_string, sizeof(buf_string), 60);
				}

				if(0==strncmp(g_wp.wp_cmd[i].process, "cloud", strlen("cloud")))
				{
					if((NULL!=mmap_info)&&0==mmap_info->wifi_connected)
					{
						exe_fun = 0;
					}
				}
				else if(0==strncmp(g_wp.wp_cmd[i].process, "p2p_tnp", strlen("p2p_tnp")))
				{
					if((NULL!=mmap_info)&&0==mmap_info->wifi_connected)
					{
						exe_fun = 0;
					}
				}
				
				if((1==exe_fun)&&(strlen(g_wp.wp_cmd[i].cmd) > 0))
				{
					system(g_wp.wp_cmd[i].cmd);
				}
			}
        }
    }
	
    return 0;
}

int main(int argc, char **argv)
{
	int fd = -1;
	memset_s(&g_wp, sizeof(g_wp), 0, sizeof(g_wp));
	
	if(access("/dev/pid_list", F_OK) != 0)
	{
		char buf_string[512] = {0};
		(void)system_cmd_withret_timeout("insmod /home/app/localko/pid_list.ko", buf_string, sizeof(buf_string), 0);
		ms_sleep(1000);
	}

    mmap_info = (mmap_info_s *)get_sharemem(MMAP_FILE_NAME, sizeof(mmap_info_s));
    if (NULL == mmap_info)
    {
        dump_string(_F_, _FU_, _L_, "get_sharemem MMAP_FILE_NAME fail");
        //return -1;
    }

	if(access("/dev/pid_list", F_OK) == 0)
	{
		fd = open( "/dev/pid_list", O_RDWR );
		if (fd<0)
		{
			dump_string(_F_, _FU_, _L_, "Open /dev/pid_list error!\n");
			return -1;
		}
		get_watch_info();

        sleep(10);  /*刚刚启动的时候不需要检测*/
		
        for(;;)
        {
            check_watch_info(fd);
            sleep(10);
        }
	}
	else
	{
		dump_string(_F_, _FU_, _L_, "init fail");
	}

    return 0;
}



