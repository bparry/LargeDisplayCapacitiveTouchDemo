#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <Wire.h>

#define FSS18 &FreeSans18pt7b

TFT_eSPI tft = TFT_eSPI(TFT_WIDTH, TFT_HEIGHT);
uint8_t addr  = 0x38;  //CTP IIC ADDRESS
struct TouchLocation
{
    uint16_t x;
    uint16_t y;
};
static lv_disp_buf_t disp_buf;
static lv_color_t buf[LV_HOR_RES_MAX * 10];

uint8_t readFT6236TouchRegister( uint8_t reg )
{
    Wire.beginTransmission(addr);
    Wire.write( reg );  // register 0
    uint8_t retVal = Wire.endTransmission();

    uint8_t returned = Wire.requestFrom(addr, uint8_t(1) );    // request 6 uint8_ts from slave device #2

    if (Wire.available())
    {
        retVal = Wire.read();
    }

    return retVal;
}
uint8_t readFT6236TouchAddr( uint8_t regAddr, uint8_t * pBuf, uint8_t len )
{
    Wire.beginTransmission(addr);
    Wire.write( regAddr );  // register 0
    uint8_t retVal = Wire.endTransmission();

    uint8_t returned = Wire.requestFrom(addr, len);    // request 1 bytes from slave device #2

    uint8_t i;
    for (i = 0; (i < len) && Wire.available(); i++)
    {
        pBuf[i] = Wire.read();
    }

    return i;
}

uint8_t readFT6236TouchLocation( TouchLocation * pLoc, uint8_t num )
{
    uint8_t retVal = 0;
    uint8_t i;
    uint8_t k;

    do
    {
        if (!pLoc) break; // must have a buffer
        if (!num)  break; // must be able to take at least one

        uint8_t status = readFT6236TouchRegister(2);

        static uint8_t tbuf[40];

        if ((status & 0x0f) == 0) break; // no points detected

        uint8_t hitPoints = status & 0x0f;

//        Serial.print("number of hit points = ");
//        Serial.println( hitPoints );

        readFT6236TouchAddr( 0x03, tbuf, hitPoints*6);

        for (k=0,i = 0; (i < hitPoints*6)&&(k < num); k++, i += 6)
        {
            pLoc[k].x = (tbuf[i+0] & 0x0f) << 8 | tbuf[i+1];
            pLoc[k].y = (tbuf[i+2] & 0x0f) << 8 | tbuf[i+3];
        }

        retVal = k;

    } while (0);

    return retVal;
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

/*Read the touchpad*/
bool my_touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data)
{
    TouchLocation touchLocations[2];

    uint8_t count = readFT6236TouchLocation( touchLocations, 2 );
    if (count > 0) {
        data->state = LV_INDEV_STATE_PR;

        /*Set the coordinates*/
        data->point.x = touchLocations[0].x;
        data->point.y = touchLocations[0].y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }

    return false; /*Return `false` because we are not buffering and no more data to read*/
}

static lv_obj_t * slider_label;


static void slider_event_cb(lv_obj_t * slider, lv_event_t event)
{
    if(event == LV_EVENT_VALUE_CHANGED) {
        static char buf[4]; /* max 3 bytes for number plus 1 null terminating byte */
        snprintf(buf, 4, "%u", lv_slider_get_value(slider));
        lv_label_set_text(slider_label, buf);
    }
}

void setup() {
    Serial.begin(9600);

    Wire.begin(32, 33);

    lv_init();

    tft.init();
    tft.fillScreen(TFT_BLACK);

//    tft.setTextColor(TFT_WHITE);
//    tft.setTextDatum(MC_DATUM);
//    tft.setFreeFont(FSS18);
//    tft.drawString("Hello, World!", TFT_WIDTH / 2, TFT_HEIGHT / 2);

    lv_disp_buf_init(&disp_buf, buf, NULL, LV_HOR_RES_MAX * 10);

    /*Initialize the display*/
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TFT_WIDTH;
    disp_drv.ver_res = TFT_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    /*Initialize the input device driver*/
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);


    // Demo code!
    /* Create a slider in the center of the display */
    lv_obj_t * slider = lv_slider_create(lv_scr_act(), NULL);
    lv_obj_set_width(slider, LV_DPI * 2);
    lv_obj_align(slider, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_event_cb(slider, slider_event_cb);
    lv_slider_set_range(slider, 0, 100);

    /* Create a label below the slider */
    slider_label = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(slider_label, "0");
    lv_obj_set_auto_realign(slider_label, true);
    lv_obj_align(slider_label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    /* Create an informative label */
    lv_obj_t * info = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(info, "Welcome to the slider+label demo!\n"
                            "Move the slider and see that the label\n"
                            "updates to match it.");
    lv_obj_align(info, NULL, LV_ALIGN_IN_TOP_LEFT, 10, 10);

    lv_obj_t * label;

    lv_obj_t * btn1 = lv_btn_create(lv_scr_act(), NULL);
//    lv_obj_set_event_cb(btn1, event_handler);
    lv_obj_align(btn1, NULL, LV_ALIGN_CENTER, 0, -80);

    label = lv_label_create(btn1, NULL);
    lv_label_set_text(label, "Button");

    lv_obj_t * btn2 = lv_btn_create(lv_scr_act(), NULL);
//    lv_obj_set_event_cb(btn2, event_handler);
    lv_obj_align(btn2, NULL, LV_ALIGN_CENTER, 0, 80);
    lv_btn_set_checkable(btn2, true);
    lv_btn_toggle(btn2);
    lv_btn_set_fit2(btn2, LV_FIT_NONE, LV_FIT_TIGHT);

    label = lv_label_create(btn2, NULL);
    lv_label_set_text(label, "Toggled");

}

void loop() {
    lv_task_handler(); /* let the GUI do its work */
    delay(5);
}