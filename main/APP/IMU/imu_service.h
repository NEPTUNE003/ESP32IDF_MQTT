#ifndef IMU_SERVICE_H
#define IMU_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

float imu_service_get_pitch(void);
float imu_service_get_roll(void);

#ifdef __cplusplus
}
#endif
#endif