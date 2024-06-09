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
static char *const SOBELFILTER_START_ADDR_0 = reinterpret_cast<char *const>(0x73000000);
static char *const SOBELFILTER_READ_ADDR_0 = reinterpret_cast<char *const>(0x73000004);
static char *const SOBELFILTER_START_ADDR_1 = reinterpret_cast<char *const>(0x76000000);
static char *const SOBELFILTER_READ_ADDR_1 = reinterpret_cast<char *const>(0x76000004);

// DMA
static volatile uint32_t *const DMA_SRC_ADDR = (uint32_t *const)0x70000000;
static volatile uint32_t *const DMA_DST_ADDR = (uint32_t *const)0x70000004;
static volatile uint32_t *const DMA_LEN_ADDR = (uint32_t *const)0x70000008;
static volatile uint32_t *const DMA_OP_ADDR = (uint32_t *const)0x7000000C;
static volatile uint32_t *const DMA_STAT_ADDR = (uint32_t *const)0x70000010;
static const uint32_t DMA_OP_MEMCPY = 1;

bool _is_using_dma = true;

int sem_init(uint32_t *__sem, uint32_t count) __THROW
{
    *__sem = count;
    return 0;
}

int sem_wait(uint32_t *__sem) __THROW
{
    uint32_t value, success; // RV32A
    __asm__ __volatile__("\
L%=:\n\t\
     lr.w %[value],(%[__sem])            # load reserved\n\t\
     beqz %[value],L%=                   # if zero, try again\n\t\
     addi %[value],%[value],-1           # value --\n\t\
     sc.w %[success],%[value],(%[__sem]) # store conditionally\n\t\
     bnez %[success], L%=                # if the store failed, try again\n\t\
"
                         : [value] "=r"(value), [success] "=r"(success)
                         : [__sem] "r"(__sem)
                         : "memory");
    return 0;
}

int sem_post(uint32_t *__sem) __THROW
{
    uint32_t value, success; // RV32A
    __asm__ __volatile__("\
L%=:\n\t\
     lr.w %[value],(%[__sem])            # load reserved\n\t\
     addi %[value],%[value], 1           # value ++\n\t\
     sc.w %[success],%[value],(%[__sem]) # store conditionally\n\t\
     bnez %[success], L%=                # if the store failed, try again\n\t\
"
                         : [value] "=r"(value), [success] "=r"(success)
                         : [__sem] "r"(__sem)
                         : "memory");
    return 0;
}

int barrier(uint32_t *__sem, uint32_t *__lock, uint32_t *counter, uint32_t thread_count)
{
    sem_wait(__lock);
    if (*counter == thread_count - 1)
    { // all finished
        *counter = 0;
        sem_post(__lock);
        for (int j = 0; j < thread_count - 1; ++j)
            sem_post(__sem);
    }
    else
    {
        (*counter)++;
        sem_post(__lock);
        sem_wait(__sem);
    }
    return 0;
}

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
// Total number of cores
// static const int PROCESSORS = 2;
#define PROCESSORS 2
// the barrier synchronization objects
uint32_t barrier_counter = 0;
uint32_t barrier_lock;
uint32_t barrier_sem;
// the mutex object to control global summation
uint32_t lock;
// print synchronication semaphore (print in core order)
uint32_t print_sem[PROCESSORS];

int main(unsigned hart_id)
{
    if (hart_id == 0)
    {
        // create a barrier object with a count of PROCESSORS
        sem_init(&barrier_lock, 1);
        sem_init(&barrier_sem, 0); // lock all cores initially
        for (int i = 0; i < PROCESSORS; ++i)
        {
            sem_init(&print_sem[i], 0); // lock printing initially
        }
        // Create mutex lock
        sem_init(&lock, 1);
    }
    unsigned begin_idx;
    unsigned end_idx;

    if (hart_id == 0)
    {
        begin_idx = 0;
        end_idx = 319;
    }
    else
    {
        begin_idx = 320;
        end_idx = 639;
    }

    sem_wait(&lock);
    printf("======================================\n");
    sem_post(&lock);
    unsigned char buffer[4] = {0};
    word output_data;
    word input_data;
    int total;
    unsigned int result;
    sem_wait(&lock);
    printf("Start processing...\n");
    sem_post(&lock);
    for (int i = begin_idx; i < end_idx; i++)
    {
        for (int j = 0; j < 640; j++)
        {
            input_data.uint16[0] = B_0[j][i];
            input_data.int8[2] = message[j][i];
            input_data.int8[3] = error_2[j][i];
            sem_wait(&lock);
            if (hart_id == 0)
            {
                write_data_to_ACC(SOBELFILTER_START_ADDR_0, input_data.uc, 4);
            }
            else
            {
                write_data_to_ACC(SOBELFILTER_START_ADDR_1, input_data.uc, 4);
            }
            sem_post(&lock);
        }

        for (int j = 0; j < 640; j++)
        {
            sem_wait(&lock);
            if (hart_id == 0)
            {
                read_data_from_ACC(SOBELFILTER_READ_ADDR_0, output_data.uc, 4);
            }
            else
            {
                read_data_from_ACC(SOBELFILTER_READ_ADDR_1, output_data.uc, 4);
            }
            sem_post(&lock);
            result = output_data.uint16[0];
            result = result - B_1_secret_0[j][i];
            result = (((result >> 12) & 1) + ((result >> 13))) % 4;

            if (result != message[j][i])
            {
                sem_wait(&lock);
                printf("core_id = %d, message[%d][%d] = %d\n", hart_id, j, i, message[j][i]);
                printf("core_id = %d, result [%d][%d] = %d\n", hart_id, j, i, result);
                printf("core_id = %d, pattern[%d][%d] = fail !\n", hart_id, j, i);
                sem_post(&lock);
            }
            else
            {
                sem_wait(&lock);
                printf("core_id = %d, pattern[%d][%d] = pass !\n", hart_id, j, i);
                sem_post(&lock);
            }
        }
    }
    barrier(&barrier_sem, &barrier_lock, &barrier_counter, PROCESSORS);
}
