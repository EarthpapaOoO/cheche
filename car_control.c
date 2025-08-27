//car_control.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <gpiod.h>
#include <time.h>

// 引脚定义（BCM编号，根据新分配修改）
// 左前轮和左后轮共用控制引脚
#define PWMA_BCM    18  // 左轮PWM（控制左前轮和左后轮）
#define AIN2_BCM    27  // 左轮方向控制2
#define AIN1_BCM    22  // 左轮方向控制1

// 右前轮和右后轮共用控制引脚
#define PWMB_BCM    23  // 右轮PWM（控制右前轮和右后轮）
#define BIN2_BCM    24  // 右轮方向控制2
#define BIN1_BCM    25  // 右轮方向控制1

// PWM配置参数
#define PWM_FREQ    100     // PWM频率(Hz)
#define PWM_PERIOD  10000   // 周期(微秒) = 1000000 / PWM_FREQ
#define MAX_SPEED   100     // 速度范围(0-100)

// 全局变量
struct gpiod_chip *chip;
struct gpiod_line *PWMA, *AIN2, *AIN1, *PWMB, *BIN2, *BIN1;
volatile sig_atomic_t running = 1;

// 信号处理函数：捕获Ctrl+C
void handle_stop(int signum) {
    running = 0;
}

// 初始化GPIO
int gpio_init() {
    // 打开GPIO芯片
    chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) {
        perror("无法打开GPIO芯片");
        return -1;
    }

    // 获取所有控制引脚
    PWMA = gpiod_chip_get_line(chip, PWMA_BCM);
    AIN2 = gpiod_chip_get_line(chip, AIN2_BCM);
    AIN1 = gpiod_chip_get_line(chip, AIN1_BCM);
    PWMB = gpiod_chip_get_line(chip, PWMB_BCM);
    BIN2 = gpiod_chip_get_line(chip, BIN2_BCM);
    BIN1 = gpiod_chip_get_line(chip, BIN1_BCM);

    // 检查引脚获取状态
    if (!PWMA || !AIN2 || !AIN1 || !PWMB || !BIN2 || !BIN1) {
        perror("无法获取GPIO引脚");
        return -1;
    }

    // 配置所有引脚为输出模式
    struct gpiod_line_request_config cfg = {
        .consumer = "car_control",
        .request_type = GPIOD_LINE_REQUEST_DIRECTION_OUTPUT,
        .flags = 0
    };
    int init_val = 0;  // 初始低电平

    if (gpiod_line_request(PWMA, &cfg, init_val) < 0 ||
        gpiod_line_request(AIN2, &cfg, init_val) < 0 ||
        gpiod_line_request(AIN1, &cfg, init_val) < 0 ||
        gpiod_line_request(PWMB, &cfg, init_val) < 0 ||
        gpiod_line_request(BIN2, &cfg, init_val) < 0 ||
        gpiod_line_request(BIN1, &cfg, init_val) < 0) {
        perror("无法配置引脚为输出模式");
        return -1;
    }

    return 0;
}

// 软件PWM模拟函数
void pwm_write(struct gpiod_line *line, int speed, int duration_ms) {
    if (speed < 0 || speed > MAX_SPEED) return;
    
    int high_us = (speed * PWM_PERIOD) / MAX_SPEED;  // 高电平时长
    int low_us = PWM_PERIOD - high_us;               // 低电平时长
    int total_cycles = (duration_ms * 1000) / PWM_PERIOD;  // 总周期数

    for (int i = 0; i < total_cycles && running; i++) {
        gpiod_line_set_value(line, 1);  // 高电平
        usleep(high_us);
        if (!running) break;
        gpiod_line_set_value(line, 0);  // 低电平
        usleep(low_us);
    }
}

// 前进（所有轮子正向转动）
void forward(int speed, int duration_ms) {
    // 左轮（前+后）正向转动
    gpiod_line_set_value(AIN1, 1);  // AIN1=1, AIN2=0
    gpiod_line_set_value(AIN2, 0);
    
    // 右轮（前+后）正向转动
    gpiod_line_set_value(BIN1, 1);  // BIN1=1, BIN2=0
    gpiod_line_set_value(BIN2, 0);
    
    // 同时给左右轮PWM信号
    time_t start = time(NULL);
    while ((time(NULL) - start) * 1000 < duration_ms && running) {
        pwm_write(PWMA, speed, 10);  // 左轮PWM
        pwm_write(PWMB, speed, 10);  // 右轮PWM
    }
}

// 后退（所有轮子反向转动）
void backward(int speed, int duration_ms) {
    // 左轮（前+后）反向转动
    gpiod_line_set_value(AIN1, 0);  // AIN1=0, AIN2=1
    gpiod_line_set_value(AIN2, 1);
    
    // 右轮（前+后）反向转动
    gpiod_line_set_value(BIN1, 0);  // BIN1=0, BIN2=1
    gpiod_line_set_value(BIN2, 1);
    
    // 同时给左右轮PWM信号
    time_t start = time(NULL);
    while ((time(NULL) - start) * 1000 < duration_ms && running) {
        pwm_write(PWMA, speed, 10);
        pwm_write(PWMB, speed, 10);
    }
}

// 左转（左轮停转，右轮前进）
void turn_left(int speed, int duration_ms) {
    // 左轮（前+后）停止
    gpiod_line_set_value(AIN1, 0);
    gpiod_line_set_value(AIN2, 0);
    gpiod_line_set_value(PWMA, 0);
    
    // 右轮（前+后）正向转动
    gpiod_line_set_value(BIN1, 1);
    gpiod_line_set_value(BIN2, 0);
    
    // 右轮PWM输出
    pwm_write(PWMB, speed, duration_ms);
}

// 右转（右轮停转，左轮前进）
void turn_right(int speed, int duration_ms) {
    // 右轮（前+后）停止
    gpiod_line_set_value(BIN1, 0);
    gpiod_line_set_value(BIN2, 0);
    gpiod_line_set_value(PWMB, 0);
    
    // 左轮（前+后）正向转动
    gpiod_line_set_value(AIN1, 1);
    gpiod_line_set_value(AIN2, 0);
    
    // 左轮PWM输出
    pwm_write(PWMA, speed, duration_ms);
}

// 停止（所有轮子断电）
void stop(int duration_ms) {
    // 左轮停止
    gpiod_line_set_value(AIN1, 0);
    gpiod_line_set_value(AIN2, 0);
    gpiod_line_set_value(PWMA, 0);
    
    // 右轮停止
    gpiod_line_set_value(BIN1, 0);
    gpiod_line_set_value(BIN2, 0);
    gpiod_line_set_value(PWMB, 0);
    
    // 保持停止状态
    time_t start = time(NULL);
    while ((time(NULL) - start) * 1000 < duration_ms && running) {
        usleep(10000);  // 10ms检查一次退出信号
    }
}

// 释放资源
void cleanup() {
    gpiod_line_release(PWMA);
    gpiod_line_release(AIN2);
    gpiod_line_release(AIN1);
    gpiod_line_release(PWMB);
    gpiod_line_release(BIN2);
    gpiod_line_release(BIN1);
    gpiod_chip_close(chip);
}

int main() {
    // 注册信号处理（捕获Ctrl+C）
    signal(SIGINT, handle_stop);
    
    // 初始化GPIO
    if (gpio_init() != 0) {
        fprintf(stderr, "GPIO初始化失败\n");
        return 1;
    }
    
    printf("智能小车控制系统启动...\n");
    printf("按Ctrl+C停止\n");
    
    while (running) {
        forward(70, 3000);    // 前进3秒，速度70
        stop(1000);           // 停止1秒
        
        backward(70, 3000);   // 后退3秒，速度70
        stop(1000);           // 停止1秒
        
        turn_left(60, 1500);  // 左转1.5秒，速度60
        stop(1000);           // 停止1秒
        
        turn_right(60, 1500); // 右转1.5秒，速度60
        stop(2000);           // 停止2秒
    }
    
    // 程序结束处理
    stop(0);
    cleanup();
    printf("\n程序已停止\n");
    return 0;
}
    
