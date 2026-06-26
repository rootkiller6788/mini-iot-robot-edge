CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -O2 -Iinclude
LDFLAGS = -lm
AR = ar

SRCS = src/imu_sensor.c src/temp_env.c src/tof_lidar.c src/motor_control.c src/camera_mipi.c
OBJS = $(SRCS:.c=.o)
HEADS = include/imu_sensor.h include/temp_env.h include/tof_lidar.h include/motor_control.h include/camera_mipi.h

EXAMPLES = examples/example_imu examples/example_environment examples/example_motor
DEMOS = demos/demo_robot demos/demo_drone

.PHONY: all clean examples demos

all: libsensoractuator.a

libsensoractuator.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

$(OBJS): $(HEADS)

src/imu_sensor.o: src/imu_sensor.c include/imu_sensor.h
	$(CC) $(CFLAGS) -c $< -o $@

src/temp_env.o: src/temp_env.c include/temp_env.h
	$(CC) $(CFLAGS) -c $< -o $@

src/tof_lidar.o: src/tof_lidar.c include/tof_lidar.h
	$(CC) $(CFLAGS) -c $< -o $@

src/motor_control.o: src/motor_control.c include/motor_control.h
	$(CC) $(CFLAGS) -c $< -o $@

src/camera_mipi.o: src/camera_mipi.c include/camera_mipi.h
	$(CC) $(CFLAGS) -c $< -o $@

examples: $(EXAMPLES)

examples/example_imu: examples/example_imu.c libsensoractuator.a
	$(CC) $(CFLAGS) $< -L. -lsensoractuator $(LDFLAGS) -o $@

examples/example_environment: examples/example_environment.c libsensoractuator.a
	$(CC) $(CFLAGS) $< -L. -lsensoractuator $(LDFLAGS) -o $@

examples/example_motor: examples/example_motor.c libsensoractuator.a
	$(CC) $(CFLAGS) $< -L. -lsensoractuator $(LDFLAGS) -o $@

demos: $(DEMOS)

demos/demo_robot: demos/demo_robot.c libsensoractuator.a
	$(CC) $(CFLAGS) $< -L. -lsensoractuator $(LDFLAGS) -o $@

demos/demo_drone: demos/demo_drone.c libsensoractuator.a
	$(CC) $(CFLAGS) $< -L. -lsensoractuator $(LDFLAGS) -o $@

clean:
	del /q src\*.o libsensoractuator.a examples\example_imu.exe examples\example_environment.exe examples\example_motor.exe demos\demo_robot.exe demos\demo_drone.exe 2>nul || rm -f src/*.o libsensoractuator.a examples/example_imu examples/example_environment examples/example_motor demos/demo_robot demos/demo_drone
