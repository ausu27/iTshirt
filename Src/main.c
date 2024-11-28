#include "stm32f767xx.h"
#include "OK-STM767.h"
#include "ff.h"
#include "diskio.h"
#include "sd_diskio.h"


#define DIRECTORY_NAME "recordings"
#define FILENAME_PREFIX "record_"
#define FILENAME_EXTENSION ".txt"

void EXTI0_IRQHandler(void); /* EXTI0 interrupt function */
void EXTI1_IRQHandler(void); /* EXTI1 interrupt function */
void create_new_file(void);
FRESULT create_directory(void);
unsigned int get_next_file_index(void);

short ADC1_data_buffer[256];
volatile unsigned int interrupt_flag_key1, interrupt_flag_key2;
unsigned int i = 0;
unsigned int ADC_count, U08sWritten;
static int error_count = 0;
FIL file;
FATFS FatFs; // ���Ͻý��� �ʱ�ȭ�Ҷ� �� ����. ff.h�� ����ü��.

//FRESULT res; �̰� ���� ����������?
//FRESULT res_open, res_write;

void EXTI0_IRQHandler(void) /* EXTI0 interrupt function */
{
  RCC->APB2ENR |= 0x00000001; // enable TIM1 clock  
 
  EXTI->PR = 0x00000001; // clear pending bit of EXTI0
  interrupt_flag_key1 = 1;
}

void EXTI1_IRQHandler(void) /* EXTI1 interrupt function */
{
  RCC->APB2ENR &= 0xFFFFFFFE; // disable TIM1 clock
 
  EXTI->PR = 0x00000002; // clear pending bit of EXTI1
  interrupt_flag_key2 = 1;
}


void TIM1_UP_TIM10_IRQHandler(void) /* TIM1 interrupt function (27MHz)*/
{
  TIM1->SR = 0x0000; // clear pending bit of TIM1 interrupt
 
  ADC_count++;
  if( ADC_count >= 612 ) // 1�ʿ� 44000�� ADC ���ø��ϱ� ���ؼ�.
  {
   ADC_count = 0;
   if ( i >= 255 )
    {
      i = 0;
      if ( f_write(&file, ADC1_data_buffer, sizeof(ADC1_data_buffer), &U08sWritten) != FR_OK ) {
        error_count++;
        TFT_string( 10, 10, White, Black, "���� ���� ����" );
        if ( error_count >= 3){
            TFT_string( 10, 10, White, Black, "���� 3�� �̻� �߻�. �����Ͻÿ�." );
            error_count = 0;
        }
      } ; // ������ ���
      memset(ADC1_data_buffer, 0, sizeof(ADC1_data_buffer)); //ADC1_data_buffer[ 255 ] = { 0, };
    }
    else
    {  
      ADC1->CR2 |= 0x40000000; // start conversion by software
      while(!(ADC1->SR & 0x00000002)); // wait for end of conversion ///////// 480+12clk = 492clk .�ɸ�
      ADC1_data_buffer[ i++ ] = ADC1->DR;
     }
    }
}




int main( void )
{
  //////KEY_INTERRUPT/////////////////
  GPIOC->MODER &= 0xFFFFFF00; // PC3,2,1,0 = input mode
  RCC->APB2ENR |= 0x00000001; // enable SYSCFG
  SYSCFG->EXTICR[0] = 0x00002222; // EXTI3,2,1,0 = PC3,2,1,0(KEY4,3,2,1)
  EXTI->IMR = 0x0000000F; // enable EXTI3,2,1,0 interrupt
  EXTI->RTSR = 0x00000000; // disable EXTI3,2,1,0 rising edge
  EXTI->FTSR = 0x0000000F; // enable EXTI3,2,1,0 falling edge
  NVIC->ISER[0] = 0x000003C0; // enable (9)EXTI3 ~ (6)EXTI0 interrupt
  interrupt_flag_key1 = 0;
  interrupt_flag_key2 = 0;

  //////TIMER1///////////////////
  //RCC->APB2ENR |= 0x00000001; // enable TIM1 clock
  TIM1->PSC = 1; // 108MHz/( 1+1 ) = 54MHz
  TIM1->ARR = 1; // 54MHz/( 1+1 ) = 27MHz
  TIM1->CNT = 0; // clear counter
  TIM1->DIER = 0x0001; // enable update interrupt
  TIM1->CR1 = 0x0005; // enable TIM1 and update event
  NVIC->ISER[0] |= 0x02000000; // enable (25)TIM1 interrupt
 
  //////ADC1//////////////////////
  GPIOA->MODER |= 0x00003000; // PA6 = analog mode(ADC12_IN6-)
  RCC->APB2ENR |= 0x00000100; // enable ADC1 clock
  ADC->CCR = 0x00000000; // ADCCLK = 54MHz/2 = 27MHz
  ADC1->SMPR2 = 0x001C0000; // sampling time of channel 6 = 480 cycle
  ADC1->CR1 = 0x00000000; // 12-bit resolution
  ADC1->CR2 = 0x00000001; // right alignment, single conversion, ADON = 1
  ADC1->SQR1 = 0x00000000; // total regular channel number = 1
  ADC1->SQR3 = 0x00000006; // channel 6
  
  
  //  1. SD �ʱ�ȭ, FAT �ʱ�ȭ
  // 1-1. SD ī�� �ʱ�ȭ
    if (disk_initialize( 1 ) != 0) {
        TFT_string(10, 10, White, Black, "SD �ʱ�ȭ ����");
        while (1); // SD ī�� �ʱ�ȭ ���� �� ���
    }

    // 1-2. FAT ���� �ý��� ����Ʈ
    if (f_mount(&FatFs, "", 1) != FR_OK) {
        TFT_string(10, 10, White, Black, "���� �ý��� ����Ʈ ����");
        while (1); // ���� �ý��� ����Ʈ ���� �� ���
    }
  
  
  
  //////2. ���ο� directory �����//
  FRESULT res0 = create_directory();
    if (res0 == FR_OK) {
        TFT_string(10, 10, White, Black, "���丮 ������");
    } else {
        TFT_string(10, 10, White, Black, "���丮 ���� ����");
    }


  while(1)
  {
    if ( interrupt_flag_key1 == 1 )
     { interrupt_flag_key1 = 0;
       create_new_file();
     }
    
    if ( interrupt_flag_key2 == 1 )
    { interrupt_flag_key2 = 0;
      if ( f_close( &file ) != FR_OK ) {
        TFT_string( 10, 10, White, Black, "���� �ݱ� ����" );
        
          if ( error_count >= 3){
              TFT_string( 10, 10, White, Black, "���� 3�� �̻� �߻�. �����Ͻÿ�." );
              error_count = 0;
          }
      } else {
          RCC->APB2ENR &= 0xFFFFFFFE; // TIM1 disable
          TFT_string( 10, 10, White, Black, "���� �ݱ� ����");
      }   
    }  
  }
}
//res_close = f_close( &file );

/////////////����� ���� �Լ�//////////////////////////////////
#define DIRECTORY_NAME "recordings"           // ���丮 �̸�
#define FILENAME_PREFIX "record_"               // ���� �̸� ���ξ�
#define FILENAME_EXTENSION ".txt"             // ���� Ȯ����
//extern FIL file;

unsigned int get_next_file_index(void) {
    DIR dir;
    FILINFO fno;
    //FIL file; main.c ���� ���������� ���Ű�, ���⼭�� extern FIL file;�� ���������� �����ؼ� ������ �� �� �ְ� �Ұ���. ���� main.c���� �� �����ؾߵ�.����������.
    
    unsigned int max_index = 0;
    char *filename;

    // ���丮 ����
    if (f_opendir(&dir, DIRECTORY_NAME) == FR_OK) {
        while (1) {
            // ���� ���� �б�
            if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) {
                break; // ���丮 ��
            }

            // ���� �̸� �м�
            filename = fno.fname;
            if (strncmp(filename, FILENAME_PREFIX, strlen(FILENAME_PREFIX)) == 0) {
                // "record_" ������ ���ڸ� ����
                unsigned int current_index = atoi(&filename[strlen(FILENAME_PREFIX)]);
                if (current_index > max_index) {
                    max_index = current_index; // �ִ� �ε��� ����
                }
            }
        }
        f_closedir(&dir); // ���丮 �ݱ�
    }

    return max_index + 1; // ���� ���� �ε��� ��ȯ
}

void create_new_file(void) {
    char filename[64]; // ���� �̸� ���� ����
    unsigned int next_index = get_next_file_index();
    unsigned int error_count = 0;
    // ���ο� ���� �̸� ����
    sprintf(filename, DIRECTORY_NAME "/" FILENAME_PREFIX "%u" FILENAME_EXTENSION, next_index);

    // ���� ����
    if (f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
            TFT_string( 10, 10, White, Black, "���� ���� ����" );
            if ( error_count >= 3){
            TFT_string( 10, 10, White, Black, "���� 3�� �̻� �߻�. �����Ͻÿ�." );
            error_count = 0;
        }
    }
}        
        //static volatile int error_flag = 0;

      //  if ( !error_flag ) {
        //    error_flag = 1; // ���� ���� ����
          //  RCC->APB2ENR &= 0xFFFFFFFF; 
            //RCC->APB2ENR |= 0x00000001;
            //error_flag = 0; // ���� ���� ����
           // if ( error_count > 3 ) {
             //   return;
            //error_count++;
    //}
      //  }


FRESULT create_directory(void) {
    DIR dir;
    FRESULT res1 = f_opendir(&dir, "recordings");  // "recordings" ���丮 ����
    if (res1 == FR_OK) {
        f_closedir(&dir);  // ���丮�� �����ϸ� ���������� ����
        return FR_OK;  // �̹� ���丮�� ������
    }
    // ���丮�� �������� ������ ����
    res1 = f_mkdir("recordings");  
    return res1;
}


///////////////////////////////////////////////


/*
���� ó���� TFT_LCD�� ���°� �ƴϸ� �� ���µ�..
res_open, res_close, res_write�� ����. ���´� ����
sprintf�� ����
f_mkdir
f_write
f_open
f_close

FRESULT f_mkdir (
  const TCHAR* path / [IN] Directory name 
);
FRESULT f_write (
  FIL* fp,          /   [IN] Pointer to the file object structure 
  const void* buff, / [IN] Pointer to the data to be written 
  U32 btw,         / [IN] Number of U08s to write 
  U32* bw          / [OUT] Pointer to the variable to return number of U08s written 
);
FRESULT f_open (
  FIL* fp,           / [OUT] Pointer to the file object structure 
  const TCHAR* path, / [IN] File name 
  U08 mode          / [IN] Mode flags 
);
FRESULT f_close (
  FIL* fp     / [IN] Pointer to the file object 
);

ADC1_data_buffer[ i ] = ADC1->DR; ���⼭ �迭 �ϳ��� ũ��� 16��Ʈ�̰�, 12��Ʈ�� ����Ǵµ� ������Ʈ���� ä��������, �������� 0���� ä��������?



================================
Ư�� �� ���ڿ� ǥ�� �� ADC ������ �ս��� �����Ϸ��� �޽��� ����� �񵿱������� ����ǰų� ��� �ߴ� ���¸� �����ؾ� �մϴ�.
�̰� ��� �ؾ� �ϴ°ž�

&file ���� file�� ������ ��, FIL file; �̷��� �Ѱ� ����, ���� ������ ��ü��� ���� �ǰڳ�
�׷��� f_write(&file, ) �ϸ� FatFS ������ FIL*fp ���� ��ü ������, �� SDī�� ���� ������ ù block�� ���� �ּҸ� ��Ÿ���°Ű�?

��� �ߴ� ���� ������ �ϰ� ������, ������ ������, KEY1 �ڵ鷯���� ������ �����ų �� ���� �ʳ�.
if (f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) {
        RCC->APB2ENR &= 0xFFFFFFFE; // TIM1 disable
        TFT_string( 10, 10, White, Black, "���� ���� ����" );
    }
�̷��� �ϸ� �浹�̳� ������ ���� �� �ֳ�?

static int�� ����? */