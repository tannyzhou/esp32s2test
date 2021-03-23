
#define KeyUpPin (13)
#define KeyDownPin (9)
#define KeyLeftPin (11)
#define KeyRightPin (10)
#define KeyCenterPin (12)
#define KeyFirePin (46)

enum __KeyStatus{
    KeyRelease=0,
    KeyShortpressRelease,
    KeyLongpressRelease,
    KeyShortpress,
    KeyLongpress
};

typedef struct __KeyInfo{
    unsigned char KeyPin;
    unsigned char KeyStatus;
    unsigned char KeyPressCount;
    int KeyCount;
}KeyInfo;

extern KeyInfo KeyIn[6];

void KeyInit(void);
