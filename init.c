/* NAND FLASH控制器 */
#define NFCONF (*((volatile unsigned long *)0x4E000000))
#define NFCONT (*((volatile unsigned long *)0x4E000004))
#define NFCMMD (*((volatile unsigned char *)0x4E000008))
#define NFADDR (*((volatile unsigned char *)0x4E00000C))
#define NFDATA (*((volatile unsigned char *)0x4E000010))
#define NFSTAT (*((volatile unsigned char *)0x4E000020))

/* GPIO */
#define GPHCON              (*(volatile unsigned long *)0x56000070)
#define GPHUP               (*(volatile unsigned long *)0x56000078)

/* UART registers*/
#define ULCON0              (*(volatile unsigned long *)0x50000000)
#define UCON0               (*(volatile unsigned long *)0x50000004)
#define UFCON0              (*(volatile unsigned long *)0x50000008)
#define UMCON0              (*(volatile unsigned long *)0x5000000c)
#define UTRSTAT0            (*(volatile unsigned long *)0x50000010)
#define UTXH0               (*(volatile unsigned char *)0x50000020)
#define URXH0               (*(volatile unsigned char *)0x50000024)
#define UBRDIV0             (*(volatile unsigned long *)0x50000028)

#define TXD0READY   (1<<2)

#define PCLK            50000000    // init.c中的clock_init函数设置PCLK为50MHz
#define UART_CLK        PCLK        //  UART0的时钟源设为PCLK
#define UART_BAUD_RATE  115200      // 波特率
#define UART_BRD        ((UART_CLK  / (UART_BAUD_RATE * 16)) - 1)

// bool Nand_boot();
void Nand_read(unsigned int addr, unsigned char *buffer, unsigned int len);
void delay_10ns(unsigned int count);

int Nand_boot(){
	volatile  int *p = 0;
	 int val = *p;
	*p = 0x12345678;
	if (*p==0x12345678)
	{
		*p = val;
		return 1; //是nandflash
	}
	else
	{
		return 0; //是norflash
	}
}

void Nand_init(){	

#define TACLS   0
#define TWRPH0  1
#define TWRPH1  0
/* 设置时序 */
NFCONF = (TACLS<<12)|(TWRPH0<<8)|(TWRPH1<<4);
/* 使能NAND Flash控制器, 初始化ECC, 禁止片选 */
NFCONT = (1<<4)|(1<<1)|(1<<0);	
}

void chip_enable(){
	NFCONT &= ~(1<<1);
}
void delay_10ns(unsigned int count){
	volatile int i;
	for (i = 0; i < count; i++);
}

void nand_cmd(unsigned char cmd){
	NFCMMD = cmd;
	delay_10ns(10);
}

void nand_addr(unsigned int addr){

	unsigned int page= addr / 2048;
	unsigned int col = addr % 2048;

	NFADDR = col & 0xff;
	delay_10ns(10);
	NFADDR = (col >> 8) & 0xff;
	delay_10ns(10);

	NFADDR  = page & 0xff;
	delay_10ns(10);
	NFADDR  = (page >> 8) & 0xff;
	delay_10ns(10);
	NFADDR  = (page >> 16) & 0xff;
	delay_10ns(10);	
}

void nand_ready(){
	while(!(NFSTAT & 1));
}

unsigned char nand_data(void)
{
	return NFDATA;
}

void chip_disenable(){
	NFCONT |= (1<<1);
}
void Nand_read(unsigned int addr, unsigned char *buffer, unsigned int len){	//从nandflash中读取代码
	int col = addr % 2048;
/*1.片选*/
	chip_enable();
	int i=0;
	while(i<len){
		/*2.发送读命令*/
		nand_cmd(0x00);
		/*3.发送地址周期*/
		nand_addr(addr);
		/*4.发送读命令*/
		nand_cmd(0x30);
		/*5.判断状态*/
		nand_ready();
		/*6.读取数据*/
		for (; col<2048 && (i < len); col++)
		{
			buffer[i] = nand_data();
			i++;
			addr++;
		}
		col = 0;
	}
	/*7.取消片选*/
	chip_disenable();
}

void copy_code_to_sdram(char *src,unsigned char *dest,unsigned int len){
	if (Nand_boot())	//nandflash启动
	{
		// Nand_init();
		Nand_read((unsigned int)src, dest, len);
	}
	else			//norflash启动
	{
		int i=0;
		while(i<len)
		{
			dest[i]=src[i];
			i++;
		}
	}
}

void clear_bss(void){	//全局变量初始值设为0
	extern int __bss_start, __bss_end;
	int *p = &__bss_start;
	
	for (; p < &__bss_end; p++)
		*p = 0;
}

/* 初始化UART0 115200,8N1,无流控 */
void uart0_init(void)
{
    GPHCON  |= 0xa0;    // GPH2,GPH3用作TXD0,RXD0
    GPHUP   = 0x0c;     // GPH2,GPH3内部上拉

    ULCON0  = 0x03;     // 8N1(8个数据位，无较验，1个停止位)
    UCON0   = 0x05;     // 查询方式，UART时钟源为PCLK
    UFCON0  = 0x00;     // 不使用FIFO
    UMCON0  = 0x00;     // 不使用流控
    UBRDIV0 = UART_BRD; // 波特率为115200
}

/* 发送一个字符*/
void putc(unsigned char c)
{
    /* 等待，直到发送缓冲区中的数据已经全部发送出去 */
    while (!(UTRSTAT0 & TXD0READY));
    
    /* 向UTXH0寄存器中写入数据，UART即自动将它发送出去 */
    UTXH0 = c;
}

void puts(char *str)
{
	int i = 0;
	while (str[i])
	{
		putc(str[i]);
		i++;
	}
}

void puthex(unsigned int val)
{
	/* 0x1234abcd */
	int i;
	int j;
	
	puts("0x");

	for (i = 0; i < 8; i++)
	{
		j = (val >> ((7-i)*4)) & 0xf;
		if ((j >= 0) && (j <= 9))
			putc('0' + j);
		else
			putc('A' + j - 0xa);
		
	}
	
}