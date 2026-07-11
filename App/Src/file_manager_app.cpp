#include "file_manager_app.h"

#include "FreeRTOS.h"
#include "display_driver.h"
#include "fatfs_file_system.hpp"
#include "file_manager_controller.hpp"
#include "file_manager_view.hpp"
#include "main.h"
#include "task.h"

namespace {

fm::FatFsFileSystem fileSystem;
fm::FileManagerController controller(fileSystem);
fm::FileManagerView view(controller);

void FileManagerTask(void*) {
  DisplayDriver_Init();
  DisplayDriver_DrawText(82U, 112U, "MOUNTING SD", DISPLAY_COLOR_BLACK,
                         DISPLAY_COLOR_WHITE, 2U);
  view.start();

  bool wasPressed = false;
  for (;;) {
    DisplayTouchPoint point = {};
    const bool pressed = DisplayDriver_ReadTouch(&point) != 0U;
    if (pressed && !wasPressed) {
      view.handleTouch(point.x, point.y);
    }
    wasPressed = pressed;
    vTaskDelay(pdMS_TO_TICKS(10U));
  }
}

}  // namespace

extern "C" void FileManagerApp_Start(void) {
  if (xTaskCreate(FileManagerTask, "FileManager", 1024U, nullptr, 2U,
                  nullptr) != pdPASS) {
    Error_Handler();
  }
  vTaskStartScheduler();
  Error_Handler();
}

extern "C" void vApplicationMallocFailedHook(void) {
  Error_Handler();
}

extern "C" void vApplicationStackOverflowHook(TaskHandle_t, char*) {
  Error_Handler();
}
