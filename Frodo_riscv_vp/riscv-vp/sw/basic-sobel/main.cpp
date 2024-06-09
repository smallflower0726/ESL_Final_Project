#include "cassert"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string"
#include "string.h"
#include <iostream>

#include "B_0.h"
#include "B_1_secret_0.h"
#include "Error_2.h"
#include "Message.h"
#include "Secret_1.h"

union word {
        int sint;
        unsigned int uint;
        unsigned char uc[4];
        int8_t int8[4];
        uint16_t uint16[2];
};

unsigned int input_rgb_raw_data_offset;
const unsigned int output_rgb_raw_data_offset = 54;
int width;
int height;
unsigned int width_bytes;
unsigned char bits_per_pixel;
unsigned short bytes_per_pixel;
unsigned char *source_bitmap;
unsigned char *target_bitmap;
const int WHITE = 255;
const int BLACK = 0;
const int THRESHOLD = 90;

// Sobel Filter ACC
static char *const SOBELFILTER_START_ADDR = reinterpret_cast<char *const>(0x73000000);
static char *const SOBELFILTER_READ_ADDR = reinterpret_cast<char *const>(0x73000004);

// DMA
static volatile uint32_t *const DMA_SRC_ADDR = (uint32_t *const)0x70000000;
static volatile uint32_t *const DMA_DST_ADDR = (uint32_t *const)0x70000004;
static volatile uint32_t *const DMA_LEN_ADDR = (uint32_t *const)0x70000008;
static volatile uint32_t *const DMA_OP_ADDR = (uint32_t *const)0x7000000C;
static volatile uint32_t *const DMA_STAT_ADDR = (uint32_t *const)0x70000010;
static const uint32_t DMA_OP_MEMCPY = 1;

bool _is_using_dma = true;

void write_data_to_ACC(char *ADDR, unsigned char *buffer, int len)
{
    if (_is_using_dma)
    {
        // Using DMA
        *DMA_SRC_ADDR = (uint32_t)(buffer);
        *DMA_DST_ADDR = (uint32_t)(ADDR);
        *DMA_LEN_ADDR = len;
        *DMA_OP_ADDR = DMA_OP_MEMCPY;
    }
    else
    {
        // Directly Send
        memcpy(ADDR, buffer, sizeof(unsigned char) * len);
    }
}
void read_data_from_ACC(char *ADDR, unsigned char *buffer, int len)
{
    if (_is_using_dma)
    {
        // Using DMA
        *DMA_SRC_ADDR = (uint32_t)(ADDR);
        *DMA_DST_ADDR = (uint32_t)(buffer);
        *DMA_LEN_ADDR = len;
        *DMA_OP_ADDR = DMA_OP_MEMCPY;
    }
    else
    {
        // Directly Read
        memcpy(buffer, ADDR, sizeof(unsigned char) * len);
    }
}

int main(int argc, char *argv[])
{
    printf("======================================\n");
    unsigned char buffer[4] = {0};
    word output_data;
    word input_data;
    int total;
    unsigned int result;
    printf("Start processing...\n");
    for (int i = 0; i < 640; i++)
    {
        for (int j = 0; j < 640; j++)
        {
            input_data.uint16[0] = B_0[j][i];
            input_data.int8[2] = message[j][i];
            input_data.int8[3] = error_2[j][i];
            write_data_to_ACC(SOBELFILTER_START_ADDR, input_data.uc, 4);
        }

        for (int j = 0; j < 640; j++)
        {
            read_data_from_ACC(SOBELFILTER_READ_ADDR, output_data.uc, 4);
            result = output_data.uint16[0];
            result = result - B_1_secret_0[j][i];
            result = (((result >> 12) & 1) + ((result >> 13))) % 4;

            if (result != message[j][i])
            {
                std::cout << "message[" << j << "][" << i << "]" << " = " << message[j][i] << std::endl;
                std::cout << "result [" << j << "][" << i << "]" << " = " << result << std::endl;
                std::cout << "pattern[" << j << "][" << i << "] fail !" << std::endl;
            }
            else
            {
                std::cout << "pattern[" << j << "][" << i << "] pass !" << std::endl;
            }
        }
    }
}
