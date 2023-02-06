#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/rtc_wdt.h"
#include "esp32-hal-cpu.h"

#define IN_1 2   //电机控制线IN1
#define IN_2 13  //电机控制线IN2
#define IN_3 14  //电机控制线IN3
#define IN_4 15  //电机控制线IN4
// #define KyMonitorControl 16 //按键开关

int control_flag=0;     //控制电机标志位 0:停止 1:正转 2:反转 default其他
int Speed=2;            //步进电机度转速度 默认2
String get_control="";  //串口指令
hw_timer_t * timer = NULL;  // 声明一个定时器
TaskHandle_t TASK_HandleOne = NULL;
//int step_speed=10;

//电机初始化
void uln2003_init()
{
  digitalWrite(IN_1,LOW);
  digitalWrite(IN_2,LOW);
  digitalWrite(IN_3,LOW);
  digitalWrite(IN_4,LOW);
}


//单四拍正转
void MotorGoAhead_s4()
{
  //uln2003_init();
  digitalWrite(IN_1,HIGH);
  digitalWrite(IN_4,LOW);
  delay(Speed);

  digitalWrite(IN_1,LOW);
  digitalWrite(IN_2,HIGH);
  delay(Speed);

  digitalWrite(IN_2,LOW);
  digitalWrite(IN_3,HIGH);
  delay(Speed);
  
  digitalWrite(IN_3,LOW);
  digitalWrite(IN_4,HIGH);
  delay(Speed);
  
}

//单四拍反转
void MotorGoBack_s4()
{
 // uln2003_init();
  digitalWrite(IN_4,HIGH);
  digitalWrite(IN_1,LOW);
  delay(Speed);

  digitalWrite(IN_4,LOW);
  digitalWrite(IN_3,HIGH);
  delay(Speed);

  digitalWrite(IN_3,LOW);
  digitalWrite(IN_2,HIGH);
  delay(Speed);
  
  digitalWrite(IN_2,LOW);
  digitalWrite(IN_1,HIGH);
  delay(Speed);
  
}

//双四拍正传
void MotorGoAhead_d4()
{
  //uln2003_init();
  digitalWrite(IN_2,HIGH);
  digitalWrite(IN_4,LOW);
  delay(Speed);

  digitalWrite(IN_1,LOW);
  digitalWrite(IN_3,HIGH);
  delay(Speed);

  digitalWrite(IN_2,LOW);
  digitalWrite(IN_4,HIGH);
  delay(Speed);
  
  digitalWrite(IN_1,HIGH);
  digitalWrite(IN_3,LOW);
  delay(Speed);

}

//双四拍反转
void MotorGoBack_d4()
{
  //uln2003_init();
  digitalWrite(IN_1,HIGH);
  digitalWrite(IN_3,LOW);
  delay(Speed);

  digitalWrite(IN_2,LOW);
  digitalWrite(IN_4,HIGH);
  delay(Speed);

  digitalWrite(IN_1,LOW);
  digitalWrite(IN_3,HIGH);
  delay(Speed);
  
  digitalWrite(IN_2,HIGH);
  digitalWrite(IN_4,LOW);
  delay(Speed);
  
}

//八拍正转
void MotorGoAhead_8()
{
  //uln2003_init();
  digitalWrite(4,LOW);
  delay(Speed);

  digitalWrite(2,HIGH);
  delay(Speed);

  digitalWrite(1,LOW);
  delay(Speed);

  digitalWrite(3,HIGH);
  delay(Speed);  
  
  digitalWrite(2,LOW);
  delay(Speed);

  digitalWrite(4,HIGH);
  delay(Speed);

  digitalWrite(3,LOW);
  delay(Speed);

  digitalWrite(1,HIGH);
  delay(Speed);
}

//八拍反转
void MotorGoBack_8()
{
  //uln2003_init();
  digitalWrite(2,LOW);
  delay(Speed);

  digitalWrite(4,HIGH);
  delay(Speed);

  digitalWrite(1,LOW);
  delay(Speed);

  digitalWrite(3,HIGH);
  delay(Speed);  

  digitalWrite(4,LOW);
  delay(Speed);

  digitalWrite(2,HIGH);
  delay(Speed);

  digitalWrite(3,LOW);
  delay(Speed);

  digitalWrite(1,HIGH);
  delay(Speed);
}

/*
void myKeymonitor()
{
   int keypointOn=digitalRead(KyMonitorControl);
   if(keypointOn)
   {
      Serial.printf("key On and go control;");
   }
}
*/

//电机状态更新根据指令
void usartGetControlRefresh(void *param )
{
  while(1)
  {
        /* 任务串口接收 */
    while (Serial.available()) 
    {   
      char inChar = (char)Serial.read();
      if(inChar != '\n'&&inChar != '\r')//单次指令回车结束
        get_control+=inChar;

      if (inChar == '\n') 
      { 
        get_control=get_control.substring(0 , get_control.length());//string长度+1以及末尾转义字符去掉
        Serial.printf("%s", get_control);
        Serial.println();
        if(get_control=="go")
        {
          //标志位 置1
          control_flag=1;
          Serial.printf("get ControlGo");
          Serial.println();
        }
        else if(get_control=="back")
        {
          //标志位 置2
          control_flag=2;
          Serial.printf("get ControlBack");
          Serial.println();
        }
        else if(get_control=="stop")
        {
          //标志位 置0
          control_flag=0;
          Serial.printf("get ControlStop");
          Serial.println();
        }
        else if(get_control.startsWith("speed="))
        {
          //修改步进电机速度
          get_control.remove(0,6);
          Speed=get_control.toInt();
          Serial.printf("speed=%d",Speed);
          Serial.println();
        }
        else 
        {
          //标志位 重置初始
          control_flag=0;
          Serial.printf("no Control");
          Serial.println();
        }
        get_control.clear();//指令缓冲数据清空
      }
    }
    //Serial.printf("TASK");
    vTaskDelay(500);
  }
}

//GPIO初始化
void GpioInit()
{
  //电机GPIO
  pinMode(IN_1,OUTPUT);
  pinMode(IN_2,OUTPUT);
  pinMode(IN_3,OUTPUT);
  pinMode(IN_4,OUTPUT);
  
}

//看门狗
void WatchDogInit(unsigned int timeout_ms)
{
  rtc_wdt_protect_off();     //看门狗写保护关闭 关闭后可以喂狗
  //rtc_wdt_protect_on();    //看门狗写保护打开 打开后不能喂狗
  //rtc_wdt_disable();       //禁用看门狗
  rtc_wdt_enable();          //启用看门狗
  rtc_wdt_set_time(RTC_WDT_STAGE0, timeout_ms); 
  
}

// 中断函数
void IRAM_ATTR onTimer() 
{                        
  uln2003_init();
  MotorGoAhead_s4();//单四拍正转

}

//初始化定时器
void MyTimeInit()
{
  timer = timerBegin(0, 80, true);                         // 初始化定时器指针        
  timerAttachInterrupt(timer, &onTimer, true);             // 绑定定时器
  timerAlarmWrite(timer, 1000000, true);          // 配置报警计数器保护值（就是设置时间）
	timerAlarmEnable(timer);                                 // 启用定时器
}

//初始程序入口函数
void setup() {
  
  Serial.begin(115200);  

  //设置时钟频率
  setCpuFrequencyMhz(240);
  //GPIO初始化
  GpioInit();

  //按键控制
  //myKeymonitor();

  //看门狗初始化 设置看门狗超时 8000ms.则reset重启 
   WatchDogInit(8000);

  //处理监控串口指令
  xTaskCreatePinnedToCore((TaskFunction_t)usartGetControlRefresh, "usartGetControlRefresh", 1024, (void *)NULL, (UBaseType_t)2, (TaskHandle_t *)NULL, (BaseType_t)tskNO_AFFINITY);
 
}

//主程序运行函数
void loop() {

  //喂狗
  rtc_wdt_feed(); 

  switch (control_flag)
  {
    
    case 0: 
    break;
    
    case 1: 
            uln2003_init();
            MotorGoAhead_s4(); break;//单四拍正转     
    case 2: 
            uln2003_init();
            MotorGoBack_s4(); break;//单四拍反转  
    default:
    break;
  }
/*
if(control_flag==0)
  {
    // Serial.printf("stop flag-%d",control_flag);
  }

  if(control_flag==1)
  {
    //Serial.printf("go flag-%d",control_flag); 
    uln2003_init();
    MotorGoAhead_s4(); //单四拍正转
    //MotorGoAhead_d4(); //双四拍正转
    //MotorGoAhead_8();  //八拍正转
  }
  
  if(control_flag==2)
  {
   // Serial.printf("back flag-%d",control_flag);
    uln2003_init();
    MotorGoBack_s4(); //单四拍反转
   // MotorGoBack_d4(); //双四拍反转
   // MotorGoBack_8();  //八拍反转
  }
  */
}