#include "jy61.h"
#include "delay.h"
#include "string.h"
#include "usart.h"

unsigned char sign,Re_buf[11],counter=0; 
static unsigned char Temp_date[11];
u16 SendData[3]={0xFF,0xAA,0x52},len;
unsigned char Temp[11];
float acc[3],speed[3],angle[3],T;//最终三轴角度信号放在angle[3]中，加速度在a[]中,速度在 speed[]中
void uart3_init(u32 bound)   //初始化 配置USART2
{
	GPIO_InitTypeDef    GPIO_InitStructure;	   //串口端口配置结构体变量
	USART_InitTypeDef   USART_InitStructure;   //串口参数配置结构体变量
	NVIC_InitTypeDef    NVIC_InitStructure; 
	
	//使能 USART3 时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);	//打开串口复用时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);   //打开PA端口时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	
	//将USART3 Tx（发送脚）的GPIO配置为推挽复用模式   
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;				  //选定哪个IO口 现选定PA2
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;           //复用推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		  //设定IO口的输出速度为50MHz
	GPIO_Init(GPIOB, &GPIO_InitStructure);    				  //初始化GPIOA

	//将USART3 Rx（接收脚）的GPIO配置为浮空输入模式														  
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;				  //选定哪个IO口 现选定PA3
//	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;	  //浮空输入
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;	//上拉输入
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	//初始化GPIOA
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;			   //指定中断源
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;	   //抢占优先级1
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;             //指定响应优先级别1
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;	               //打开中断
	NVIC_Init(&NVIC_InitStructure);	
	
	//配置USART2参数
	USART_InitStructure.USART_BaudRate = bound;	                    //波特率设置：115200
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;	    //数据位数设置：8位
	USART_InitStructure.USART_StopBits = USART_StopBits_1; 	        //停止位设置：1位
	USART_InitStructure.USART_Parity = USART_Parity_No ;            //是否奇偶校验：无
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;	//硬件流控制模式设置：没有使能
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; //接收与发送都使能
	USART_Init(USART3, &USART_InitStructure);                       //初始化USART3
	
	//打开发送中断和接收中断(如果需要中断)
	//USART_ITConfig(USART3, USART_IT_TXE, ENABLE);  // 使能指定的USART2发送中断
  	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE); // 使能指定的USART2接收中断

	//使能 USART3， 配置完毕
	USART_Cmd(USART3, ENABLE);                             // USART2使能

	//如下语句解决第1个字节无法正确发送出去的问题
    USART_ClearFlag(USART3, USART_FLAG_TC);                 //清串口2发送标志
}


void USART3_IRQHandler(void)
{
   if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)  //接收中断有效,若接收数据寄存器满
     {
      Temp_date[counter] = USART_ReceiveData(USART3);   //接收数据

	  if(counter == 0 && Temp_date[0] != 0x55) return;      //第 0 号数据不是帧头，跳过
      counter++; 
      if(counter==11) //接收到 11 个数据
      { 
         memcpy(Re_buf,Temp_date,11);
         counter=0; //重新赋值，准备下一帧数据的接收
         sign=1;
      }    
   }

}
void z_axis_calibration(void)//z轴角度归零=将当前位置设为0，需要就引用
{
	USART_ClearFlag(USART3,USART_FLAG_TC);//避免丢失第一个数据
	for(len=0;len<3;len++)
	{
	 	USART_SendData(USART3,SendData[len] );//向串口1发送数据SendData[t]
		while(USART_GetFlagStatus(USART3,USART_FLAG_TC)!=SET);//等待发送结束	 
	}
}
void imu_date(void)//显示欧拉角和3轴速度
{
      if(sign)
      {  
         memcpy(Temp,Re_buf,11);
         sign=0;
         if(Re_buf[0]==0x55)       //检查帧头
         {  
            switch(Re_buf[1])
            {
               case 0x51: //标识这个包是加速度包
                  acc[0] = ((short)(Temp[3]<<8 | Temp[2]))/32768.0*16;      //X轴加速度
                  acc[1] = ((short)(Temp[5]<<8 | Temp[4]))/32768.0*16;      //Y轴加速度
                  acc[2] = ((short)(Temp[7]<<8 | Temp[6]))/32768.0*16;      //Z轴加速度
                  T    = ((short)(Temp[9]<<8 | Temp[8]))/340.0+36.25;      //温度
                  break;
               case 0x52: //标识这个包是角速度包
                  speed[0] = ((short)(Temp[3]<<8| Temp[2]))/32768.0*2000;      //X轴角速度
                  speed[1] = ((short)(Temp[5]<<8| Temp[4]))/32768.0*2000;      //Y轴角速度
                  speed[2] = ((short)(Temp[7]<<8| Temp[6]))/32768.0*2000;      //Z轴角速度
                  T    = ((short)(Temp[9]<<8| Temp[8]))/340.0+36.25;      //温度
                  break;
               case 0x53: //标识这个包是角度包
                  angle[0] = ((short)(Temp[3]<<8| Temp[2]))/32768.0*180;   //X轴滚转角（x 轴）
                  angle[1] = ((short)(Temp[5]<<8| Temp[4]))/32768.0*180;   //Y轴俯仰角（y 轴）
                  angle[2] = ((short)(Temp[7]<<8| Temp[6]))/32768.0*180;   //Z轴偏航角（z 轴）
                  T        = ((short)(Temp[9]<<8| Temp[8]))/340.0+36.25;   //温度
                  break;
               default:  break;
            }
//			printf("X角度：%.2f  Y角度：%.2f  Z角度：%.2f  X速度：%.2f  Y速度：%.2f  Z速度：%.2f\r\n",angle[0],angle[1],angle[2],speed[0],speed[1],speed[2]);
         }      
      }
}

