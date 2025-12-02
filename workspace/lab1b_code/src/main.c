#include "xstatus.h"
#include "extra.h"
#include "sevenSeg_new.h"
#include "xgpio.h"

XGpio Gpio;
#define LED_CHANNEL 1

extern volatile int timer_0_interupted;
extern volatile int timer_1_interupted;
extern volatile int reset_button_event;
extern volatile int start_button_event;
extern volatile int stop_button_event;
extern volatile int forward_button_event;
extern volatile int backward_button_event;

extern volatile unsigned int counter;
extern volatile int pos;          // logical digit index (0..6)
extern volatile int dir;         // +1 up, -1 down
extern volatile int running;      // 0 stop, 1 run





int main(void)
{
    if (timer_0_enable() != XST_SUCCESS) return 0;
    if (timer_1_enable() != XST_SUCCESS) return 0;
    if (interupt_enable() != XST_SUCCESS) return 0;
    if (universal_button_enable() != XST_SUCCESS) return 0;

    XGpio_Initialize(&Gpio, XPAR_AXI_GPIO_LED_DEVICE_ID);



    while (1) {

    }
}
