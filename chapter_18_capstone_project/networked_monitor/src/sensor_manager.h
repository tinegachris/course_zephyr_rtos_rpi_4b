#ifndef SENSOR_MANAGER_H_
#define SENSOR_MANAGER_H_

struct sensor_reading {
    struct sensor_value temp;
    struct sensor_value press;
    struct sensor_value humidity;
};

void sensor_thread(void);

#endif /* SENSOR_MANAGER_H_ */