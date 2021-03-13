/* Includes ------------------------------------------------------------------*/
#include "elektronika.h"

#include "ClickButton.h"
#include <string.h>
#include "math.h"
#include "crc32.h"
#include "time.h"
#include "images.h"
#include "fonts.h"
#include <Pixels_PPI8.h> 
#include <Pixels_ST7735.h> 
#include <stddef.h>     /* offsetof */
//#include <OpenWindowControl.h>
//#include <wifi.h>

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
typedef struct MenuItem
{
	uint32_t ID; 
	uint16_t counts;
	struct MenuItem* items;
	void (*prev)(void); 
	void (*next)(void); 
	void (*enter)(void);
	int8_t selected;
	struct MenuItem* parent;
} MenuItem_t;

struct MenuItem* old_set;	
const char* _calendarPresetName[8] = {"P1", "P2", "P3", "P4", "P5", "P6", "P7", "C"};
static const RGB powerLevelColors[] = { RGB(255,255,255), RGB(255,242,90), RGB(235,147,79), RGB(227,109,79), RGB(224,86,74) };
static const RGB modeColors[] = {RGB(255,237,0), RGB(255,255,255), RGB(14,117,188), RGB(0,0,0) };

static MenuItem_t _modeOK = {999, 0, NULL, NULL, NULL, MenuBack }; // OK

// HeatMode menu
static MenuItem_t _modeMenu[] = {
	{11, 0, NULL, 					TempMinus, TempPlus, NULL }, // Comfort
	{12, 0, NULL, 					TempMinus, TempPlus, NULL }, // Eco
	{13, 0, NULL, 					TempMinus, TempPlus, NULL }  // Antifrezee
};
// OW settings
static MenuItem_t _open_window[] = {
	{711, 0, NULL, 					On, Off, NULL }, // 
};
// heat settings
static MenuItem_t _heatMenu[] = {
	{611, 1, _open_window, 					NULL, NULL, NULL }, // 
	{612, 3, NULL, 									NULL, NULL, NULL }, // 
};

// Date&time menu
static MenuItem_t _datetimeMenu[] = {
	{411, 0, NULL, 					DateMinus, DatePlus, NULL }, // Date
	{412, 0, NULL, 					TimeMinus, TimePlus, NULL }, // Time
};

// Timer menu
static MenuItem_t _timerMenu[] = {
	{31, 0, NULL, 					On, Off, NULL }, // On/off
	{32, 0, NULL, 					TimeMinus, TimePlus, NULL }, // Time
};

// Display menu
static MenuItem_t _displayMenu[] = {
	{421, 0, NULL, 					On, Off, NULL }, // On/off
	{422, 0, NULL, 					On, Off, NULL }, // Time
};

// Service menu
static MenuItem_t _serviceMenu[] = {
	{441, 0, NULL, 					On, Off, NULL }, // Reset
	{442, 0, NULL, 					NULL, NULL, MenuBack }, // Info
};

// Settings menu
static MenuItem_t _settingsMenu[] = {
	{41, 2, _datetimeMenu, 	NULL, NULL, NULL }, // Date&time
	{42, 2, _displayMenu, 	NULL, NULL, NULL }, // Display
	{43, 0, NULL, 					On, Off, NULL }, 		// Sound
	{44, 2, _serviceMenu, 	NULL, NULL, NULL }, // Service
};


static MenuItem_t _presetMenu = 
	{510, 8, NULL, 					NULL, NULL, NULL };

static MenuItem_t _presetViewMenu = 
	{511, 2, NULL, 					NULL, NULL, NULL };

static MenuItem_t _selectModeMenu = 
	{530, 4, NULL, 					NULL, NULL, NULL };

// Program menu
static MenuItem_t _programMenu[] = {
	{51, 7, NULL, 					NULL, NULL, NULL }, // Setup
	{52, 0, NULL, 					On, Off, NULL }, // Calendar
	{53, 24, NULL, 					CustomPrev, CustomNext, NULL }, // Custom day
};






// Main menu
static MenuItem_t _mainMenu[] = {
	{1, 3, _modeMenu,  			NULL, NULL, NULL }, // Mode
	{2, 2,  NULL,  		      NULL, NULL, NULL }, // power settings	
	{3, 2, _heatMenu,  			NULL, NULL, NULL }, // heat settings
	{4, 2, _timerMenu, 			NULL, NULL, NULL }, // Timer
	{5, 4, _settingsMenu,		NULL, NULL, NULL }, // Settings
	{6, 3, _programMenu, 		NULL, NULL, NULL }, // Program
};

// Root menu
static MenuItem_t _menu = 
	{0, 6, _mainMenu, 			NULL, NULL, NULL};

uint8_t version_menu = 0;	

MenuItem_t* currentMenu = NULL;
uint32_t idleTimeout = 0; 
uint8_t _currentPower = 0;
uint16_t _backLight = 0;
uint16_t _backLight_div = 0;
uint8_t _eventTimer = 0;
uint8_t _blocked = 0;
uint32_t _durationClick = 0;
uint32_t _timeoutSaveFlash = 0;
uint8_t _error = 0;
uint8_t _error_fl = 0;
int16_t _xWifi;
uint32_t _timerBlink = 0;
uint32_t _timerStart = 0;
bool _blink = false;
//OpenWindowControl _windowOpened;
StateBrightness _stateBrightness = StateBrightness_ON;
//Wifi _wifi;
uint32_t nextChangeLevel = 0;
uint32_t refrash_time = 0;
static float temp_steinhart = 25;
uint8_t open_window_temp_main_start;
uint8_t power_limit = 20;
uint8_t open_window_counter = 0;
uint8_t histeresis_low = 2;
uint8_t histeresis_high = 1;
uint8_t open_window_times = 5;
bool window_is_opened = 0;
bool window_was_opened = 0;
uint8_t power_current;
uint16_t timer_time_set = 0;
uint16_t raw;
ClickButton _key_window(12);
ClickButton _key_power(11);
ClickButton _key_menu(10);
ClickButton _key_back(9);
ClickButton _key_down(7, 500, true);
ClickButton _key_up(6, 500, true);
rtc_parameter_struct rtc_initpara;
rtc_alarm_struct  rtc_alarm;
Pixels pxs(130, 162);
static struct DeviceSettings _settings;
struct tm _dateTime;
static struct OnOffSettings _onoffSet = { 0, 0};
static struct PresetSettings _presetSet = {0, 0};
static struct TemperatureSettings _tempConfig;


void alarm_set(uint8_t minutes)
{
	  rtc_alarm_disable();

    rtc_alarm.rtc_alarm_mask = RTC_ALARM_DATE_MASK | RTC_ALARM_MINUTE_MASK | RTC_ALARM_HOUR_MASK | RTC_ALARM_SECOND_MASK;// | RTC_ALARM_SECOND_MASK | RTC_ALARM_HOUR_MASK;// | RTC_ALARM_SECOND_MASK;//RTC_ALARM_HOUR_MASK;
    rtc_alarm.rtc_weekday_or_date = RTC_ALARM_WEEKDAY_SELECTED;
    rtc_alarm.rtc_alarm_day = RTC_MONDAY | RTC_TUESDAY | RTC_WEDSDAY | RTC_THURSDAY | RTC_FRIDAY | RTC_SATURDAY | RTC_SUNDAY;
    //rtc_alarm.rtc_am_pm = RTC_AM;

    /* RTC alarm input */
    rtc_alarm.rtc_alarm_hour = 0x00;

    rtc_alarm.rtc_alarm_minute = 0x00;

    rtc_alarm.rtc_alarm_second = 0x00;
    
    /* RTC alarm configuration */
    rtc_alarm_config(&rtc_alarm);
    
    //rtc_interrupt_enable(RTC_INT_ALARM);  
    rtc_alarm_enable(); 
	  rtc_flag_clear(RTC_STAT_ALRM0F);
		delay_1ms(100);
}


void smooth_backlight(uint8_t mode)
{
	if(mode)
	{
		for(uint16_t i=0;i<(_stateBrightness ? 100:500);i+=5)
		{
			timer_channel_output_pulse_value_config(TIMER2,TIMER_CH_1,i);
			delay_1ms(2);	
		}	
	}
	else
	{
		for(uint16_t j=(_stateBrightness ? 100:500);j>0;j--)
		{
			timer_channel_output_pulse_value_config(TIMER2,TIMER_CH_1,j);
		}
		timer_channel_output_pulse_value_config(TIMER2,TIMER_CH_1,0);
	}
}


void I2C_ClearBusyFlagErratum()
{

	 
  i2c_disable(I2C0);
	
	  gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_9);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);	
		PIN_SET(GPIOA, GPIO_PIN_9);
		gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10);	
		PIN_SET(GPIOA, GPIO_PIN_10);

  // 3. Check SCL and SDA High level in GPIOx_IDR.
  while (SET != gpio_output_bit_get(GPIOA, GPIO_PIN_9))
  {
    __asm("nop");
  }

  while (SET != gpio_output_bit_get(GPIOA, GPIO_PIN_9))
  {
    __asm("nop");
  }

  // 4. Configure the SDA I/O as General Purpose Output Open-Drain, Low level (Write 0 to GPIOx_ODR).
  PIN_RESET(GPIOA, GPIO_PIN_9);

  //  5. Check SDA Low level in GPIOx_IDR.
  while (RESET != gpio_output_bit_get(GPIOA, GPIO_PIN_9))
  {
    __asm("nop");
  }

  // 6. Configure the SCL I/O as General Purpose Output Open-Drain, Low level (Write 0 to GPIOx_ODR).
  PIN_RESET(GPIOA, GPIO_PIN_10);

  //  7. Check SCL Low level in GPIOx_IDR.
  while (RESET != gpio_output_bit_get(GPIOA, GPIO_PIN_10))
  {
    __asm("nop");
  }

  // 8. Configure the SCL I/O as General Purpose Output Open-Drain, High level (Write 1 to GPIOx_ODR).
  PIN_SET(GPIOA, GPIO_PIN_10);

  // 9. Check SCL High level in GPIOx_IDR.
  while (SET != gpio_output_bit_get(GPIOA, GPIO_PIN_10))
  {
    __asm("nop");
  }

  // 10. Configure the SDA I/O as General Purpose Output Open-Drain , High level (Write 1 to GPIOx_ODR).
  PIN_SET(GPIOA, GPIO_PIN_9);

  // 11. Check SDA High level in GPIOx_IDR.
  while (SET != gpio_output_bit_get(GPIOA, GPIO_PIN_9))
  {
    __asm("nop");
  }

  // 12. Configure the SCL and SDA I/Os as Alternate function Open-Drain.
    gpio_af_set(GPIOA, GPIO_AF_4, GPIO_PIN_9);
    gpio_af_set(GPIOA, GPIO_AF_4, GPIO_PIN_10);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE,GPIO_PIN_9);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ,GPIO_PIN_9);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE,GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ,GPIO_PIN_10);

  // 13. Set SWRST bit in I2Cx_CR1 register.
	i2c_software_reset_config(I2C0, I2C_SRESET_SET);

  __asm("nop");

  // 14. Clear SWRST bit in I2Cx_CR1 register.
	i2c_software_reset_config(I2C0, I2C_SRESET_RESET);

  __asm("nop");

  // 15. Enable the I2C peripheral by setting the PE bit in I2Cx_CR1 register
	i2c_enable(I2C0);

  // Call initialization function.
    i2c_clock_config(I2C0,400000,I2C_DTCY_16_9);
    /* configure I2C address */
    i2c_mode_addr_config(I2C0,I2C_I2CMODE_ENABLE,I2C_ADDFORMAT_7BITS,0x81);
    /* enable I2C0 */
    i2c_enable(I2C0);
}

uint16_t result;
uint8_t p_buffer[2];
uint8_t xw09A_read_data(uint8_t button_num)
{  

		uint32_t cnt = 0;
		
    while(i2c_flag_get(I2C0, I2C_FLAG_I2CBSY))
		{
			if (cnt++ > 100000)
			{
				I2C_ClearBusyFlagErratum();				
				return false;
			}			
		}
		i2c_ackpos_config(I2C0,I2C_ACKPOS_NEXT);
		i2c_ack_config(I2C0, I2C_ACK_ENABLE);		
    i2c_start_on_bus(I2C0);
	
		cnt = 0;
		
		
    while(!i2c_flag_get(I2C0, I2C_FLAG_SBSEND))
		{
			if (cnt++ > 100000)
			{
				I2C_ClearBusyFlagErratum();				
				return false;
			}			
		}			
    i2c_master_addressing(I2C0, 0x81, I2C_RECEIVER);
		
		while(!i2c_flag_get(I2C0, I2C_FLAG_ADDSEND))
		{
			if (cnt++ > 100000)
			{
				I2C_ClearBusyFlagErratum();				
				return false;
			}			
		}				
		i2c_flag_clear(I2C0, I2C_FLAG_ADDSEND);
		i2c_stop_on_bus(I2C0);

		cnt = 0;

		while(!i2c_flag_get(I2C0, I2C_FLAG_RBNE))
		{
			if (cnt++ > 100000)
			{
				I2C_ClearBusyFlagErratum();				
				return false;
			}			
		}		
		p_buffer[0] = i2c_data_receive(I2C0);
		i2c_ack_config(I2C0, I2C_ACK_DISABLE);
	cnt = 0;
		while(!i2c_flag_get(I2C0, I2C_FLAG_RBNE))
		{
			if (cnt++ > 100000)
			{
				I2C_ClearBusyFlagErratum();				
				return false;
			}			
		}		
		p_buffer[1] = i2c_data_receive(I2C0);		
		i2c_stop_on_bus(I2C0);
        	
		//efff  f7ff
		//fbff	fdff
		//ff7f	ffbf

		result = ~(p_buffer[0] << 8 | p_buffer[1]);
    
		if(result & (1 << button_num))
			return true;
		else
			return false;
}


void SaveFlash()
{
	fmc_unlock();

	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);
	fmc_page_erase(SETTINGSADDR);
	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);
	
	
	uint32_t Address = SETTINGSADDR;
	uint8_t* AddressSrc = (uint8_t*)&_settings;
	_settings.crc = crc32_1byte(AddressSrc, offsetof(DeviceSettings, crc), 0xFFFFFFFF);
	int count = sizeof(_settings)/4 ;
	while(count--){
		fmc_word_program(Address, *(__IO uint32_t *)AddressSrc);
		Address += 4U;
		AddressSrc += 4U;
		
	}
	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);
	fmc_lock();
}

void DrawLeftRight(int offsetY = 0)
{
	pxs.drawCompressedBitmap(11, 36 + offsetY, img_left_png_comp);
	pxs.drawCompressedBitmap(136, 36 + offsetY, img_right_png_comp);
}

void DrawMenu()
{
	if (currentMenu == NULL)
		return;
	
	if (currentMenu->items == NULL)
	{
		DrawEditParameter();
		return;
	}

	pxs.clear();
	if(currentMenu->items[currentMenu->selected].ID != 711) // draw arrows where is needed
	{
		DrawLeftRight();
	}			
	
	const uint8_t *icon = NULL;
	char* text = NULL;
	
	switch (currentMenu->items[currentMenu->selected].ID)
	{
		case 1:
			icon = img_menu_heatmode_icon_png_comp;
			text = "Heat mode";
			break;
		case 2:
			icon = _settings.model_set == 2 ? img_power_select_png_comp : img_menu_power_mode_icon_png_comp;
			text = "Power mode";			
			break;
		case 3:
			icon = img_menu_heatset_icon_png_comp;
			text = "Heating elements";
			break;		
		case 4:
			icon = img_menu_timer_icon_png_comp;
			text = "Timer";
			break;
		case 5:
			icon = img_menu_setting_icon_png_comp;
			text = "Settings";
			break;
		case 6:
			icon = img_menu_program_icon_png_comp;
			text = "Programme";
			break;
		case 11:
			icon = img_menu_mode_comfort_png_comp;
			text = "Comfort";
			break;
		case 12:
			icon = img_menu_mode_eco_png_comp;
			text = "Eco";
			break;
		case 13:
			icon = img_menu_mode_anti_png_comp;
			text = "Anti-frost";
			break;
		case 31:
			icon = _settings.timerOn ? img_menu_timer_on_png_comp : img_menu_timer_off_png_comp;
			text = _settings.timerOn ? "Timer is on" : "Timer is off";
			break;
		case 32:
			icon = img_menu_settimer_png_comp;
			text = "Set timer";
			break;
		case 51:
			icon = img_menu_program_setup_icon_png_comp;
			text = "Setup";
			break;
		case 52:
			icon = _settings.calendarOn ? img_program_cal_on_icon_png_comp : img_program_cal_off_icon_png_comp;
			text = _settings.calendarOn ? "On" : "Off";
			break;
		case 53:
			icon = img_menu_program_custom_png_comp;
			text = "Custom day";
			break;
		case 41:
			icon = img_menu_setting_datetime_png_comp;
			text = "Date & time";
			break;
		case 42:
			icon = img_menu_display_png_comp;
			text = "Display";
			break;
		case 43:
			icon = _settings.soundOn ? img_menu_setting_sound_on_png_comp : img_menu_setting_sound_off_png_comp;
			text = "Sound";
			break;
		case 44:
			icon = img_menu_setting_service_png_comp;
			text = "Service";
			break;
		case 441:
			icon = img_menu_setting_reset_png_comp ;
			text = "Reset";
			break;
		case 442:
			icon = img_menu_setting_info_png_comp ;
			text = "Information";
			break;
		case 411:
			icon = img_menu_program_icon_png_comp;
			text = "Set date";
			break;
		case 412:
			icon = img_menu_settime_png_comp;
			text = "Set time";
			break;
		case 421:
			icon = img_menu_display_bri_png_comp;
			text = "Brightness";
			break;
		case 422:
			//icon = _settings.displayAutoOff ? img_menu_display_png_comp : img_menu_display_auto_png_comp;
		  icon = img_menu_display_auto_png_comp;
			text = "Auto switch off";
			break;
		case 611:
			icon = img_menu_open_window_png_comp;
			text = "Open window";
			break;		
		case 612:
			icon = img_menu_heatset_icon_png_comp;
			text = "Heating";
			break;
		case 711:
			icon = _settings.mycotherm ? img_mycotherm_on_png_comp : img_mycotherm_off_png_comp;
			text = _settings.mycotherm ? "Micathermic is On" : "Micathermic is Off";
			break;		
	}
	
	if (icon != NULL)
	{
		int16_t width, height;
		if (pxs.sizeCompressedBitmap(width, height, icon) == 0)
		{
			pxs.drawCompressedBitmap(SW / 2 - width / 2 + 1, 49 - height / 2, icon);
		}

		if(currentMenu->items[currentMenu->selected].ID == 32)
		{
			pxs.setFont(ElectroluxSansRegular14a);
			char buf[3];
			sprintf(buf, "%02d:%02d", (timer_time_set / 60), (timer_time_set % 60));
			DrawTextSelected(SW / 2 - pxs.getTextWidth(buf)/2 +1, 38, buf, false, false, 0,0);
		}
	}
	
	if (text != NULL)
		DrawMenuText(text);
}

void MainScreen()
{
	_backLight = 130;
	currentMenu = NULL;
	DrawMainScreen();
}

void DrawCustomDay(int _old = -1)
{
	struct Presets* _pr = NULL;
	pxs.setFont(ElectroluxSansRegular10a); // 18?
	_pr = &_settings.custom;
	for (int iy = 0; iy < 4; iy++)
	{
		for (int ix = 0; ix < 3; ix++)
		{
			int i = iy * 3 + ix + (currentMenu->selected > 11 ? 12 : 0);
			
			int cX = ix * 32 + 35;
			int cY = iy * 22 + 30;

			char buf[4];
			sprintf(buf, "%d", i);
			if (_pr->hour[i] > 3) _pr->hour[i] = pEco;
			//DrawTextAligment(cX, cY, 30, 20, buf, false, false, (currentMenu->selected == i) ? 2 : 0, BG_COLOR, modeColors[_pr->hour[i]]);
		  DrawTextAligment(cX, cY, 30, 20, buf, false, false, (currentMenu->selected == i) ? 0 : ((_pr->hour[i] == 3) ? 2 : 0), _pr->hour[i] == 3 ? MAIN_COLOR : BG_COLOR, currentMenu->selected == i ? GREEN_COLOR : modeColors[_pr->hour[i]]);
		}
	}
/*
	pxs.setFont(ElectroluxSansRegular20a); // 18?
	_pr = &_settings.custom;
	for (int iy = 0; iy < 4; iy++)
	{
		for (int ix = 0; ix < 6; ix++)
		{
			int i = iy * 6 + ix;
			
			int cX = ix * 50 + 10;
			int cY = iy * 40 + 60;

			char buf[4];
			sprintf(buf, "%d", i);
			if (_pr->hour[i] > 3) _pr->hour[i] = pEco;
			if (i == _old || currentMenu->selected == i || _old == -1)
				DrawTextAligment(cX, cY, 42, 32, buf, false, false, (currentMenu->selected == i) ? 2 : 0, MAIN_COLOR, modeColors[_pr->hour[i]]);
		}
	}
*/	
}

void CustomNext()
{
	if (currentMenu->counts > 0)
	{
		int old = currentMenu->selected;
		currentMenu->selected++;
		if (currentMenu->selected >= currentMenu->counts)
			currentMenu->selected = 0;
		
		DrawCustomDay(old);
	}
}

void CustomPrev()
{
	if (currentMenu->counts > 0)
	{
		int old = currentMenu->selected;
		currentMenu->selected--;
		if (currentMenu->selected < 0)
			currentMenu->selected = currentMenu->counts - 1;
		
		DrawCustomDay(old);
	}
}

void MenuNext()
{
	if (currentMenu->next != NULL)
		currentMenu->next(); //do function from menu struct
	
	else if (currentMenu->counts > 0)
	{
		smooth_backlight(0);
		currentMenu->selected++;
		if((_settings.model_set == 1) && (currentMenu->selected == 1) && (currentMenu == &_menu))
		{
			currentMenu->selected = 3;
		}
		else if((_settings.model_set == 2) && (currentMenu->selected == 2) && (currentMenu == &_menu))
		{
			currentMenu->selected = 3;
		}
		if (currentMenu->selected >= currentMenu->counts)
			currentMenu->selected = 0;
	
		DrawMenu();
		smooth_backlight(1);
	}
}

void MenuPrev()
{
	

	if (currentMenu->prev != NULL)
		currentMenu->prev();
	else if (currentMenu->counts > 0)
	{
		smooth_backlight(0);
		currentMenu->selected--;
		if(_settings.model_set == 1 && (currentMenu->selected == 2) && (currentMenu == &_menu))
		{
			currentMenu->selected = 0;
		}
		else if(_settings.model_set == 2 && (currentMenu->selected == 2) && (currentMenu == &_menu))
		{
			currentMenu->selected = 1;
		}
		if (currentMenu->selected < 0)
			currentMenu->selected = currentMenu->counts - 1;
		
		DrawMenu();
		smooth_backlight(1);
	}
}

void MenuOK()
{
	_backLight = 130;
	if (currentMenu->enter != NULL)
	{
		currentMenu->enter();
		return;
	}
	
	MenuItem_t* oldMenu = currentMenu;
	
	if (currentMenu->items != NULL && currentMenu->counts > 0)
	{
		MenuItem_t* nextMenu = &currentMenu->items[currentMenu->selected];
		
		nextMenu->parent = currentMenu;
		currentMenu = nextMenu;
		currentMenu->selected = 0;
		if (currentMenu->ID == 2 &&_settings.heatMode == HeatMode_User)
			currentMenu->selected = 1;
	}
	
	if (currentMenu->ID == 6) // if date/time not set - go to set (ID = 411)
	{
//		RTC_TimeTypeDef sTime;
//		RTC_DateTypeDef sDate;
//		HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
//		HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
//		if (sDate.Year < 19)
		{
			old_set = currentMenu->parent;
			currentMenu = &_settingsMenu[0];
			currentMenu->parent = old_set;
		}
	}


	// first enter from tree menu to edit parameter
	if (oldMenu->items != NULL && currentMenu->items == NULL)
		PrepareEditParameter();
	else
		AcceptParameter();
	
	smooth_backlight(0);
	DrawMenu();
	smooth_backlight(1);
}

void GoOK()
{
	_timeoutSaveFlash = GetSystemTick();

	if ((currentMenu->ID == 411) && (currentMenu->parent->parent->selected == 5))
	{
		_modeOK.parent = currentMenu->parent;
		currentMenu = &_datetimeMenu[1];
		currentMenu->parent = _modeOK.parent;
		smooth_backlight(0);
		pxs.clear();
		int16_t width, height;
		pxs.sizeCompressedBitmap(width, height, img_ok_png_comp);
		pxs.drawCompressedBitmap(SW / 2 - width / 2, SH / 2 - height / 2, img_ok_png_comp);
		smooth_backlight(1);
		delay_1ms(2000);
		
	}	
	else if ((currentMenu->ID == 412) && (currentMenu->parent->parent->selected == 5))
	{
		_modeOK.parent = currentMenu->parent;
		currentMenu = &_mainMenu[5];
		currentMenu->parent = old_set;
		smooth_backlight(0);
		pxs.clear();
		int16_t width, height;
		pxs.sizeCompressedBitmap(width, height, img_ok_png_comp);
	  pxs.drawCompressedBitmap(SW / 2 - width / 2, SH / 2 - height / 2, img_ok_png_comp);
		smooth_backlight(1);
		delay_1ms(2000);
	}		
  else
	{	
	  _modeOK.parent = currentMenu->parent;
	  currentMenu = &_modeOK;
	}
	idleTimeout = GetSystemTick();
}

void DrawDateEdit()
{
	char buf_day[3];
	char buf_month[3];
	char buf_year[5];
	sprintf(buf_day, "%02d", _dateTime.tm_mday);
	sprintf(buf_month, "%02d", _dateTime.tm_mon);
	sprintf(buf_year, "%04d", _dateTime.tm_year);
	
	pxs.setFont(ElectroluxSansRegular18a);
	//int16_t width = pxs.getTextWidth(buf_day);
	//DrawTextSelected(SW/2-width/2 - 25, 20, buf_day, (currentMenu->selected == 0), false, 8, 12);
	//width = pxs.getTextWidth(buf_month);
	//DrawTextSelected(SW/2-width/2 + 30, 20, buf_month, (currentMenu->selected == 1), false, 8, 12);
	
	//width = pxs.getTextWidth(buf_year);
	//DrawTextSelected(SW/2-width/2, 65, buf_year, (currentMenu->selected == 2), false, 8, 8);
	

	

	uint8_t widthX = pxs.getTextWidth(buf_year);
	DrawTextAligment(SW/2 - widthX/2 -2, 10, widthX + 10, 40, buf_year, (currentMenu->selected == 0),0,0,  (currentMenu->selected == 0) ? GREEN_COLOR : MAIN_COLOR, (currentMenu->selected == 0) ? MAIN_COLOR   : BG_COLOR );
	widthX = pxs.getTextWidth(buf_month);
	DrawTextAligment(SW/2 - widthX/2  - 30, 50, 40, 40, buf_month, (currentMenu->selected == 1),0,0,  (currentMenu->selected == 1) ? GREEN_COLOR : MAIN_COLOR, (currentMenu->selected == 1) ? MAIN_COLOR   : BG_COLOR );
	pxs.getTextWidth(buf_day);
	DrawTextAligment(SW/2 - widthX/2 + 28, 50, 40, 40, buf_day,   (currentMenu->selected == 2),0,0,  (currentMenu->selected == 2) ? GREEN_COLOR : MAIN_COLOR,  (currentMenu->selected == 2) ? MAIN_COLOR : BG_COLOR );


	pxs.setColor(MAIN_COLOR);
	pxs.setBackground(BG_COLOR);
	DrawMenuText((currentMenu->selected == 2) ? "Set day" : (currentMenu->selected == 1) ? "Set month" : "Set year");
}
void DrawTimeEdit()
{
	char buf[10];
	pxs.setFont(ElectroluxSansRegular18a);
	int16_t y = SH / 2 - pxs.getTextLineHeight() / 2 - 10;
	//int16_t width;
/*
	sprintf(buf, "%02d", _dateTime.tm_hour);
	DrawTextSelected(SW / 2 - pxs.getTextWidth(buf) - 15, y, buf, (currentMenu->selected == 0), false, 5, 11);

	sprintf(buf, "%02d", _dateTime.tm_min);
	DrawTextSelected(SW / 2 + 15, y, buf, (currentMenu->selected == 1), false, 5, 11);
	*/
	
	sprintf(buf, "%02d", _dateTime.tm_hour);
	uint8_t widthX = pxs.getTextWidth(buf);
	DrawTextAligment(SW/2 - widthX/2 - 33, y, 40, 40, buf, (currentMenu->selected == 0),0,0,  (currentMenu->selected == 0) ? GREEN_COLOR : MAIN_COLOR, (currentMenu->selected == 0) ? MAIN_COLOR : BG_COLOR );
	sprintf(buf, "%02d", _dateTime.tm_min);
	widthX = pxs.getTextWidth(buf);
	DrawTextAligment(SW/2 - widthX/2 + 24, y, 40, 40, buf, (currentMenu->selected == 1),0,0,  (currentMenu->selected == 1) ? GREEN_COLOR : MAIN_COLOR, (currentMenu->selected == 1) ? MAIN_COLOR : BG_COLOR );	
	
	
	
	pxs.setColor(MAIN_COLOR);
	pxs.setBackground(BG_COLOR);
	DrawTextAligment(SW / 2 - pxs.getTextWidth(":") / 2, y, 4, 34, ":",0);
	DrawMenuText((currentMenu->selected == 0) ? "Set hour" : "Set minute");
}

void DrawEditParameter()
{
	char buf[30];
	int16_t width, height;
	struct Presets* _pr = NULL;
	pxs.clear();
	
	switch (currentMenu->ID)
	{
		case 999:
			pxs.sizeCompressedBitmap(width, height, img_ok_png_comp);
			pxs.drawCompressedBitmap(SW / 2 - width / 2, SH / 2 - height / 2, img_ok_png_comp);
			break;
		case 2:
			DrawMenuText("Power level");
		  DrawLeftRight();
			width = (currentMenu->selected + 1) * 10 + currentMenu->selected * 15;
			for (int i = 0; i < currentMenu->selected + 1; i++)
			{
				pxs.setColor(YELLOW_COLOR);
				pxs.fillRectangle(SW / 2 - width / 2 + i * 10 + i * 12, 23, 8, 54);
			}			
		break;
		case 12: // eco
			DrawLeftRight(15);
			DrawMenuTitle2("Eco < Comfort");
			DrawTemperature(_tempConfig.desired, -16, 4);
		break;
		case 11: // comform
		case 13: // anti
			DrawLeftRight(15);
			DrawTemperature(_tempConfig.desired, -16, 4);
			break;
		case 52:
			DrawMenuTitle("Programme");
			pxs.setFont(ElectroluxSansRegular14a);
			DrawTextAligment(SW/2 - 60, 60, 50, 50,"ON", _onoffSet.parameter,0,0,  _onoffSet.parameter ? GREEN_COLOR : MAIN_COLOR, _onoffSet.parameter ? MAIN_COLOR : BG_COLOR );
			DrawTextAligment(SW/2 + 10, 60, 50, 50,"OFF", !_onoffSet.parameter,0,0, _onoffSet.parameter ? MAIN_COLOR : GREEN_COLOR, _onoffSet.parameter ? BG_COLOR : MAIN_COLOR );
			break;
		case 31:
		case 43:
		case 422:
			pxs.setFont(ElectroluxSansRegular14a);
			DrawTextAligment(SW/2 - 60, 35, 55, 55,"ON", _onoffSet.parameter,_onoffSet.current,0,  _onoffSet.parameter ? GREEN_COLOR : MAIN_COLOR, _onoffSet.parameter ? MAIN_COLOR : BG_COLOR );
			DrawTextAligment(SW/2 +  5, 35, 55, 55,"OFF", !_onoffSet.parameter,!_onoffSet.current,0, _onoffSet.parameter ? MAIN_COLOR : GREEN_COLOR, _onoffSet.parameter ? BG_COLOR : MAIN_COLOR );
			break;
		case 421:
			pxs.setFont(ElectroluxSansRegular14a);
			DrawTextAligment(SW/2 - 64, 35, 60, 60,"50%", _onoffSet.parameter,0,0,  _onoffSet.parameter ? GREEN_COLOR : MAIN_COLOR, _onoffSet.parameter ? MAIN_COLOR : BG_COLOR );
			DrawTextAligment(SW/2 +  5, 35, 60, 60,"100%", !_onoffSet.parameter,0,0, _onoffSet.parameter ? MAIN_COLOR : GREEN_COLOR, _onoffSet.parameter ? BG_COLOR : MAIN_COLOR );
			break;
		case 32:
			DrawTimeEdit();
			break;
		case 411: // set date
			DrawDateEdit();
			break;
		case 412: // set time
			DrawTimeEdit();
			break;
		case 441: // reset
			pxs.setFont(ElectroluxSansRegular10a);
			if (currentMenu->selected == 0)
			{
				int16_t width = pxs.getTextWidth("Reset all device");
				DrawTextSelected(SW/2 - width/2, 14, "Reset all device", false, false, 0, 0);
				width = pxs.getTextWidth("settings?");
				DrawTextSelected(SW/2 - width/2, 35, "settings?", false, false, 0, 0);
				//DrawTextAligment(0, 10, SW, 30, "Reset all device", false);
				//DrawTextAligment(0, 30, SW, 30, "settings?", false);
			}
			else if (currentMenu->selected == 1)
			{
				DrawTextAligment(0, 15, SW, 30, "Are you sure?", false);
			}

			pxs.setFont(ElectroluxSansRegular14a);
			
			DrawTextAligment(20, 65, 40, 40,"Yes", _onoffSet.parameter,0,0,  _onoffSet.parameter ? GREEN_COLOR : MAIN_COLOR, _onoffSet.parameter ? MAIN_COLOR : BG_COLOR );
			DrawTextAligment(105, 65, 40, 40,"No", !_onoffSet.parameter,0,0, _onoffSet.parameter ? MAIN_COLOR : GREEN_COLOR, _onoffSet.parameter ? BG_COLOR : MAIN_COLOR );
			//DrawTextSelected(20, SH - pxs.getTextLineHeight() - 20, "Yes", _onoffSet.parameter, false, 5, 12);
			//DrawTextSelected(110, SH - pxs.getTextLineHeight() - 20, "No", !_onoffSet.parameter, false, 5, 12);

			break;
		case 442: // info
			pxs.setFont(ElectroluxSansRegular10a);
			int16_t width = pxs.getTextWidth("Current firmware");
			DrawTextSelected(SW/2 - width/2, 40, "Current firmware", false, false, 0, 0);
		
			char buffer[20];
		  sprintf(buffer, "%s%s", "version: ", VERSION);		
		
			width = pxs.getTextWidth(buffer);
			DrawTextSelected(SW/2 - width/2, 60, buffer, false, false, 0, 0);		
		
			//DrawTextAligment(0, 40, SW, 30, "Current firmware", false);
			//DrawTextAligment(0, 60, SW, 30, "version: 12.0.02", false);
			break;
		case 51:
//			DrawLeftRight(15);
      rtc_current_time_get(&rtc_initpara);
			for (int i = (currentMenu->selected > 3 ? 4 : 0); i < (currentMenu->selected > 3 ? 7 : 4); i++)
			{
				if(currentMenu->selected < 4)
				{
					pxs.setColor(BG_COLOR);
					pxs.fillRectangle(0, 50, 25, 40);
					pxs.setColor(MAIN_COLOR);
					pxs.drawCompressedBitmap(136, 36 + 15, img_right_png_comp);		
					if (_settings.calendar[i] > 7) _settings.calendar[i] = 0;
					pxs.setColor(MAIN_COLOR);
					pxs.setFont(ElectroluxSansRegular10a);
					DrawTextAligment(_calendarInfo[i].x - 10, i > 1 ? (_calendarInfo[i].y - 3) : (_calendarInfo[i].y), 36, 15, (char*)_calendarInfo[i].week, false, false, 0);
					pxs.setFont(ElectroluxSansRegular10a);
					DrawTextAligment(_calendarInfo[i].x - 10, i > 1 ? (_calendarInfo[i].y + 12) : (_calendarInfo[i].y + 15), 36, 36, (char*)_calendarPresetName[_settings.calendar[i]], (currentMenu->selected == i), i == (rtc_initpara.rtc_day_of_week -1) ? 1 : 0 , 2, 
														currentMenu->selected == i ? GREEN_COLOR : MAIN_COLOR, currentMenu->selected == i ? MAIN_COLOR : BG_COLOR );
				}
				else
				{
					pxs.setColor(BG_COLOR);
					pxs.fillRectangle(137, 50, 20, 40);
					pxs.setColor(MAIN_COLOR);
					pxs.drawCompressedBitmap(11, 36 + 15, img_left_png_comp);
					if (_settings.calendar[i] > 7) _settings.calendar[i] = 0;
					pxs.setColor(MAIN_COLOR);
					pxs.setFont(ElectroluxSansRegular10a);
					DrawTextAligment(_calendarInfo[i].x + 5, i > 5 ? (_calendarInfo[i].y - 3) : (_calendarInfo[i].y), 36, 15, (char*)_calendarInfo[i].week, false, false, 0);
					pxs.setFont(ElectroluxSansRegular10a);
					DrawTextAligment(_calendarInfo[i].x + 5, i > 5 ? (_calendarInfo[i].y + 12) : (_calendarInfo[i].y + 15), 36, 36, (char*)_calendarPresetName[_settings.calendar[i]], (currentMenu->selected == i), i == (rtc_initpara.rtc_day_of_week -1) ? 1 : 0 , 2, 
														currentMenu->selected == i ? GREEN_COLOR : MAIN_COLOR, currentMenu->selected == i ? MAIN_COLOR : BG_COLOR );
					
				}
			}
			break;
		case 510:
			//DrawLeftRight(15);
			int8_t xoffset;
			if(currentMenu->selected < 4)
			{
				xoffset = -5;
			}
			else
			{
				xoffset = 9;
			}		
			switch (_presetSet.week)
			{
				case 0: DrawMenuTitle("Monday", xoffset); break;
				case 1: DrawMenuTitle("Tuesday", xoffset); break;
				case 2: DrawMenuTitle("Wednesday", xoffset); break;
				case 3: DrawMenuTitle("Thursday", xoffset); break;
				case 4: DrawMenuTitle("Friday", xoffset); break;
				case 5: DrawMenuTitle("Saturday", xoffset); break;
				case 6: DrawMenuTitle("Sunday", xoffset); break;
			}
			
			pxs.setFont(ElectroluxSansRegular10a);
			
			
			if(currentMenu->selected < 4)
			{
				pxs.setColor(BG_COLOR);
				pxs.fillRectangle(0, 50, 25, 40);
				pxs.setColor(MAIN_COLOR);
				pxs.drawCompressedBitmap(136, 36 + 15, img_right_png_comp);		
				for (int iy = 0; iy < 2; iy++)
				{
					for (int ix = 0; ix < 2; ix++)
					{
						int i = iy * 2 + ix + (currentMenu->selected > 3 ? 4 : 0);
						int16_t cX = ix * 50 + 30;
						int16_t cY = iy * 50 + 30;
						DrawTextAligment(ix ? (cX + 5) : cX, cY , 35, 35, (char*)_calendarPresetName[i], (currentMenu->selected == i), (_settings.calendar[_presetSet.week] == i), 2, 
														 currentMenu->selected == i ? GREEN_COLOR : MAIN_COLOR, currentMenu->selected == i ? MAIN_COLOR : BG_COLOR);
					}
				}
			}
			else
			{
				pxs.setColor(BG_COLOR);
				pxs.fillRectangle(137, 50, 20, 40);
				pxs.setColor(MAIN_COLOR);
				pxs.drawCompressedBitmap(11, 36 + 15, img_left_png_comp);
				for (int iy = 0; iy < 2; iy++)
				{
					for (int ix = 0; ix < 2; ix++)
					{
						int i = iy * 2 + ix + (currentMenu->selected > 3 ? 4 : 0);
						int16_t cX = ix * 50 + 45;
						int16_t cY = iy * 50 + 30;
						DrawTextAligment(ix  ? (cX + 5) : cX, cY, 35, 35, (char*)_calendarPresetName[i], (currentMenu->selected == i), (_settings.calendar[_presetSet.week] == i), 2, 
														 currentMenu->selected == i ? GREEN_COLOR : MAIN_COLOR, currentMenu->selected == i ? MAIN_COLOR : BG_COLOR);
					}
				}
			}	
			
			break;
		case 511:
			//DrawLeftRight(15);
			//int8_t xoffset;
			if(!currentMenu->selected)
			{
				xoffset = -7;
			}
			else
			{
				xoffset = 7;
			}
			switch (_presetSet.preset)
			{
				case 0: DrawMenuTitle("PRESET 1", xoffset); break;
				case 1: DrawMenuTitle("PRESET 2", xoffset); break;
				case 2: DrawMenuTitle("PRESET 3", xoffset); break;
				case 3: DrawMenuTitle("PRESET 4", xoffset); break;
				case 4: DrawMenuTitle("PRESET 5", xoffset); break;
				case 5: DrawMenuTitle("PRESET 6", xoffset); break;
				case 6: DrawMenuTitle("PRESET 7", xoffset); break;
				case 7: DrawMenuTitle("CUSTOM", xoffset); break;
			}

			pxs.setFont(ElectroluxSansRegular10a); // 18?
			_pr = (_presetSet.preset < 7) ? (struct Presets*)&_presets[_presetSet.preset] : &_settings.custom;

			if(!currentMenu->selected)
			{
				pxs.setColor(BG_COLOR);
				pxs.fillRectangle(0, 50, 25, 40);
				pxs.setColor(MAIN_COLOR);
				pxs.drawCompressedBitmap(136, 36 + 15, img_right_png_comp);					
				for (int iy = 0; iy < 4; iy++)
				{
					for (int ix = 0; ix < 3; ix++)
					{
						int i = iy * 3 + ix + (currentMenu->selected ? 12 : 0);
						
						int cX = ix * 32 + 25;
						int cY = iy * 22 + 28;

						sprintf(buf, "%d", i);
						if (_pr->hour[i] > 3) _pr->hour[i] = pEco;
						DrawTextAligment(cX, cY, 30, 20, buf, false, false, ((_pr->hour[i] == 3) ? 2 : 0), _pr->hour[i] == 3 ? MAIN_COLOR : BG_COLOR, modeColors[_pr->hour[i]]);
					}
				}
			}				
			else
			{
				pxs.setColor(BG_COLOR);
				pxs.fillRectangle(137, 50, 20, 40);
				pxs.setColor(MAIN_COLOR);
				pxs.drawCompressedBitmap(11, 36 + 15, img_left_png_comp);			
				for (int iy = 0; iy < 4; iy++)
				{
					for (int ix = 0; ix < 3; ix++)
					{
						int i = iy * 3 + ix + (currentMenu->selected ? 12 : 0);
						
						int cX = ix * 32 + 40;
						int cY = iy * 22 + 28;

						sprintf(buf, "%d", i);
						if (_pr->hour[i] > 3) _pr->hour[i] = pEco;
						DrawTextAligment(cX, cY, 30, 20, buf, false, false, ((_pr->hour[i] == 3) ? 2 : 0), _pr->hour[i] == 3 ? MAIN_COLOR : BG_COLOR, modeColors[_pr->hour[i]]);
					}
				}			
			}
			break;
		case 53: // custom day
			DrawLeftRight(15);
			DrawMenuTitle("CUSTOM 24h");
			DrawCustomDay();
			break;
		case 530: // custom day select mode
			DrawLeftRight();

			switch (currentMenu->selected)
			{
				case pComfort:
					if (pxs.sizeCompressedBitmap(width, height, img_menu_mode_comfort_png_comp) == 0) pxs.drawCompressedBitmap(SW / 2 - width / 2, 49 - height / 2, img_menu_mode_comfort_png_comp);
					DrawMenuText("Comfort");
					break;
				case pEco:
					if (pxs.sizeCompressedBitmap(width, height, img_menu_mode_eco_png_comp) == 0) pxs.drawCompressedBitmap(SW / 2 - width / 2, 49 - height / 2, img_menu_mode_eco_png_comp);
					DrawMenuText("Eco");
					break;
				case pAntiFrost:
					if (pxs.sizeCompressedBitmap(width, height, img_menu_mode_anti_png_comp) == 0) pxs.drawCompressedBitmap(SW / 2 - width / 2, 49 - height / 2, img_menu_mode_anti_png_comp);
					DrawMenuText("Anti-frost");
					break;
				default:
					if (pxs.sizeCompressedBitmap(width, height, img_mode_off_png_comp) == 0) pxs.drawCompressedBitmap(SW / 2 - width / 2, 49 - height / 2, img_mode_off_png_comp);
					DrawMenuText("Off");
					break;
			}
				break;
		case 711:
			pxs.setFont(ElectroluxSansRegular10a);
			int16_t width_t;
			if(_onoffSet.current)
			{
				width_t = pxs.getTextWidth("Switch off");
				pxs.print(SW / 2 - width_t/2, 10, "Switch off");				
			}
			else
			{
				width_t = pxs.getTextWidth("Switch on");
				pxs.print(SW / 2 - width_t/2, 10, "Switch on");				
			}
			width_t = pxs.getTextWidth("micathermic");
			pxs.print(SW / 2 - width_t/2, 27, "micathermic");			
			width_t = pxs.getTextWidth("heater");
			pxs.print(SW / 2 - width_t/2, 44, "heater?");				
			pxs.setFont(ElectroluxSansRegular14a);
			//DrawTextAligment(10, 60, 55, 55,"ON", _onoffSet.parameter,_onoffSet.current,0,  _onoffSet.parameter ? GREEN_COLOR : MAIN_COLOR, _onoffSet.parameter ? MAIN_COLOR : BG_COLOR );
			//DrawTextAligment(97, 60, 55, 55,"OFF", !_onoffSet.parameter,!_onoffSet.current,0, _onoffSet.parameter ? MAIN_COLOR : GREEN_COLOR, _onoffSet.parameter ? BG_COLOR : MAIN_COLOR );

			DrawTextAligment(20, 70, 40, 40,"Yes", _onoffSet.parameter,0,0,  _onoffSet.parameter ? GREEN_COLOR : MAIN_COLOR, _onoffSet.parameter ? MAIN_COLOR : BG_COLOR );
			DrawTextAligment(105, 70, 40, 40,"No", !_onoffSet.parameter,0,0, _onoffSet.parameter ? MAIN_COLOR : GREEN_COLOR, _onoffSet.parameter ? BG_COLOR : MAIN_COLOR );


		break;
		case 612:
			if(currentMenu->selected == 0)
			{
				if (pxs.sizeCompressedBitmap(width, height, img_both_on_png_comp) == 0)
				pxs.drawCompressedBitmap(SW/2 - width/2 + 2,  SH/2 - height - 19 , img_both_on_png_comp);			
			}
			else
			{
				if (pxs.sizeCompressedBitmap(width, height, img_both_off_png_comp) == 0)
				pxs.drawCompressedBitmap(SW/2 - width/2 + 2,  SH/2 - height - 18 , img_both_off_png_comp);				
			}
						
			if(currentMenu->selected == 1)
			{
				if (pxs.sizeCompressedBitmap(width, height, img_hh_on_png_comp) == 0)
				pxs.drawCompressedBitmap(SW/2 - width - 15 ,  SH/2 + height/2 - 26 , img_hh_on_png_comp);			
			}
			else
			{
				if (pxs.sizeCompressedBitmap(width, height, img_hh_off_png_comp) == 0)
				pxs.drawCompressedBitmap(SW/2 - width - 15 ,  SH/2 + height/2 - 28 , img_hh_off_png_comp);				
			}
			
			if(currentMenu->selected == 2)
			{
				if (pxs.sizeCompressedBitmap(width, height, img_mt_on_png_comp) == 0)
				pxs.drawCompressedBitmap(SW/2 + width - 20 ,  SH/2 + height/2 - 26 , img_mt_on_png_comp);			
			}
			else
			{
				if (pxs.sizeCompressedBitmap(width, height, img_mt_off_png_comp) == 0)
				pxs.drawCompressedBitmap(SW/2 + width - 20 ,  SH/2 + height/2 - 28 , img_mt_off_png_comp);				
			}
			
			DrawMenuText((currentMenu->selected == 0) ? "Use both" : "Use only");			
			break;				
		default:
			DrawMenuText("Not implemented");
			break;
	}
}

void PrepareEditParameter()
{
	
//	RTC_TimeTypeDef sTime;
//	RTC_DateTypeDef sDate;

	switch (currentMenu->ID)
	{
		case 2:
			currentMenu->selected = _settings.powerLevel - 1;
			break;
		case 11: // comform
			_tempConfig.desired = _settings.tempComfort;
			_tempConfig.min = MIN_TEMP_COMFORT;
			_tempConfig.max = MAX_TEMP_COMFORT;
			break;
		case 12: // eco
			_tempConfig.desired = _settings.tempEco;
			_tempConfig.min = 3;
			_tempConfig.max = 7;
			break;
		case 13: // anti
			_tempConfig.desired = _settings.tempAntifrost;
			_tempConfig.min = 3;
			_tempConfig.max = 7;
			break;
		case 31: // timer on/off
			_onoffSet.current = _settings.timerOn;
			_onoffSet.parameter = _settings.timerOn;
			break;
		case 32: // timer time
			_dateTime.tm_hour = _settings.timerTime / 60;
			_dateTime.tm_min = _settings.timerTime % 60;
			_dateTime.tm_sec = 0;
			currentMenu->selected = 0;
			break;
		case 43: // sound on/off
			_onoffSet.current = _settings.soundOn;
			_onoffSet.parameter = _settings.soundOn;
			break;
		case 711: 
			_onoffSet.current = _settings.mycotherm;
			_onoffSet.parameter = 0;	
			break;		
		case 411: // set date
			rtc_current_time_get(&rtc_initpara); 
			_dateTime.tm_mday = bcdToDec(rtc_initpara.rtc_date);
			_dateTime.tm_mon = bcdToDec(rtc_initpara.rtc_month);
			_dateTime.tm_year = (bcdToDec(rtc_initpara.rtc_year)) < 19 ? 2019 : (bcdToDec(rtc_initpara.rtc_year)) + 2000;
			currentMenu->selected = 0;
			break;
		case 412: // set time
			rtc_current_time_get(&rtc_initpara); 
			_dateTime.tm_hour = bcdToDec(rtc_initpara.rtc_hour);
			_dateTime.tm_min  = bcdToDec(rtc_initpara.rtc_minute);
			_dateTime.tm_sec  = bcdToDec(rtc_initpara.rtc_second);
			currentMenu->selected = 0;
			break;
		case 52: // calendar on/off
			_onoffSet.current = _settings.calendarOn;
			_onoffSet.parameter = _settings.calendarOn;
			break;
		case 51: // presets
			currentMenu->selected = 0;
			break;
		case 53: // custom day
			currentMenu->selected = 0;
			break;
		case 510: // presets
			currentMenu->selected = 0;
			break;
		case 612: 
			currentMenu->selected = _settings.heatset;
			break;
		case 421: // bight 50/100
			_onoffSet.current = !_settings.brightness;
			_onoffSet.parameter = !_settings.brightness;
			break;
		case 422: // auto on/off
			_onoffSet.current = _settings.displayAutoOff;
			_onoffSet.parameter = _settings.displayAutoOff;
			break;
		case 441: // reset
			currentMenu->selected = 0;
			_onoffSet.current = _onoffSet.current;
			_onoffSet.parameter = 0;
			break;
	}
}

rtc_parameter_struct rtc_init_param;
void AcceptParameter()
{
	
	switch (currentMenu->ID)
	{
		case 2:
			_settings.powerLevel = currentMenu->selected + 1;
			GoOK();
			break;
		case 11: // comform
			_settings.tempComfort = _tempConfig.desired;
			GoOK();
			break;
		case 12: // eco
			_settings.tempEco = _tempConfig.desired;
			GoOK();
			break;
		case 13: // anti
			_settings.tempAntifrost = _tempConfig.desired;
			GoOK();
			break;
		case 31:
			_settings.timerOn = _onoffSet.parameter;
		  if(_settings.timerOn)
			{
				if((getCalendarMode() == WorkMode_Off))
				{
					_settings.workMode = WorkMode_Comfort;
				}
				_settings.calendarOn = 0;
				_eventTimer = 0;
				InitTimer();
		  }
			GoOK();
			break;
		case 32:
	
			currentMenu->selected++;
			if ((currentMenu->selected > 0) && (currentMenu->selected < 2))
			{		
				_settings.timerTime = _dateTime.tm_hour * 60 + _dateTime.tm_min;
				timer_time_set = _settings.timerTime;
				pxs.clear();
				int16_t width, height;
				pxs.sizeCompressedBitmap(width, height, img_ok_png_comp);
				pxs.drawCompressedBitmap(SW / 2 - width / 2, SH / 2 - height / 2, img_ok_png_comp);
				delay_1ms(1000);
				pxs.clear();
				_timeoutSaveFlash = GetSystemTick();
				idleTimeout = GetSystemTick();	
				InitTimer();				
			}		
			if (currentMenu->selected == 2)
			{
				_settings.timerTime = _dateTime.tm_hour * 60 + _dateTime.tm_min;
				timer_time_set = _settings.timerTime;
				GoOK();
				InitTimer();
			}			
			
			break;
		case 43:
			_settings.soundOn = _onoffSet.parameter;
			GoOK();
			break;
		case 711:
			if((_settings.mycotherm == 1) && (_onoffSet.parameter == 1))
			{
				_settings.mycotherm = 0;
			}
			else if((_settings.mycotherm == 0) && (_onoffSet.parameter == 1))
			{
				_settings.mycotherm = 1;
			}
			GoOK();
			break;
		case 411: // date
			currentMenu->selected++;
		  rtc_current_time_get(&rtc_init_param); 
					if ((currentMenu->selected > 0) && (currentMenu->selected < 3))
					{
						if (isValidDate(_dateTime.tm_mday, _dateTime.tm_mon, _dateTime.tm_year))
				    {
							_dateTime.tm_mon--;
							_dateTime.tm_year -= 1900;

							time_t time_temp = mktime(&_dateTime);
							const struct tm* time_out = localtime(&time_temp);
							
							rtc_init_param.rtc_day_of_week = time_out->tm_wday == 0 ? RTC_SUNDAY : time_out->tm_wday;
							rtc_init_param.rtc_year = decToBcd(_dateTime.tm_year - 100);
							rtc_init_param.rtc_month = decToBcd(_dateTime.tm_mon + 1);
							rtc_init_param.rtc_date = decToBcd(_dateTime.tm_mday);		
							
							rtc_init(&rtc_init_param);
			
							_dateTime.tm_mon++;
							_dateTime.tm_year += 1900;
						}
						pxs.clear();
						int16_t width, height;
						pxs.sizeCompressedBitmap(width, height, img_ok_png_comp);
						pxs.drawCompressedBitmap(SW / 2 - width / 2, SH / 2 - height / 2, img_ok_png_comp);
						delay_1ms(1000);
						pxs.clear();
						_timeoutSaveFlash = GetSystemTick();
						idleTimeout = GetSystemTick();
					}
			if (currentMenu->selected == 3)
			{
				if (isValidDate(_dateTime.tm_mday, _dateTime.tm_mon, _dateTime.tm_year))
				{
					_dateTime.tm_mon--;
					_dateTime.tm_year -= 1900;

					time_t time_temp = mktime(&_dateTime);
					const struct tm* time_out = localtime(&time_temp);
					rtc_init_param.rtc_day_of_week = time_out->tm_wday == 0 ? RTC_SUNDAY : time_out->tm_wday;
					rtc_init_param.rtc_year = decToBcd(_dateTime.tm_year - 100);
					rtc_init_param.rtc_month = decToBcd(_dateTime.tm_mon + 1);
					rtc_init_param.rtc_date = decToBcd(_dateTime.tm_mday);
					rtc_init(&rtc_init_param);
					GoOK();
				}
				else
				{
					currentMenu->selected = 0;
				}
			}
			break;
		case 412: // time
			currentMenu->selected++;
		  rtc_current_time_get(&rtc_init_param); 
			if ((currentMenu->selected > 0) && (currentMenu->selected < 2))
			{		
				rtc_init_param.rtc_hour = decToBcd(_dateTime.tm_hour);
				rtc_init_param.rtc_minute = decToBcd(_dateTime.tm_min);
				rtc_init_param.rtc_second = 0x00;
				rtc_init(&rtc_init_param);
				pxs.clear();
				int16_t width, height;
				pxs.sizeCompressedBitmap(width, height, img_ok_png_comp);
				pxs.drawCompressedBitmap(SW / 2 - width / 2, SH / 2 - height / 2, img_ok_png_comp);
				delay_1ms(1000);
				pxs.clear();
				_timeoutSaveFlash = GetSystemTick();
				idleTimeout = GetSystemTick();				
			}
			if (currentMenu->selected == 2)
			{
				rtc_init_param.rtc_hour = decToBcd(_dateTime.tm_hour);
				rtc_init_param.rtc_minute = decToBcd(_dateTime.tm_min);
				rtc_init_param.rtc_second = 0x00;
				rtc_init(&rtc_init_param);
				GoOK();				
			}
			break;
		case 52:
			_settings.calendarOn = _onoffSet.parameter;		
		  if(_settings.calendarOn)
			{
				_settings.heatMode = HeatMode_Auto;
				_settings.timerOn = 0;
				_eventTimer = 0;
				InitTimer();
		  }		
			GoOK();	
			break;
		case 53: // custom day
			uint8_t select = currentMenu->selected;
			_selectModeMenu.parent = currentMenu;
			currentMenu = &_selectModeMenu;
			currentMenu->selected = _settings.custom.hour[select];
			break;
		case 530: // custom day
			_settings.custom.hour[currentMenu->parent->selected] = currentMenu->selected;
			GoOK();
			break;
		case 51: // presets
			_presetSet.week = currentMenu->selected;
			_presetSet.preset = _settings.calendar[_presetSet.week];
			_presetMenu.parent = currentMenu;
			currentMenu = &_presetMenu;
			currentMenu->selected = 0;
			break;
		case 510: // presets
			_presetSet.preset = currentMenu->selected;
			_presetViewMenu.parent = currentMenu;
			currentMenu = &_presetViewMenu;
			currentMenu->selected = 0;
			break;
		case 511: // presets
			_settings.calendar[_presetSet.week] = _presetSet.preset;
			GoOK();
			break;
		case 612: 
			_settings.heatset = currentMenu->selected;
			GoOK();
			break;		
		case 421:
			_settings.brightness = !_onoffSet.parameter;
			if (_settings.brightness)
			{
				_stateBrightness = StateBrightness_ON;
			}
			else
			{
				_stateBrightness = StateBrightness_LOW;
			}
			GoOK();
			break;
		case 422:
			_settings.displayAutoOff = _onoffSet.parameter;
			GoOK();
			break;
		case 441: // reset
			if (!_onoffSet.parameter)
			{
				MenuBack();
				break;
			}
			
			currentMenu->selected++;
			if (currentMenu->selected == 1)
				_onoffSet.current = _onoffSet.parameter = 1;
			else if (currentMenu->selected == 2)
			{
				ResetAllSettings();
				//GoOK();
				SaveFlash();
				NVIC_SystemReset();
			}
			break;
	}
}

void TempMinus()
{
	
	_tempConfig.desired--;
	if (_tempConfig.desired < _tempConfig.min)
			_tempConfig.desired = _tempConfig.max;

	DrawEditParameter();
}

void TempPlus()
{
	
	_tempConfig.desired++;
	if (_tempConfig.desired > _tempConfig.max)
			_tempConfig.desired = _tempConfig.min;

	DrawEditParameter();
}

void DateMinus()
{
	
	
	if (currentMenu->selected == 2)
	{
		_dateTime.tm_mday--;
		if (_dateTime.tm_mday == 0)
		{
			_dateTime.tm_mday = 31;
			while (!isValidDate(_dateTime.tm_mday, _dateTime.tm_mon, _dateTime.tm_year))
				_dateTime.tm_mday--;
		}
	}
	else if (currentMenu->selected == 1)
	{
		_dateTime.tm_mon--;
		if (_dateTime.tm_mon == 0)
			_dateTime.tm_mon = 12;
	}
	else if (currentMenu->selected == 0)
	{
		_dateTime.tm_year--;
		if (_dateTime.tm_year < 2019)
			_dateTime.tm_year = 2099;
	}
	DrawEditParameter();
}

void DatePlus()
{
	
	
	if (currentMenu->selected == 2)
	{
		_dateTime.tm_mday++;
		if (!isValidDate(_dateTime.tm_mday, _dateTime.tm_mon, _dateTime.tm_year))
			_dateTime.tm_mday = 1;
	}
	else if (currentMenu->selected == 1)
	{
		_dateTime.tm_mon++;
		if (_dateTime.tm_mon == 13)
			_dateTime.tm_mon = 1;
	}
	else if (currentMenu->selected == 0)
	{
		_dateTime.tm_year++;
		if (_dateTime.tm_year == 2100)
			_dateTime.tm_year = 2019;
	}
	DrawEditParameter();
}

void TimeMinus()
{
	if (currentMenu->selected == 0)
	{
		_dateTime.tm_hour--;
		if((currentMenu->ID == 32) && (_dateTime.tm_hour == 0) && (_dateTime.tm_min == 0))
		{
			_dateTime.tm_min = 1;
		}
		if (_dateTime.tm_hour < 0)
			_dateTime.tm_hour = 23;
	}
	else if (currentMenu->selected == 1)
	{
		_dateTime.tm_min--;
		if((currentMenu->ID == 32) && (_dateTime.tm_hour == 0))
		{			
			if (_dateTime.tm_min < 1)
			_dateTime.tm_min = 59;
		}
		else
		{
		if (_dateTime.tm_min < 0)
			_dateTime.tm_min = 59;
		}
	}
	DrawEditParameter();
}

void TimePlus()
{
	if (currentMenu->selected == 0)
	{
		_dateTime.tm_hour++;
		if (_dateTime.tm_hour > 23)
			_dateTime.tm_hour = 0;

		if((currentMenu->ID == 32) && (_dateTime.tm_hour == 0) && (_dateTime.tm_min == 0))
		{
			_dateTime.tm_min = 1;
		}
	}
	else if (currentMenu->selected == 1)
	{		
		_dateTime.tm_min++;
		if((currentMenu->ID == 32) && (_dateTime.tm_hour == 0))
		{			
			if (_dateTime.tm_min > 59)
			_dateTime.tm_min = 1;
		}
		else
		{
		if (_dateTime.tm_min > 59)
			_dateTime.tm_min = 0;
		}
	}
	DrawEditParameter();
}

void Off()
{
	if (_onoffSet.parameter == 0)
		_onoffSet.parameter = 1;
	else
		_onoffSet.parameter = 0;

	DrawEditParameter();
}

void On()
{
	if (_onoffSet.parameter == 0)
		_onoffSet.parameter = 1;
	else
		_onoffSet.parameter = 0;

	DrawEditParameter();
}



void MenuBack()
{
	_backLight = 130;
	if (currentMenu->parent != NULL) 
	{
		smooth_backlight(0);
		// if back in schedule to calendar on/off
		if ((currentMenu->ID == 51) && (!_settings.calendarOn))
		{
			struct MenuItem* old = currentMenu->parent;
			currentMenu = &_programMenu[1];
			currentMenu->parent = old;
			_onoffSet.current = _settings.calendarOn;
			_onoffSet.parameter = _settings.calendarOn;
			DrawMenu();
		}
		else
		{
			if(currentMenu->ID == 441) 
			{
				_onoffSet.current = 0;
				_onoffSet.parameter = 0;				
			}
			currentMenu = currentMenu->parent;
			DrawMenu();
		}
		smooth_backlight(1);
	}
	else
	{
		MainScreen();
	}
}

const int MAX_VALID_YR = 2099; 
const int MIN_VALID_YR = 2019; 
  
bool isLeap(int year) 
{ 
	return (((year % 4 == 0) &&  (year % 100 != 0)) ||  (year % 400 == 0)); 
} 

bool isValidDate(int d, int m, int y) 
{ 
    // If year, month and day  
    // are not in given range 
    if (y > MAX_VALID_YR || y < MIN_VALID_YR) 
			return false; 
		if (m < 1 || m > 12) 
			return false; 
    if (d < 1 || d > 31) 
			return false; 
  
    // Handle February month  
    // with leap year 
    if (m == 2) 
    { 
        if (isLeap(y)) 
        return (d <= 29); 
        else
        return (d <= 28); 
    } 
  
    // Months of April, June,  
    // Sept and Nov must have  
    // number of days less than 
    // or equal to 30. 
    if (m == 4 || m == 6 || 
        m == 9 || m == 11) 
        return (d <= 30); 
  
    return true; 
} 

WorkMode getCalendarMode()
{
	rtc_current_time_get(&rtc_initpara); 
	struct Presets* _pr = (_settings.calendar[rtc_initpara.rtc_day_of_week - 1] < 7) ? (struct Presets*)&_presets[_settings.calendar[rtc_initpara.rtc_day_of_week - 1]] : &_settings.custom;
	return (WorkMode)_pr->hour[bcdToDec(rtc_initpara.rtc_hour)];
}

void EnterMenu()
{
	smooth_backlight(0);	
	currentMenu = &_menu;
	currentMenu->selected = 0;
	
	DrawMenu();
	smooth_backlight(1);	
}

void SetPower(int8_t value)
{
	if (value < 0)
		value = 0;
	if (value > 20)
		value = 20;

		
	if (value == 20) // power on
	{
		switch(_settings.heatset){
			case 0:
				_currentPower = value;
				LL_GPIO_SetOutputPin(GPIOA, GPIO_PIN_8);   // LOW
				LL_GPIO_SetOutputPin(GPIOA, GPIO_PIN_11);		// MICA
				if(_settings.powerLevel == 2)
				{
					LL_GPIO_SetOutputPin(GPIOB, GPIO_PIN_15); // HIGH
				}
				else
				{
					LL_GPIO_ResetOutputPin(GPIOB, GPIO_PIN_15); // HIGH OFF
				}
				break;
			case 1:
				_currentPower = value;
				LL_GPIO_SetOutputPin(GPIOA, GPIO_PIN_8);   // LOW
				LL_GPIO_ResetOutputPin(GPIOA, GPIO_PIN_11); // MICA
				if(_settings.powerLevel == 2)
				{
					LL_GPIO_SetOutputPin(GPIOB, GPIO_PIN_15); // HIGH
				}	
				else
				{
					LL_GPIO_ResetOutputPin(GPIOB, GPIO_PIN_15); // HIGH
				}				
				break;
			case 2:
				LL_GPIO_ResetOutputPin(GPIOB, GPIO_PIN_15); // HIGH
				LL_GPIO_ResetOutputPin(GPIOA, GPIO_PIN_8); // LOW
			  LL_GPIO_SetOutputPin(GPIOA, GPIO_PIN_11);   // MICA
				break;
		}
	}
	else
	{
		_currentPower = value;
		LL_GPIO_ResetOutputPin(GPIOB, GPIO_PIN_15); // ALL OFF
		LL_GPIO_ResetOutputPin(GPIOA, GPIO_PIN_8);
		LL_GPIO_ResetOutputPin(GPIOA, GPIO_PIN_11);		
	}
	
	//Enable MICA
	if(_settings.mycotherm && window_is_opened && !window_was_opened)
	{
		LL_GPIO_SetOutputPin(GPIOA, GPIO_PIN_11);
	}
}
	

void beep()
{
	if (_settings.soundOn)
	{
		for(uint8_t i = 0; i<30; i++)
		{
			timer_channel_output_pulse_value_config(TIMER2,TIMER_CH_0,i);
			delay_1ms(2);
		}		
		for(int8_t i = 30; i>=0; i--)
		{
			timer_channel_output_pulse_value_config(TIMER2,TIMER_CH_0,i);
			delay_1ms(3);
		}
	}
}

int8_t getTemperature()
{
	while(adc_flag_get(ADC_FLAG_EOC))
	{
		raw = adc_regular_data_read();
		delay_1ms(1);
		adc_flag_clear(ADC_FLAG_EOC);
	}
	if (raw >= 50 && raw <= 4000)
	{
		float R = BALANCE_RESISTOR * ((4095.0 / raw) - 1);
		//double tKelvin = (BETA * ROOM_TEMP) / (BETA + (ROOM_TEMP * std::log(float(R / RESISTOR_ROOM_TEMP))));
		//double tKelvin = (BETA + (ROOM_TEMP * std::log(float(R / RESISTOR_ROOM_TEMP)))) / (std::log(float(R / RESISTOR_ROOM_TEMP)));
		//return (int)(tKelvin - 273.15);
		float steinhart;
		steinhart = R / RESISTOR_ROOM_TEMP; // (R/Ro)
		steinhart = std::log(float(steinhart)); // ln(R/Ro)
		steinhart /= BETA; // 1/B * ln(R/Ro)
		steinhart += 1.0 / (ROOM_TEMP); // + (1/To)
		steinhart = 1.0 / steinhart; // Invert
		steinhart -= 273.15; // convert to C
		if((steinhart <= (temp_steinhart + 3)) && (steinhart >= (temp_steinhart - 3))) // if temp increase or decrease impulsively, we skip averaging
		{
			steinhart = (steinhart * 0.3) + (temp_steinhart * 0.7);
		}
		temp_steinhart = steinhart;
		return (int8_t)rint(steinhart);
	}
	else if(raw < 50)
	{
		return -127;
	}
		else if(raw > 4000)
	{
		return 127;
	}
}

void DrawWindowOpen()
{
	_stateBrightness = StateBrightness_ON;
	smooth_backlight(1);
	if (_timerBlink < GetSystemTick())
	{
		_blink = !_blink;
		_timerBlink = GetSystemTick() + 500;

		pxs.clear();
		if (_blink)
			pxs.drawCompressedBitmap(52, 35, (uint8_t*)img_WindowOpen_png_comp);
	}
}

void DrawTemperature(int8_t temp, int8_t xo, int8_t yo)
{
	pxs.setColor(MAIN_COLOR);
	char buffer[20];
	sprintf(buffer, "%d", temp);
	pxs.setFont(ElectroluxSansLight40a);
	int widthX = pxs.getTextWidth(buffer);
	int cX = SW / 2 - widthX / 2 + xo + 5;
	pxs.print(cX, 40 + yo, buffer);
	pxs.setFont(ElectroluxSansLight16a);
	pxs.print(cX + widthX, 40 + yo, "\xB0\x43");
	_xWifi = cX + widthX;
}


void DrawMenuText(const char *text)
{
	pxs.setColor(MAIN_COLOR);
	pxs.setFont(ElectroluxSansRegular10a);
	int16_t width;
	switch (currentMenu->items[currentMenu->selected].ID)
	{
		case 3:
			width = pxs.getTextWidth("Heating");
			pxs.print(SW / 2 - width / 2, 86, "Heating");
		  width = pxs.getTextWidth("elements");
			pxs.print(SW / 2 - width / 2, 104, "elements");		
			break;
		case 711:
			width = pxs.getTextWidth("Micathermic");
			pxs.print(SW / 2 - width / 2, 84, "Micathermic");
			char *text1 = _settings.mycotherm ? "heater is On" : "heater is Off";
		  width = pxs.getTextWidth(text1);
			pxs.print(SW / 2 - width / 2, 102, text1);	
			break;
		default:
			width = pxs.getTextWidth((char*)text);
			pxs.print(SW / 2 - width / 2, 98, (char*)text);
	}
}

void DrawMenuTitle(const char *text, int8_t xo)
{
	pxs.setColor(MAIN_COLOR);
	pxs.setFont(ElectroluxSansRegular10a);
	int16_t width = pxs.getTextWidth((char*)text);
	pxs.print(SW / 2 - width / 2 + xo, 10, (char*)text);
}

void DrawMenuTitle2(const char *text)
{
	pxs.setColor(MAIN_COLOR);
	pxs.setFont(ElectroluxSansRegular10a);
	int16_t width = pxs.getTextWidth((char*)text);
	pxs.print(SW / 2 - width / 2, 10, (char*)text);
}

void DrawTextAligment(int16_t x, int16_t y, int16_t w, int16_t h, char* text, bool selected, bool underline, uint8_t border, RGB fore, RGB back)
{
	int16_t width = pxs.getTextWidth(text);
	int16_t height = pxs.getTextLineHeight();
	
	int16_t cX = x + w / 2 - width / 2;
	int16_t cY = y + h / 2 - height / 2;

	pxs.setColor(selected ? fore : back);
	pxs.fillRectangle(x, y, w, h);
	
	pxs.setColor(!selected ? fore : back);
	
	if (border > 0 && !selected)
	{
		for (int i = 0; i < border; i++)
			pxs.drawRectangle(x + i, y + i, w - i * 2, h - i * 2);
	}
	
	pxs.setBackground(selected ? fore : back);
	pxs.print(cX, cY, text);

	if (underline)
	{
		pxs.setColor(!selected ? fore : back);
		pxs.fillRectangle(cX, cY + height - 2, width, 3);
	}

	pxs.setBackground(BG_COLOR);
}
	
void DrawTextSelected(int16_t x, int16_t y, char* text, bool selected, bool underline = false, int16_t oX = 5, int16_t oY = 5)
{
	pxs.setColor(selected ? MAIN_COLOR : BG_COLOR);
	int16_t width = pxs.getTextWidth(text);
	int16_t height = pxs.getTextLineHeight();
	pxs.fillRectangle(x - oX, y - oY, width + oX * 2, height + oY * 2);
	pxs.setColor(!selected ? MAIN_COLOR : BG_COLOR);
	pxs.setBackground(selected ? MAIN_COLOR : BG_COLOR);
	pxs.print(x, y, text);
	
	if (underline)
	{
		pxs.setColor(!selected ? MAIN_COLOR : BG_COLOR);
		pxs.fillRectangle(x, y + height + 5, width, 4);
	}

	pxs.setBackground(BG_COLOR);
}

void DrawMainScreen(uint32_t updater)
{
	if (currentMenu != NULL)
		return;
	
	
	if (updater == 0x01)
	{
		pxs.setColor(BG_COLOR);
		pxs.fillRectangle(50, 20, 100, _settings.model_set ? 70 : 50);
	}
	else
	{
		smooth_backlight(0);
		pxs.clear();
	}
	pxs.setColor(MAIN_COLOR);
	
	switch (_settings.workMode)
	{
		case WorkMode_Comfort:
			DrawTemperature(getModeTemperature(), 7, _settings.model_set ? 0 : -16);
			pxs.drawCompressedBitmap(20, 12, (uint8_t*)img_icon_comfort1_png_comp);
			break;
		case WorkMode_Eco:
			DrawTemperature(getModeTemperature(), 7, _settings.model_set ? 0 : -16);
			pxs.drawCompressedBitmap(20, 12, (uint8_t*)img_icon_eco1_png_comp);
			break;
		case WorkMode_Antifrost:
			DrawTemperature(getModeTemperature(), 7, _settings.model_set ? 0 : -16);
			pxs.drawCompressedBitmap(21, 12, (uint8_t*)img_icon_antifrost1_png_comp);
			break;
		case WorkMode_Off:
			pxs.drawCompressedBitmap(75, 28, (uint8_t*)img_menu_program_off_png_comp);
			pxs.setFont(ElectroluxSansRegular14a);
			DrawTextAligment(64, 85, 60, 20, "Off", false, false);
			break;
	}
//=====
	int16_t width;
	int16_t height;
	if((_settings.model_set == 0) && (_settings.workMode != WorkMode_Off))
	{
		if ((power_current == 20)) // max power
		{		
			if(_settings.heatset < 2)
			{					
				if (pxs.sizeCompressedBitmap(width, height, img_hh_on_icon_png_comp) == 0)
				{
					pxs.setColor(BG_COLOR);
					pxs.fillRectangle(SW/2 - 17, SH - 42, 29, 29);	
					pxs.setColor(MAIN_COLOR);
					pxs.drawCompressedBitmap(SW/2 - width + 11 ,  SH - height - 2 -11, img_hh_on_icon_png_comp);	
				}		
				pxs.setColor(BG_COLOR);
				pxs.fillRectangle(93, 87, 8, 27);
				pxs.setColor(YELLOW_COLOR);
				pxs.fillRectangle(93, 102, 6, 13);
				if(_settings.powerLevel == 2)
				{				
					pxs.fillRectangle(93, 87, 6, 13);	
				}				
			}
			else
			{
				if (pxs.sizeCompressedBitmap(width, height, img_hh_off_icon_png_comp) == 0)
				pxs.drawCompressedBitmap(SW/2 - width + 19 ,  SH - height - 2 -11, img_hh_off_icon_png_comp);			
			}						
			
			if((_settings.heatset == 2) || (_settings.heatset == 0))
			{
				if (pxs.sizeCompressedBitmap(width, height, img_mt_on_icon_png_comp) == 0)
				{
					pxs.setColor(BG_COLOR);
					pxs.fillRectangle(SW/2 + 33, SH - 42, 36, 29);	
					pxs.setColor(MAIN_COLOR);	
					pxs.drawCompressedBitmap(SW/2 + width +6 ,  SH - height - 2 -11, img_mt_on_icon_png_comp);		
				}					
			}
			else
			{
				if (pxs.sizeCompressedBitmap(width, height, img_mt_off_icon_png_comp) == 0)
				pxs.drawCompressedBitmap(SW/2 + width +4 ,  SH - height - 2 -11, img_mt_off_icon_png_comp);			
			}		
		}
		else
		{				

				if (pxs.sizeCompressedBitmap(width, height, img_hh_off_icon_png_comp) == 0)
				pxs.drawCompressedBitmap(SW/2 - width + 19,  SH - height - 2 -11, img_hh_off_icon_png_comp);		

				if (pxs.sizeCompressedBitmap(width, height, img_mt_off_icon_png_comp) == 0)
				pxs.drawCompressedBitmap(SW/2 + width +4 ,  SH - height - 2 -11, img_mt_off_icon_png_comp);

		}	
	}
	else 	if((_settings.model_set == 2) && (_settings.workMode != WorkMode_Off))
	{
			if ((power_current == 20)) // max power
			{
				if((currentMenu == NULL)  && !_error)
				{
					pxs.setColor(YELLOW_COLOR);
					pxs.fillRectangle(20, 110, 57, 5);
					if(_settings.powerLevel == 2)
					{				
						pxs.fillRectangle(85, 110, 57, 5);
					}
					else
					{
						pxs.setColor(GREY_COLOR);
						pxs.fillRectangle(85, 110, 57, 5);
					}
				}
			}
			else
			{
				if((currentMenu == NULL) && !_error)
				{	
					pxs.setColor(GREY_COLOR);
					pxs.fillRectangle(20, 110, 57, 5);
					pxs.fillRectangle(85, 110, 57, 5);
				}
			}
	}
//=====
	
	if ((_settings.modeOpenWindow) && (_settings.workMode != WorkMode_Off))
		pxs.drawCompressedBitmap(20, _settings.model_set == 2 ? 78 : 87, (uint8_t*)img_icon_open_png_comp);

	if (_settings.timerOn == 1)
		pxs.drawCompressedBitmap(20, _settings.model_set == 2 ? 45 : 49, (uint8_t*)img_icon_timer_png_comp);
	else if (_settings.calendarOn == 1)
	{
		if(_settings.workMode == WorkMode_Off)
		{
			pxs.drawCompressedBitmap(20, 74, (uint8_t*)img_icon_calendar_png_comp);
		}
		else
		{
		  pxs.drawCompressedBitmap(20, _settings.model_set == 2 ? 45 : 48, (uint8_t*)img_icon_calendar_png_comp);
		}
	}
	if (updater != 0x01)
	{
		smooth_backlight(1);
	}
}

int8_t getModeTemperature()
{
	switch (_settings.workMode)
	{
		case WorkMode_Comfort:
			return _settings.tempComfort;
		case WorkMode_Eco:
			return (_settings.tempComfort - _settings.tempEco);
		case WorkMode_Antifrost:
			return _settings.tempAntifrost;
		default:
			return 0;
	}
}
void rtc_alarm_callback()
{

	if (_settings.timerOn)
	{
		if (timer_time_set > 0)
		{
			timer_time_set--;
		}
		
		if (timer_time_set == 0)
		{
			timer_time_set = _settings.timerTime;
			_eventTimer = 1;
		}
		//query_settings();
	}
	else
	{
		_eventTimer = 1;
	}
}
void InitTimer()
{
	if (_settings.on == 0 || (_settings.timerOn == 0 && _settings.calendarOn == 0))
	{
		rtc_alarm_disable();
		return;
	}
	rtc_current_time_get(&rtc_initpara); 
	
	if (bcdToDec(rtc_initpara.rtc_year) < 19 && _settings.calendarOn)
	{
		// disable timer and calendar
		_settings.timerOn = 0;
		_settings.calendarOn = 0;
		_settings.workMode = WorkMode_Comfort;
		rtc_alarm_disable();
		return;
	}	
	
	rtc_alarm_disable(); 
//-----------------------------------------------------	
	if (_settings.timerOn == 1)
	{
    rtc_alarm.rtc_alarm_hour = 0x00;
    rtc_alarm.rtc_alarm_minute = 0x00;
    rtc_alarm.rtc_alarm_second = rtc_initpara.rtc_second;
		rtc_alarm.rtc_alarm_mask = RTC_ALARM_MINUTE_MASK | RTC_ALARM_HOUR_MASK | RTC_ALARM_DATE_MASK;
		
		rtc_alarm.rtc_weekday_or_date = RTC_ALARM_WEEKDAY_SELECTED;
		rtc_alarm.rtc_alarm_day = RTC_WEDSDAY;
		rtc_alarm_config(&rtc_alarm);
		rtc_flag_clear(RTC_STAT_ALRM0F);
		//rtc_interrupt_enable(RTC_INT_ALARM);  
		rtc_alarm_enable();		
	}
	else if (_settings.calendarOn == 1)
	{
		
    rtc_alarm.rtc_alarm_hour = 0x00;
    rtc_alarm.rtc_alarm_minute = 0x00;
    rtc_alarm.rtc_alarm_second = 0x00;
		rtc_alarm.rtc_alarm_mask = RTC_ALARM_MINUTE_MASK | RTC_ALARM_HOUR_MASK | RTC_ALARM_SECOND_MASK | RTC_ALARM_DATE_MASK;
		
		
		rtc_alarm.rtc_weekday_or_date = RTC_ALARM_WEEKDAY_SELECTED;
		rtc_alarm.rtc_alarm_day = RTC_WEDSDAY;
		rtc_alarm_config(&rtc_alarm);
		rtc_flag_clear(RTC_STAT_ALRM0F);
		//rtc_interrupt_enable(RTC_INT_ALARM);  
		rtc_alarm_enable();
		
		//alarm_set(0);
	}	
}


void ResetAllSettings()
{

	_settings.on = 1;
	_settings.blocked = 0;
	_settings.tempAntifrost = 5;
	_settings.tempComfort = 24;
	_settings.tempEco = 4;
	_settings.calendarOn = 0;
	_settings.mycotherm = 1;
	_settings.powerLevel = 2;
	switch(_settings.model_set)
	{
		case 0: //mica+hedge
			_settings.heatset = 0;
			break;
		case 1: //mica
			_settings.heatset = 2;
		  _settings.powerLevel = 1;
			break;
		case 2: //hedge
			_settings.heatset = 1;
			break;
	}
	
	_settings.workMode = WorkMode_Comfort;
	_settings.heatMode = HeatMode_Auto;
	_settings.modeOpenWindow = 0;
	_settings.timerOn = 0;
	_settings.timerTime = 12 * 60; // 12:00
	_settings.soundOn = 1;
	_settings.brightness = 1;
	_settings.displayAutoOff = 0;
	_settings.calendar[0] = 3;
	_settings.calendar[1] = 3;
	_settings.calendar[2] = 3;
	_settings.calendar[3] = 3;
	_settings.calendar[4] = 3;
	_settings.calendar[5] = 3;
	_settings.calendar[6] = 3;
	memset(&_settings.custom, pEco, sizeof(_settings.custom));
	memset(&_settings.UDID, 0, sizeof(_settings.UDID));

	
	rtc_deinit();
	rtc_initpara.rtc_factor_asyn 		= 0x7F;
	rtc_initpara.rtc_factor_syn 		= 0xFF;
	rtc_initpara.rtc_year 					= 0x20;
	rtc_initpara.rtc_day_of_week 		= RTC_WEDSDAY;
	rtc_initpara.rtc_month 					= RTC_JAN;
	rtc_initpara.rtc_date 					= 0x01;
	rtc_initpara.rtc_display_format = RTC_24HOUR;
	rtc_initpara.rtc_am_pm 					= 0;

	rtc_initpara.rtc_hour   = 0x12;
	rtc_initpara.rtc_minute = 0x00;
	rtc_initpara.rtc_second = 0x00;

	if(ERROR == rtc_init(&rtc_initpara))
	{    
		while(1);
	}
}

void blocked()
{
	int16_t width, height;
	pxs.setBackground(BG_COLOR);
	pxs.setColor(MAIN_COLOR); 
	pxs.clear(); 

	if (pxs.sizeCompressedBitmap(width, height, img_blocked_png_comp) == 0)
		pxs.drawCompressedBitmap(SW / 2 - width / 2, SH / 2 - height / 2, img_blocked_png_comp);
	smooth_backlight(1);
	delay_1ms(2000);
	if(_settings.on)
	{

		DrawMainScreen();		
	}
	else
	{
		smooth_backlight(0);
		pxs.clear();
	  pxs.displayOff();
	}
}

void unblocked()
{
	int16_t width, height;
	pxs.setBackground(BG_COLOR);
	pxs.setColor(MAIN_COLOR); 
	pxs.clear(); 
	_backLight = 130;
	if (pxs.sizeCompressedBitmap(width, height, img_unblocked_png_comp) == 0)
		pxs.drawCompressedBitmap(SW / 2 - width / 2, SH / 2 - height / 2, img_unblocked_png_comp);
	smooth_backlight(1);
	delay_1ms(2000);
	if(_settings.on)
	{
		_backLight = 130;
		DrawMainScreen();
	}
	else
	{
		smooth_backlight(0);
		pxs.clear();
	  pxs.displayOff();
	}
}

void startScreen()
{
	int16_t width, height;
	pxs.setBackground(BG_COLOR);
	pxs.setColor(MAIN_COLOR); 
		if(_settings.brightness) _stateBrightness = StateBrightness_ON;
		else _stateBrightness = StateBrightness_LOW;
	pxs.clear(); 
	pxs.displayOn();
	

	/*
	pxs.setColor(modeColors[0]); 
	pxs.setFont(ElectroluxSansRegular10a);
	int16_t width_t = pxs.getTextWidth("FOR TESTING");
	pxs.print(SW / 2 - width_t/2, 40, "FOR TESTING");
	width_t = pxs.getTextWidth("PURPOSES ONLY!");
	pxs.print(SW / 2 - width_t/2, 60, "PURPOSES ONLY!");
	pxs.setColor(MAIN_COLOR); 
	delay_1ms(5000);
	pxs.clear(); 
	_backLight = 130;	
	*/
	/*
  if(_settings.model_set == 2)
	{
		pxs.drawCompressedBitmap(0, 0, img_fon_ballu_png_comp);
		if (pxs.sizeCompressedBitmap(width, height, img_logo_text1_png_comp) == 0)
			pxs.drawCompressedBitmap(10 ,  SH - height - 10  , img_logo_text1_png_comp);
		pxs.setColor(BG_COLOR);
		pxs.fillRectangle(0, 115, 3, 133);
		pxs.setColor(MAIN_COLOR);
		delay_1ms(2000);
		pxs.setColor(BG_COLOR);
		pxs.fillRectangle(0, 60, 102, 130);
		pxs.setColor(MAIN_COLOR); 
		pxs.setFont(MyriadPro_Regular8a);
		DrawTextSelected(10, 71, "Innovations",0);
		DrawTextSelected(10, 90, "in heating",0);
		DrawTextSelected(10, 109, "Electronic control",0);
	}
	else if (_settings.model_set == 1)
	{
		if (pxs.sizeCompressedBitmap(width, height, img_logo_text1_png_comp) == 0)
			pxs.drawCompressedBitmap(SW/2 - width/2 + 5 ,  SH/2 - height/2 , img_logo_text1_png_comp);
		delay_1ms(2000);
		pxs.clear(); 
		_backLight = 130;
		pxs.setColor(modeColors[0]); 
		pxs.setFont(ElectroluxSansRegular10a);
		int16_t width_t = pxs.getTextWidth("INFRARED");
		pxs.print(SW / 2 - width_t/2, 40, "INFRARED");
		width_t = pxs.getTextWidth("HEATING");
		pxs.print(SW / 2 - width_t/2, 60, "HEATING");
		pxs.setColor(MAIN_COLOR); 
		width_t = pxs.getTextWidth("Electronic control");
		pxs.print(SW / 2 - width_t/2, 100, "Electronic control");	
	}
	else */
	
		if (pxs.sizeCompressedBitmap(width, height, img_logo_text1_png_comp) == 0)
			pxs.drawCompressedBitmap(SW/2 - width/2 + 5 ,  SH/2 - height/2 - 5, img_logo_text1_png_comp);
		smooth_backlight(1);
		delay_1ms(2000);
		smooth_backlight(0);
		pxs.clear(); 
		_backLight = 130;	
		pxs.setColor(modeColors[0]); 
		pxs.setFont(ElectroluxSansRegular10a);
		int16_t width_t = pxs.getTextWidth("Innovations");
		pxs.print(SW / 2 - width_t/2, 20, "Innovations");
		width_t = pxs.getTextWidth("in heating");
		pxs.print(SW / 2 - width_t/2, 40, "in heating");
		pxs.setColor(MAIN_COLOR); 
		width_t = pxs.getTextWidth("Electronic control");
		pxs.print(SW / 2 - width_t/2, 70, "Electronic control");	
	
		pxs.setFont(ElectroluxSansRegular10a);
		
		char ver_buffer[20];
		sprintf(ver_buffer, "%s%s", "V.", VERSION);	
		width_t = pxs.getTextWidth(ver_buffer);
		pxs.print(SW/2 - width_t/2, 100, ver_buffer);
		smooth_backlight(1);
		delay_1ms(2000);
		smooth_backlight(0);		
		_backLight = 130;
		currentMenu = NULL;
		nextChangeLevel = 0;
	
			int8_t temp_current = getTemperature();
			
			if (temp_current  == -127)
			{
				SetPower(0);
				_error = 1;
				if(!_error_fl)
				{
					_error_fl = 1;
					pxs.clear();
					pxs.setFont(ElectroluxSansLight40a);
					DrawTextAligment(0, 0, SW, SH, "E1", false, false);
				}
			}
			else if (temp_current  == 127)
			{
				SetPower(0);
				_error = 2;
				if(!_error_fl)
				{
					_error_fl = 1;
					pxs.clear();
					pxs.setFont(ElectroluxSansLight40a);
					DrawTextAligment(0, 0, SW, SH, "E2", false, false);
				}
			}
			else if (temp_current > 48)
			{
				SetPower(0);
				_error = 3;
				if(!_error_fl)
				{
					_error_fl = 1;
					pxs.clear();
					pxs.setFont(ElectroluxSansLight40a);
					DrawTextAligment(0, 0, SW, SH, "E3", false, false);
				}
			}
			else if (temp_current < -26)
			{
				SetPower(0);
				_error = 4;
				if(!_error_fl)
				{
					_error_fl = 1;
					pxs.clear();
					pxs.setFont(ElectroluxSansLight40a);
					DrawTextAligment(0, 0, SW, SH, "E4", false, false);
				}
			}
			else	
			{
				DrawMainScreen();
			}
}

void deviceON()
{
	_backLight = 500;
	_backLight_div = 0;
	SetPower(20);
	_settings.on = 1;
	startScreen();
 _error_fl = 0;
	InitTimer();
	_timeoutSaveFlash = GetSystemTick();
	_timerStart = GetSystemTick();
}

void deviceOFF()
{
	
	_settings.timerOn = 0;
	timer_time_set = _settings.timerTime;
	
	SetPower(0);
	_settings.on = 0;
	open_window_counter = 0;
	open_window_temp_main_start = 255;
	window_was_opened = 0;
	window_is_opened = 0;
	smooth_backlight(0);
	pxs.clear();
	pxs.displayOff();
	
	InitTimer();
	_timeoutSaveFlash = GetSystemTick();
}

bool keyPressed()
{
	bool result = true;
	
	if ((_stateBrightness == StateBrightness_OFF) && _settings.on)
	{
		beep();
		result = false;
	}	
	idleTimeout = GetSystemTick();
	
	if (_settings.blocked && !_settings.on)
	{
		pxs.displayOn();
		if(_settings.brightness) _stateBrightness = StateBrightness_ON;
		else _stateBrightness = StateBrightness_LOW;
		beep();
		smooth_backlight(0);
		blocked();
		return false;
	}

	if (_settings.blocked && _settings.on)
	{
		if((_stateBrightness == StateBrightness_LOW || (_stateBrightness == StateBrightness_ON))) beep();
		pxs.displayOn();
		if(_settings.brightness) _stateBrightness = StateBrightness_ON;
		else _stateBrightness = StateBrightness_LOW;
		smooth_backlight(0);
		blocked();
		idleTimeout = GetSystemTick() - 27000;
		result = false;
	}
	
	if (!_settings.blocked && _settings.on)
	{
		if((_settings.displayAutoOff) && (_stateBrightness == StateBrightness_OFF))
		{
			DrawMainScreen();
			nextChangeLevel = GetSystemTick();
		}
		pxs.displayOn();
		if(_settings.brightness) _stateBrightness = StateBrightness_ON;
		else _stateBrightness = StateBrightness_LOW;
		
		//LL_GPIO_SetOutputPin(LCD_BL_GPIO_Port, LCD_BL_Pin);
	}
	
	nextChangeLevel = GetSystemTick() + 5000;
	return result;
}
uint8_t bcdToDec(uint8_t val)
{
  return( (val/16*10) + (val%16) );
}

uint8_t decToBcd(uint8_t val)
{
  return( (val/10*16) + (val%10) );
}
bool f_open_window (int8_t temp_current, uint8_t power_current)
{
    //Set current temp to compare and increase if current temp is going higher up to requested

    if (open_window_temp_main_start == 255)                                             // If we change operating mode = 255
    {
			open_window_temp_main_start = temp_current;
		}

    if ((power_current == power_limit) && (open_window_temp_main_start > temp_current) && (open_window_counter != open_window_times)) // if we loose warm when heating
    {
        open_window_counter++;
    }
		
    if ((temp_current <= (_settings.tempAntifrost - histeresis_low)) // if we reached af mode when temp falling down
		  && _settings.workMode != WorkMode_Antifrost 									// if it was not af mode
		  && open_window_temp_main_start > temp_current)								// if we loose warm
    {
			open_window_counter = 0;
			open_window_temp_main_start = 255;	
			_settings.modeOpenWindow = 0;
			_settings.workMode = WorkMode_Antifrost;
			_settings.calendarOn = 0;
			window_was_opened = 1;
			_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
			_backLight = 130;
			DrawMainScreen();
			return true;
    }

    if ((open_window_temp_main_start < temp_current) && (temp_current <= (getModeTemperature() - histeresis_low)) && (open_window_counter != open_window_times)) // ???????????? open_window_temp_start ??? ????? ???????????
    {
        open_window_temp_main_start = temp_current;
        open_window_counter = 0;
    }
       
    //Return true if temp goes down x times and open window state achieved
    if (open_window_counter == open_window_times)
		{
			_backLight = 130;
      return true;
		}
    else
        return false;
}

void open_window_func()
{
	if (currentMenu == NULL && !_error)
	{
		if(!window_is_opened)
		{			
			if(_settings.modeOpenWindow)
			{
				pxs.drawCompressedBitmap(20, _settings.model_set == 2 ? 78 : 87, (uint8_t*)img_icon_open_png_comp);
			}
			else if(!_settings.modeOpenWindow)
			{
				pxs.setColor(BG_COLOR);
				pxs.fillRectangle(20, _settings.model_set == 2 ? 78 : 87, 28, 28);
				pxs.setColor(MAIN_COLOR);
			}			
		}
		else
		{
			if(!window_was_opened) // if not antifrost when open window
			{					
				open_window_counter = 0;
				open_window_temp_main_start = 255;
				window_is_opened = 0;
				_backLight = 130;
				DrawMainScreen();
			}
			else
			{
				window_was_opened = 0;
				window_is_opened = 0;
				pxs.setColor(BG_COLOR);
				pxs.fillRectangle(20, _settings.model_set == 2 ? 78 : 87, 28, 28);
				pxs.setColor(MAIN_COLOR);
			}
		}		
	}	
}

void set_watchdog()
{
		rcu_osci_on(RCU_IRC40K);
	  while(ERROR == rcu_osci_stab_wait(RCU_IRC40K));
	  fwdgt_config(0x0FFF, FWDGT_PSC_DIV128);
    fwdgt_enable();
}
void loop(void)
{
  
	uint8_t* p = (uint8_t*)&_settings;
	memcpy(p, (uint8_t*)SETTINGSADDR, sizeof(_settings));
  if (_settings.crc != crc32_1byte(p, offsetof(DeviceSettings, crc), 0xFFFFFFFF)) // not valid crc
	{
		_settings.model_set = 0;
		_settings.ver_menu = 1;
		ResetAllSettings();
	}
	
  timer_time_set = _settings.timerTime;



	pxs.setOrientation(LANDSCAPE);
	pxs.enableAntialiasing(true);
	pxs.init();
	

	getTemperature();
	getTemperature();
	getTemperature();
	getTemperature();
	getTemperature();
	

	if (_settings.on)
	{
		startScreen();
		_timerStart = GetSystemTick();
		
	}
	else
	{
		pxs.displayOff();
	}
	InitTimer();
  idleTimeout = GetSystemTick();
	set_watchdog();
	while (1)
  {
//		LL_IWDG_ReloadCounter(IWDG);

		static int keyTimer = 0;
		if (keyTimer-- <= 0)
		{
			_key_window.update();
			_key_power.update();
			_key_menu.update();
			_key_back.update();
			_key_down.update();
			_key_up.update();

			keyTimer = 5;
		}
		if(RESET != rtc_flag_get(RTC_STAT_ALRM0F))
		{
			rtc_flag_clear(RTC_STAT_ALRM0F);
			rtc_alarm_callback();
		}
		
		if(!_settings.on){
		if (_key_power.getState() && _key_down.getState()  && !version_menu && !_settings.ver_menu)
		{
			if (_key_power.duration() > 2000 && _key_down.duration() > 2000)
			{	
				pxs.clear();
				pxs.displayOn();
				LL_GPIO_SetOutputPin(LCD_BL_GPIO_Port, LCD_BL_Pin);
				
				version_menu = true;
				
				pxs.setColor(modeColors[0]); 
				pxs.setFont(ElectroluxSansRegular10a);
				int16_t width_t = pxs.getTextWidth("MicaHedge");
			  DrawTextAligment(10, 13, width_t+10, 25,"MicaHedge", 0,0,_settings.model_set == 0 ? 1 : 0, MAIN_COLOR, BG_COLOR);
				width_t = pxs.getTextWidth("Mica");
			  DrawTextAligment(10, 48, width_t+10, 25,"Mica", 0,0,_settings.model_set == 1 ? 1 : 0, MAIN_COLOR, BG_COLOR);
				width_t = pxs.getTextWidth("Ballu");
				DrawTextAligment(10, 83, width_t+10, 25,"Ballu", 0,0,_settings.model_set == 2 ? 1 : 0, MAIN_COLOR, BG_COLOR);

			}
		}
		
		if (_key_up.getPressed() && version_menu)
		{	
			_settings.model_set++;
			if(_settings.model_set == 3)
			{
				_settings.model_set = 0;
			}
			  pxs.clear();
				pxs.setColor(modeColors[0]); 
				pxs.setFont(ElectroluxSansRegular10a);
				int16_t width_t = pxs.getTextWidth("MicaHedge");
			  DrawTextAligment(10, 13, width_t+10, 25,"MicaHedge", 0,0,_settings.model_set == 0 ? 1 : 0, MAIN_COLOR, BG_COLOR);
				width_t = pxs.getTextWidth("Mica");
			  DrawTextAligment(10, 48, width_t+10, 25,"Mica", 0,0,_settings.model_set == 1 ? 1 : 0, MAIN_COLOR, BG_COLOR);
				width_t = pxs.getTextWidth("Ballu");
				DrawTextAligment(10, 83, width_t+10, 25,"Ballu", 0,0,_settings.model_set == 2 ? 1 : 0, MAIN_COLOR, BG_COLOR);
		}
		
		if (_key_power.getPressed()  && version_menu)
		{
			if (!keyPressed())
				continue;
			ResetAllSettings();
			//_settings.ver_menu = true;
			SaveFlash();
			version_menu = 0;
			NVIC_SystemReset();
		}
	  if (_key_back.getPressed()  && version_menu)
		{
			if (!keyPressed())
				continue;
			version_menu = 0;
			NVIC_SystemReset();
			
		}
	}
		if(version_menu) continue;
		

		if (_key_window.getPressed() && !_error)
		{
			if(!_settings.on)
				continue;				
			
			if (!keyPressed())
				continue;
			beep();					
			if(!window_is_opened)
			{
				_settings.modeOpenWindow = !_settings.modeOpenWindow;
				_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
			}
			open_window_func();
		}

		if (_key_power.getPressed() && !_key_down.getState())
		{
			if (!keyPressed())
				continue;
			beep();
		}

		if (_key_power.getLongPressed() && !_key_down.getState())
		{
			if (!keyPressed())
				continue;

			if (_settings.on)
				deviceOFF();
			else
				deviceON();
		}		
		
		if(!window_is_opened)
		{		
		if (_timeoutSaveFlash != 0 && GetSystemTick() >= _timeoutSaveFlash)
		{
			_timeoutSaveFlash = 0;
			SaveFlash();
		}

		if (_key_down.getState() && _key_up.getState() && ((!_error && _settings.on) || (!_settings.on)))
		{
			if (_key_down.duration() > 2000 && _key_up.duration() > 2000)
			{
				beep();
				_settings.blocked = !_settings.blocked;
				pxs.displayOn();
				idleTimeout = GetSystemTick();
		    if(_settings.brightness) _stateBrightness = StateBrightness_ON;
		    else _stateBrightness = StateBrightness_LOW;
				smooth_backlight(0);
				if (_settings.blocked)
					blocked();
				else
					unblocked();
			
				_key_down.getPressed();
				_key_up.getPressed();
				_key_down.getLongPressed();
				_key_up.getLongPressed();
			}
			else {
				_key_down.getPressed();
				_key_up.getPressed();
				_key_down.getLongPressed();
				_key_up.getLongPressed();
				
				continue;
			}
		}
		else if (_key_down.getPressed() && !_error)
		{
			if (!keyPressed())
				continue;

			if (!_key_down.isLongPressed())
				beep();
			
			if(!_settings.on)
				continue;
			
			if (currentMenu != NULL)
			{
				MenuPrev();
			}
			else
			{
				uint32_t updater = 0x01;
				if (_settings.calendarOn)
				{
					_settings.calendarOn = 0;
					_settings.workMode = WorkMode_Comfort;
					_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
					updater = 0;
					_backLight = 130;
					DrawMainScreen(updater);
					continue;					
				}

				if (_settings.workMode != WorkMode_Comfort)
				{
					_settings.workMode = WorkMode_Comfort;
					_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
					updater = 0;
					_backLight = 130;
				}
				else
				{
					_settings.tempComfort--;
					_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
					
					if (_settings.tempComfort > MAX_TEMP_COMFORT)
					{
						_settings.tempComfort = MIN_TEMP_COMFORT;
						_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
					}
					else if (_settings.tempComfort < MIN_TEMP_COMFORT)
					{
						_settings.tempComfort = MAX_TEMP_COMFORT;
						_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
					}
				}
				
				DrawMainScreen(updater);
			}
		}
		else if (_key_up.getPressed()&& !_error)
		{
			if (!keyPressed())
				continue;
	
			if (!_key_up.isLongPressed())
				beep();
			
			if(!_settings.on)
				continue;
				
			if (currentMenu != NULL)
			{
				MenuNext();
			}
			else
			{
				uint32_t updater = 0x01;
				if (_settings.calendarOn)
				{
					_settings.calendarOn = 0;
					_settings.workMode = WorkMode_Comfort;
					_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
					updater = 0;
					_backLight = 130;
					DrawMainScreen(updater);
					continue;					
				}

				if (_settings.workMode != WorkMode_Comfort)
				{
					_settings.workMode = WorkMode_Comfort;
					_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
					updater = 0;
					_backLight = 130;
				}
				else
				{
					_settings.tempComfort++;
					_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
					
					if (_settings.tempComfort > MAX_TEMP_COMFORT)
					{
						_settings.tempComfort = MIN_TEMP_COMFORT;
						_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
					}
					else if (_settings.tempComfort < MIN_TEMP_COMFORT)
					{
						_settings.tempComfort = MAX_TEMP_COMFORT;
						_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
					}
				}
				DrawMainScreen(updater);
			}
		}	
		
			if(!_settings.on)
				continue;
		

		
		if (_key_menu.getPressed()&& !_error)
		{
			if (!keyPressed())
				continue;

			beep();
			
			if (currentMenu != NULL)
			{
				MenuOK();
			}
			else
			{
				EnterMenu();
			}
			
		}

		if (_key_back.getPressed()&& !_error)
		{
			if (!keyPressed())
				continue;

			beep();			
			
			if (currentMenu != NULL)
			{
				MenuBack();
			}
			else
			{
				if (_settings.calendarOn)
				{
					_settings.calendarOn = 0;
					_settings.workMode = WorkMode_Comfort;
				}
				else
				{
					if (_settings.workMode == WorkMode_Comfort)
						_settings.workMode = WorkMode_Eco;
					else if (_settings.workMode == WorkMode_Eco)
						_settings.workMode = WorkMode_Antifrost;
					else
						_settings.workMode = WorkMode_Comfort;
				}
				
				_timeoutSaveFlash = GetSystemTick() + SAVE_TIMEOUT;
				_backLight = 130;
				DrawMainScreen();
			}
		}
	}
		// off state
		if (!_settings.on)
			continue;

		// auto switch off
		if (_settings.displayAutoOff && _error == 0 && !window_is_opened)
		{
			if (GetSystemTick() > idleTimeout + 30000)
			{
				_stateBrightness = StateBrightness_OFF;
				smooth_backlight(0);
				pxs.clear();
				pxs.displayOff();
			}
			else if (GetSystemTick() > idleTimeout + 15000)
			{
				_stateBrightness = StateBrightness_LOW;
				smooth_backlight(1);
			}
		}
		
		if (GetSystemTick() > idleTimeout + 2000 && currentMenu != NULL && currentMenu->ID == 999)
		{
			MenuBack();
		}
		else if (GetSystemTick() > idleTimeout + 15000 && currentMenu != NULL)
		{
			MainScreen();
		}
		
		if ((GetSystemTick() > idleTimeout + 5000) && (_settings.brightness == 0) && (_stateBrightness == StateBrightness_ON) /*&& (!_settings.displayAutoOff)*/) // If 50% brightness, back from 100% to 50% after 5s
		{
		  //_stateBrightness = StateBrightness_LOW;
		}

//==================================================================== refrash display		
		if (GetSystemTick() > nextChangeLevel && _settings.on)
		{
			int8_t temp_current = getTemperature();
			int8_t modeTemp = getModeTemperature();
			power_current = _currentPower;
			
			if (temp_current  == -127)
			{
				SetPower(0);
				_error = 1;
				if(_settings.brightness) _stateBrightness = StateBrightness_ON;
				else _stateBrightness = StateBrightness_LOW;
				if(!_error_fl)
				{
					_error_fl = 1;
					pxs.clear();
					pxs.setFont(ElectroluxSansLight40a);
					
					DrawTextAligment(0, 0, SW, SH, "E1", false, false);
					smooth_backlight(1);
				}
			}
			else if (temp_current  == 127)
			{
				SetPower(0);
				_error = 2;
				if(_settings.brightness) _stateBrightness = StateBrightness_ON;
				else _stateBrightness = StateBrightness_LOW;
				if(!_error_fl)
				{
					_error_fl = 1;
					pxs.clear();
					pxs.setFont(ElectroluxSansLight40a);
					_backLight = 130;
					DrawTextAligment(0, 0, SW, SH, "E2", false, false);
					smooth_backlight(1);
				}
			}
			else if (temp_current > 48)
			{
				SetPower(0);
				_error = 3;
				if(_settings.brightness) _stateBrightness = StateBrightness_ON;
				else _stateBrightness = StateBrightness_LOW;
				if(!_error_fl)
				{
					_error_fl = 1;
					pxs.clear();
					pxs.setFont(ElectroluxSansLight40a);
					_backLight = 130;
					DrawTextAligment(0, 0, SW, SH, "E3", false, false);
					smooth_backlight(1);
				}
			}
			else if (temp_current < -26)
			{
				SetPower(0);
				_error = 4;
				if(_settings.brightness) _stateBrightness = StateBrightness_ON;
				else _stateBrightness = StateBrightness_LOW;
				if(!_error_fl)
				{
					_error_fl = 1;
					pxs.clear();
					pxs.setFont(ElectroluxSansLight40a);
					_backLight = 130;
					DrawTextAligment(0, 0, SW, SH, "E4", false, false);
					smooth_backlight(1);
				}
			}
			else
			{
				_error = 0;
				if(_error_fl)
				{
					_error_fl = 0;
					temp_steinhart = 25;
					_backLight = 130;
					DrawMainScreen();
				}
				
				//============================================== open window maintance
				if ((_settings.modeOpenWindow)&& (_settings.workMode != WorkMode_Off))
				{
					window_is_opened = f_open_window (temp_current, power_current);
				}
				//====================================================================
				if((!window_is_opened || window_was_opened) && (_settings.workMode != WorkMode_Off))
				{
					if (temp_current  <= (modeTemp - histeresis_low))
						power_current = power_limit;
					
					else if (temp_current >= (modeTemp + histeresis_high))
						power_current = 0;	
				}
				else
					power_current = 0;
				
				SetPower(power_current);
				
				
				int16_t width;
				int16_t height;
				if((_settings.model_set == 0) && (_settings.workMode != WorkMode_Off))
				{
					if ((power_current == 20)) // max power
					{
						if((currentMenu == NULL)  && !_error)
						{
							if(_settings.heatset < 2)
							{						
								if (pxs.sizeCompressedBitmap(width, height, img_hh_on_icon_png_comp) == 0)
								{
									pxs.setColor(BG_COLOR);
									pxs.fillRectangle(SW/2 - 17, SH - 42, 36, 29);	
									pxs.setColor(MAIN_COLOR);
									pxs.drawCompressedBitmap(SW/2 - width + 11 ,  SH - height - 2 -11, img_hh_on_icon_png_comp);	
								}
								pxs.setColor(BG_COLOR);
								pxs.fillRectangle(93, 87, 8, 27);
								pxs.setColor(YELLOW_COLOR);
								pxs.fillRectangle(93, 102, 6, 13);
								if(_settings.powerLevel == 2)
								{				
									pxs.fillRectangle(93, 87, 6, 13);	
								}										
							}
							else
							{
								if (pxs.sizeCompressedBitmap(width, height, img_hh_off_icon_png_comp) == 0)
								pxs.drawCompressedBitmap(SW/2 - width + 19 ,  SH - height - 2 -11, img_hh_off_icon_png_comp);			
							}						
							
							if((_settings.heatset == 2) || (_settings.heatset == 0))
							{
								if (pxs.sizeCompressedBitmap(width, height, img_mt_on_icon_png_comp) == 0)
								{
									pxs.setColor(BG_COLOR);
									pxs.fillRectangle(SW/2 + 33, SH - 42, 29, 29);	
									pxs.setColor(MAIN_COLOR);									
									pxs.drawCompressedBitmap(SW/2 + width +6 ,  SH - height - 2 -11, img_mt_on_icon_png_comp);
								}									
							}
							else
							{
								if (pxs.sizeCompressedBitmap(width, height, img_mt_off_icon_png_comp) == 0)
								pxs.drawCompressedBitmap(SW/2 + width +4 ,  SH - height - 2 -11, img_mt_off_icon_png_comp);			
							}							
						}			
					}
					else
					{
						if((currentMenu == NULL) && !_error)
						{		

								if (pxs.sizeCompressedBitmap(width, height, img_hh_off_icon_png_comp) == 0)
								pxs.drawCompressedBitmap(SW/2 - width + 19,  SH - height - 2 -11, img_hh_off_icon_png_comp);		

								if (pxs.sizeCompressedBitmap(width, height, img_mt_off_icon_png_comp) == 0)
								pxs.drawCompressedBitmap(SW/2 + width +4 ,  SH - height - 2 -11, img_mt_off_icon_png_comp);

						}			
					}	
				}
				else 	if((_settings.model_set == 2) && (_settings.workMode != WorkMode_Off))
				{
					if ((power_current == 20)) // max power
					{
						if((currentMenu == NULL)  && !_error)
						{
							pxs.setColor(YELLOW_COLOR);
							pxs.fillRectangle(20, 110, 57, 6);
							if(_settings.powerLevel == 2)
							{				
								pxs.fillRectangle(85, 110, 57, 6);
							}
							else
							{
								pxs.setColor(GREY_COLOR);
								pxs.fillRectangle(85, 110, 57, 6);
							}
						}
					}
					else
					{
						if((currentMenu == NULL) && !_error)
						{	
							pxs.setColor(GREY_COLOR);
							pxs.fillRectangle(20, 110, 57, 6);
							pxs.fillRectangle(85, 110, 57, 6);
						}
					}
				}
			}
			nextChangeLevel = GetSystemTick() + 60000;
		}
//========================================================= refrash 1 sec		
		if (GetSystemTick() > refrash_time && _settings.on)
		{	
			  fwdgt_counter_reload();	
			
			#ifdef DEBUG
				char buffer[10];
				pxs.setColor(MAIN_COLOR);
				sprintf(buffer, "%d", getTemperature());
				pxs.setFont(ElectroluxSansRegular10a);
				DrawTextAligment(135, 1, 25, 20, buffer, false);
								
				RTC_TimeTypeDef sTime1;
				RTC_DateTypeDef sDate1;
				HAL_RTC_GetTime(&hrtc, &sTime1, RTC_FORMAT_BIN);
				HAL_RTC_GetDate(&hrtc, &sDate1, RTC_FORMAT_BIN);
				sprintf(buffer, "%02d %02d %02d  %02d", sTime1.Hours, sTime1.Minutes, sTime1.Seconds, sDate1.WeekDay);
				uint8_t widthX = pxs.getTextWidth(buffer);
				pxs.setColor(BG_COLOR);
				pxs.fillRectangle(45, 0, widthX+10, 15);
				pxs.setColor(MAIN_COLOR);
				DrawTextAligment(50, 1, widthX, 20, buffer, false);
				#endif
			
			
//============================================================ Calendar & Timer		
	
			if (_eventTimer) // timer event
			{
				if (_settings.timerOn == 1)
				{
					deviceOFF();
					_stateBrightness = StateBrightness_ON;
				}
				else if (_settings.calendarOn == 1)
				{
					WorkMode currentWorkMode = getCalendarMode();
					if (_settings.workMode != currentWorkMode)
					{
						_settings.workMode = currentWorkMode;
						nextChangeLevel = GetSystemTick() + 1000;
						_backLight = 130;
						DrawMainScreen();
					}
				}
				_eventTimer = 0;
			}
//=============================================================================

				if(window_was_opened)
				{
					static bool show_icon;
					if(show_icon)
						pxs.drawCompressedBitmap(20, _settings.model_set == 2 ? 78 : 87, (uint8_t*)img_icon_open_png_comp);
				  else
				  {
						pxs.setColor(BG_COLOR);
						pxs.fillRectangle(20, _settings.model_set == 2 ? 78 : 87, 28, 28);
						pxs.setColor(MAIN_COLOR);
				  }
					show_icon = !show_icon;
				}
			refrash_time = GetSystemTick() + 1000;
		}
//=============================================================================================		

		if (currentMenu == NULL && !_error)
		{
			if (window_is_opened && !window_was_opened)
				DrawWindowOpen();
		}
  }
}

