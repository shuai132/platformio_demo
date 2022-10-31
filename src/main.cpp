#include <Arduino.h>
#include <M5Core2.h>
#include <SPI.h>
#include <Wire.h>
#include <lvgl.h>

#include "sys/ioctl.h"
#include "log.h"
#include "lvgl/benchmark/lv_demo_benchmark.h"
#include "lvgl/widgets/lv_demo_widgets.h"

#define LV_HOR_RES_MAX 320
#define LV_VER_RES_MAX 240

static TFT_eSPI tft = TFT_eSPI();
static lv_disp_draw_buf_t disp_buf;
static lv_color_t buf[LV_HOR_RES_MAX * 10];

/*Read the touchpad*/
extern "C" void my_touchpad_read(struct _lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
  TouchPoint_t pos = M5.Touch.getPressPoint();
  bool touched = pos.x != -1;
  if (!touched) {
    data->state = LV_INDEV_STATE_REL;
  } else {
    data->state = LV_INDEV_STATE_PR;
    /*Set the coordinates*/
    data->point.x = pos.x;
    data->point.y = pos.y;
  }
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors(&color_p->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

void setup() {
  M5.begin(true, false, true, false);
  tft.begin();
  tft.setRotation(1);
  M5.Axp.SetLcdVoltage(2800);
  M5.Axp.SetLcdVoltage(3300);
  M5.Axp.SetBusPowerMode(0);
  M5.Axp.SetCHGCurrent(AXP192::kCHG_190mA);
  M5.Axp.SetLDOEnable(3, true);
  delay(150);
  M5.Axp.SetLDOEnable(3, false);
  M5.Axp.SetLed(1);
  delay(100);
  M5.Axp.SetLed(0);
  M5.Axp.SetLDOVoltage(3, 3300);
  M5.Axp.SetLed(1);
  //-------------------------------------------------------------------
  lv_init();
  lv_disp_draw_buf_init(&disp_buf, buf, nullptr, LV_HOR_RES_MAX * 10);
  //-------------------------------------------------------------------
  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LV_HOR_RES_MAX;
  disp_drv.ver_res = LV_VER_RES_MAX;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &disp_buf;
  lv_disp_drv_register(&disp_drv);

  //-------------------------------------------------------------------
  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  //  lv_demo_benchmark(LV_DEMO_BENCHMARK_MODE_REAL);
  lv_demo_widgets();
}

//=====================================================================
void loop() {
  lv_task_handler(); /* let the GUI do its work */
  delay(5);
}
