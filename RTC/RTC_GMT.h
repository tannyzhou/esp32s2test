
typedef struct LocalTimeInformation
{
	unsigned int year;
	unsigned int month;
	unsigned int day;
	unsigned int hour;
	unsigned int minute;
	unsigned int second;
}LocalTimeInfo;
 
unsigned long mktime_second(unsigned int year0,unsigned int mon0,unsigned int day,unsigned int hour,unsigned int min,unsigned int sec);
int GMT_toLocalTime(unsigned long gmt_time,unsigned int* year,unsigned int* month,unsigned int* day,unsigned int* hour,unsigned int* minute,unsigned int* sec);
