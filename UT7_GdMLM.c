#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "ws2818b.pio.h"
#include "pico/binary_info.h"
#include "ssd1306.h"
#include "hardware/i2c.h"

// Definições
#define LED_COUNT 25
#define LED_PIN 7
#define JOY_X 26
#define JOY_Y 27
#define BUTTON_A 5  // Botão A: usado para LED fixo vermelho
#define BUTTON_B 6  // Botão B: usado para LED fixo verde
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

// Estrutura para representar um LED RGB
typedef struct {
    uint8_t G, R, B;
} npLED_t;

// Buffer de LEDs
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO
PIO np_pio;
uint sm;

// Posições e estado do jogo
int x = 2, y = 2;       // Posição do LED controlável (azul)
int fixed_x, fixed_y;   // Posição do LED fixo
int fixed_color;        // 0 = vermelho, 1 = verde
int score = 0;          // Contador de pontos

// Timer de rodada
uint32_t round_duration_ms = 15000;  // Inicia com 15 segundos
uint64_t round_start = 0;

// Inicializa a matriz de LEDs via PIO
void npInit(uint pin) {
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true);
    }
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
    for (uint i = 0; i < LED_COUNT; ++i) {
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
}

// Define a cor de um LED
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}

// Limpa os LEDs
void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i)
        npSetLED(i, 0, 0, 0);
}

// Envia o buffer para os LEDs
void npWrite() {
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100);
}

// Converte coordenadas (x, y) para o índice na matriz (fiação serpenteada)
int getIndex(int x, int y) {
    if (y % 2 == 0)
        return 24 - (y * 5 + x);
    else
        return 24 - (y * 5 + (4 - x));
}

// Lê os valores do joystick (via ADC) e converte para coordenadas de 0 a 4
// Note que para o eixo Y invertemos (4 - valor) para adequar a orientação.
void readJoystick(int *x, int *y) {
    adc_select_input(0);
    uint adc_y = adc_read();
    adc_select_input(1);
    uint adc_x = adc_read();
    *x = adc_x * 5 / 4096;
    *y = 4 - (adc_y * 5 / 4096);
}

// Gera uma posição e cor aleatória para o LED fixo,
// garantindo que não coincida com a posição do LED controlável.
void generateRandomPositionAndColor() {
    do {
        fixed_x = rand() % 5;
        fixed_y = rand() % 5;
    } while (fixed_x == x && fixed_y == y);
    fixed_color = rand() % 2;  // 0 = vermelho, 1 = verde
}

// Lê o estado de um botão (considerando pull-up: pressionado = 0)
bool isButtonPressed(uint pin) {
    return gpio_get(pin) == 0;
}

// Exibe a imagem de erro na matriz de LEDs
void displayErrorImage() {
    int errorMatrix[5][5][3] = {
        {{242, 0, 0}, {0, 0, 0},   {0, 0, 0},   {0, 0, 0},   {242, 0, 0}},
        {{0, 0, 0},   {242, 0, 0}, {0, 0, 0},   {242, 0, 0}, {0, 0, 0}},
        {{0, 0, 0},   {0, 0, 0},   {242, 0, 0}, {0, 0, 0},   {0, 0, 0}},
        {{0, 0, 0},   {242, 0, 0}, {0, 0, 0},   {242, 0, 0}, {0, 0, 0}},
        {{242, 0, 0}, {0, 0, 0},   {0, 0, 0},   {0, 0, 0},   {242, 0, 0}}
    };
    npClear();
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            int pos = getIndex(col, row);
            npSetLED(pos, errorMatrix[row][col][0], errorMatrix[row][col][1], errorMatrix[row][col][2]);
        }
    }
    npWrite();
    sleep_ms(200);
    npClear();
    npWrite();
}
void displayErrorImage2() {
    int errorMatrix[5][5][3] = {
        {{242, 0, 0}, {0, 0, 0},   {0, 0, 0},   {0, 0, 0},   {242, 0, 0}},
        {{0, 0, 0},   {242, 0, 0}, {0, 0, 0},   {242, 0, 0}, {0, 0, 0}},
        {{0, 0, 0},   {0, 0, 0},   {242, 0, 0}, {0, 0, 0},   {0, 0, 0}},
        {{0, 0, 0},   {242, 0, 0}, {0, 0, 0},   {242, 0, 0}, {0, 0, 0}},
        {{242, 0, 0}, {0, 0, 0},   {0, 0, 0},   {0, 0, 0},   {242, 0, 0}}
    };
    npClear();
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            int pos = getIndex(col, row);
            npSetLED(pos, errorMatrix[row][col][0], errorMatrix[row][col][1], errorMatrix[row][col][2]);
        }
    }
    npWrite();
    sleep_ms(5000);
    npClear();
    npWrite();
}

// Exibe a imagem de aprovação na matriz de LEDs
void displayApprovedImage() {
    int approvedMatrix[5][5][3] = {
        {{0, 0, 0},   {24,242,0}, {24,242,0}, {24,242,0}, {0, 0, 0}},
        {{24,242,0},  {0, 0, 0},  {0, 0, 0},  {0, 0, 0},  {24,242,0}},
        {{24,242,0},  {0, 0, 0},  {0, 0, 0},  {0, 0, 0},  {24,242,0}},
        {{24,242,0},  {0, 0, 0},  {0, 0, 0},  {0, 0, 0},  {24,242,0}},
        {{0, 0, 0},   {24,242,0}, {24,242,0}, {24,242,0}, {0, 0, 0}}
    };
    npClear();
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            int pos = getIndex(col, row);
            npSetLED(pos, approvedMatrix[row][col][0], approvedMatrix[row][col][1], approvedMatrix[row][col][2]);
        }
    }
    npWrite();
    sleep_ms(5000);
    npClear();
    npWrite();
}

// Funções de animação pré‑fase divididas em 3 frames
void animationFrame1() {
    int frame1[5][5][3] = {
        { {0,0,0},   {242,0,0}, {242,0,0}, {242,0,0}, {0,0,0} },
        { {0,0,0},   {0,0,0},   {0,0,0},   {242,0,0}, {0,0,0} },
        { {0,0,0},   {242,0,0}, {242,0,0}, {242,0,0}, {0,0,0} },
        { {0,0,0},   {0,0,0},   {0,0,0},   {242,0,0}, {0,0,0} },
        { {0,0,0},   {242,0,0}, {242,0,0}, {242,0,0}, {0,0,0} }
    };
    npClear();
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            int pos = getIndex(col, row);
            npSetLED(pos, frame1[row][col][0], frame1[row][col][1], frame1[row][col][2]);
        }
    }
    npWrite();
    sleep_ms(1000);
}

void animationFrame2() {
    int frame2[5][5][3] = {
        { {0,0,0},   {242,0,0}, {242,0,0}, {242,0,0}, {0,0,0} },
        { {0,0,0},   {242,0,0}, {0,0,0},   {242,0,0}, {0,0,0} },
        { {0,0,0},   {0,0,0},   {242,0,0}, {0,0,0},   {0,0,0} },
        { {0,0,0},   {242,0,0}, {0,0,0},   {0,0,0},   {0,0,0} },
        { {0,0,0},   {242,0,0}, {242,0,0}, {242,0,0}, {0,0,0} }
    };
    npClear();
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            int pos = getIndex(col, row);
            npSetLED(pos, frame2[row][col][0], frame2[row][col][1], frame2[row][col][2]);
        }
    }
    npWrite();
    sleep_ms(1000);
}

void animationFrame3() {
    int frame3[5][5][3] = {
        { {0,0,0},   {0,0,0},   {242,0,0}, {0,0,0},   {0,0,0} },
        { {0,0,0},   {242,0,0}, {242,0,0}, {0,0,0},   {0,0,0} },
        { {0,0,0},   {0,0,0},   {242,0,0}, {0,0,0},   {0,0,0} },
        { {0,0,0},   {0,0,0},   {242,0,0}, {0,0,0},   {0,0,0} },
        { {0,0,0},   {242,0,0}, {242,0,0}, {242,0,0}, {0,0,0} }
    };
    npClear();
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            int pos = getIndex(col, row);
            npSetLED(pos, frame3[row][col][0], frame3[row][col][1], frame3[row][col][2]);
        }
    }
    npWrite();
    sleep_ms(1000);
}

// Exibe a animação pré‑fase chamando os 3 frames em sequência
void showPreRoundAnimation() {
    npClear();
    npWrite();
    sleep_ms(2000);
    animationFrame1();
    animationFrame2();
    animationFrame3();
}

int main() {
    stdio_init_all();
    adc_init();
    adc_gpio_init(JOY_X);
    adc_gpio_init(JOY_Y);
    npInit(LED_PIN);
    
    // Inicializa os botões com pull-up
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);

    // Inicialização do i2c
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Processo de inicialização completo do OLED SSD1306
    ssd1306_init();

    // Preparar área de renderização para o display (ssd1306_width pixels por ssd1306_n_pages páginas)
    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);

    // zera o display inteiro
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

restart:

// Parte do código para exibir a mensagem no display (opcional: mudar ssd1306_height para 32 em ssd1306_i2c.h)
// /**
    char *text[] = {
        "Treino",
        "de",
        "coordenacao",
        "   ",
        "Pontuacao",
        "de corte",
        "   ",
        "20 pontos"};

    int y = 0;
    for (uint i = 0; i < count_of(text); i++)
    {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, &frame_area);
    sleep_ms(3000);
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);
    char *text05[] = {
        "Controle",
        "o LED azul",
        "com o joystick",
        "   ",
        "Aperte A no led",
        "Vermelho e B ",
        "no verde",
        };

    y = 0;
    for (uint i = 0; i < count_of(text05); i++)
    {
        ssd1306_draw_string(ssd, 5, y, text05[i]);
        y += 8;
    }
    render_on_display(ssd, &frame_area);
    sleep_ms(3000);
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);
    
    char *text2[] = {
        "Boa",
        "    ",
        "Sorte"};

    y = 0;
    for (uint i = 0; i < count_of(text2); i++)
    {
        ssd1306_draw_string(ssd, 5, y, text2[i]);
        y += 8;
    }
    render_on_display(ssd, &frame_area);
    sleep_ms(3000);
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

    
    // Semente para números aleatórios
    srand(time(NULL));
    
    // Gera a primeira posição e cor do LED fixo
    generateRandomPositionAndColor();
    
    showPreRoundAnimation();
    // Inicializa o timer da rodada
    round_start = time_us_64();

    // Loop principal do jogo: ele continuará enquanto não for finalizado o round de 5s
    while (true) {
        // Se o tempo da rodada expirou...
        if (time_us_64() - round_start >= (uint64_t)round_duration_ms * 1000) {
            if (round_duration_ms > 5000) {
                // Rodadas intermediárias: exibe a animação pré‑fase e reduz 5s da duração
                showPreRoundAnimation();
                round_duration_ms -= 5000;
                round_start = time_us_64();
            } else {
                // Final: rodada de 5 segundos concluída; interrompe o loop
                break;
            }
        }
        
        // Lê a posição do joystick e garante que está no intervalo 0 a 4
        readJoystick(&x, &y);
        if (x < 0) x = 0;
        if (x > 4) x = 4;
        if (y < 0) y = 0;
        if (y > 4) y = 4;
        
        // Se o LED controlável (azul) está sobre o LED fixo, verifica os botões
        if (x == fixed_x && y == fixed_y) {
            bool btnA = isButtonPressed(BUTTON_A);
            bool btnB = isButtonPressed(BUTTON_B);
            
            if (btnA && !btnB) {
                if (fixed_color == 0) {  // Correto para LED fixo vermelho
                    score++;
                    generateRandomPositionAndColor();
                } else {
                    // Penalidade aumenta conforme a fase atual:
                    // 1 ponto se round_duration_ms == 15000, 2 pontos se == 10000, 3 pontos se == 5000.
                    int penalty = (round_duration_ms == 15000) ? 1 : ((round_duration_ms == 10000) ? 2 : 3);
                    score -= penalty;
                    displayErrorImage();
                    sleep_ms(200);
                    displayErrorImage();
                }
            } else if (btnB && !btnA) {
                if (fixed_color == 1) {  // Correto para LED fixo verde
                    score++;
                    generateRandomPositionAndColor();
                } else {
                    // Penalidade aumenta conforme a fase atual:
                    // 1 ponto se round_duration_ms == 15000, 2 pontos se == 10000, 3 pontos se == 5000.
                    int penalty = (round_duration_ms == 15000) ? 1 : ((round_duration_ms == 10000) ? 2 : 3);
                    score -= penalty;
                    displayErrorImage();
                    sleep_ms(200);
                    displayErrorImage();
                }
            }
        }
        
        // Atualiza a matriz:
        // - LED controlável: azul
        // - LED fixo: vermelho ou verde conforme fixed_color
        npClear();
        npSetLED(getIndex(x, y), 0, 0, 255);
        if (fixed_color == 0)
            npSetLED(getIndex(fixed_x, fixed_y), 255, 0, 0);
        else
            npSetLED(getIndex(fixed_x, fixed_y), 0, 255, 0);
        npWrite();
        
        sleep_ms(100);
    } 
    
    // Após a fase final, apaga a matriz por 5 segundos
    npClear();
    npWrite();
    sleep_ms(5000);
    
    // Exibe a imagem final por 5 segundos:
    // Se score >= 20, jogador aprovado; caso contrário, exibe imagem de erro
    if (score >= 20) {
        
        char *text3[] = {
            "Resultado",
            "    ",
            "Aprovado ", };
    
        y = 0;
        for (uint i = 0; i < count_of(text3); i++)
        {
            ssd1306_draw_string(ssd, 5, y, text3[i]);
            y += 8;
        }
        char scoreStr[16];
        sprintf(scoreStr, "Pontuacao = %d", score);
        ssd1306_draw_string(ssd, 5, y, scoreStr);
        render_on_display(ssd, &frame_area);
        displayApprovedImage();
        sleep_ms(5000);
        memset(ssd, 0, ssd1306_buffer_length);
        render_on_display(ssd, &frame_area);
        npClear();
    } else {
        
        char *text4[] = {
            "Resultado",
            "    ",
            "Reprovado"};
    
        y = 0;
        for (uint i = 0; i < count_of(text4); i++)
        {
            ssd1306_draw_string(ssd, 5, y, text4[i]);
            y += 8;
        }
        char scoreStr[16];
        sprintf(scoreStr, "Pontuacao = %d", score);
        ssd1306_draw_string(ssd, 5, y, scoreStr);
        render_on_display(ssd, &frame_area);
        displayErrorImage2();
        sleep_ms(5000); 
        memset(ssd, 0, ssd1306_buffer_length);
        render_on_display(ssd, &frame_area);
        
        npClear();
    }
    sleep_ms(5000);
    npClear();
    
    // Fim da execução
    return 0;
}
