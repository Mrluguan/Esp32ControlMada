#include <EPD_display.h>

int BUSY_Pin = A14;
int RES_Pin = A15;
int DC_Pin = A16;
int CS_Pin = A17;
int SCK_Pin = A18;
int SDI_Pin = A19;
unsigned char HRES_byte1, HRES_byte2, VRES_byte1, VRES_byte2;
void driver_delay_us(unsigned int xus) //1us
{
  for (; xus > 1; xus--)
    ;
}
void driver_delay_xms(unsigned long xms) //1ms
{
  unsigned long i = 0, j = 0;

  for (j = 0; j < xms; j++)
  {
    for (i = 0; i < 256; i++)
      ;
  }
}
void DELAY_S(unsigned int delaytime)
{
  int i, j, k;
  for (i = 0; i < delaytime; i++)
  {
    for (j = 0; j < 4000; j++)
    {
      for (k = 0; k < 222; k++)
        ;
    }
  }
}

void SPI_Delay(unsigned char xrate)
{
  unsigned char i;
  while (xrate)
  {
    for (i = 0; i < 2; i++)
      ;
    xrate--;
  }
}

void SPI_Write(unsigned char value)
{
  unsigned char i;
  SPI_Delay(1);
  for (i = 0; i < 8; i++)
  {
    EPD_W21_CLK_0;
    SPI_Delay(1);
    if (value & 0x80)
      EPD_W21_MOSI_1;
    else
      EPD_W21_MOSI_0;
    value = (value << 1);
    SPI_Delay(1);
    driver_delay_us(1);
    EPD_W21_CLK_1;
    SPI_Delay(1);
  }
}

void EPD_W21_WriteCMD(unsigned char command)
{
  SPI_Delay(1);
  EPD_W21_CS_0;
  EPD_W21_DC_0; // command write
  SPI_Write(command);
  EPD_W21_CS_1;
}
void EPD_W21_WriteDATA(unsigned char command)
{
  SPI_Delay(1);
  EPD_W21_CS_0;
  EPD_W21_DC_1; // command write
  SPI_Write(command);
  EPD_W21_CS_1;
}

void EPD_W21_Init(void)
{
  EPD_W21_RST_0; // Module reset
  delay(100);    //At least 10ms
  EPD_W21_RST_1;
  delay(100);
}
void EPD_init(void)
{
  unsigned char HRES_byte1 = 0x03; //800
  unsigned char HRES_byte2 = 0x20;
  unsigned char VRES_byte1 = 0x01; //480
  unsigned char VRES_byte2 = 0xE0;

  EPD_W21_Init();

  EPD_W21_WriteCMD(0x01); //POWER SETTING
  EPD_W21_WriteDATA(0x07);
  EPD_W21_WriteDATA(0x07); //VGH=20V,VGL=-20V
  EPD_W21_WriteDATA(0x3f); //VDH=15V
  EPD_W21_WriteDATA(0x3f); //VDL=-15V

  EPD_W21_WriteCMD(0x04); //Power on
  lcd_chkstatus();        //waiting for the electronic paper IC to release the idle signal

  EPD_W21_WriteCMD(0X00);  //PANNEL SETTING
  EPD_W21_WriteDATA(0x0F); //KW-3f   KWR-2F BWROTP 0f BWOTP 1f

  EPD_W21_WriteCMD(0x61);        //tres
  EPD_W21_WriteDATA(HRES_byte1); //source 800
  EPD_W21_WriteDATA(HRES_byte2);
  EPD_W21_WriteDATA(VRES_byte1); //gate 480
  EPD_W21_WriteDATA(VRES_byte2);

  EPD_W21_WriteCMD(0X15);
  EPD_W21_WriteDATA(0x00);

  EPD_W21_WriteCMD(0X50); //VCOM AND DATA INTERVAL SETTING
  EPD_W21_WriteDATA(0x11);
  EPD_W21_WriteDATA(0x07);

  EPD_W21_WriteCMD(0X60); //TCON SETTING
  EPD_W21_WriteDATA(0x22);
}
void EPD_refresh(void)
{
  EPD_W21_WriteCMD(0x12); //DISPLAY REFRESH
  driver_delay_xms(100);  //!!!The delay here is necessary, 200uS at least!!!
  lcd_chkstatus();
}
void EPD_sleep(void)
{
  EPD_W21_WriteCMD(0X50); //VCOM AND DATA INTERVAL SETTING
  EPD_W21_WriteDATA(0xf7);

  EPD_W21_WriteCMD(0X02); //power off
  lcd_chkstatus();
  EPD_W21_WriteCMD(0X07); //deep sleep
  EPD_W21_WriteDATA(0xA5);
}

void PIC_display1(unsigned char *p)
{
  unsigned int i;
  EPD_W21_WriteCMD(0x10); //Transfer old data
  for (i = 0; i < 48000; i++)
    EPD_W21_WriteDATA(0xff); //white

  EPD_W21_WriteCMD(0x13); //Transfer new data
  for (i = 0; i < 20000; i++)
    EPD_W21_WriteDATA(pgm_read_byte(p[i])); //red
  for (i = 0; i < 48000 - 20000; i++)
    EPD_W21_WriteDATA(0x00); //white
}

void PIC_display_start()
{
  unsigned int i;
  EPD_W21_WriteCMD(0x10); //Transfer old data
  for (i = 0; i < 48000; i++)
    EPD_W21_WriteDATA(0xff); //white

  EPD_W21_WriteCMD(0x13); //Transfer new data
}

void PIC_display_part(unsigned char *p,int pos,int length)
{
  unsigned int i;
  for (i = pos; i < length; i++)
    EPD_W21_WriteDATA(pgm_read_byte(p[i])); //red
}

void PIC_display_end()
{
  for (unsigned int i = 0; i < 48000 - 20000; i++)
    EPD_W21_WriteDATA(0x00); //white
}

void PIC_display_Clean(void)
{
  unsigned int i;
  EPD_W21_WriteCMD(0x10); //Transfer old data
  for (i = 0; i < 48000; i++)
  {
    EPD_W21_WriteDATA(0xff);
  }

  EPD_W21_WriteCMD(0x13); //Transfer new data
  for (i = 0; i < 48000; i++)
  {
    EPD_W21_WriteDATA(0x00);
  }
}
void lcd_chkstatus(void)
{
  unsigned char busy;
  do
  {
    EPD_W21_WriteCMD(0x71);
    busy = isEPD_W21_BUSY;
    busy = !(busy & 0x01);
  } while (busy);
  driver_delay_xms(200);
}