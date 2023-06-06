#pragma once

/* 16C550 Uart emulation, in a header file.
  
*/

#include <stdint.h>
#include <stdbool.h>
#include <SDL_net.h>

#ifdef __cplusplus
#include <iostream> // TODO: Remove debug
extern "C" {
#endif

#define FIFO_SIZE   16
#define RX_BUFFER_SIZE 512

// Bit numbers for registers
#define LCR_BIT_PARITY (3)
#define MSR_BIT_CTS    (4)
#define MSR_BIT_DSR    (5)

// Bitmasks for registers
#define FIFO_ENABLE       (0x01)
#define LCR_PARITY_EVEN   (0x10)
#define LCR_PARITY_STICKY (0x20)
#define LSR_DR            (0x01)
#define LSR_THRE          (0x20)
#define LSR_TEMT          (0x40)
#define MCR_OUT2          (0x08)

// Bit numbers for pins
#define UART_PIN_TXRDY   (0)
#define UART_PIN_SO      (1)
#define UART_PIN_CTS     (2)
#define UART_PIN_DSR     (3)

// Bitmasks for pins
#define UART_TXRDY  (1ULL<<UART_PIN_TXRDY)
#define UART_SO     (1ULL<<UART_PIN_SO)
#define UART_CTS    (1ULL<<UART_PIN_CTS)

typedef struct {
    uint64_t clock_hz, cycle_ps;
    uint64_t last_tick_ps;
    uint16_t divisor;

    uint16_t pins;  // Pin state

    uint8_t rx_fifo[FIFO_SIZE];
    uint8_t rx_pos;
    uint8_t rx_bytes;
    uint8_t rx_cycles;
    uint8_t rx_bit;
    bool    is_receiving;

    uint8_t  tx_fifo[FIFO_SIZE];
    uint8_t  tx_pos;
    uint8_t  tx_bytes;
    uint8_t  tx_cycles;
    uint16_t tx_shift;
    uint8_t  tx_bit;

    uint8_t interrupt_enable_register;
    uint8_t interrupt_id_register;
    uint8_t fifo_control_register;
    uint8_t line_control_register;
    uint8_t modem_control_register;
    uint8_t line_status_register;
    uint8_t modem_status_register;
    uint8_t scratch_register;

    TCPsocket  client, server;
    SDLNet_SocketSet socketSet;
    int        port;
    uint8_t    rx_buffer[RX_BUFFER_SIZE];
    uint16_t   rx_available;
    uint16_t   rx_offset;
} uart_t;

void uart_init(uart_t* uart, uint64_t clock_hz, uint64_t time_ps);

uint64_t uart_tick(uart_t* uart, uint64_t time_ps);

void uart_write(uart_t* uart, uint8_t addr, uint8_t data, uint64_t time_ps);

uint8_t uart_read(uart_t* uart, uint8_t addr);

void uart_connect(uart_t* uart, bool connect);

bool uart_connected(uart_t* uart);

int  uart_port(uart_t* uart);

#ifdef __cplusplus
} // extern C
#endif

//-- IMPLEMENTATION ------------------------------------------------------------
#ifdef CHIPS_IMPL
#include <string.h> // memset
#ifndef CHIPS_ASSERT
#include <assert.h>
#define CHIPS_ASSERT(c) assert(c)
#endif

#if defined(__GNUC__)
#define _UART_UNREACHABLE __builtin_unreachable()
#elif defined(_MSC_VER)
#define _UART_UNREACHABLE __assume(0)
#else
#define _UART_UNREACHABLE
#endif

void uart_init(uart_t* uart, uint64_t clock_hz, uint64_t time_ps) {
    std::cout << "UART init. Clock rate " << clock_hz << std::endl;

    CHIPS_ASSERT(uart);
    // initial state as described in TI Datasheet TL16C550D
    memset(uart, 0, sizeof(uart_t));

    uart->clock_hz = clock_hz;
    uart->cycle_ps = UINT64_C(1000000000000) / clock_hz;
    uart->divisor = 1;

    uart->interrupt_enable_register = 0;
    uart->fifo_control_register = 0;
    uart->line_control_register = 0;
    uart->line_status_register = LSR_THRE | LSR_TEMT; // Bits 5 and 6 set
    uart->modem_status_register = 0x00; // Bits 4-7 are inputs

    uart->is_receiving = false;

    uart->last_tick_ps = time_ps;

    uart->pins |= UART_SO;  // CTS and DSR are clear
    std::cout << "Divisor "<< uart->divisor << " Baud rate : " << (uart->clock_hz / (uart->divisor) / 16) << std::endl;

    uart->port = 8456;

    IPaddress ip;

    if (SDLNet_ResolveHost(&ip, NULL, uart->port) == -1) {
      std::cout << "SDLNet_ResolveHost error: " << SDLNet_GetError() << std::endl;
      return;
    }

    uart->server = SDLNet_TCP_Open(&ip);
    if (!uart->server) {
      std::cout << "SDLNet_TCP_Open error: " << SDLNet_GetError() << std::endl;
      return;
    }
}

void uart_connect(uart_t* uart, bool connect) {
    if( !connect && uart->client ) {
        SDLNet_TCP_Close(uart->client);
        uart->client = NULL;
        return;
    }

    uart->client = SDLNet_TCP_Accept(uart->server);

    uart->rx_available = 0;

    if( !uart->client ) {
        return;
    }
    else {
        std::cout << "Network client created on port " << uart->port << std::endl;

        uart->socketSet = SDLNet_AllocSocketSet(1);
        if( uart->socketSet ) {
            SDLNet_TCP_AddSocket(uart->socketSet, uart->client);
        }
        else {
            std::cout << "Couldn't create socket set, closing connection" <<std::endl;
            uart->client = NULL;
        }
    }
}

bool uart_connected(uart_t* uart) {
    return uart->client;
}

int  uart_port(uart_t* uart) {
    return uart->port;
}

#define MSR_DELTA_CTS    (0x01)
#define MSR_DELTA_DSR    (0x02)
#define MSR_CTS          (0x10)
#define MSR_DSR          (0x20)

uint64_t uart_tick(uart_t* uart, uint64_t time_ps) {
    if( uart->last_tick_ps + (uart->cycle_ps * uart->divisor) > time_ps ) {
        return uart->pins;
    }

    if( ((uart->pins >> UART_PIN_CTS) & 0x0) == ((uart->modem_status_register >> MSR_BIT_CTS ) & 0x01) ) {
        uart->modem_status_register = (uart->modem_status_register ^ MSR_CTS ) | MSR_DELTA_CTS;
    }

    if( ((uart->pins >> UART_PIN_DSR) & 0x0) == ((uart->modem_status_register >> MSR_BIT_DSR ) & 0x01) ) {
        uart->modem_status_register = (uart->modem_status_register ^ MSR_DSR ) | MSR_DELTA_DSR;
    }

    while( uart->last_tick_ps + (uart->cycle_ps * uart->divisor) <= time_ps ) {
        if( (uart->tx_bytes > 0) | (uart->tx_bit > 0) ) {
            // Handle transmit
            if( uart->tx_cycles == 0 ) {
                if( uart->tx_bit > 0 ) {
                    // Send out shift register...
                    int bits = 5 + (uart->line_control_register & 0x03);
                    int parity = (uart->line_control_register >> LCR_BIT_PARITY) & 0x01;

                    if( uart->tx_bit <= bits ) {
                        uart->pins = (uart->pins & ~UART_SO) | (((uart->tx_shift >> (uart->tx_bit-1)) & 0x01) << UART_PIN_SO);
                        uart->tx_bit += 1;
                    }
                    else if( uart->tx_bit <= bits+parity ) {
                        // Parity / stop bit(s)
                        int count = 0;
                        for(int i=0; i<bits; i++) {
                            count += (uart->tx_shift >> i) & 0x01;
                        }

                        if( uart->line_control_register & LCR_PARITY_STICKY) {
                            if( uart->line_control_register & LCR_PARITY_EVEN ) {
                                uart->pins &= ~UART_SO;
                            }
                            else {
                                uart->pins |= UART_SO;
                            }
                        }
                        else {
                            if( uart->line_control_register & LCR_PARITY_EVEN ) {   
                                uart->pins = (uart->pins & ~UART_SO) | ((count & 0x01) << UART_PIN_SO);
                            }
                            else {
                                uart->pins = (uart->pins & ~UART_SO) | (((~count) & 0x01) << UART_PIN_SO);
                            }
                        }
                        uart->tx_bit += 1;
                    }
                    else {
                        uart->pins |= UART_SO;
                        if( uart->tx_bit >= bits+parity+2+((uart->line_control_register >> 2)&1)) {
                            // Note the calculation above gives an incorrect duration of stop output for 5 bits + 1.5 stop bits.
                            uart->tx_bit = 0;

                            // DEBUG OUTPUT
                            //std::cout << "Sent byte :" << (0+uart->tx_shift) << "(" << (char)uart->tx_shift << ")" << std::endl;
                            std::cout << (char)uart->tx_shift;
                            if( uart->client ) {
                                SDLNet_TCP_Send(uart->client, &uart->tx_shift, 1);
                            }
                        }
                        else {
                            uart->tx_bit += 1;
                        }
                    }
                }
                
                if( uart->tx_bit == 0 ) {
                    // We've written the last bit of the previous byte
                    if( uart->tx_bytes > 0 ) {
                        // load shift register, send start bit..
                        uart->tx_shift = uart->tx_fifo[uart->tx_pos];
                        uart->tx_bytes -= 1;
                        if( uart->tx_bytes == 0 ) {
                            uart->line_status_register |= LSR_THRE;
                            // TODO: Generate interrupt
                        }
                        if( uart->fifo_control_register & FIFO_ENABLE ) {
                            uart->tx_pos = (uart->tx_pos+1) % FIFO_SIZE;
                        }
                        uart->tx_bit = 1;
                        uart->pins &= ~UART_SO;
                    }
                    else {
                        uart->line_status_register |= LSR_TEMT;
                    }
                }
            }
            uart->tx_cycles = (uart->tx_cycles+1) & 0x0F;
        }

        if( uart->is_receiving ) {
            // Handle receieve
            uart->rx_cycles = (uart->rx_cycles+1) & 0x0F;
            if( uart->rx_cycles == 0 ) {
                int bits = 5 + (uart->line_control_register & 0x03);
                int parity = (uart->line_control_register >> LCR_BIT_PARITY) & 0x01;

                uart->rx_bit++;

                if( uart->rx_bit == bits+parity+2+((uart->line_control_register >> 2)&1) ) {
                    uart->rx_bit = 0;

                    if( uart->fifo_control_register & FIFO_ENABLE ) {
                        uart->rx_fifo[uart->rx_bytes++] = uart->rx_buffer[uart->rx_offset++];  // Receive the next byte

                        if( uart->rx_bytes == FIFO_SIZE) {
                            std::cout << "Buffer overrun " << std::endl;
                            uart->rx_bytes--;
                        }
                        // TODO - Interrupt on FIFO size
                    }
                    else {
                        uart->rx_fifo[0] = uart->rx_buffer[uart->rx_offset++];  // Receive the next byte
                        uart->rx_bytes = 1;
                    }

                    uart->line_status_register |= LSR_DR;

                    if(uart->rx_offset == uart->rx_available) {
                        uart->is_receiving = false;
                    }
                } 
            }
        } else {
            if( uart->client ) {
                int check = SDLNet_CheckSockets(uart->socketSet, 0);
                if( check > 0) {
                    int available = SDLNet_TCP_Recv(uart->client, uart->rx_buffer, RX_BUFFER_SIZE);
                    if( available > 0 ) {
                        uart->rx_available = available;
                        uart->is_receiving = true;
                        uart->rx_offset = 0;
                        uart->rx_cycles = 0;
                        /**
                        std::cout << "Read " << available << " bytes:\n"<<std::endl;
                        int index = 0;
                        while( index < available ) {
                            std::cout << std::hex << "Read " << (index) << ": ";
                            for( int i=0; i<16; i++ ) {
                                if(index+i < available) {
                                    std::cout << (0+uart->rx_buffer[index+i]) << " ";
                                }
                            }
                            std::cout << std::dec << std::endl;
                            index+=16;
                        }
                        */
                    }
                }
            }
        }

        uart->last_tick_ps += (uart->cycle_ps * uart->divisor);
    }

    return uart->last_tick_ps + (uart->cycle_ps * uart->divisor);
}

void uart_write(uart_t* uart, uint8_t addr, uint8_t data, uint64_t time_ps) {
    // std::cout << "Uart write " << (0+addr) << " <- " << (0+data) << std::endl;
    switch( addr & 0x07 ) {
        case 0: // transmit holding register OR Divisor latch LSB
            if(uart->line_control_register & 0x80) {
                // DEBUG OUTPUT
                if( (uart->divisor & 0x0FF) != data ) {
                    uart->divisor = (uart->divisor & 0x0FF00) | data;
                    std::cout << "Divisor "<< uart->divisor << " Baud rate : " << (uart->clock_hz / (uart->divisor) / 16) << std::endl;
                }
                
                uart->last_tick_ps = time_ps; // Reset the tick delay..

            }
            else {
                if( uart->tx_bytes == 0 ) {
                    uart->tx_cycles = 0; // Reset the cycle count to send immediately.
                }

                if( uart->fifo_control_register & FIFO_ENABLE ) {
                    uart->tx_fifo[(uart->tx_pos+uart->tx_bytes) % FIFO_SIZE] = data;
                    if( uart->tx_bytes < FIFO_SIZE ) {
                        uart->tx_bytes += 1;
                    }
                    if( uart->tx_bytes == FIFO_SIZE ) {
                        uart->pins |= UART_TXRDY;
                    }
                    else {
                        uart->pins &= ~UART_TXRDY;
                    }
                }
                else {
                    uart->tx_fifo[uart->tx_pos] = data;
                    uart->tx_bytes = 1;
                    uart->pins |= UART_TXRDY;
                }
                uart->line_status_register &= ~(LSR_TEMT | LSR_THRE); // Cleared when THR or shift contain data
            }
            break;
        case 1: // Interrupt enable register OR Divisor latch MSB
            if(uart->line_control_register & 0x80) {
                // DEBUG OUTPUT
                if( (uart->divisor >> 8 & 0xFF) != data ) {
                    uart->divisor = (uart->divisor & 0x000FF) | (data << 8);
                    std::cout << "Divisor "<< uart->divisor << " Baud rate : " << (uart->clock_hz / (uart->divisor) / 16) << std::endl;
                }
                
                uart->last_tick_ps = time_ps; // Reset the tick delay..
            }
            else {
                uart->interrupt_enable_register = data;
            }
            break;
        case 2: // FIFO Control register (write)
            if( (uart->fifo_control_register & FIFO_ENABLE) != (data & FIFO_ENABLE) ) {
                // Reset both FIFOs on enable bit change
                uart->rx_bytes = 0;
                uart->tx_bytes = 0;

                uart->fifo_control_register ^= FIFO_ENABLE;
            }
            if( data & FIFO_ENABLE ) {
                // Bit 0 must be set in order to change any other bits...
                if( data & 0x02 ) {
                    // Reset receiver FIFO
                    uart->rx_bytes = 0;
                }
                if( data & 0x04 ) {
                    // Reset transmitter FIFO
                    uart->tx_bytes = 0;
                }
                uart->fifo_control_register = data & 0x0F9;
                // TODO: Check interrupts since FIFO depth bits may have changed...
            }
            break;
        case 3: // Line control register
            uart->line_control_register = data;
            break;
        case 4: // Modem control register
            uart->modem_control_register = data;
            break;
        case 5: // Line status register
            break;
        case 6: // Modem status register
            break;
        case 7: // scratch register
            uart->scratch_register = data;
            break;
        default:
            _UART_UNREACHABLE;
    }
}


uint8_t uart_read(uart_t* uart, uint8_t addr) {
    //std::cout << "Uart read " << (0+addr) << std::endl;

    switch( addr & 0x07 ) {
        case 0: // receive holding register OR Divisor latch LSB
            if(uart->line_control_register & 0x80) {
                return uart->divisor & 0x0FF;
            }
            else {
                uint8_t result = uart->rx_fifo[0];

                if( uart->fifo_control_register & FIFO_ENABLE ) {
                    for( int i=0; i<FIFO_SIZE-1; i++) {
                        uart->rx_fifo[i] = uart->rx_fifo[i+1];
                    }
                }
                
                if( uart->rx_bytes > 0 ) {
                    uart->rx_bytes--;
                }

                if( uart->rx_bytes == 0 ) {
                    uart->line_status_register &= ~LSR_DR;  // Clear data ready flag
                }

                return result;
            }
            break;
        case 1: // Interrupt enable register OR Divisor latch MSB
            break;
        case 2: // Interrupt identification register (read)
            break;
        case 3: // Line control register
            return uart->line_control_register;
        case 4: // Modem control register
            return uart->modem_control_register;
        case 5: // Line status register
            return uart->line_status_register;
        case 6: // Modem status register
            {
                uint8_t result =  uart->modem_status_register;
                uart->modem_status_register &= 0xF0;  // Bottom four, delta bits, cleared by read
                return result;
            }
        case 7: // scratch register
            return uart->scratch_register;
        default:
            _UART_UNREACHABLE;
    }

    return 0;
}

#endif // CHIPS_IMPL