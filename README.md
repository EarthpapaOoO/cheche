# cheche
实训项目跑树莓派小车,组内自用
##  小车第一步,用finalshell或者putty连接小车 
  * 记住在树莓派内写入的wifi名称和密码,务必让电脑热点的wifi名称和密码与树莓派内的保持相同
  * 将卡插入小车,打开小车后再打开电脑热点,等待小车连接电脑热点
  * 小车连接上电脑热点后,在网络设置中找到小车的ip
  * 用ssh连接该ip
##  小车第二步,用vscode之类的软件连接小车(可选,极度推荐)
  * finalshell之类的软件非常轻量化和通用,但是使用和编辑体验太差,建议使用vscode,
  * 但是使用vscode之前必须**先在树莓派内下载gcc**,这样在第一次连接时才能下载vscode server为ssh连接服务
  * 如何用vscode进行ssh连接省略...
##  小车第三步,编辑代码
### 环境配置
gcc
lgpiod
### 电机驱动模块
以下是**AI**总结内容:
电机驱动模块总结

智能小车的运动由 四个直流电机 驱动完成，每个车轮一个电机。电机通过 L298N 电机驱动模块 控制，树莓派作为主控板，通过 GPIO 输出信号实现 前进、后退、左转、右转 等动作。

1. 硬件组成

L298N 电机驱动芯片

可驱动两路直流电机（每个模块控制左右两个电机）。

每一路电机均有 IN1/IN2（或 IN3/IN4） 控制正反转，ENA/ENB 引脚控制电机使能和调速。

树莓派 GPIO

输出高/低电平，决定电机的转动方向。

输出 PWM 信号（若需要调速），调节电机转速。

2. 控制原理

以一路电机（M1）为例：

正转：IN1=高电平, IN2=低电平

反转：IN1=低电平, IN2=高电平

停止：IN1=IN2=低电平 或 ENA=低电平

调速：在 ENA 输入 PWM 脉宽调制信号，调整占空比即可。

同理，M2 由 IN3/IN4 控制。

3. 驱动逻辑（树莓派 → L298N）

左电机：IN1, IN2, ENA

右电机：IN3, IN4, ENB

控制方式如下表：

动作	左电机 (IN1, IN2)	右电机 (IN3, IN4)	说明
前进	1, 0	1, 0	双电机正转
后退	0, 1	0, 1	双电机反转
左转	0, 1	1, 0	左轮后退，右轮前进
右转	1, 0	0, 1	左轮前进，右轮后退

停止	0, 0	0, 0	电机关闭
以下为实际代码示例
```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/select.h>
#include <gpiod.h>
#include <time.h>

// GPIO引脚定义
#define PWMA_BCM 18
#define AIN2_BCM 27
#define AIN1_BCM 22
#define PWMB_BCM 23
#define BIN2_BCM 24
#define BIN1_BCM 25

#define MAX_SPEED 100
#define PWM_PERIOD 10000  // 10ms周期

struct gpiod_chip *chip;
struct gpiod_line *PWMA, *AIN2, *AIN1, *PWMB, *BIN2, *BIN1;
volatile sig_atomic_t running = 1;
int speed = 70;  // 初始速度

// 信号处理
void handle_stop(int sig) { running = 0; }

// 初始化GPIO
int gpio_init() {
    chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) { perror("无法打开GPIO芯片"); return -1; }

    PWMA = gpiod_chip_get_line(chip, PWMA_BCM);
    AIN2 = gpiod_chip_get_line(chip, AIN2_BCM);
    AIN1 = gpiod_chip_get_line(chip, AIN1_BCM);
    PWMB = gpiod_chip_get_line(chip, PWMB_BCM);
    BIN2 = gpiod_chip_get_line(chip, BIN2_BCM);
    BIN1 = gpiod_chip_get_line(chip, BIN1_BCM);

    struct gpiod_line_request_config cfg = {
        .consumer = "car_keyboard",
        .request_type = GPIOD_LINE_REQUEST_DIRECTION_OUTPUT,
        .flags = 0
    };
    int init_val = 0;
    gpiod_line_request(PWMA, &cfg, init_val);
    gpiod_line_request(AIN2, &cfg, init_val);
    gpiod_line_request(AIN1, &cfg, init_val);
    gpiod_line_request(PWMB, &cfg, init_val);
    gpiod_line_request(BIN2, &cfg, init_val);
    gpiod_line_request(BIN1, &cfg, init_val);

    return 0;
}

// 清理GPIO
void cleanup() {
    gpiod_line_release(PWMA);
    gpiod_line_release(AIN2);
    gpiod_line_release(AIN1);
    gpiod_line_release(PWMB);
    gpiod_line_release(BIN2);
    gpiod_line_release(BIN1);
    gpiod_chip_close(chip);
}

// 设置小车状态
void stop_car() {
    gpiod_line_set_value(AIN1,0);
    gpiod_line_set_value(AIN2,0);
    gpiod_line_set_value(PWMA,0);
    gpiod_line_set_value(BIN1,0);
    gpiod_line_set_value(BIN2,0);
    gpiod_line_set_value(PWMB,0);
}

void forward() {
    gpiod_line_set_value(AIN1,1); gpiod_line_set_value(AIN2,0);
    gpiod_line_set_value(BIN1,1); gpiod_line_set_value(BIN2,0);
    gpiod_line_set_value(PWMA,1); gpiod_line_set_value(PWMB,1);
}

void backward() {
    gpiod_line_set_value(AIN1,0); gpiod_line_set_value(AIN2,1);
    gpiod_line_set_value(BIN1,0); gpiod_line_set_value(BIN2,1);
    gpiod_line_set_value(PWMA,1); gpiod_line_set_value(PWMB,1);
}

void turn_left() {
    gpiod_line_set_value(AIN1,0); gpiod_line_set_value(AIN2,0); gpiod_line_set_value(PWMA,0);
    gpiod_line_set_value(BIN1,1); gpiod_line_set_value(BIN2,0); gpiod_line_set_value(PWMB,1);
}

void turn_right() {
    gpiod_line_set_value(AIN1,1); gpiod_line_set_value(AIN2,0); gpiod_line_set_value(PWMA,1);
    gpiod_line_set_value(BIN1,0); gpiod_line_set_value(BIN2,0); gpiod_line_set_value(PWMB,0);
}

// 非阻塞读取按键
int kbhit() {
    struct timeval tv = {0,0};
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(0,&readfds);
    return select(1,&readfds,NULL,NULL,&tv);
}

int getch_noblock() {
    struct termios oldt, newt;
    int ch = 0;
    tcgetattr(0,&oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0,TCSANOW,&newt);
    if (kbhit()) ch = getchar();
    tcsetattr(0,TCSANOW,&oldt);
    return ch;
}

int main() {
    signal(SIGINT, handle_stop);
    if (gpio_init() != 0) { fprintf(stderr,"GPIO初始化失败\n"); return 1; }

    printf("键盘控制小车启动，W/A/S/D 前进/左/后/右, X 停止, +/- 调节速度\n");

    while(running) {
        int c = getch_noblock();
        if (c) {
            switch(c) {
                case 'w': forward(); break;
                case 's': backward(); break;
                case 'a': turn_left(); break;
                case 'd': turn_right(); break;
                case 'x': stop_car(); break;
                case '+': if(speed<MAX_SPEED) speed+=10; break;
                case '-': if(speed>10) speed-=10; break;
                default: stop_car(); break;
            }
        } else {
            stop_car();  // 没有按键时停止
        }
        usleep(20000); // 20ms循环检查
    }

    stop_car();
    cleanup();
    printf("\n程序退出\n");
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/select.h>
#include <gpiod.h>
#include <time.h>

// GPIO引脚定义
#define PWMA_BCM 18
#define AIN2_BCM 27
#define AIN1_BCM 22
#define PWMB_BCM 23
#define BIN2_BCM 24
#define BIN1_BCM 25

#define MAX_SPEED 100
#define PWM_PERIOD 10000  // 10ms周期

struct gpiod_chip *chip;
struct gpiod_line *PWMA, *AIN2, *AIN1, *PWMB, *BIN2, *BIN1;
volatile sig_atomic_t running = 1;
int speed = 70;  // 初始速度

// 信号处理
void handle_stop(int sig) { running = 0; }

// 初始化GPIO
int gpio_init() {
    chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) { perror("无法打开GPIO芯片"); return -1; }

    PWMA = gpiod_chip_get_line(chip, PWMA_BCM);
    AIN2 = gpiod_chip_get_line(chip, AIN2_BCM);
    AIN1 = gpiod_chip_get_line(chip, AIN1_BCM);
    PWMB = gpiod_chip_get_line(chip, PWMB_BCM);
    BIN2 = gpiod_chip_get_line(chip, BIN2_BCM);
    BIN1 = gpiod_chip_get_line(chip, BIN1_BCM);

    struct gpiod_line_request_config cfg = {
        .consumer = "car_keyboard",
        .request_type = GPIOD_LINE_REQUEST_DIRECTION_OUTPUT,
        .flags = 0
    };
    int init_val = 0;
    gpiod_line_request(PWMA, &cfg, init_val);
    gpiod_line_request(AIN2, &cfg, init_val);
    gpiod_line_request(AIN1, &cfg, init_val);
    gpiod_line_request(PWMB, &cfg, init_val);
    gpiod_line_request(BIN2, &cfg, init_val);
    gpiod_line_request(BIN1, &cfg, init_val);

    return 0;
}

// 清理GPIO
void cleanup() {
    gpiod_line_release(PWMA);
    gpiod_line_release(AIN2);
    gpiod_line_release(AIN1);
    gpiod_line_release(PWMB);
    gpiod_line_release(BIN2);
    gpiod_line_release(BIN1);
    gpiod_chip_close(chip);
}

// 设置小车状态
void stop_car() {
    gpiod_line_set_value(AIN1,0);
    gpiod_line_set_value(AIN2,0);
    gpiod_line_set_value(PWMA,0);
    gpiod_line_set_value(BIN1,0);
    gpiod_line_set_value(BIN2,0);
    gpiod_line_set_value(PWMB,0);
}

void forward() {
    gpiod_line_set_value(AIN1,1); gpiod_line_set_value(AIN2,0);
    gpiod_line_set_value(BIN1,1); gpiod_line_set_value(BIN2,0);
    gpiod_line_set_value(PWMA,1); gpiod_line_set_value(PWMB,1);
}

void backward() {
    gpiod_line_set_value(AIN1,0); gpiod_line_set_value(AIN2,1);
    gpiod_line_set_value(BIN1,0); gpiod_line_set_value(BIN2,1);
    gpiod_line_set_value(PWMA,1); gpiod_line_set_value(PWMB,1);
}

void turn_left() {
    gpiod_line_set_value(AIN1,0); gpiod_line_set_value(AIN2,0); gpiod_line_set_value(PWMA,0);
    gpiod_line_set_value(BIN1,1); gpiod_line_set_value(BIN2,0); gpiod_line_set_value(PWMB,1);
}

void turn_right() {
    gpiod_line_set_value(AIN1,1); gpiod_line_set_value(AIN2,0); gpiod_line_set_value(PWMA,1);
    gpiod_line_set_value(BIN1,0); gpiod_line_set_value(BIN2,0); gpiod_line_set_value(PWMB,0);
}

// 非阻塞读取按键
int kbhit() {
    struct timeval tv = {0,0};
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(0,&readfds);
    return select(1,&readfds,NULL,NULL,&tv);
}

int getch_noblock() {
    struct termios oldt, newt;
    int ch = 0;
    tcgetattr(0,&oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0,TCSANOW,&newt);
    if (kbhit()) ch = getchar();
    tcsetattr(0,TCSANOW,&oldt);
    return ch;
}

int main() {
    signal(SIGINT, handle_stop);
    if (gpio_init() != 0) { fprintf(stderr,"GPIO初始化失败\n"); return 1; }

    printf("键盘控制小车启动，W/A/S/D 前进/左/后/右, X 停止, +/- 调节速度\n");

    while(running) {
        int c = getch_noblock();
        if (c) {
            switch(c) {
                case 'w': forward(); break;
                case 's': backward(); break;
                case 'a': turn_left(); break;
                case 'd': turn_right(); break;
                case 'x': stop_car(); break;
                case '+': if(speed<MAX_SPEED) speed+=10; break;
                case '-': if(speed>10) speed-=10; break;
                default: stop_car(); break;
            }
        } else {
            stop_car();  // 没有按键时停止
        }
        usleep(20000); // 20ms循环检查
    }

    stop_car();
    cleanup();
    printf("\n程序退出\n");
    return 0;
}



```










