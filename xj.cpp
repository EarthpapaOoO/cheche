code
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <softPwm.h>
#include <netdb.h>
#include <sys/types.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <wiringPi.h>
#define LEFT 23
#define RIGHT 25
#define MIDDLE 24
#define BUFSIZE 512
#define PWMA 1
#define AIN2 2
#define AIN1 3
#define PWMB 4
#define BIN2 5
#define BIN1 6
unsigned int speed = 60;
int SR;
int SL;

// 全局变量
int last_direction = 0; // 0:无, 1:左, 2:右
int crossroad_count = 0; // 十字路口计数器
int line_lost_timer = 0; // 丢失黑线计时器
const int MAX_LOST_TIME = 150; // 增加最大丢失时间（循环次数）
int search_direction = 1; // 搜索方向: 1=右转, 2=左转
int search_phase = 0; // 搜索阶段: 0=初始, 1=沿最后方向转向, 2=左右交替
int search_duration = 0; // 当前转向持续时间
const int INITIAL_TURN_DURATION = 100; // 初始转向持续时间（约1秒，100*10ms）
const int ALTERNATE_TURN_DURATION = 15; // 交替转向持续时间

// 紧急停止信号处理函数
void emergency_stop(int sig)
{
    printf("\n程序被中断, 紧急停止电机!\n");
    digitalWrite(AIN2, 0);
    digitalWrite(AIN1, 0);
    digitalWrite(BIN2, 0);
    digitalWrite(BIN1, 0);
    softPwmWrite(PWMA, 0);
    softPwmWrite(PWMB, 0);
    exit(0);
}

void t_up()
{
    digitalWrite(AIN2, 0);
    digitalWrite(AIN1, 1);
    softPwmWrite(PWMA, speed);

    digitalWrite(BIN2, 0);
    digitalWrite(BIN1, 1);
    softPwmWrite(PWMB, speed);
}

void t_down()
{
    digitalWrite(AIN2, 1);
    digitalWrite(AIN1, 0);
    softPwmWrite(PWMA, speed);

    digitalWrite(BIN2, 1);
    digitalWrite(BIN1, 0);
    softPwmWrite(PWMB, speed);
}
void t_left()
{
    digitalWrite(AIN2, 1);
    digitalWrite(AIN1, 0);
    softPwmWrite(PWMA, speed);

    digitalWrite(BIN2, 0);
    digitalWrite(BIN1, 1);
    softPwmWrite(PWMB, speed);
}
void t_right()
{
    digitalWrite(AIN2, 0);
    digitalWrite(AIN1, 1);
    softPwmWrite(PWMA, speed);

    digitalWrite(BIN2, 1);
    digitalWrite(BIN1, 0);
    softPwmWrite(PWMB, speed);
}
void t_stop()
{
    digitalWrite(AIN2, 0);
    digitalWrite(AIN1, 0);
    softPwmWrite(PWMA, 0);

    digitalWrite(BIN2, 0);
    digitalWrite(BIN1, 0);
    softPwmWrite(PWMB, 0);
}

// 轻微右转函数
void soft_right()
{
    digitalWrite(AIN2, 0);
    digitalWrite(AIN1, 1);
    softPwmWrite(PWMA, speed); // 左电机全速

    digitalWrite(BIN2, 0);
    digitalWrite(BIN1, 1);
    softPwmWrite(PWMB, speed / 3); // 右电机低速（同向但速度慢）
}

// 轻微左转函数
void soft_left()
{
    digitalWrite(AIN2, 0);
    digitalWrite(AIN1, 1);
    softPwmWrite(PWMA, speed / 3); // 左电机低速

    digitalWrite(BIN2, 0);
    digitalWrite(BIN1, 1);
    softPwmWrite(PWMB, speed); // 右电机全速
}

int main()
{
    signal(SIGINT, emergency_stop);
    if (wiringPiSetup() == -1)
    {
        printf("wiringPi初始化失败！\n");
        return 1;
    }
    printf("wiringPi初始化成功\n");
    fflush(stdout);
    // 初始化各个引脚
    pinMode(PWMA, OUTPUT);
    pinMode(AIN2, OUTPUT);
    pinMode(AIN1, OUTPUT);
    pinMode(PWMB, OUTPUT);
    pinMode(BIN2, OUTPUT);
    pinMode(BIN1, OUTPUT);

    // 初始化循迹传感器引脚为输入
    pinMode(LEFT, INPUT);
    pinMode(RIGHT, INPUT);
    pinMode(MIDDLE, INPUT);
    pullUpDnControl(MIDDLE, PUD_UP);
    pullUpDnControl(LEFT, PUD_UP);   // 启用上拉电阻
    pullUpDnControl(RIGHT, PUD_UP);  // 启用上拉电阻

    // PWM初始化
    if (softPwmCreate(PWMA, 0, 100) != 0)
    {
        printf("PWMA PWM创建失败\n");
    }
    else
    {
        printf("PWMA PWM创建成功\n");
    }
    if (softPwmCreate(PWMB, 0, 100) != 0)
    {
        printf("PWMB PWM创建失败\n");
    }
    else
    {
        printf("PWMB PWM创建成功\n");
    }
// 主循环
while (1)
{
    // 读取三个传感器状态
    int SL = digitalRead(LEFT);    // 左传感器
    int SM = digitalRead(MIDDLE);  // 中间传感器
    int SR = digitalRead(RIGHT);   // 右传感器

    printf("Sensors: L=%d, M=%d, R=%d | Phase: %d, Dir: %d, Dur: %d\n",
           SL, SM, SR, search_phase, search_direction, search_duration);
    fflush(stdout);

    // 检查是否在任何阶段检测到黑线
    int detected_line = 0;

    // 中间传感器检测到黑线 - 直行
    if (SM == HIGH && SL == LOW && SR == LOW)
    {
        printf("MIDDLE LINE DETECTED - EXIT SEARCH, GO STRAIGHT\n");
        t_up();
        last_direction = 0;
        line_lost_timer = 0;
        search_phase = 0;
        search_direction = 1;
        detected_line = 1;
    }
    // 左侧传感器检测到黑线 - 右转
    else if (SL == HIGH && SM == LOW)
    {
        printf("LEFT LINE DETECTED - EXIT SEARCH, TURN RIGHT\n");
        t_right();
        last_direction = 1;
        line_lost_timer = 0;
        search_phase = 0;
        search_direction = 1;
        detected_line = 1;
    }
    // 右侧传感器检测到黑线 - 左转
    else if (SR == HIGH && SM == LOW)
    {
        printf("RIGHT LINE DETECTED - EXIT SEARCH, TURN LEFT\n");
        t_left();
        last_direction = 2;
        line_lost_timer = 0;
        search_phase = 0;
        search_direction = 1;
        detected_line = 1;
    }
    // 中间和左侧传感器检测到黑线 - 轻微右转
    else if (SM == HIGH && SL == HIGH && SR == LOW)
    {
        printf("LEFT+MIDDLE DETECTED - EXIT SEARCH, SOFT RIGHT\n");
        soft_right();
        last_direction = 1;
        line_lost_timer = 0;
        search_phase = 0;
        search_direction = 1;
        detected_line = 1;
    }
    // 中间和右侧传感器检测到黑线 - 轻微左转
    else if (SM == HIGH && SR == HIGH && SL == LOW)
    {
        printf("RIGHT+MIDDLE DETECTED - EXIT SEARCH, SOFT LEFT\n");
        soft_left();
        last_direction = 2;
        line_lost_timer = 0;
        search_phase = 0;
        search_direction = 1;
        detected_line = 1;
    }
    // 十字路口检测
    else if (SL == HIGH && SR == HIGH)
    {
        crossroad_count++;
        printf("CROSSROAD %d DETECTED - EXIT SEARCH\n", crossroad_count);

        // 根据计数决定行动
        if (crossroad_count % 2 == 1)
        {
            printf("TURN RIGHT\n");
            t_right();
            delay(300);
        }
        else
        {
            printf("GO STRAIGHT\n");
            t_up();
        }

        line_lost_timer = 0;
        search_phase = 0;
        search_direction = 1;
        detected_line = 1;
    }

    // 如果检测到黑线，跳过交替转向逻辑
    if (detected_line)
    {
        delay(10);
        continue; // 跳过本次循环的剩余部分
    }

    // 情况: 所有传感器都在白色区域（完全丢失黑线）
    if (SL == LOW && SM == LOW && SR == LOW)
    {
        line_lost_timer++;
        printf("COMPLETE LINE LOST %d/%d - ", line_lost_timer, MAX_LOST_TIME);

        if (line_lost_timer > MAX_LOST_TIME)
        {
            // 长时间完全丢失黑线，停止
            printf("STOP\n");
            t_stop();
        }
        else
        {
            // 处理搜索阶段
            if (search_phase == 0)
            {
                // 初始阶段：沿上一次检测到黑线的方向转向约1秒
                printf("INITIAL TURN BASED ON LAST DIRECTION: ");

                if (last_direction == 1)
                {
                    printf("TURN RIGHT (LAST LEFT)\n");
                    t_right();
                    search_direction = 1;
                }
                else if (last_direction == 2)
                {
                    printf("TURN LEFT (LAST RIGHT)\n");
                    t_left();
                    search_direction = 2;
                }
                else
                {
                    // 没有历史记录，从右转开始
                    printf("TURN RIGHT (NO HISTORY)\n");
                    t_right();
                    search_direction = 1;
                }

                search_phase = 1; // 进入沿最后方向转向阶段
                search_duration = 0; // 重置持续时间
            }
            else if (search_phase == 1)
            {
                // 沿最后方向转向阶段
                search_duration++;
                printf("CONTINUE TURN %s - DURATION: %d/%d\n",
                       search_direction == 1 ? "RIGHT" : "LEFT",
                       search_duration, INITIAL_TURN_DURATION);

                // 执行转向
                if (search_direction == 1)
                {
                    t_right();
                }
                else
                {
                    t_left();
                }

                // 持续转向约1秒后进入交替转向阶段
                if (search_duration >= INITIAL_TURN_DURATION)
                {
                    search_phase = 2; // 进入交替转向阶段
                    search_duration = 0; // 重置持续时间
                    printf("SWITCHING TO ALTERNATE SEARCH\n");
                }
            }
            else // search_phase == 2
            {
                // 交替转向阶段
                search_duration++;
                printf("ALTERNATE SEARCH %s - DURATION: %d/%d\n",
                       search_direction == 1 ? "RIGHT" : "LEFT",
                       search_duration, ALTERNATE_TURN_DURATION);

                // 执行转向
                if (search_direction == 1)
                {
                    t_right();
                }
                else
                {
                    t_left();
                }

                // 持续转向一段时间后切换方向
                if (search_duration >= ALTERNATE_TURN_DURATION)
                {
                    // 切换方向
                    search_direction = (search_direction == 1) ? 2 : 1;
                    search_duration = 0; // 重置持续时间
                    printf("SWITCHING SEARCH DIRECTION TO %s\n",
                           search_direction == 1 ? "RIGHT" : "LEFT");
                }
            }
        }
    }
    // 其他未预料到的情况
    else
    {
        printf("UNEXPECTED SENSOR PATTERN - STOP\n");
        t_stop();
        line_lost_timer = 0;
        search_phase = 0;
        search_direction = 1;
    }

    delay(10); // 小延迟
}
    printf("程序正常结束\n");
}
