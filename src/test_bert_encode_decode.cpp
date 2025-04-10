#include <iostream>
#include <array>
#include <m17.h>
#include <cstring>

using namespace std;

int main(int argc, char *argv[])
{
    unsigned int from = 1;
    unsigned int to = 511;

    if(argc == 2)
    {
        if(strcmp(argv[1], "help") == 0)
        {
            cout << "Usage: " << argv[0] << " init_state\n"
                 << "\tinit_state       (optional) is the initial state for the BERT LFSR. If no initial state is provided, the code will check every initial state from 1 to 511.\n"
                 << endl;
            return EXIT_SUCCESS;
        }
        try
        {
            unsigned int tmp = stoul(argv[1]);
            if(tmp < 512)
            {
                from = to = tmp;
            }
            else
            {
                cerr << "Initial state must be between 0 and 512." << endl;
                return EXIT_FAILURE;
            }
        }
        catch(const std::exception& e)
        {
            cerr << "Incorrect initial state \"" << argv[1] << "\"." << endl;
            return EXIT_FAILURE;
        }
    }
 
    array<uint8_t, 197>             unpacked_bits           = {0};
    array<uint8_t, 25>              packed_bits             = {0};
    array<float, SYM_PER_FRA>       frame                   = {0};
    array<uint16_t, 2*SYM_PER_PLD>  soft_bits               = {0};
    array<uint16_t, 2*SYM_PER_PLD>  deinterleaved_soft_bits = {0};
    array<uint8_t, 26>              packed_decoded_bits     = {0};
    array<uint8_t, 197>             decoded_bits            = {0};
    
    for(uint16_t lfsr_init = from; lfsr_init <= to; lfsr_init++)
    {
        uint16_t lfsr = lfsr_init;
        cout << "lfsr = " << lfsr << endl;

        for(size_t i = 0; i < 197; i++)
        {
            uint16_t bit = ( (lfsr >> 4) ^ (lfsr >> 8) ) & 0x1;
            lfsr = (lfsr << 1) | bit;
            unpacked_bits[i] = bit;
        }

        cout << "BERT bits: ";
        for(auto b : unpacked_bits)
        {
            cout << static_cast<unsigned int>(b);
        }
        cout << endl;

        // Packing bits
        packed_bits.fill(0);
        for(size_t i = 0; i < 197; i++)
        {
            size_t byte_idx = i/8;
            size_t bit_idx = 7-(i%8);

            packed_bits[byte_idx] |= (unpacked_bits[i] << bit_idx);
        }

        
        send_frame(frame.data(), packed_bits.data(), FRAME_BERT, nullptr, 0, 0);

        // Print frame symbols
        /*cout << "Generated symbols:";
        for(auto s : frame)
        {
            cout << showpos << s << ", ";
        }
        cout << endl;*/
        

        // Convert symbols to soft bits
        size_t soft_idx = 0;
        for(auto s = frame.begin()+8; s < frame.end(); s++)
        {

            if(*s == 3.0)
            {
                soft_bits[soft_idx++] = 0x0000;
                soft_bits[soft_idx++] = 0xFFFF;
            }
            else if (*s == 1.0)
            {
                soft_bits[soft_idx++] = 0x0000;
                soft_bits[soft_idx++] = 0x0000;
            }
            else if(*s == -1.0)
            {
                soft_bits[soft_idx++] = 0xFFFF;
                soft_bits[soft_idx++] = 0x0000;
            }
            else if(*s == -3.0)
            {
                soft_bits[soft_idx++] = 0xFFFF;
                soft_bits[soft_idx++] = 0xFFFF;
            }
            else
            {
                cerr << "unknown symbol " << *s << endl;;
            }
        }

        /*cout << "Soft bits: ";
        for(auto sb : soft_bits)
        {
            cout << sb << ", ";
        }
        cout << endl;
        */

        // De-randomize soft-bits
        randomize_soft_bits(soft_bits.data());

        // de-interleave soft-bits
        reorder_soft_bits(deinterleaved_soft_bits.data(), soft_bits.data());
        
        // Use viterbi algorithm to decode bits
        viterbi_decode_punctured(packed_decoded_bits.data(), deinterleaved_soft_bits.data(), puncture_pattern_2, deinterleaved_soft_bits.size(), sizeof(puncture_pattern_2));

        cout << "Decoded BERT data:";
        for(size_t i = 7; i < 197+7; i++)
        {
            size_t byte_idx = i/8;
            size_t bit_idx = 7-(i%8);

            unsigned bit = (packed_decoded_bits[byte_idx] >> bit_idx) & 0x1;
            decoded_bits[i-7] = bit;
            cout << bit;
        }
        cout << endl;

        bool correct = true;

        size_t cmp_idx = 0;
        for(cmp_idx = 0; cmp_idx < 197; cmp_idx++)
        {
            if(unpacked_bits[cmp_idx] != decoded_bits[cmp_idx])
            {
                correct = false;
                break;
            }
        }
        
        if(!correct)
        {
            cout << "Encoded and decoded payloads differ at bit index " << cmp_idx << "." << endl;
            return EXIT_FAILURE;
        }
        
        cout << "Encoded and decoded payloads are identical !" << endl;
    }

    return EXIT_SUCCESS;
}
