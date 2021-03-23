

enum __AppLanguage
{
    EnglishMode=0,
    ChineseSimpleMode
};


enum __TemperatureMode
{
    DegreeCelsiusMode = 0,
    DegreeKelvinMode
};


typedef enum __TftShowMode
{
    LogoMode1=0,
    LogoMode2,
    ClockMode1,
    ClockMode2,
    ClockMode3,
    ClockMode4,
    ClockMode5,
    ClockModeUser,
    ThemesMode1,
    ThemesMode2,
    ThemesMode3,
    ThemesMode4,
    ThemesModeUser,
    SettingMode
}TftMode;


typedef enum __ParaStatus
{
    StatOff=0,
    StatShowOnly,
    StatShow,
    StatHighlight,
    StatFocus,
    StatSelected,
    StatMax
}ParaStat;

typedef struct __Point
{
    uint16_t x;
    uint16_t y;
}TftPoint;

typedef struct __ParaValue
{
    int32_t ParaNow;
    int32_t ParaMin;
    int32_t ParaMax;
    int32_t ParaDivisor;//para value = ParaNow / ParaDivisor
}ParaValue;


typedef struct __ParaInfo
{
    ParaValue ParaData;
    ParaStat ParaStatus;
    const char *ParaRegAddr;
    TftPoint ParaPoint;
    const char *ParaRegShowStr;
    uint8_t ParaFontSize;
}ParaInfo;

ParaInfo RealTempture,RealVoltageBattery,RealVoltageOut;

typedef struct __UserConfig
{
    uint8_t UserMode;
    uint8_t ClockMode;
    uint8_t AppLanguage;
    uint8_t TemperatureMode;
}UserConfig;

TftMode TftShowMode,TftShowModeLast,TftShowModeReturn;
uint8_t TemperatureNow;
uint16_t PowerNow,VoltageBattery,VoltageOutput;
uint16_t JouleData;
uint8_t HeatingWireType;
uint16_t TcrValue,RcrValue,RresetValue;

uint8_t FwUpdateReady;

//Menu[0]:power on logo; Menu[1]:Clock Mode; Menu[2]:Function Mode;
//Menu[3]-[7]:setting mode
uint8_t Menu[8];

