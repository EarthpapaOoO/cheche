#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <gpiod.h>
#include <termios.h>

// ---------------- 引脚定义 ----------------
#define PWMA_BCM    18
#define AIN2_BCM    27
#define AIN1_BCM    22
#define PWMB_BCM    23
#define BIN2_BCM    24
#define BIN1_BCM    25

// ---------------- 全局变量 ----------------
struct gpiod_chip *chip;
struct gpiod_line *PWMA, *AIN2, *AIN1, *PWMB, *BIN2, *BIN1;
volatile sig_atomic_t running = 1;

// 退出信号
void handle_stop(int signum) {
    running = 0;
}

// 设置终端为非阻塞输入模式（立即响应按键）
void set_terminal_mode(int enable) {
    static struct termios oldt, newt;
    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
}

// 初始化GPIO
int gpio_init() {
    chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) {
        perror("无法打开GPIO芯片");
        return -1;
    }

    PWMA = gpiod_chip_get_line(chip, PWMA_BCM);
    AIN2 = gpiod_chip_get_line(chip, AIN2_BCM);
    AIN1 = gpiod_chip_get_line(chip, AIN1_BCM);
    PWMB = gpiod_chip_get_line(chip, PWMB_BCM);
    BIN2 = gpiod_chip_get_line(chip, BIN2_BCM);
    BIN1 = gpiod_chip_get_line(chip, BIN1_BCM);

    if (!PWMA || !AIN2 || !AIN1 || !PWMB || !BIN2 || !BIN1) {
        perror("无法获取GPIO引脚");
        return -1;
    }

    struct gpiod_line_request_config cfg = {
        .consumer = "car_control",
        .request_type = GPIOD_LINE_REQUEST_DIRECTION_OUTPUT,
        .flags = 0
    };

    if (gpiod_line_request(PWMA, &cfg, 0) < 0 ||
        gpiod_line_request(AIN2, &cfg, 0) < 0 ||
        gpiod_line_request(AIN1, &cfg, 0) < 0 ||
        gpiod_line_request(PWMB, &cfg, 0) < 0 ||
        gpiod_line_request(BIN2, &cfg, 0) < 0 ||
        gpiod_line_request(BIN1, &cfg, 0) < 0) {
        perror("无法配置引脚为输出模式");
        return -1;
    }

    return 0;
}

// ---------------- 动作函数 ----------------
void stop() {
    gpiod_line_set_value(AIN1, 0);
    gpiod_line_set_value(AIN2, 0);
    gpiod_line_set_value(PWMA, 0);
    gpiod_line_set_value(BIN1, 0);
    gpiod_line_set_value(BIN2, 0);
    gpiod_line_set_value(PWMB, 0);
    printf(">>> 停止\n");
}

void forward() {
    gpiod_line_set_value(AIN1, 1);
    gpiod_line_set_value(AIN2, 0);
    gpiod_line_set_value(PWMA, 1);

    gpiod_line_set_value(BIN1, 1);
    gpiod_line_set_value(BIN2, 0);
    gpiod_line_set_value(PWMB, 1);

    printf(">>> 前进\n");
}

void backward() {
    gpiod_line_set_value(AIN1, 0);
    gpiod_line_set_value(AIN2, 1);
    gpiod_line_set_value(PWMA, 1);

    gpiod_line_set_value(BIN1, 0);
    gpiod_line_set_value(BIN2, 1);
    gpiod_line_set_value(PWMB, 1);

    printf(">>> 后退\n");
}

void turn_left() {
    gpiod_line_set_value(AIN1, 0);
    gpiod_line_set_value(AIN2, 0);
    gpiod_line_set_value(PWMA, 0);

    gpiod_line_set_value(BIN1, 1);
    gpiod_line_set_value(BIN2, 0);
    gpiod_line_set_value(PWMB, 1);

    printf(">>> 左转\n");
}

void turn_right() {
    gpiod_line_set_value(BIN1, 0);
    gpiod_line_set_value(BIN2, 0);
    gpiod_line_set_value(PWMB, 0);

    gpiod_line_set_value(AIN1, 1);
    gpiod_line_set_value(AIN2, 0);
    gpiod_line_set_value(PWMA, 1);

    printf(">>> 右转\n");
}

// ---------------- 释放资源 ----------------
void cleanup() {
    stop();
    gpiod_line_release(PWMA);
    gpiod_line_release(AIN2);
    gpiod_line_release(AIN1);
    gpiod_line_release(PWMB);
    gpiod_line_release(BIN2);
    gpiod_line_release(BIN1);
    gpiod_chip_close(chip);
}

// ---------------- 主函数 ----------------
int main() {
    signal(SIGINT, handle_stop);

    if (gpio_init() != 0) {
        fprintf(stderr, "GPIO初始化失败\n");
        return 1;
    }

    printf("小车键盘控制启动...\n");
    printf("控制键： w=前进, s=后退, a=左转, d=右转, q=停止, x=退出\n");

    set_terminal_mode(1);

    while (running) {
        int c = getchar();
        if (c == EOF) continue;

        switch (c) {
            case 'w': forward(); break;
            case 's': backward(); break;
            case 'a': turn_left(); break;
            case 'd': turn_right(); break;
            case 'q': stop(); break;
            case 'x': running = 0; break;
            default: break;
        }
    }

    set_terminal_mode(0);
    cleanup();
    printf("程序已退出\n");
    return 0;
}
