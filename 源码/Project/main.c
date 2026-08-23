/******************** (C) BOBO   ********************************
 * 文件名  ：main.c
 * 描述    ：主要是完成BMS相关检测和保护
 * 库版本  ：V3.50
 * 作者    ：BOBO
 * 版本更新: 2019-04-12
 * 调试方式：J-Link
**********************************************************************************/

//头文件
#include "stm32f10x.h"
#include "led.h"
#include "wdg.h"
#include "SYSTICK.h"
#include "usart.h"
#include "usart2.h"
#include "i2c1.h"
#include "BQ76930.h"
#include "IO_CTRL.h"
#include <stdio.h>
#include "math.h"
#include "timer.h"
#include "stm32f10x_it.h"
#include "can.h"
/**
  * @file   main
  * @brief  Main program.
  * @param  None
  * @retval None
  */
	
	
	

extern unsigned char ucUSART1_ReceiveDataBuffer[];
unsigned char BMS_DATA_FLAG;
void RECEICE_DATA_DEAL(void)
{
	
if( Get_USART1_StopFlag() == USART1_STOP_TRUE)  //????????????
  {
		
		if((ucUSART1_ReceiveDataBuffer[0] ==0X01) && (ucUSART1_ReceiveDataBuffer[1] ==0X02)&& (ucUSART1_ReceiveDataBuffer[2] ==0X55))
		 {
       LEDXToggle(5);
			BMS_DATA_FLAG=1;				
		 }
		 if((ucUSART1_ReceiveDataBuffer[0] ==0X01) && (ucUSART1_ReceiveDataBuffer[1] ==0X03)&& (ucUSART1_ReceiveDataBuffer[2] ==0X55))
		 {
       LEDXToggle(5);
			BMS_DATA_FLAG=0;				
		 }
		 if((ucUSART1_ReceiveDataBuffer[0] ==0X01) && (ucUSART1_ReceiveDataBuffer[1] ==0X04)&& (ucUSART1_ReceiveDataBuffer[2] ==0X55))
		 {
       LEDXToggle(5);
			Only_Open_DSG	();		
		 }
		 if((ucUSART1_ReceiveDataBuffer[0] ==0X01) && (ucUSART1_ReceiveDataBuffer[1] ==0X05)&& (ucUSART1_ReceiveDataBuffer[2] ==0X55))
		 {
       LEDXToggle(5);
			Only_Close_DSG();			
		 }
		 if((ucUSART1_ReceiveDataBuffer[0] ==0X01) && (ucUSART1_ReceiveDataBuffer[1] ==0X06)&& (ucUSART1_ReceiveDataBuffer[2] ==0X55))
		 {
       LEDXToggle(5);
			Only_Open_CHG	();		
		 }
		 if((ucUSART1_ReceiveDataBuffer[0] ==0X01) && (ucUSART1_ReceiveDataBuffer[1] ==0X07)&& (ucUSART1_ReceiveDataBuffer[2] ==0X55))
		 {
       LEDXToggle(5);
			Only_Close_CHG();			
		 }
	}
			Set_USART1_StopFlag( USART1_STOP_FALSE );
	}

	
	
	const u8 TEXT_Buffer[]={0,1};
#define SIZE sizeof(TEXT_Buffer)
	u8 datatemp[SIZE],OV_FLAG,UV_FLAG,OC_FLAG,temp_up;
	u32 FLASH_SIZE = 16*1024*1024;
extern int Batteryval[50];
int main(void)
{
    SYSTICK_Init(); //系统初始化，时钟配置；
	  delay_ms(1000);
	  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); //设置NVIC中断分组2:2位抢占优先级，2位响应优先级
    
	  uart_init(115200);	 //串口初始化为115200
	  USART2_Config();    //蓝牙串口初始化为9600
    LED_GPIO_Config();//电量显示，4个LED设置；
    IO_CTRL_Config(); //系统的一些IO口设置；	   
	  I2C1_Configuration();  //BQ76930_1的IIC配置；
	  BQ76930_config();      //BQ76930的初始化，唤醒设备，OV,UV,SCD,OCD的配置；	
	  delay_ms(100);
	  PB15_ONOFF(1);	  
	  delay_ms(1000);
	  TIM2_Config(49,7199);//100mS定时器中断
	  //UartSend("BL(350);\r\n");
	  CAN_Mode_Init(CAN_SJW_1tq,CAN_BS2_8tq,CAN_BS1_9tq,4,CAN_Mode_Normal);//CAN初始化环回模式,波特率500Kbps    
	
	  UartSend("MODE_CFG(1);DIR(1);FSIMG(2097152,0,0,220,176,0);\r\n");
	  delay_ms(1000);   	
    UartSend("CLR(61);\r\n");
		IWDG_Init(6,1250);      //看门狗4S左右
 	 
    while (1)
    {	
	   IWDG_Feed();
//	   ALERT_1_Recognition();
		 RECEICE_DATA_DEAL();
//			if(BMS_DATA_FLAG==1)
//			{
		    Get_Update_Data();
			   Cell_Balance(50);           //均衡开启条件为压差大于50mV即开启；
			  LEDXToggle(5);
			//}
			if((Batteryval[0]>4200)||(Batteryval[1]>4200)||(Batteryval[4]>4200)||(Batteryval[5]>4200)||(Batteryval[6]>4200)||(Batteryval[9]>4200))
					{
						Only_Close_CHG();
						IIC1_write_one_byte_CRC(SYS_STAT,0xFF); //清除状态
						OV_FLAG=1;
					}
       if(OV_FLAG==1)
			 {
					if((Batteryval[0]<4000)&&(Batteryval[1]<4000)&&(Batteryval[4]<4000)&&(Batteryval[5]<4000)&&(Batteryval[6]<4000)&&(Batteryval[9]<4000))
					{
						Only_Open_CHG();
						IIC1_write_one_byte_CRC(SYS_STAT,0xFF); //清除状态
						OV_FLAG=0;
					}
			}
					if((Batteryval[0]<2800)||(Batteryval[1]<2800)||(Batteryval[4]<2800)||(Batteryval[5]<2800)||(Batteryval[6]<2800)||(Batteryval[9]<2800))
					{
						Only_Close_DSG();
						IIC1_write_one_byte_CRC(SYS_STAT,0xFF); //清除状态
						UV_FLAG=1;
					}
      if(UV_FLAG==1)
			{
					if((Batteryval[0]>3000)&&(Batteryval[1]>3000)&&(Batteryval[4]>3000)&&(Batteryval[5]>3000)&&(Batteryval[6]>3000)&&(Batteryval[9]>3000))
					{
						Only_Open_DSG();
						IIC1_write_one_byte_CRC(SYS_STAT,0xFF); //清除状态
						UV_FLAG=0;
					}
			}
			if(Batteryval[11]>5000)//如果电流大于5000ma，关闭充放电MOS管
			{
			      Close_DSG_CHG();
						IIC1_write_one_byte_CRC(SYS_STAT,0xFF); //清除状态
						OC_FLAG =1;
			}
			if(OC_FLAG ==1)
			{
						if(Batteryval[11]<5000)//如果电流小于5000ma，关闭充放电MOS管
					{
								Open_DSG_CHG();
								IIC1_write_one_byte_CRC(SYS_STAT,0xFF); //清除状态
								OC_FLAG =0;
					}

			}

			if(Batteryval[12]>35)//如果温度大于35，关闭充放电MOS管
			{
			      Close_DSG_CHG();
						IIC1_write_one_byte_CRC(SYS_STAT,0xFF); //清除状态
            temp_up=1;				
			}
			      if(temp_up==1)
			{
					if(Batteryval[12]<35)//如果温度小于35，关闭充放电MOS管
					{
						Open_DSG_CHG();
						IIC1_write_one_byte_CRC(SYS_STAT,0xFF); //清除状态
						 temp_up=0;
					}
			}

//									IIC1_write_one_byte_CRC(SYS_STAT,0xFF); //清除状态

		}
    
    
}




/*********************************************************************************************************
      END FILE
*********************************************************************************************************/





