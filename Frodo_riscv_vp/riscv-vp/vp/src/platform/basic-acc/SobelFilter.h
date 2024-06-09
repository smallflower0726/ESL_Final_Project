#ifndef SOBEL_FILTER_H_
#define SOBEL_FILTER_H_
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdint.h>
#include <systemc>
using namespace sc_core;

#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include "Secret_1.h"
#include "filter_def.h"

union smallflower {
        int sint;
        unsigned int uint;
        unsigned char uc[4];
        int8_t int8[4];
        uint16_t uint16[2];
};

struct SobelFilter : public sc_module
{
        tlm_utils::simple_target_socket<SobelFilter> tsock;

        sc_fifo<uint16_t> i_r; // i_r : B_0
        sc_fifo<int8_t> i_g;   // i_g : message
        sc_fifo<int8_t> i_b;   // i_b : error_2
        sc_fifo<unsigned int> o_result;

        SC_HAS_PROCESS(SobelFilter);

        SobelFilter(sc_module_name n) : sc_module(n), tsock("t_skt"), base_offset(0)
        {
            tsock.register_b_transport(this, &SobelFilter::blocking_transport);
            SC_THREAD(do_filter);
        }

        ~SobelFilter()
        {
        }

        int val[640];
        unsigned int base_offset;

        void do_filter()
        {
            {
                wait(CLOCK_PERIOD, SC_NS);
            }
            while (true)
            {
                for (int j = 0; j < 640; j++)
                {
                    val[j] = 0;
                    wait(CLOCK_PERIOD, SC_NS);
                }

                unsigned int B_matrix[640];
                unsigned int message_matrix[640];
                int error_matrix[640];

                for (int j = 0; j < 640; j++)
                {
                    B_matrix[j] = i_r.read();
                    message_matrix[j] = i_g.read();
                    error_matrix[j] = i_b.read();
                    wait(CLOCK_PERIOD, SC_NS);
                }

                sc_time start_time = sc_time_stamp();

                for (int k = 0; k < 640; k++)
                {
                    for (int j = 0; j < 640; j++)
                    {
                        val[k] += secret_1[k][j] * B_matrix[j];
                    }
                }

                for (int j = 0; j < 640; j++)
                {
                    val[j] = (val[j] + (message_matrix[j] << 13) + error_matrix[j]) % 32768;
                    wait(CLOCK_PERIOD, SC_NS);
                }

                sc_time finish_time = sc_time_stamp();
                sc_time run_time = finish_time - start_time;
                std::cout << "compute time = " << run_time << " ns\n" << std::endl;

                for (int j = 0; j < 640; j++)
                {
                    // cout << (int)result << endl;
                    o_result.write(val[j]);
                }
            }
        }

        void blocking_transport(tlm::tlm_generic_payload &payload, sc_core::sc_time &delay)
        {
            wait(delay);
            // unsigned char *mask_ptr = payload.get_byte_enable_ptr();
            // auto len = payload.get_data_length();
            tlm::tlm_command cmd = payload.get_command();
            sc_dt::uint64 addr = payload.get_address();
            unsigned char *data_ptr = payload.get_data_ptr();

            addr -= base_offset;

            // cout << (int)data_ptr[0] << endl;
            // cout << (int)data_ptr[1] << endl;
            // cout << (int)data_ptr[2] << endl;
            smallflower buffer;
            smallflower to_fifo;

            switch (cmd)
            {
            case tlm::TLM_READ_COMMAND:
                // cout << "READ" << endl;
                switch (addr)
                {
                case SOBEL_FILTER_RESULT_ADDR:
                    buffer.uint = o_result.read();
                    break;
                default:
                    std::cerr << "READ Error! SobelFilter::blocking_transport: address 0x" << std::setfill('0')
                              << std::setw(8) << std::hex << addr << std::dec << " is not valid" << std::endl;
                }
                data_ptr[0] = buffer.uc[0];
                data_ptr[1] = buffer.uc[1];
                data_ptr[2] = buffer.uc[2];
                data_ptr[3] = buffer.uc[3];
                break;
            case tlm::TLM_WRITE_COMMAND:
                // cout << "WRITE" << endl;
                switch (addr)
                {
                case SOBEL_FILTER_R_ADDR:
                    to_fifo.uc[0] = data_ptr[0];
                    to_fifo.uc[1] = data_ptr[1];
                    to_fifo.uc[2] = data_ptr[2];
                    to_fifo.uc[3] = data_ptr[3];
                    i_r.write(to_fifo.uint16[0]); // i_r : B_0
                    i_g.write(to_fifo.int8[2]);   // i_g : message
                    i_b.write(to_fifo.int8[3]);   // i_b : error_2
                    break;
                default:
                    std::cerr << "WRITE Error! SobelFilter::blocking_transport: address 0x" << std::setfill('0')
                              << std::setw(8) << std::hex << addr << std::dec << " is not valid" << std::endl;
                }
                break;
            case tlm::TLM_IGNORE_COMMAND:
                payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
                return;
            default:
                payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
                return;
            }
            payload.set_response_status(tlm::TLM_OK_RESPONSE); // Always OK
        }
};
#endif
