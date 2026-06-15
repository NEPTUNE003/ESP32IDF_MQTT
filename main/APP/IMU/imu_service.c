#include "imu_service.h"
#include "qma6100p_task.h"   

float imu_service_get_pitch(void) {
    return g_pitch;
}
float imu_service_get_roll(void) {
    return g_roll;
}