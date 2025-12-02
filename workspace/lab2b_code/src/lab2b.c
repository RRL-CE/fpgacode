#define AO_LAB2B

#include "qpn_port.h"
#include "bsp.h"
#include "lab2b.h"

#include "xspi.h"
#include "xspi_l.h"
#include "lcd.h"

/* ==================== Volume bar geometry ==================== */
#define VOL_X_START   24
#define VOL_X_END     216
#define VOL_Y_TOP     100
#define VOL_Y_BOTTOM  120
#define VOL_STEPS     63
#define VOL_STEP_PX   ((VOL_X_END - VOL_X_START) / VOL_STEPS)  /* not used now, but kept */

/* Map volume (0..63) -> x (40..232), with rounding so vol=63 → x=232 */
/* Map volume (0..63) -> EXCLUSIVE right edge x (40..232).
   Exclusive means the bar covers [VOL_X_START .. x_excl-1] inclusive.
   This guarantees no sliver at volume==0. */
static inline int vol_to_x(int volume) {
    if (volume < 0) volume = 0;
    if (volume > VOL_STEPS) volume = VOL_STEPS;
    const int span = (VOL_X_END - VOL_X_START);    /* 232-40 = 192 */
    /* Linear mapping: 0->40, 63->232 */
    return VOL_X_START + (volume * span) / VOL_STEPS;
}

/* Draw the full bar for the given volume (no clearing).
   Uses exclusive right edge from vol_to_x() and inclusive fillRect end. */
void drawVolumeBar(int volume) {
    int x = vol_to_x(volume);
    /* nothing to draw if the exclusive edge is at (or left of) the start */
    if (x <= VOL_X_START) return;

    setColor(0, 255, 0); /* green */
    /* inclusive end = x_excl - 1 */
    fillRect(VOL_X_START, VOL_Y_BOTTOM, x - 1, VOL_Y_TOP);
}

/* Clear only the currently occupied bar span for THIS volume by restoring triangles. */
void clearVolumeBar(int volume) {
    int x = vol_to_x(volume);
    if (x <= VOL_X_START) return; /* no span to clear */
    /* restore [VOL_X_START .. x_excl-1] */
//    setColor(0, 0, 0);
//	fillRect(VOL_X_START, VOL_Y_BOTTOM, x, VOL_Y_TOP);
    redrawTrianglesInRect(VOL_X_START, x, VOL_Y_TOP, VOL_Y_BOTTOM);
}

/* Incremental update:
   - if new > old: draw green slice in [old_excl .. new_excl-1]
   - if new < old: restore triangles in [new_excl .. old_excl-1] */
void updateVolumeBar(int oldVolume, int newVolume) {
    if (newVolume < 0) newVolume = 0;
    if (newVolume > VOL_STEPS) newVolume = VOL_STEPS;

    int old = vol_to_x(oldVolume);
    int new = vol_to_x(newVolume);

    if (new > old) {
        /* draw added slice */
        setColor(0, 255, 0);
        fillRect(old, VOL_Y_BOTTOM, new - 1, VOL_Y_TOP);
    } else if (new < old) {
        /* restore removed slice */
        redrawTrianglesInRect(new, old - 1, VOL_Y_TOP, VOL_Y_BOTTOM);
    } /* else equal → no-op */
}

/* ==================== Triangle background redraw (scoped) ==================== */
/* Re-draw ONLY the triangles that intersect the rectangle [x1..x2] × [y_top..y_bot] */
void redrawTrianglesInRect(int x1, int x2, int y_top, int y_bot) {
//    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
//    if (x1 < 0) x1 = 0;
//    if (x2 < x1) return;

    /* Triangle field parameters (match your init) */
    const int base_step_x = 40;
    const int base_step_y = 40;
    const int half_base   = 20;
    const int height      = 35;
    const int color_r     = 255, color_g = 0, color_b = 0;

    /* 1) Repaint the band background to black for each scanline */
    setColor(0, 0, 0);
    for (int y = y_top; y <= y_bot; y++) {
        fillRect(x1, y, x2, y);  /* one-pixel-high horizontal clear */
    }

    /* 2) Draw only the triangle scanline segments that intersect this band */
    for (int row_y = 40; row_y <= 320; row_y += base_step_y) {
        for (int col_x = 0; col_x <= 200; col_x += base_step_x) {
            for (int i = 0; i < height; i++) {
                int y = row_y - i;
                if (y < y_top || y > y_bot) continue;

                int x_start = col_x + half_base - (half_base * (height - i)) / height;
                int line_len = 2 * (half_base * (height - i)) / height;
                int segL = x_start;
                int segR = x_start + line_len;

                /* Clip the segment against [x1..x2] */
                int a = (segL > x1) ? segL : x1;
                int b = (segR < x2) ? segR : x2;
                if (a <= b) {
                    setColor(color_r, color_g, color_b);
                    fillRect(a, y, b, y);  /* paint the triangle segment on this scanline */
                }
            }
        }
    }
}


/* ==================== Overlay helpers (unchanged) ==================== */

static void drawButtonOverlay(const char *text) {
    setColorBg(0, 0, 0);
    setColor(0, 180, 255);
    fillRect(40, 240, 200, 280);
    setFont(BigFont);
    lcdPrint((char*)text, 60, 252);
}

static void restoreTrianglesOverlayArea(void) {
    setColor(0, 0, 0);
    fillRect(40, 240, 200, 280);
    /* repaint triangles only in [240..280] */
    redrawTrianglesInRect(40, 200, 240, 280);
}

/* ==================== LCD/SPI statics from your code ==================== */

static XGpio dc;
static XSpi  spi;

XSpi_Config *spiConfig;

u32 status;
u32 controlReg;

int  sec = 0;
int  secTmp;
char secStr[4];

/* ==================== AO and QSM ==================== */

typedef struct Lab2BTag {
    QActive super;
    int volume;
    int isMuted;
    int button_pressed;
    int volume_changed;
    int button;
} Lab2B;

/* Forward states */
static QState Lab2B_initial (Lab2B *me);
static QState Lab2B_Idle    (Lab2B *me);
static QState Lab2B_Print   (Lab2B *me);
static QState Lab2B_Mute    (Lab2B *me);

Lab2B AO_Lab2B;

void Lab2B_ctor(void) {
    Lab2B *me = &AO_Lab2B;
    QActive_ctor(&me->super, (QStateHandler)&Lab2B_initial);
    me->volume         = 0;
    me->volume_changed = 0;
    me->isMuted        = 0;
    me->button_pressed = 0;
    me->button         = 0;
}

/* ==================== Initial ==================== */
QState Lab2B_initial(Lab2B *me) {
    xil_printf("Initialization\n");

    //copied from starter code for LCD
    {
        int status;
//        print("---Entering main (CK)---\n\r");

        status = XGpio_Initialize(&dc, XPAR_SPI_DC_DEVICE_ID);
        if (status == XST_SUCCESS) XGpio_SetDataDirection(&dc, 1, 0x0);
        else xil_printf("DC init fail!\n");

        spiConfig = XSpi_LookupConfig(XPAR_SPI_DEVICE_ID);
        if (!spiConfig) {
            xil_printf("SPI cfg not found!\n");
        } else {
            status = XSpi_CfgInitialize(&spi, spiConfig, spiConfig->BaseAddress);
            if (status != XST_SUCCESS) xil_printf("SPI init fail!\n");
            else {
                XSpi_Reset(&spi);
                controlReg = XSpi_GetControlReg(&spi);
                XSpi_SetControlReg(&spi,
                    (controlReg | XSP_CR_ENABLE_MASK | XSP_CR_MASTER_MODE_MASK) &
                    (~XSP_CR_TRANS_INHIBIT_MASK));
                XSpi_SetSlaveSelectReg(&spi, ~0x01);
            }
        }

        initLCD();
        clrScr();

        /* draw triangle background once */
        const int base_step_x = 40, base_step_y = 40, half_base = 20, height = 35;
        const int color_r = 255, color_g = 0, color_b = 0;
        for (int row_y = 40; row_y <= 320; row_y += base_step_y) {
            for (int col_x = 0; col_x <= 200; col_x += base_step_x) {
                for (int i = 0; i < height; i++) {
                    setColor(color_r, color_g, color_b);
                    int y = row_y - i;
                    int x_start = col_x + half_base - (half_base * (height - i)) / height;
                    int line_len = 2 * (half_base * (height - i)) / height;
                    drawHLine(x_start, y, line_len);
                }
            }
        }
    }

    return Q_TRAN(&Lab2B_Idle);
}

#define CLAMP_VOL(v) do { if ((v) < 0) (v) = 0; else if ((v) > VOL_STEPS) (v) = VOL_STEPS; } while (0)

/* ==================== Idle ==================== */
QState Lab2B_Idle(Lab2B *me) {
    switch (Q_SIG(me)) {

    case Q_ENTRY_SIG: {
        //XGpio_DiscreteWrite(&rgbled, 1, 1);

        if (me->volume_changed || me->button_pressed) {
            clearVolumeBar(me->volume);
            me->volume_changed = 0;
        }
        if (me->button_pressed) {
            restoreTrianglesOverlayArea();
            me->button_pressed = 0;
        }
        return Q_HANDLED();
    }

    case Q_EXIT_SIG: {
        return Q_HANDLED();
    }

    /* -------- Buttons: always go to Print -------- */
    case BUT_1: { me->button = 1; me->button_pressed = 1; return Q_TRAN(&Lab2B_Print); }
    case BUT_2: { me->button = 2; me->button_pressed = 1; return Q_TRAN(&Lab2B_Print); }
    case BUT_3: { me->button = 3; me->button_pressed = 1; return Q_TRAN(&Lab2B_Print); }
    case BUT_4: { me->button = 4; me->button_pressed = 1; return Q_TRAN(&Lab2B_Print); }
    case BUT_5: { me->button = 5; me->button_pressed = 1; return Q_TRAN(&Lab2B_Print); }

    /* -------- Encoder movement: change volume and go to Print --- */
    case ENCODER_UP: {
        me->volume_changed = 1;
        me->volume++;
        CLAMP_VOL(me->volume);
        return Q_TRAN(&Lab2B_Print);
    }
    case ENCODER_DOWN: {
        me->volume_changed = 1;
        me->volume--;
        CLAMP_VOL(me->volume);
        return Q_TRAN(&Lab2B_Print);
    }

    /* -------- Encoder click: toggle mute, then go Print -------- */
    case ENCODER_CLICK: {
        me->isMuted ^= 1;
        me->volume_changed = 1;
        return Q_TRAN(&Lab2B_Print);
    }

    case WAITED:
        return Q_HANDLED();

    default:
        return Q_SUPER(&QHsm_top);
    }
}

/* ==================== Unified Print (Print + Mute) ==================== */
QState Lab2B_Print(Lab2B *me) {
    switch (Q_SIG(me)) {

    case Q_ENTRY_SIG: {
        //XGpio_DiscreteWrite(&rgbled, 1, me->isMuted ? 10 : 2);

        /* Muted: hide bar; Unmuted: show current volume bar */
        if (me->isMuted) {
            clearVolumeBar(me->volume);
        } else if (me->volume_changed || me->button_pressed) {
            drawVolumeBar(me->volume);
        }

        /* Show button overlay if requested */
        if (me->button_pressed) {
            switch (me->button) {
                case 1: drawButtonOverlay("BUT_1"); break;
                case 2: drawButtonOverlay("BUT_2"); break;
                case 3: drawButtonOverlay("BUT_3"); break;
                case 4: drawButtonOverlay("BUT_4"); break;
                case 5: drawButtonOverlay("BUT_5"); break;
                default: break;
            }
        }
        return Q_HANDLED();
    }

    case Q_EXIT_SIG: {
        return Q_HANDLED();
    }

    /* -------- Toggle Mute -------- */
    case ENCODER_CLICK: {
        me->isMuted ^= 1;
        me->volume_changed = 1;
        if (me->isMuted) {
            clearVolumeBar(me->volume);
        } else {
            drawVolumeBar(me->volume);
        }
        return Q_HANDLED();
    }

    /* -------- Encoder rotation -------- */
    case ENCODER_UP: {
        if (me->isMuted) return Q_HANDLED();   // ignore while muted
        int old = me->volume;
        me->volume++;
        CLAMP_VOL(me->volume);
        me->volume_changed = 1;
        updateVolumeBar(old, me->volume);
        //XGpio_DiscreteWrite(&rgbled, 1, 3);
        return Q_HANDLED();
    }

    case ENCODER_DOWN: {
        if (me->isMuted) return Q_HANDLED();   // ignore while muted
        int old = me->volume;
        me->volume--;
        CLAMP_VOL(me->volume);
        me->volume_changed = 1;
        updateVolumeBar(old, me->volume);
        //XGpio_DiscreteWrite(&rgbled, 1, 4);
        return Q_HANDLED();
    }

    /* -------- Buttons -------- */
    case BUT_1: { me->button = 1; me->button_pressed = 1; drawButtonOverlay("BUT_1"); return Q_HANDLED(); }
    case BUT_2: { me->button = 2; me->button_pressed = 1; drawButtonOverlay("BUT_2"); return Q_HANDLED(); }
    case BUT_3: { me->button = 3; me->button_pressed = 1; drawButtonOverlay("BUT_3"); return Q_HANDLED(); }
    case BUT_4: { me->button = 4; me->button_pressed = 1; drawButtonOverlay("BUT_4"); return Q_HANDLED(); }
    case BUT_5: { me->button = 5; me->button_pressed = 1; drawButtonOverlay("BUT_5"); return Q_HANDLED(); }

    /* -------- Timeout to Idle -------- */
    case WAITED: {
        return Q_TRAN(&Lab2B_Idle);
    }

    default:
        return Q_SUPER(&QHsm_top);
    }
}
