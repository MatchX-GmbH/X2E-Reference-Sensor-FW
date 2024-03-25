//==========================================================================
// Entry point - app_main()
//==========================================================================
#include <stdio.h>

#include "mx_target.h"
#include "task_priority.h"
#include "debug.h"

#include "test.h"

//==========================================================================
//==========================================================================

//==========================================================================
//==========================================================================
void app_main(void) {
  // Set task priority
  vTaskPrioritySet(NULL, TASK_PRIO_STARTING);

  // Kick start application tasks
  DEBUG_INIT();
  MxTargetInit();

  // Enter forever loop
  vTaskPrioritySet(NULL, TASK_PRIO_GENERAL);
  DEBUG_PRINTLINE("Set app_main to prio%d", uxTaskPriorityGet(NULL));

  // Jump to Test
  TestGo();

  //
  DEBUG_PRINTLINE("app_main Ended.");
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  DEBUG_EXIT();
}
