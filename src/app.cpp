#include "app.hpp"

using namespace freertos;
using namespace abstract;

// =============================================================================
// abstract::app
// =============================================================================

app::app(freertos::abstract::task& task) : task(task) {}

bool app::start(bool is_from_isr){
    if (this->is_running()){
        return false;
    }

    return is_from_isr ? this->task.resume_from_isr() : this->task.resume();
}

bool app::stop(bool is_from_isr){
    bool status = is_from_isr ? this->task.suspend_from_isr() : this->task.suspend();

    if (status){
        this->task.join();
    }

    return status;
}

bool app::is_running(void){
    return this->task.is_running();
}
