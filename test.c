#include <ncurses.h>
#include <math.h>

#define NUM_COLORS 216

int main() {
    // 要显示的矩阵
    int matrix[5][5][3] = {
        {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0}, {255, 0, 255}},
        {{0, 255, 255}, {128, 128, 128}, {255, 255, 255}, {0, 0, 0}, {255, 128, 0}},
        {{128, 128, 255}, {128, 255, 128}, {255, 128, 128}, {128, 255, 255}, {255, 128, 255}},
        {{255, 255, 128}, {128, 128, 0}, {128, 0, 128}, {0, 128, 128}, {0, 0, 128}},
        {{128, 128, 192}, {128, 192, 128}, {192, 128, 128}, {128, 192, 192}, {192, 128, 192}}
    };

    // 初始化 ncurses 程式庫
    initscr();

    // 檢查是否支援顏色
    if (!has_colors()) {
        endwin();
        printf("Error: Terminal does not support colors.\n");
        return 1;
    }

    // 啟用顏色模式
    start_color();

    // 設定調色板
    if (COLORS >= NUM_COLORS) {
        // 使用 216 种颜色
        int color_num = 0;
        for (int r = 0; r <= 5; r++) {
            for (int g = 0; g <= 5; g++) {
                for (int b = 0; b <= 5; b++) {
                    int r_val = r * 51;
                    int g_val = g * 51;
                    int b_val = b * 51;
                    init_color(color_num, r_val, g_val, b_val);
                    init_pair(color_num + 1, COLOR_BLACK, color_num);
                    color_num++;
                }
            }
        }
    } else {
        // 使用 16 种颜色
        for (int i = 0; i < 16; i++) {
            init_pair(i + 1, i, COLOR_BLACK);
        }
    }

    // 顯示矩形
    int x_start =5;
int y_start = 5;
int width = 3;
int height = 1;
for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
        int r = matrix[i][j][0];
        int g = matrix[i][j][1];
        int b = matrix[i][j][2];
        int color_num;
        if (COLORS >= NUM_COLORS) {
            // 使用 216 种颜色
            int r_val = round(r * 5.0 / 255.0) * 51;
            int g_val = round(g * 5.0 / 255.0) * 51;
            int b_val = round(b * 5.0 / 255.0) * 51;
            color_num = r_val / 51 * 36 + g_val / 51 * 6 + b_val / 51 + 1;
        } else {
            // 使用 16 种颜色
            if (r > 128 && g < 128 && b < 128) {
                color_num = 1;  // 红色
            } else if (r < 128 && g > 128 && b < 128) {
                color_num = 2;  // 绿色
            } else if (r < 128 && g < 128 && b > 128) {
                color_num = 3;  // 蓝色
            } else if (r > 128 && g > 128 && b < 128) {
                color_num = 4;  // 黄色
            } else if (r > 128 && g < 128 && b > 128) {
                color_num = 5;  // 洋红色
            } else if (r < 128 && g > 128 && b > 128) {
                color_num = 6;  // 青色
            } else {
                color_num = 7;  // 白色
            }
        }
        attron(COLOR_PAIR(color_num));
        for (int k = 0; k < height; k++) {
            for (int l = 0; l < width; l++) {
                mvaddch(y_start + k + i * height, x_start + l + j * width, ' ');
            }
        }
        attroff(COLOR_PAIR(color_num));
    }
}

// 更新螢幕上的內容
refresh();

// 等待使用者按下任意鍵後結束程式
getch();

// 結束 ncurses 程式
endwin();
}
