/*
* WashingMachine.c
*
* 
* Created: 2018-08-22 오전 11:42:09
* 작성자 : 엄장현
* 내용 : 세탁기
* 최종 수정 : 18-09-07
*/

#define F_CPU 16000000L
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "LCD.h"

void avrInit();
uint8_t GetSWSts1();				// 세탁버튼
uint8_t GetSWSts2();				// 탈수버튼
uint8_t GetSWSts3();				// 동작버튼
void RunWash();						//
void RunDry();						//
void MotorSpeed();
void ClearVariable();
void MotorRun(uint8_t Sflag);

volatile uint8_t btnState1, btnState2, btnState3;
volatile uint8_t preState1 = 1, preState2 = 1, preState3 = 1, state = 0, motor_flag=1, washFlag = 0, dryFlag = 0;
const uint8_t  washSpd = 150, drySpd=230;
volatile uint8_t ovf_cnt;
volatile int stop_cnt=0;
enum WashState{IDLE, WASH, DRY} WashState;



ISR(TIMER1_OVF_vect)		// 오버플로우 발생시 ovf_cnt 1씩 증가
{
	ovf_cnt++;
	
}
int main(void)
{
	
	avrInit();				// avr초기화
	LCD_init();
	while (1)
	{
		switch(WashState)
		{
			
			case IDLE:				// 최초상태 무 반응
			LCD_clear();
			if(GetSWSts1())	
			{
				washFlag = 1;
				dryFlag = 0;
			}
			else if(GetSWSts2())
			{
				washFlag = 0;
				dryFlag = 1;
			}
			else if(GetSWSts3())
			{
				
				if(washFlag)
				{
					WashState = WASH;				// 상태 변화의 조건	세탁으로 이동
					washFlag = 0;
					dryFlag = 0;
					LCD_clear();
					LCD_write_string("Wash");
					TIMSK |= (1 << TOIE1);			// 16비트 타이머/카운터 오버플로우 인터럽트 허용
					
				}
				else if(dryFlag)
				{
					WashState = DRY;		// 탈수로 상태 변화
					LCD_clear();
					LCD_write_string("Dry!");
					washFlag = 0;
					dryFlag = 0;
				}
			}
			break;
			case WASH:
			RunWash();						// 3초 좌동작, 3초 우동작 후 IDLE
			break;
			case DRY:
			RunDry();						// 최대속도로 5초 동작 후 IDLE
			break;
		}
	}
	return 0;
}

void avrInit()
{
	DDRB |= (1 << PORTB4) | (1 << PORTB7);
	PORTB = 0x00;
	DDRF = 0x00;
	PORTF |= (1 << PORTF4) | (1 << PORTF5) | (1 << PORTF6) | (1 << PORTF7);
	
	// 8비트 오버플로우 인터럽트
	TCCR0 |= (1 << CS02);						// 분주비 64,
	TCCR0 |= (1 << WGM01) | (1 << WGM00);		// 고속PWM모드
	TCCR0 |= (1 << COM00) | (1 << COM01);		// 비교일치 출력모드 COM00 = 1 COM01 = 1
	
	// oc2
	TCCR2 |= (1 << CS01) | (1 << CS00);
	TCCR2 |= (1 << WGM01) | (1 << WGM00);		// 고속PWM모드, 분주비 64, 비교일치 출력모드 COM00 = 1 COM01 = 1
	TCCR2 |= (1 << COM00) | (1 << COM01);
	
	// 16 비트
	TCCR1B |= (1 << CS12);		// 분주비 256
	sei();
	
}

uint8_t GetSWSts1()
{
	btnState1 = (PINF & 0x10) >> 4;
	_delay_ms(10);
	if (btnState1 != preState1)
	{
		preState1 = btnState1;
		
		if (btnState1 == 1)
		{
			return 1;
		}
	}
	return 0;
}

uint8_t GetSWSts2()
{
	btnState2 = (PINF & 0x20) >> 5;
	_delay_ms(10);
	if (btnState2 != preState2)
	{
		preState2 = btnState2;
		
		if (btnState2 == 1)
		{
			return 1;
		}
	}
	return 0;
}

uint8_t GetSWSts3()
{
	btnState3 = (PINF & 0x40) >> 6;
	_delay_ms(10);
	if (btnState3 != preState3)
	{
		preState3 = btnState3;
		
		if (btnState3 == 1)
		{
			return 1;
		}
	}
	return 0;
}

void RunWash()
{
	MotorRun(1);
}

void RunDry()
{
	MotorRun(2);
}

void ClearVariable(){			// 상태값 초기화
	stop_cnt=0;
	OCR0 = 0;
	OCR2 = 0;
	_delay_ms(100);
	
}

void MotorRun(uint8_t Sflag)
{
	
	if(Sflag==1)
	{
		if(motor_flag == 1)
		{
			OCR0 = 0;
			OCR2 = washSpd;
			
		}
		else
		{
			OCR0 = washSpd;
			OCR2 = 0;
			
		}
		if(ovf_cnt==1)
		{
			motor_flag = !motor_flag;
			_delay_ms(20);
			ovf_cnt=0;
			stop_cnt++;
			if(stop_cnt == 8)
			{
				TIMSK &= ~(1 << TOIE1);			// 세탁이 끝나면 인터럽트도 종료되도록 TIMSK를 0으로세팅
				ClearVariable();				// 초기화
				WashState = IDLE;
			}
		}
		
	}
	else if(Sflag==2)
	{
		
		for(;;){			// 무한루프에서 stop_cnt를 증가시켜서 일정시간 후 정지
			stop_cnt++;
			OCR0 = 0;
			OCR2 = drySpd;
			if(stop_cnt >= 200)
			{
				ClearVariable();
				WashState = IDLE;
				break;
			}
			_delay_ms(10);
			
		}
		
	}
}
