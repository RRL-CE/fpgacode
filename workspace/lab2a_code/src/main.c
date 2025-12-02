/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include <stdbool.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xtmrctr.h"
#include "xintc.h"
#include "xtmrctr_l.h"
#include "xintc_l.h"
#include "mb_interface.h"
#include <xbasic_types.h>
#include <xio.h>


XIntc sys_intc; //Interrupt controller
XGpio led; //Display LEDs
XGpio rgbled; //Status LED
XGpio BtnGpio;
XGpio ENCODER; //Encoder
XTmrCtr sys_tmrctr; //Timer
//

volatile int      display_num = 0x1;  // one-hot LED mask, main mirrors this
volatile int  curr_state  = 0;  // 0..6 as in reference: detent hub is state 0

#define DEBOUNCE_TICKS 2000000u   // 20 ms @ 100 MHz; use 1000000u for 10 ms

volatile int  btn_prev        = 0;   // last sampled button level (0/1)
volatile int  btn_debouncing  = 0;   // 1 while one-shot is running
volatile int  off             = 0;   // your user flag to toggle


int encoder_setup(void)
{
    int Status = XGpio_Initialize(&ENCODER, XPAR_GPIO_0_DEVICE_ID); // == XPAR_ENCODER_DEVICE_ID
    if (Status != XST_SUCCESS) {
        xil_printf("ENCODER init failed\r\n");
        return XST_FAILURE;
    }

    XGpio_InterruptEnable(&ENCODER, 1);
    XGpio_InterruptGlobalEnable(&ENCODER);

    return XST_SUCCESS;
}



// Debounce constants (tune as needed)
#define ENC_DEBOUNCE_COUNT  3   // how many stable ISR samples before accepting new press
#define ENC_DEBOUNCE_DELAY  500 // microseconds between samples if you poll inside ISR (optional)

static uint8_t enc_btn_stable   = 0;  // last accepted stable state
static uint8_t enc_btn_raw_prev = 0;  // last raw read
static uint8_t enc_btn_counter  = 0;  // debounce counter

void EncoderIsr(void *CallbackRef)
{
    unsigned enc = XGpio_DiscreteRead(&ENCODER, 1);
    uint8_t btn_raw = (enc & 0b100) ? 1 : 0;   // raw instantaneous value
    uint8_t pins    = (enc & 0b11);            // A/B channels

    //------------------------------------------------------------------
    // Software debounce for encoder pushbutton (no timer)
    //------------------------------------------------------------------
    if (btn_raw == enc_btn_raw_prev) {
        // Stable sample; increment counter up to ENC_DEBOUNCE_COUNT
        if (enc_btn_counter < ENC_DEBOUNCE_COUNT)
            enc_btn_counter++;
    } else {
        // Input changed; reset counter
        enc_btn_counter = 0;
    }

    enc_btn_raw_prev = btn_raw;

    // When stable for enough samples → accept new logical state
    if (enc_btn_counter >= ENC_DEBOUNCE_COUNT && btn_raw != enc_btn_stable) {
        enc_btn_stable = btn_raw;
        if (enc_btn_stable) {
            // rising edge of debounced button
            off ^= 1; // your action (toggle flag, start/stop, etc.)
        }
    }

    //------------------------------------------------------------------
    // Rotary quadrature decoding (unchanged)
    //------------------------------------------------------------------
    static uint8_t last_pins = 0xFF;
    if (pins == last_pins) {
        XGpio_InterruptClear(&ENCODER, 1);
        return;
    }
    last_pins = pins;

    switch (curr_state)
    {
    case 0:
        switch (pins) {
        case 0b10: curr_state = 1; break;
        case 0b01: curr_state = 4; break;
        }
        break;
    case 1:
        switch (pins) {
        case 0b00: curr_state = 2; break;
        case 0b11: curr_state = 0; break;
        }
        break;
    case 2:
        switch (pins) {
        case 0b01: curr_state = 3; break;
        }
        break;
    case 3:
        switch (pins) {
        case 0b11:
            curr_state = 0;
            display_num = (display_num < 32768) ? (display_num << 1) : 1;
            break;
        case 0b00: curr_state = 2; break;
        }
        break;
    case 4:
        switch (pins) {
        case 0b00: curr_state = 5; break;
        case 0b11: curr_state = 0; break;
        }
        break;
    case 5:
        switch (pins) {
        case 0b10: curr_state = 6; break;
        }
        break;
    case 6:
        switch (pins) {
        case 0b11:
            curr_state = 0;
            display_num = (display_num > 1) ? (display_num >> 1) : 32768;
            break;
        case 0b00: curr_state = 5; break;
        }
        break;
    }

    XGpio_InterruptClear(&ENCODER, 1);
}


void TimerIsr(void *CallBackRef, u8 TmrCtrNumber)
{
    // Stop the one-shot (belt & suspenders)
    XTmrCtr_Stop(&sys_tmrctr, 0);

    // Re-sample the button after the debounce interval
    unsigned enc = XGpio_DiscreteRead(&ENCODER, 1);
    uint8_t btn  = (enc & 0b100) ? 1 : 0;

    if (btn) {
        // Still pressed after debounce → accept
        off ^= 1;   // toggle your flag (or whatever action you want)
    }

    btn_debouncing = 0;
    // NOTE: No need to clear the timer IRQ yourself; the driver’s
    // XTmrCtr_InterruptHandler (which we connected to INTC) does that,
    // then calls this callback.
}



int interupter(void)
{
    int Status;

    Status = XIntc_Initialize(&sys_intc, XPAR_INTC_0_DEVICE_ID);
    if (Status != XST_SUCCESS) {
        xil_printf("INTC: init failed\r\n");
        return XST_FAILURE;
    }

    // Hook encoder ISR using your reference’s vector macro
    Status = XIntc_Connect(&sys_intc,
                           XPAR_MICROBLAZE_0_AXI_INTC_ENCODER_IP2INTC_IRPT_INTR,
                           (XInterruptHandler)EncoderIsr,
                           (void *)&ENCODER);
    if (Status != XST_SUCCESS) {
        xil_printf("INTC: connect encoder ISR failed\r\n");
        return XST_FAILURE;
    }

    Status = XIntc_Start(&sys_intc, XIN_REAL_MODE);
    if (Status != XST_SUCCESS) {
        xil_printf("INTC: start failed\r\n");
        return XST_FAILURE;
    }

    XIntc_Enable(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_ENCODER_IP2INTC_IRPT_INTR);

    // Match your reference: register the central handler + enable interrupts
    microblaze_register_handler((XInterruptHandler)XIntc_DeviceInterruptHandler,
                                (void*)XPAR_INTC_0_DEVICE_ID);
    microblaze_enable_interrupts();




    //TIMER
    // Connect AXI Timer 0 IRQ
    Status = XIntc_Connect(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR, (XInterruptHandler)XTmrCtr_InterruptHandler, &sys_tmrctr);
    if (Status != XST_SUCCESS) {
        xil_printf("INTC: connect TIMER ISR failed\r\n");
        return XST_FAILURE;
    }
    XIntc_Enable(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR);


    return XST_SUCCESS;
}





int main() {
    init_platform();

    int Status = XGpio_Initialize(&rgbled, XPAR_RGBLED_DEVICE_ID);
    if (Status != XST_SUCCESS) { xil_printf("RGB Failed\r\n"); return XST_FAILURE; }
    Status = XGpio_Initialize(&led, XPAR_AXI_GPIO_LED_DEVICE_ID);
    if (Status != XST_SUCCESS) { xil_printf("LED Failed\r\n"); return XST_FAILURE; }

    if (encoder_setup() != XST_SUCCESS) return XST_FAILURE;
    if (interupter()    != XST_SUCCESS) return XST_FAILURE;

    if (XTmrCtr_Initialize(&sys_tmrctr, XPAR_TMRCTR_0_DEVICE_ID) != XST_SUCCESS) {
        xil_printf("Timer init failed\r\n");
        return XST_FAILURE;
    }

    XTmrCtr_SetHandler(&sys_tmrctr, (XTmrCtr_Handler)TimerIsr, NULL);

    // One-shot: INT mode only (no auto-reload)
    XTmrCtr_SetOptions(&sys_tmrctr, 0, XTC_INT_MODE_OPTION);


    // --- LED tracking variables ---
    uint16_t led_mask = 0x0001;   // start with LED0 ON
    XGpio_DiscreteWrite(&led, 1, led_mask);

    while (1) {

//        }
    	if(!off){
    		XGpio_DiscreteWrite(&led, 1, display_num);
    	}
    	else{
    		XGpio_DiscreteWrite(&led, 1, 0);
    	}

        XGpio_DiscreteWrite(&rgbled,1, 0);
    }
}


