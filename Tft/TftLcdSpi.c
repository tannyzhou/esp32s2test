/* SPI Master example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <math.h>

#include "esp_sleep.h"
#include "../NVS/nvsUserData.h"
#include "KeyFunction.h"

//#include "pretty_effect.h"

#include "TftLcdSpi.h"
#include "img.h"
#include "font.h"

#include "../rtc/RTC.h"
#include "../LED/Led_Main.h"
#include "../AppParameter.h"

static const char *TAG = "TftLcdSpi";

#define PI 3.1415926 

/*
 This code displays some fancy graphics on the 320x240 LCD on an ESP-WROVER_KIT board.
 This example demonstrates the use of both spi_device_transmit as well as
 spi_device_queue_trans/spi_device_get_trans_result and pre-transmit callbacks.

 Some info about the ILI9341/ST7789V: It has an C/D line, which is connected to a GPIO here. It expects this
 line to be low for a command and high for data. We use a pre-transmit callback here to control that
 line: every transaction has as the user-definable argument the needed state of the D/C line and just
 before the transaction is sent, the callback will set this line to the correct state.
*/
#define spi_clock_speed_hz 40*1000*1000

#ifdef CONFIG_IDF_TARGET_ESP32
#define LCD_HOST    HSPI_HOST
#define DMA_CHAN    2

#define PIN_NUM_MISO 25
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  19
#define PIN_NUM_CS   22

#define PIN_NUM_DC   21
#define PIN_NUM_RST  18
#define PIN_NUM_BCKL 5
#elif defined CONFIG_IDF_TARGET_ESP32S2
#define LCD_HOST    SPI2_HOST
#define DMA_CHAN    LCD_HOST

#define PIN_NUM_MISO 37
#define PIN_NUM_MOSI 35
#define PIN_NUM_CLK  36
#define PIN_NUM_CS   34

#define PIN_NUM_DC   4
#define PIN_NUM_RST  5
#define PIN_NUM_BCKL 6
#endif

//To speed up transfers, every SPI transfer sends a bunch of lines. This define specifies how many. More means more memory use,
//but less overhead for setting up / finishing transfers. Make sure 240 is dividable by this.
#define PARALLEL_LINES 40//48//80

spi_device_handle_t spi;
uint8_t *FrameBackData,LineAddrLast[480][2];
int sending_line=-1,calc_line=0;
uint16_t *lines[2];

char StrTem[32];


enum {MenuNext=1,MenuBack,MenuEnter,MenuReturn};
void ChangeMenu(uint8_t Fun)//0:none; 1:+1; 2:-1; 3:enter; 4:return
{
    uint8_t i;
    for(i=7;i>=0;i--)
    {
        if(Menu[i]>0)
        {
            if(Fun==1)
            {
                if(Menu[i]<200)
                    Menu[i]++;
                else
                    Menu[i]=1;
            }
            if(Fun==2)
            {
                if(Menu[i]>1)
                    Menu[i]--;
                else
                    Menu[i]=200;
            }
            if(Fun==3 && i<7)
                Menu[i+1]=1;
            if(Fun==4)
            {
                if(i>=3)
                    Menu[i]=0;
            }
            return;
        }
    }
}

void MenuCheck(void)
{
    for(uint8_t i=0;i<6;i++)
    {
        if(KeyIn[i].KeyPin==KeyUpPin && KeyIn[i].KeyStatus==KeyShortpressRelease)
        {
            ChangeMenu(MenuBack);
            KeyIn[i].KeyStatus=KeyRelease;
            
        }
        if(KeyIn[i].KeyPin==KeyDownPin && KeyIn[i].KeyStatus==KeyShortpressRelease)
        { 
            ChangeMenu(MenuNext);
            KeyIn[i].KeyStatus=KeyRelease;
            
        }
        if(KeyIn[i].KeyPin==KeyLeftPin && KeyIn[i].KeyStatus==KeyShortpressRelease)
        { 
            ChangeMenu(MenuReturn);
            KeyIn[i].KeyStatus=KeyRelease;
        }
        if(KeyIn[i].KeyPin==KeyRightPin && KeyIn[i].KeyStatus==KeyShortpressRelease)
        { 
            KeyIn[i].KeyStatus=KeyRelease;
        }
        if(KeyIn[i].KeyPin==KeyCenterPin && KeyIn[i].KeyStatus==KeyShortpressRelease)
        {
            ChangeMenu(MenuEnter);
            KeyIn[i].KeyStatus=KeyRelease;
        }
        if(KeyIn[i].KeyPin==KeyFirePin && KeyIn[i].KeyStatus==KeyShortpressRelease)
        { 
            if(KeyIn[i].KeyPressCount>2 && KeyIn[i].KeyPressCount<5 )
            {
                if(TftShowMode == SettingMode)
                    TftShowMode = TftShowModeReturn;
                else if(TftShowMode != ClockMode1)
                    TftShowMode = ClockMode1;
                else
                    TftShowMode = ThemesMode2;
            }
            if(KeyIn[i].KeyPressCount>4)
            {
                if(TftShowMode != SettingMode)
                    TftShowModeReturn = TftShowModeLast;
                TftShowMode = SettingMode;
            }

            KeyIn[i].KeyStatus=KeyRelease;
        }
    }
}


/*
 The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct.
*/
typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

typedef enum {
    LCD_TYPE_ILI = 1,
    LCD_TYPE_ST,
    LCD_TYPE_MAX,
} type_lcd_t;

//Place data into DRAM. Constant data gets placed into DROM by default, which is not accessible by DMA.
DRAM_ATTR static const lcd_init_cmd_t st_init_cmds[]={
    /*INVOFF (20h): Display Inversion Off*/
    //{0x20, {0}, 0},
    /*INVON (21h): Display Inversion On*/
    {0x21, {0}, 0},
    /* Memory Data Access Control, MX=MV=1, MY=ML=MH=0, RGB=0 */
    {0x36, {(1<<7)|(1<<6)}, 1},
    // {0x36, {(1<<5)|(1<<6)}, 1},
    // {0x36, {0x00}, 1},
    /* Interface Pixel Format, 65K(RGB 5,6,5-bit input) 16bits/pixel for RGB/MCU interface */
    {0x3A, {0x55}, 1},
    /* Porch Setting */
    {0xB2, {0x0c, 0x0c, 0x00, 0x33, 0x33}, 5},
    /* Frame Rate Control 1 */
    //{0xB3, {0x00, 0x1f, 0x00}, 3},
    /* Gate Control, Vgh=13.65V, Vgl=-10.43V */
    {0xB7, {0x45}, 1},
    /* VCOM Setting, VCOM=1.175V */
    {0xBB, {0x2B}, 1},
    /* LCM Control, XOR: BGR, MX, MH */
    {0xC0, {0x2C}, 1},
    /* VDV and VRH Command Enable, enable=1 */
    {0xC2, {0x01, 0xff}, 2},
    /* VRH Set, Vap=4.4+... */
    {0xC3, {0x11}, 1},
    /* VDV Set, VDV=0 */
    {0xC4, {0x20}, 1},
    /* Frame Rate Control, 60Hz, inversion=0 */
    {0xC6, {0x0f}, 1},
    /* Power Control 1, AVDD=6.8V, AVCL=-4.8V, VDDS=2.3V */
    {0xD0, {0xA4, 0xA1}, 1},
    /* Positive Voltage Gamma Control */
    {0xE0, {0xD0, 0x00, 0x05, 0x0E, 0x15, 0x0D, 0x37, 0x43, 0x47, 0x09, 0x15, 0x12, 0x16, 0x19}, 14},
    /* Negative Voltage Gamma Control */
    {0xE1, {0xD0, 0x00, 0x05, 0x0D, 0x0C, 0x06, 0x2D, 0x44, 0x40, 0x0E, 0x1C, 0x18, 0x16, 0x19}, 14},
    /* Sleep Out */
    {0x11, {0}, 0x80},
    /* Display On */
    {0x29, {0}, 0x80},
    {0, {0}, 0xff}
};

DRAM_ATTR static const lcd_init_cmd_t ili_init_cmds[]={
    /* Power contorl B, power control = 0, DC_ENA = 1 */
    {0xCF, {0x00, 0x83, 0X30}, 3},
    /* Power on sequence control,
     * cp1 keeps 1 frame, 1st frame enable
     * vcl = 0, ddvdh=3, vgh=1, vgl=2
     * DDVDH_ENH=1
     */
    {0xED, {0x64, 0x03, 0X12, 0X81}, 4},
    /* Driver timing control A,
     * non-overlap=default +1
     * EQ=default - 1, CR=default
     * pre-charge=default - 1
     */
    {0xE8, {0x85, 0x01, 0x79}, 3},
    /* Power control A, Vcore=1.6V, DDVDH=5.6V */
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
    /* Pump ratio control, DDVDH=2xVCl */
    {0xF7, {0x20}, 1},
    /* Driver timing control, all=0 unit */
    {0xEA, {0x00, 0x00}, 2},
    /* Power control 1, GVDD=4.75V */
    {0xC0, {0x26}, 1},
    /* Power control 2, DDVDH=VCl*2, VGH=VCl*7, VGL=-VCl*3 */
    {0xC1, {0x11}, 1},
    /* VCOM control 1, VCOMH=4.025V, VCOML=-0.950V */
    {0xC5, {0x35, 0x3E}, 2},
    /* VCOM control 2, VCOMH=VMH-2, VCOML=VML-2 */
    {0xC7, {0xBE}, 1},
    /* Memory access contorl, MX=MY=0, MV=1, ML=0, BGR=1, MH=0 */
    {0x36, {0x28}, 1},
    /* Pixel format, 16bits/pixel for RGB/MCU interface */
    {0x3A, {0x55}, 1},
    /* Frame rate control, f=fosc, 70Hz fps */
    {0xB1, {0x00, 0x1B}, 2},
    /* Enable 3G, disabled */
    {0xF2, {0x08}, 1},
    /* Gamma set, curve 1 */
    {0x26, {0x01}, 1},
    /* Positive gamma correction */
    {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0X87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},
    /* Negative gamma correction */
    {0XE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},
    /* Column address set, SC=0, EC=0xEF */
    {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
    /* Page address set, SP=0, EP=0x013F */
    {0x2B, {0x00, 0x00, 0x01, 0x3f}, 4},
    /* Memory write */
    {0x2C, {0}, 0},
    /* Entry mode set, Low vol detect disabled, normal display */
    {0xB7, {0x07}, 1},
    /* Display function control */
    {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
    /* Sleep out */
    {0x11, {0}, 0x80},
    /* Display on */
    {0x29, {0}, 0x80},
    {0, {0}, 0xff},
};

/* Send a command to the LCD. Uses spi_device_polling_transmit, which waits
 * until the transfer is complete.
 *
 * Since command transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
void lcd_cmd(const uint8_t cmd)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&cmd;               //The data is the cmd itself
    t.user=(void*)0;                //D/C needs to be set to 0
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

/* Send data to the LCD. Uses spi_device_polling_transmit, which waits until the
 * transfer is complete.
 *
 * Since data transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
void lcd_data(const uint8_t *data, int len)
{
    esp_err_t ret;
    spi_transaction_t t;
    if (len==0) return;             //no need to send anything
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=len*8;                 //Len is in bytes, transaction length is in bits.
    t.tx_buffer=data;               //Data
    t.user=(void*)1;                //D/C needs to be set to 1
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

//This function is called (in irq context!) just before a transmission starts. It will
//set the D/C line to the value indicated in the user field.
void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc=(int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}

uint32_t lcd_get_id(spi_device_handle_t spi)
{
    //get_id cmd
    lcd_cmd(0x04);

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length=8*3;
    t.flags = SPI_TRANS_USE_RXDATA;
    t.user = (void*)1;

    esp_err_t ret = spi_device_polling_transmit(spi, &t);
    assert(ret == ESP_OK );

    return *(uint32_t*)t.rx_data;
}

//Initialize the display
void lcd_init(spi_device_handle_t spi)
{
    int cmd=0;
    const lcd_init_cmd_t* lcd_init_cmds;

    //Initialize non-SPI GPIOs
    gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
    // gpio_set_direction(PIN_NUM_BCKL, GPIO_MODE_OUTPUT);

    //Reset the display
    //gpio_set_level(PIN_NUM_RST, 0);
    //vTaskDelay(100 / portTICK_RATE_MS);
    gpio_set_level(PIN_NUM_RST, 1);
    //vTaskDelay(100 / portTICK_RATE_MS);

    //detect LCD type
    int lcd_type;
    lcd_type = LCD_TYPE_ST;
  
    if (lcd_type == LCD_TYPE_ST ) {
        printf("LCD ST7789V initialization.\n");
        lcd_init_cmds = st_init_cmds;
    } else {
        printf("LCD ILI9341 initialization.\n");
        lcd_init_cmds = ili_init_cmds;
    }

    //Send all the commands
    while (lcd_init_cmds[cmd].databytes!=0xff) {
        lcd_cmd(lcd_init_cmds[cmd].cmd);
        lcd_data(lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes&0x1F);
        if (lcd_init_cmds[cmd].databytes&0x80) {
            vTaskDelay(10 / portTICK_RATE_MS);//vTaskDelay(100 / portTICK_RATE_MS);
        }
        cmd++;
    }

    // Enable backlight
    // gpio_set_level(PIN_NUM_BCKL, 1);
}


void GetLineDatas(uint16_t *dest, int line, int xLen, uint8_t *frame, int linect)
{
    for (int y=line; y<line+linect; y++) {
        for (int x=0; x<xLen; x++) {
            *dest++=(frame[(y*2*xLen)+(x*2)]<<8) +frame[(y*2*xLen)+(x*2)+1];
            //*dest++=frame[(y*240)+x];
        }
    }
}

/* To send a block of lines we have to send a command, 2 data bytes, another command, 2 more data bytes and another command
 * before sending the line data itself; a total of 6 transactions. (We can't put all of this in just one transaction
 * because the D/C line needs to be toggled in the middle.)
 * This routine queues these commands up as interrupt transactions so they get
 * sent faster (compared to calling spi_device_transmit several times), and at
 * the mean while the lines for next transactions can get calculated.
 * xpos as x start point address,
 * ypos as y start point address,
 */
static void send_datas(int xStart, int yStart, int xStop, int yStop, uint16_t *linedata)
{
    esp_err_t ret;
    int x;
    //Transaction descriptors. Declared static so they're not allocated on the stack; we need this memory even when this
    //function is finished because the SPI driver needs access to it even while we're already calculating the next line.
    static spi_transaction_t trans[6];

    if(xStop>240)
        xStop=240;
    if(yStop>240)
        yStop=240;
    if(xStart>xStop)
        xStart = xStop;
    if(yStart>yStop)
        yStart = yStop;
    yStart += 80;
    yStop += 80;
    //In theory, it's better to initialize trans and data only once and hang on to the initialized
    //variables. We allocate them on the stack, so we need to re-init them each call.
    for (x=0; x<6; x++) {
        memset(&trans[x], 0, sizeof(spi_transaction_t));
        if ((x&1)==0) {
            //Even transfers are commands
            trans[x].length=8;
            trans[x].user=(void*)0;
        } else {
            //Odd transfers are data
            trans[x].length=8*4;
            trans[x].user=(void*)1;
        }
        trans[x].flags=SPI_TRANS_USE_TXDATA;
    }
    trans[0].tx_data[0]=0x2A;           //Column Address Set
    trans[1].tx_data[0]=0;              //Start Col High
    trans[1].tx_data[1]=xStart&0xff;              //Start Col Low
    trans[1].tx_data[2]=0;       //End Col High
    trans[1].tx_data[3]=(xStop&0xff)-1;     //End Col Low
    trans[2].tx_data[0]=0x2B;           //Page address set
    trans[3].tx_data[0]=yStart>>8;        //Start page high
    trans[3].tx_data[1]=yStart&0xff;      //start page low
    trans[3].tx_data[2]=yStop>>8;    //end page high
    trans[3].tx_data[3]=(yStop&0xff)-1;  //end page low
    trans[4].tx_data[0]=0x2C;           //memory write
    trans[5].tx_buffer=linedata;        //finally send the line data
    trans[5].length=(xStop-xStart)*2*8*(yStop-yStart);          //Data length, in bits
    trans[5].flags=0; //undo SPI_TRANS_USE_TXDATA flag

    //Queue all transactions.
    for (x=0; x<6; x++) {
        ret=spi_device_queue_trans(spi, &trans[x], portMAX_DELAY);
        assert(ret==ESP_OK);
    }

}


static void send_line_finish()
{
    spi_transaction_t *rtrans;
    esp_err_t ret;
    //Wait for all 6 transactions to be done and get back the results.
    for (int x=0; x<6; x++) {
        ret=spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
        assert(ret==ESP_OK);
        //We could inspect rtrans now if we received any info back. The LCD is treated as write-only, though.
    }
}


void TftDrawPoint(uint16_t x, uint16_t y, uint16_t PointColor)
{
    uint16_t PointColorNew;
    PointColorNew = (PointColor&0xff)<<8;
    PointColorNew += PointColor>>8;
    send_line_finish(spi);
    send_datas(x,y,x+1,y+1,&PointColorNew);
}

void TftDrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t PointColor)
{
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, row, col;

    if(y1>239)
        y1=239;
    if(y2>239)
        y2=239;
    if(x1>239)
        x1=239;
    if(x2>239)
        x2=239;

    delta_x = x2 - x1;
    delta_y = y2 - y1;
    row = x1;
    col = y1;

    if(delta_x > 0)incx = 1;

    else if(delta_x == 0)incx = 0;

    else
    {
        incx = -1;
        delta_x = -delta_x;
    }

    if(delta_y > 0)incy = 1;

    else if(delta_y == 0)incy = 0;

    else
    {
        incy = -1;
        delta_y = -delta_y;
    }

    if(delta_x > delta_y)distance = delta_x;

    else distance = delta_y;

    for(t = 0; t <= distance + 1; t++)
    {
        LineAddrLast[t][0]=row&0xff;
        LineAddrLast[t][1]=col&0xff;
        TftDrawPoint(row, col,PointColor);
        xerr += delta_x ;
        yerr += delta_y ;

        if(xerr > distance)
        {
            xerr -= distance;
            row += incx;
        }

        if(yerr > distance)
        {
            yerr -= distance;
            col += incy;
        }
    }
    LineAddrLast[t][0]=255;
    LineAddrLast[t][1]=255;

}

void TftReDrawLine(void)
{
    uint16_t t;
    int row, col;
    uint16_t PointColor;

    for(t = 0; (t <480) && (LineAddrLast[t][0]<240); t++)
    {
        row = LineAddrLast[t][0];
        col = LineAddrLast[t][1];
        PointColor = (FrameBackData[col*240*2+row*2]<<8) + FrameBackData[col*240*2+row*2+1];
        TftDrawPoint(row, col,PointColor);
    }

}
void TftDrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2 , uint16_t PointColor)
{
    TftDrawLine(x1, y1, x2, y1 ,PointColor);
    TftDrawLine(x1, y1, x1, y2 ,PointColor);
    TftDrawLine(x1, y2, x2, y2 ,PointColor);
    TftDrawLine(x2, y1, x2, y2 ,PointColor);
}

void TftDrawCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t PointColor)
{
    int a, b;
    int di;
    a = 0;
    b = r;
    di = 3 - (r << 1);

    while(a <= b)
    {
        TftDrawPoint(x0 - b, y0 - a, PointColor);
        TftDrawPoint(x0 + b, y0 - a, PointColor);
        TftDrawPoint(x0 - a, y0 + b, PointColor);
        TftDrawPoint(x0 - b, y0 - a, PointColor);
        TftDrawPoint(x0 - a, y0 - b, PointColor);
        TftDrawPoint(x0 + b, y0 + a, PointColor);
        TftDrawPoint(x0 + a, y0 - b, PointColor);
        TftDrawPoint(x0 + a, y0 + b, PointColor);
        TftDrawPoint(x0 - b, y0 + a, PointColor);
        a++;

        if(di < 0)di += 4 * a + 6;
        else
        {
            di += 10 + 4 * (a - b);
            b--;
        }

        TftDrawPoint(x0 + a, y0 + b, PointColor);
    }
}


//Simple routine to generate some patterns and send them to the LCD. Don't expect anything too
//impressive. Because the SPI driver handles transactions in the background, we can calculate the next line
//while the previous one is being sent.
void TftDisplay(int xStart, int yStart, int xStop, int yStop, uint8_t *FrameData)
{
    //Allocate memory for the pixel buffers
    for (int y=yStart; y<yStop; y+=PARALLEL_LINES) {
        //Calculate a line.
        if(FrameData != NULL)
            GetLineDatas(lines[calc_line], y-yStart, xStop-xStart, FrameData, PARALLEL_LINES);
        //Finish up the sending process of the previous line, if any
        if (sending_line!=-1) send_line_finish(spi);
        //Swap sending_line and calc_line
        sending_line=calc_line;
        calc_line=(calc_line==1)?0:1;
        //Send the line we currently calculated.
        //send_lines(y, lines[sending_line]);
        if((y+PARALLEL_LINES) <= yStop)
            send_datas(xStart,y, xStop,y+PARALLEL_LINES,lines[sending_line]);
        else
            send_datas(xStart,y, xStop,yStop,lines[sending_line]);
        //The line set is queued up for sending now; the actual sending happens in the
        //background. We can go on to calculate the next line set as long as we do not
        //touch line[sending_line]; the SPI sending process is still reading from that.
        
    }
    // send_line_finish(spi);
    // send_datas(50,100, 240,120,lines[sending_line]);
}

void TftSetBackImg(uint8_t *bImage)
{
    FrameBackData = bImage;
    TftDisplay(0, 0, 240, 240, bImage);
}

void TftClearArea(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t PointColor)
{
    uint8_t TftDataTemp[480];
    uint16_t i;
    for(i=0;i<240;i++)
    {
        TftDataTemp[i*2]=PointColor;
        TftDataTemp[i*2+1]=PointColor>>8;
    }
    for(;y1<y2;y1++)
    {
        if(y1<240)
            TftDisplay(x1, y1, x2, y1+1, TftDataTemp);
    }
}

/*
NumSize = 12/16/24/32/
*/
void TftDisplayAsciiNoBackcolor(int x,int y,unsigned char num, uint16_t PointColor,unsigned char NumSize)
{

    uint16_t BackColor;
    uint32_t temp;
	uint32_t temp_one;
	uint8_t pos,t;
	//unsigned int x0=x;
	uint16_t colortemp;
    uint8_t Xlen,Ylen;
    uint8_t CharDatas[24*32*2];

    // NumSize = 16;
    switch(NumSize)
    {
        default:
            NumSize = 12;
        case 12:
            Xlen = 6;
            Ylen = 12;
        break;
        case 16:
            Xlen = 8;
            Ylen = 16;
        break;
        case 24:
            Xlen = 12;
            Ylen = 24;
        break;
        case 32:
            Xlen = 16;
            Ylen = 32;
        break;
        case NumWifi:
            Xlen = 12;
            Ylen = 24;
        break;
        case NumReg:
            Xlen = 16;
            Ylen = 16;
        break;
        case NumNieEr:
        case NumHuaShi:
            Xlen = 24;
            Ylen = 24;
        break;
        
    }

	for(pos=0;pos<Ylen;pos++)
	{ 
        switch(NumSize)
        {
            default:
            case 12:
                temp=asc2_1206[(unsigned int)(num-' ')][pos];
                temp<<=24;
            break;
            case 16:
                temp=asc2_1608[(unsigned int)(num-' ')][pos];
                temp<<=24;
            break;
            case 24:
                temp=asc2_2412[(unsigned int)(num-' ')][pos*2];
                temp<<=8;
                temp_one=asc2_2412[(unsigned int)(num-' ')][pos*2+1];
                temp|=temp_one;
                temp<<=16;
            break;
            case 32:
                temp=asc2_3216[(unsigned int)(num-' ')][pos*2];
                temp<<=8;
                temp_one=asc2_3216[(unsigned int)(num-' ')][pos*2+1];
                temp|=temp_one;
                temp<<=16;
            break;
            case NumWifi:
                temp=asc2_Wifi[pos*2];
                temp<<=8;
                temp_one=asc2_Wifi[pos*2+1];
                temp|=temp_one;
                temp<<=16;
            break;
            case NumReg:
                temp=asc2_Reg[pos*2];
                temp<<=8;
                temp_one=asc2_Reg[pos*2+1];
                temp|=temp_one;
                temp<<=16;
            break;
            case NumNieEr:
                temp=asc2_NieEr[pos*3];
                temp<<=24;
                temp_one=asc2_NieEr[pos*3+1] << 16;
                temp|=temp_one;
                temp_one=asc2_NieEr[pos*3+2] << 8;
                temp|=temp_one;
            break;
            case NumHuaShi:
                temp=asc2_HuaShi[pos*3];
                temp<<=24;
                temp_one=asc2_HuaShi[pos*3+1] << 16;
                temp|=temp_one;
                temp_one=asc2_HuaShi[pos*3+2] << 8;
                temp|=temp_one;
            break;
        }

		for(t=0;t<Xlen;t++)
		{
            BackColor = FrameBackData[(y+pos)*480+(x+t)*2+1]<<8;
            BackColor += FrameBackData[(y+pos)*480+(x+t)*2]&0xff;
			if(temp&0x80000000)
				colortemp=PointColor;
			else 
				colortemp=BackColor;
			CharDatas[((pos*Xlen)+t)*2+1]=colortemp>>8;
			CharDatas[((pos*Xlen)+t)*2]=colortemp&0xff;	
			temp<<=1; 
		}
	}

    TftDisplay(x, y, x+Xlen, y+Ylen, CharDatas);
}

/*
NumSize = 12/16/24/32/
*/
void TftDisplayAscii(int x,int y,unsigned char num, uint16_t PointColor, uint16_t BackColor,unsigned char NumSize)
{
    
    // uint16_t BackColor;
    uint32_t temp;
	uint32_t temp_one;
	uint8_t pos,t;
	//unsigned int x0=x;
	uint16_t colortemp;
    uint8_t Xlen,Ylen;
    uint8_t CharDatas[24*32*2];

    // NumSize = 16;
    switch(NumSize)
    {
        default:
            NumSize = 12;
        case 12:
            Xlen = 6;
            Ylen = 12;
        break;
        case 16:
            Xlen = 8;
            Ylen = 16;
        break;
        case 24:
            Xlen = 12;
            Ylen = 24;
        break;
        case 32:
            Xlen = 16;
            Ylen = 32;
        break;
        case NumWifi:
            Xlen = 12;
            Ylen = 24;
        break;
        case NumReg:
            Xlen = 16;
            Ylen = 16;
        break;
        case NumNieEr:
        case NumHuaShi:
            Xlen = 24;
            Ylen = 24;
        break;
        
    }

	for(pos=0;pos<Ylen;pos++)
	{ 
        switch(NumSize)
        {
            default:
            case 12:
                temp=asc2_1206[(unsigned int)(num-' ')][pos];
                temp<<=24;
            break;
            case 16:
                temp=asc2_1608[(unsigned int)(num-' ')][pos];
                temp<<=24;
            break;
            case 24:
                temp=asc2_2412[(unsigned int)(num-' ')][pos*2];
                temp<<=8;
                temp_one=asc2_2412[(unsigned int)(num-' ')][pos*2+1];
                temp|=temp_one;
                temp<<=16;
            break;
            case 32:
                temp=asc2_3216[(unsigned int)(num-' ')][pos*2];
                temp<<=8;
                temp_one=asc2_3216[(unsigned int)(num-' ')][pos*2+1];
                temp|=temp_one;
                temp<<=16;
            break;
            case NumWifi:
                temp=asc2_Wifi[pos*2];
                temp<<=8;
                temp_one=asc2_Wifi[pos*2+1];
                temp|=temp_one;
                temp<<=16;
            break;
            case NumReg:
                temp=asc2_Reg[pos*2];
                temp<<=8;
                temp_one=asc2_Reg[pos*2+1];
                temp|=temp_one;
                temp<<=16;
            break;
            case NumNieEr:
                temp=asc2_NieEr[pos*3];
                temp<<=24;
                temp_one=asc2_NieEr[pos*3+1] << 16;
                temp|=temp_one;
                temp_one=asc2_NieEr[pos*3+2] << 8;
                temp|=temp_one;
            break;
            case NumHuaShi:
                temp=asc2_HuaShi[pos*3];
                temp<<=24;
                temp_one=asc2_HuaShi[pos*3+1] << 16;
                temp|=temp_one;
                temp_one=asc2_HuaShi[pos*3+2] << 8;
                temp|=temp_one;
            break;
        }

		for(t=0;t<Xlen;t++)
		{
            // BackColor = FrameBackData[(y+pos)*480+(x+t)*2+1]<<8;
            // BackColor += FrameBackData[(y+pos)*480+(x+t)*2]&0xff;
			if(temp&0x80000000)
				colortemp=PointColor;
			else 
				colortemp=BackColor;
			CharDatas[((pos*Xlen)+t)*2+1]=colortemp>>8;
			CharDatas[((pos*Xlen)+t)*2]=colortemp&0xff;	
			temp<<=1; 
		}
	}

    TftDisplay(x, y, x+Xlen, y+Ylen, CharDatas);
}

void TftDisplayStringNoBackcolor(unsigned char x,unsigned char y,unsigned char *StringData, uint16_t PointColor,unsigned char NumSize)
{
    uint16_t BackColor;
    int xNest = x;
    for(int i=0;*(StringData+i) != NULL;i++)
    {   
        if(xNest>239-(NumSize/2))
        {
            y += NumSize;
            xNest = x;//reset x point
        }
        BackColor = FrameBackData[y*480+xNest*2+1]<<8;
        BackColor += FrameBackData[y*480+xNest*2]&0xff;
        //TftDisplayAscii(xNest,y,*(StringData+i),PointColor,BackColor);
        TftDisplayAsciiNoBackcolor(xNest,y,*(StringData+i),PointColor,NumSize);
        xNest += (NumSize/2);
        if(*(StringData+i) == '.' || *(StringData+i) == ' ')
            xNest -= (NumSize/4);
    }
}

void TftDisplayString(unsigned char x,unsigned char y,unsigned char *StringData, uint16_t PointColor, uint16_t BackColor,unsigned char NumSize)
{
    int xNest = x;
    for(int i=0;*(StringData+i) != NULL;i++)
    {   
        if(xNest>239-(NumSize/2))
        {
            y += NumSize;
            xNest = x;//reset x point
        }
        TftDisplayAscii(xNest,y,*(StringData+i),PointColor,BackColor,NumSize);
        xNest += (NumSize/2);
        if(*(StringData+i) == '.' || *(StringData+i) == ' ')
            xNest -= (NumSize/4);
    }
}

void TftDisplay_main(void)
{
    // xTaskCreate(TftLedSpiInit, "TftDisplay",4096*4, NULL,1,NULL);
}