
static void send_datas(int xStart, int yStart, int xStop, int yStop, uint16_t *linedata);
static void send_line_finish();
void TftDrawPoint(uint16_t x, uint16_t y, uint16_t PointColor);
void TftDrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t PointColor);
void TftReDrawLine(void);
void TftDrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2 , uint16_t PointColor);
void TftDrawCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t PointColor);
void TftDisplay(int xStart, int yStart, int xStop, int yStop, uint8_t *FrameData);
void TftSetBackImg(uint8_t *bImage);
void TftClearArea(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t PointColor);
void TftDisplayAsciiNoBackcolor(int x,int y,unsigned char num, uint16_t PointColor,unsigned char NumSize);
void TftDisplayAscii(int x,int y,unsigned char num, uint16_t PointColor, uint16_t BackColor,unsigned char NumSize);
void TftDisplayStringNoBackcolor(unsigned char x,unsigned char y,unsigned char *StringData, uint16_t PointColor,unsigned char NumSize);
void TftDisplayString(unsigned char x,unsigned char y,unsigned char *StringData, uint16_t PointColor, uint16_t BackColor,unsigned char NumSize);
void TftDrawHandClear(int hhour,int mmin,int ssec,int i);
void TftDrawHand(int hhour,int mmin,int ssec);
void TftDrawHandClearS();
void TftDrawHandS(int ssec, uint16_t CollorSet);
void TftDrawClock(int hhour,int mmin);
void TftLedSpiInit(void *pvParameters);

void TftDisplay_main(void);

#define WHITE 0xffff
#define BLACK 0x0000
#define RED 0xf800
#define ORANGE 0xfc00
#define YELLOW 0xffe0
#define GREEN 0x07e0
#define BLUE 0x001f
#define PURPLE 0xd21c
#define GRAY_FRDNCH 0x2104
#define GRAY 0x8410
