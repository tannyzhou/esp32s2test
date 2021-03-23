#include <stdio.h>
#include <string.h>
#include <time.h>

#include "RTC_GMT.h"
 
/*计算当前时间到格林威治时间总共过了多少秒，以当前北京地区东八区时间为准*/
unsigned long mktime_second(unsigned int year0,unsigned int mon0,unsigned int day,unsigned int hour,unsigned int min,unsigned int sec)
{
	int leap_years = 0;
	unsigned long days = 0;
	unsigned long seconds = 0;
	unsigned long resultValue = 0;
	int i = 0;
	int year = year0 - 1 ;
	int TIME_ZONE	= 8;//用于表示当前时区，=8表示北京时区东八区，单位小时，因为比本初子午线时间快8个小时
	//              			     1,  2, 3, 4, 5, 6, 7, 8, 9, 10,11,12
	const int month_days[] = {31, 28, 31,30,31, 30,31,31, 30,31,30,31};
	int isleapyear = 0;
    
	leap_years = year/4 - year/100;//计算普通闰年
	leap_years += year/400;//加上世纪闰年
	//闰年为366天，平年为365天
	days = year * 365 + leap_years;//如果当前年份是2000年，则到此便计算出了从公元0年初到1999年尾的天数
	
	//今年是否是闰年
	if((year0%4 == 0 && year0 % 100!=0) || year0%400==0) isleapyear = 1;//今年是闰年
	//按平年计算，到上个月为止总共度过的天数
	for(i=0;i<mon0 - 1;i++) days += month_days[i];
	if(mon0 >2) days +=isleapyear;//2月份闰年要按29天计算
	days= days + day - 1;
    
	//days应该减去1970年以前的天数,1970/1/1 0:0:0 0
	//year = 1969 	leap_years = 1969/4-1969/100 + 1969/400 = 492 - 19 + 4 = 477
	//isleapyear = 0
	//days = 1969 * 365 + 477 = 719162
 
	//考虑到时区的问题，实际秒钟数据应该在当前小时的基础之上加上时区时间TIME_ZONE
	//即在北京时间东八区，实际应该计算当前时间到1970/1/1 08:0:0 0的秒钟数
	//即 seconds = 8 * 60 * 60
     
	seconds = (hour) * 60 * 60 + (min) * 60 + sec;
	resultValue = (days - 719162) * 24 * 60 * 60;
	resultValue	+= seconds;
	resultValue -= ((unsigned long)TIME_ZONE)*60*60;
	
	return resultValue;
}
 
 
/*通过格林威治时间，计算本地时间*/
int GMT_toLocalTime(unsigned long gmt_time,unsigned int* year,unsigned int* month,unsigned int* day,unsigned int* hour,unsigned int* minute,unsigned int* sec)
{
	int TIME_ZONE	= 8;
	unsigned long gmtTime = gmt_time + TIME_ZONE * 60 * 60;
	int leap_years = 0;
	int month_days[] = {31, 28, 31,30,31, 30,31,31, 30,31,30,31};
	int i =0;
	int days;
			
	*sec = (int)(gmtTime%60);//秒钟数
	gmtTime = gmtTime/60;//总共有多少分钟
 
	*minute = (int)(gmtTime%60);
	gmtTime = gmtTime/60;//总共有多少小时
	
	*hour = (int)(gmtTime%24);
	gmtTime = gmtTime/24;//总共有多少天
	
	//去掉小时分钟秒钟后，转换成从公元元年开始到现在的天数 
	//不包括今天
	gmtTime += 719162;
	首先不考虑闰年计算年份和天数
	计算年份
	*year = (int)(gmtTime/365);
	days = （从公元元年开始到year的闰年个数 + 当前年份已经度过的天数）除以365后的余数
	days = (int)(gmtTime%365);
	while(1)
	{
		//总共有多少个闰年，天数要相应的减去这些天数
		leap_years = (*year)/4 - (*year)/100; //计算普通闰年
		leap_years += (*year)/400; //加上世纪闰年
		if(days < leap_years)
		{
			days+=365;
			(*year)--;
		}else break;
	}
	days -= leap_years;
	(*year)++;
	days++;
	//计算今年总共度过了多少秒
	if(((*year)%4 == 0 && (*year) % 100!=0) || (*year)%400==0) month_days[1] = 29;//今年是闰年,修改二月份为29天
	*month = 1;
	for(i=0;i<12;i++)
	{
		if(days <= month_days[i])
		{
			break;
		}
		else
		{
			days -=month_days[i];
			(*month)++; 
		}
	}
	*day =days; 
	return 0;
}
/*格林威治时间就是1970年01月01日00时00分00秒起至现在的总秒数*/
int Test_main()
{
	LocalTimeInfo LocalTime;
	unsigned long gmt_time=1552288600;  // 2019/3/11 15:16:40
	time_t seconds=1552288600;
	struct tm *gm_date;
	printf("方法一：调用自己写的计算算法：\n");
	/*调用自己的转换函数 格林威治时间 -> 本地时间*/
	GMT_toLocalTime(gmt_time,&LocalTime.year,&LocalTime.month,&LocalTime.day,&LocalTime.hour,&LocalTime.minute,&LocalTime.second);				
	printf("GMT_Time Input=%ld\n",gmt_time);
	/*打印本地时间*/
	printf("MyLocalTime=%d-%d-%d %d:%d:%d\n",LocalTime.year,LocalTime.month,LocalTime.day,LocalTime.hour,LocalTime.minute,LocalTime.second); 										 
	/*调用自己的转换函数 本地时间 -> 格林威治时间 */										 
	printf("MyLocalTime To GMT_Time=%ld\n",mktime_second(LocalTime.year,LocalTime.month,LocalTime.day,LocalTime.hour,LocalTime.minute,LocalTime.second));
 
	printf("方法二：调用库函数计算算法：\n");									 
	/*调用库转换函数 格林威治时间 -> 本地时间   */
	gm_date=localtime(&seconds);
	//库函数为了好计算就是从1900年开始计算 GMT+8就是北京时间,stm32上要加8，中文linux不需要。
	printf("SysLocalTime=%d-%d-%d %d:%d:%d\n",gm_date->tm_year+1900,gm_date->tm_mon+1, gm_date->tm_mday,gm_date->tm_hour,  gm_date->tm_min,gm_date->tm_sec);
												
	/*调用库转换函数 本地时间 -> 格林威治时间 */								   
	printf("SysLocalTime To LocalTime=%ld\n",mktime(gm_date));
}
