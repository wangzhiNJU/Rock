#include "RockContext.h"

TaskTracker* RockContext::get_tasktracker() {
 return new TaskTracker(this);  
}
