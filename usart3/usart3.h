#ifndef __USART3_H
#define __USART3_H

#include "sys.h"
#include <stdio.h>
#include <stdarg.h>

// 需根据需求调整的配置
#define EN_USART3_RX 1    // 使能串口3接收（1=使能，0=关闭）
#define USART3_REC_LEN 200 // 串口3接收缓冲区长度（最大支持2^14=16384）

// 函数声明
void uart3_init(u32 bound);    // 串口3初始化（指定波特率）
void uart3_send_char(u8 ch);   // 串口3发送单个字符
void uart3_send_string(u8 *str);// 串口3发送字符串
void uart3_printf(const char *fmt, ...);

int fputc3(int ch, FILE *f);   // printf重定向到串口3（可选）

#if EN_USART3_RX
// 全局接收缓冲区和状态（与串口1逻辑一致）
extern u8 USART3_RX_BUF[USART3_REC_LEN];
// 接收状态标记：
// bit15 - 接收完成标志 | bit14 - 接收到0x0d | bit13~0 - 有效字节数
extern u16 USART3_RX_STA;
#endif

#endif
