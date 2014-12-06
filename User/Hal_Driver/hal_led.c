#include "hal_led.h"

//unsigned char code fseg[]={0xc0,0xf9,0xa4,0xb0,0x99,0x92,0x82,0xf8,0x80,0x90};
//unsigned char code segbit[]={0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
// unsigned char  disbuf[4]={0,0,0,0};


 //-----------------------------------------------------------------------------
// ����ԭ�ζ���
#define uchar unsigned char

//void main (void);					// ������
//void LED4_Display (void);			// LED��ʾ
void LED_OUT(uchar X);				// LED���ֽڴ�����λ����

unsigned char LED_0F[];		// LED��ģ��

// sbit DIO = P1^0;				//������������
// sbit RCLK  = P1^1;				//ʱ�������źš�����������Ч
// sbit SCLK = P1^2;				//�����źš���������������Ч

//-----------------------------------------------------------------------------
// ȫ�ֱ���
uchar LED[8];	//����LED��8λ��ʾ����


//*****************************************************************************
// ������
/*
void main (void) 
{

	LED[0]=1;
	LED[1]=2;
	LED[2]=3;
	LED[3]=4;

	while(1)
	{
		LED4_Display ();
	} 
} */


void LED_OUT(uchar X)
{
	uchar i;
	for(i=8;i>=1;i--)
	{
		if (X&0x80) DIO_HIGH; else DIO_LOW;
		X<<=1;
		SCLK_LOW;
		SCLK_HIGH;
	}
}

unsigned char LED_0F[] = 
{// 0	 1	  2	   3	4	 5	  6	   7	8	 9	  A	   b	C    d	  E    F    -
	0xC0,0xF9,0xA4,0xB0,0x99,0x92,0x82,0xF8,0x80,0x90,0x8C,0xBF,0xC6,0xA1,0x86,0xFF,0xbf
};


void LED4_Init(void)
{
	
	GPIO_InitTypeDef GPIO_InitStruct;
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA,ENABLE);
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;//GPIO_Mode_Out_OD;
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_5;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz; // GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStruct);
	
	LED4_Display(8, 8, 8, 8);
}

void LED4_Display(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4)
{
	unsigned char *led_table;          // ���ָ��
	uchar i;
	//��ʾ��1λ
	led_table = LED_0F + d1;
	i = *led_table;

	LED_OUT(i);			
	LED_OUT(0x01);		

	RCLK_LOW;
	RCLK_HIGH;
	//��ʾ��2λ
	led_table = LED_0F + d2;
	i = *led_table;

	LED_OUT(i);		
	LED_OUT(0x02);		

	RCLK_LOW;
	RCLK_HIGH;
	//��ʾ��3λ
	led_table = LED_0F + d3;
	i = *led_table;

	LED_OUT(i);			
	LED_OUT(0x04);	

	RCLK_LOW;
	RCLK_HIGH;
	//��ʾ��4λ
	led_table = LED_0F + d4;
	i = *led_table;

	LED_OUT(i);			
	LED_OUT(0x08);		

	RCLK_LOW;
	RCLK_HIGH;
}
