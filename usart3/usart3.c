#include "usart3.h"
#include "esp8266.h"

#if SYSTEM_SUPPORT_OS  // 兼容ucos（与你的串口1代码一致）
#include "includes.h"
#endif

// ===================== 全局变量定义 =====================
#if EN_USART3_RX
u8 USART3_RX_BUF[USART3_REC_LEN];     // 接收缓冲区
u16 USART3_RX_STA = 0;                // 接收状态标记
#endif

// ===================== 基础发送函数 =====================
// 串口3发送单个字符
void uart3_send_char(u8 ch)
{
    while((USART3->SR & 0X40) == 0);  // 等待发送寄存器为空
    USART3->DR = (u8)ch;              // 发送字符
}

// 串口3发送字符串
void uart3_send_string(u8 *str)
{
    while(*str != '\0')
    {
        uart3_send_char(*str);
        str++;
    }
}

// ===================== printf重定向到串口3（可选） =====================
// 用法：使用前执行 freopen("stdout", "w", fopen("/dev/ttyS3", "w"));
// 或直接调用 uart3_printf 宏（推荐）
int fputc3(int ch, FILE *f)
{
    uart3_send_char((u8)ch);
    return ch;
}
// 简化宏：直接通过串口3打印（无需重定向全局printf）
void uart3_printf(const char *fmt, ...)
{
	char buf[256];	// 扩大缓冲区，避免溢出
	va_list args;
	va_start(args, fmt);
	// 安全格式化：防止缓冲区溢出
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	uart3_send_string((u8*)buf);
}



// ===================== 串口3初始化（核心） =====================
void uart3_init(u32 bound)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 1. 使能时钟（USART3挂APB1，GPIOB挂AHB1）
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);  // GPIOB时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE); // USART3时钟

    // 2. 引脚复用映射（PB10=TX，PB11=RX）
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_USART3); // PB10→USART3_TX
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_USART3); // PB11→USART3_RX

    // 3. GPIO配置（与串口1逻辑一致）
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11; // PB10/PB11
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;             // 复用模式
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;        // 50MHz速率
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;           // 推挽输出
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;             // 上拉
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 4. USART3参数配置（与串口1一致）
    USART_InitStructure.USART_BaudRate = bound;              // 波特率
    USART_InitStructure.USART_WordLength = USART_WordLength_8b; // 8位数据
    USART_InitStructure.USART_StopBits = USART_StopBits_1;   // 1位停止位
    USART_InitStructure.USART_Parity = USART_Parity_No;      // 无校验
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; // 收发模式
    USART_Init(USART3, &USART_InitStructure);

    // 5. 使能串口3
    USART_Cmd(USART3, ENABLE);

#if EN_USART3_RX
    // 6. 配置接收中断（与串口1逻辑一致）
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE); // 开启接收中断

    // 7. NVIC配置（优先级可根据需求调整）
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn; // 串口3中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3; // 抢占优先级3
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 4;        // 子优先级4（与串口1区分）
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;           // 使能中断
    NVIC_Init(&NVIC_InitStructure);
#endif
}

// ===================== 串口3中断服务函数 =====================
#if EN_USART3_RX
void USART3_IRQHandler(void)
{
    u8 Res;
#if SYSTEM_SUPPORT_OS  // 兼容ucos
    OSIntEnter();
#endif

    // 接收中断（与串口1逻辑完全一致）
    if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        Res = USART_ReceiveData(USART3); // 读取接收数据
        
		ESP8266_HandleRecvData(Res);
		
        if((USART3_RX_STA & 0x8000) == 0) // 接收未完成
        {
            if(USART3_RX_STA & 0x4000) // 已接收到0x0d
            {
                if(Res != 0x0a) USART3_RX_STA = 0; // 接收错误，重新开始
                else USART3_RX_STA |= 0x8000;      // 接收完成（0x0d+0x0a）
            }
            else // 未接收到0x0d
            {
                if(Res == 0x0d) USART3_RX_STA |= 0x4000;
                else
                {
                    USART3_RX_BUF[USART3_RX_STA & 0x3FFF] = Res;
                    USART3_RX_STA++;
                    // 超出缓冲区，重新接收
                    if(USART3_RX_STA > (USART3_REC_LEN - 1)) USART3_RX_STA = 0;
                }
            }
        }
        USART_ClearITPendingBit(USART3, USART_IT_RXNE); // 清除中断标志
    }

#if SYSTEM_SUPPORT_OS
    OSIntExit();
#endif
}
#endif
