
#define ADC_CONFIG_SAH_TIME_S          8
#define ADC_CONFIG_SAH_TIME_M          (0x3F << ADC_CONFIG_SAH_TIME_S)

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

	UART_Init(UART_0, OSC_SYSTEM_VALUE/115200, UART_CONTROL1_TE_M | UART_CONTROL1_M_8BIT_M, 0, 0);

	ADC_Init();

	int16_t adc_corrected_value, adc_raw_value;

	while (1) {

		 if(ADC_CHANNELS == 1)
		 	HAL_ADC_SINGLE_AND_SET_CH(hadc.Instance, 0);

		if(count % 1000 == 0)
			xprintf("Time: %08X:%08X,\t", SCR1_TIMER->MTIMEH, SCR1_TIMER->MTIME);

		/* Получение значения с разных каналов */
		for (int j = 0; j < ADC_CHANNELS; j++) {

			/* Установить номер канала и сделать одиночное преобразование для того, чтобы канал переключился */
			if(ADC_CHANNELS > 1)
				HAL_ADC_SINGLE_AND_SET_CH(hadc.Instance, (j + 1) % ADC_CHANNELS);

			/* Ожидание и чтение актуальных данных (режим одиночного преобразования) */
			adc_raw_value = HAL_ADC_WaitAndGetValue(&hadc);

			adc_corrected_value = adc_raw_value - ADC_OFFSET;
			//adc_avg[j] = (adc_avg[j] + adc_corrected_value) / 2; /* Скользящее среднее */
			adc_avg[j] = (adc_avg[j] * 14 + adc_corrected_value * 2) / 16; /* Сглаживание по альфа-бета */

			/* Печатать усредненное значение после 1000 преобразований */
			if(count % 1000 == 0)
				xprintf("ADC[%d]: %4d %4d %4d (%d.%03d V)\t",
					j, adc_raw_value, adc_corrected_value, adc_avg[j],
					((adc_avg[j] * 1200) / 4095) / 1000,
					((adc_avg[j] * 1200) / 4095) % 1000);

		}

		if(count % 1000 == 0)
			xprintf("\r\n");

		//HAL_Time_SCR1TIM_DelayMs(250); // Hangs here, broken HAL ?

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
	/* Источник тактирования */
	/* Делитель частоты 10-битное число */

	HAL_SCR1_Timer_Init(HAL_SCR1_TIMER_CLKSRC_INTERNAL, 0); // 0 - 32 MHz
}


static void ADC_Init(void)
{
	hadc.Instance = ANALOG_REG;

	/* Выбор канала АЦП */
	hadc.Init.Sel = ADC_CHANNEL4;

	/* Выбор источника опорного напряжения: «1» - внешний; «0» - встроенный */
	hadc.Init.EXTRef = ADC_EXTREF_OFF;	
	//hadc.Init.EXTRef = ADC_EXTREF_ON;

	/* Выбор внешнего опорного напряжения: «1» - внешний вывод; «0» - настраиваемый ОИН */
	hadc.Init.EXTClb = ADC_EXTCLB_ADCREF;

	HAL_ADC_Init(&hadc);
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	__HAL_PCC_ANALOG_REGS_CLK_ENABLE();

	GPIO_InitStruct.Mode = HAL_GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = HAL_GPIO_PULL_NONE;

	/* Настройка входа референсного напряжения для АЦП */
	if ((hadc->Init.EXTClb == ADC_EXTCLB_ADCREF) && (hadc->Init.EXTRef == ADC_EXTREF_ON)) {
		GPIO_InitStruct.Pin = GPIO_PIN_11;
	}

	HAL_GPIO_Init(GPIO_1, &GPIO_InitStruct);

	/* Настройка выводов АЦП */
	GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_7;
	HAL_GPIO_Init(GPIO_1, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_7 | GPIO_PIN_9;
	HAL_GPIO_Init(GPIO_0, &GPIO_InitStruct);
}
