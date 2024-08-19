#include "mik32_hal_adc.h"
#include "mik32_hal_pcc.h"
#include "mik32_hal_scr1_timer.h"

#include "uart_lib.h"
#include "xprintf.h"

/*
* В данном примере демонстрируется работа с АЦП.
* 
* Канал АЦП переключается не сразу после записи в регистр, а в конце преобразования.
* Для одиночных измерений с разных каналов рекомендуется записывать следующий канал сразу 
* после старта преобразования на текущем канале.
*
* При многократных измерениях на одном канале, после переключения канала рекомендуется сделать одно дополнительное измерение
* для переключения на выбранный канал.
*
*/

#define	MIK32V2

ADC_HandleTypeDef hadc;
SCR1_TIMER_HandleTypeDef hscr1_timer;

#define	ADC_OFFSET	172
//#define	ADC_CHANNELS	3
#define	ADC_CHANNELS	1


void SystemClock_Config(void);
static void ADC_Init(void);
static void Scr1_Timer_Init(void);

int32_t adc_avg[6] = { 0 };
uint32_t count = 0;

int main()
{

	SystemClock_Config();
	Scr1_Timer_Init();

	UART_Init(UART_0, OSC_SYSTEM_VALUE/9600, UART_CONTROL1_TE_M | UART_CONTROL1_M_8BIT_M, 0, 0);

	ADC_Init();

	int16_t adc_corrected_value, adc_raw_value;

	// HAL_ADC_ContinuousEnable(&hadc); /* Запуск преобразования в непрерывном режиме */
	// adc_value = HAL_ADC_GetValue(&hadc); /* Получить текущий результат преобразования (режим непрерывного преобразования) */
	while (1)
	{

		/***********************************Получение значения с канала***************************************/
		// HAL_ADC_Single(&hadc);					  /* Запуск однократного преобразования для того чтобы канал переключился на заданный*/
		// adc_value = HAL_ADC_WaitAndGetValue(&hadc); /* Ожидание и чтение данных (режим одиночного преобразования) */

		// xprintf("ADC%d: %d (V = %d,%03d)\n", i, adc_value, ((adc_value * 1200) / 4095) / 1000, ((adc_value * 1200) / 4095) % 1000);

		// for (volatile int i = 0; i < 1000000; i++)
		//	 ;
		/*****************************************************************************************************/

		 if(ADC_CHANNELS == 1)
		 	HAL_ADC_SINGLE_AND_SET_CH(hadc.Instance, 0);

		/* Получение значения с разных каналов */
		for (int j = 0; j < ADC_CHANNELS; j++)
		{

			if(ADC_CHANNELS > 1)
				HAL_ADC_SINGLE_AND_SET_CH(hadc.Instance, (j + 1) % ADC_CHANNELS);

		adc_raw_value = HAL_ADC_WaitAndGetValue(&hadc); /* Ожидание и чтение актуальных данных (режим одиночного преобразования) */

		adc_corrected_value = adc_raw_value - ADC_OFFSET;
		//adc_avg[j] = (adc_avg[j] + adc_corrected_value) / 2; /* Скользящее среднее */
		adc_avg[j] = (adc_avg[j] * 14 + adc_corrected_value * 2) / 16; /* Сглаживание по алфа-бета */

			if(count % 1000 == 0)
			xprintf("ADC[%d]: %4d/%4d/%4d (%d,%03d V)\t", j, adc_raw_value, adc_corrected_value, adc_avg[j],
									((adc_avg[j] * 1200) / 4095) / 1000,
									((adc_avg[j] * 1200) / 4095) % 1000);
		}

		if(count % 1000 == 0) {
		xprintf("\n");
		HAL_DelayMs(&hscr1_timer, 250);
	}

	count++;
	}
}

void SystemClock_Config(void)
{
	PCC_InitTypeDef PCC_OscInit = {0};

	PCC_OscInit.OscillatorEnable = PCC_OSCILLATORTYPE_ALL;
	PCC_OscInit.FreqMon.OscillatorSystem = PCC_OSCILLATORTYPE_OSC32M;
	PCC_OscInit.FreqMon.ForceOscSys = PCC_FORCE_OSC_SYS_UNFIXED;
	PCC_OscInit.FreqMon.Force32KClk = PCC_FREQ_MONITOR_SOURCE_OSC32K;
	PCC_OscInit.AHBDivider = 0;
	PCC_OscInit.APBMDivider = 0;
	PCC_OscInit.APBPDivider = 0;
	PCC_OscInit.HSI32MCalibrationValue = 128;
	PCC_OscInit.LSI32KCalibrationValue = 128;
	PCC_OscInit.RTCClockSelection = PCC_RTC_CLOCK_SOURCE_AUTO;
	PCC_OscInit.RTCClockCPUSelection = PCC_CPU_RTC_CLOCK_SOURCE_OSC32K;
	HAL_PCC_Config(&PCC_OscInit);
}


static void Scr1_Timer_Init(void)
{   
	hscr1_timer.Instance = SCR1_TIMER;

	hscr1_timer.ClockSource = SCR1_TIMER_CLKSRC_INTERNAL; /* Источник тактирования */
	hscr1_timer.Divider = 0;							  /* Делитель частоты 10-битное число */

	HAL_SCR1_Timer_Init(&hscr1_timer);
}


static void ADC_Init(void)
{
	hadc.Instance = ANALOG_REG;

	hadc.Init.Sel = ADC_CHANNEL4;
	hadc.Init.EXTRef = ADC_EXTREF_OFF;	/* Выбор источника опорного напряжения: «1» - внешний; «0» - встроенный */
	//hadc.Init.EXTRef = ADC_EXTREF_ON;
	hadc.Init.EXTClb = ADC_EXTCLB_ADCREF; /* Выбор источника внешнего опорного напряжения: «1» - внешний вывод; «0» - настраиваемый ОИН */

	HAL_ADC_Init(&hadc);
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	__HAL_PCC_ANALOG_REGS_CLK_ENABLE();

	if ((hadc->Init.EXTClb == ADC_EXTCLB_ADCREF) && (hadc->Init.EXTRef == ADC_EXTREF_ON))
	{
#ifdef MIK32V0
		GPIO_InitStruct.Pin = GPIO_PIN_10;
#endif

#ifdef MIK32V2
		GPIO_InitStruct.Pin = GPIO_PIN_11;
#endif
	}

	GPIO_InitStruct.Mode = HAL_GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = HAL_GPIO_PULL_NONE;
	HAL_GPIO_Init(GPIO_1, &GPIO_InitStruct);

	/* Настройка выводов АЦП */
	GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_7;
	HAL_GPIO_Init(GPIO_1, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_7 | GPIO_PIN_9;
	HAL_GPIO_Init(GPIO_0, &GPIO_InitStruct);
}
