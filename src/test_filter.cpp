/****************************************************************************
 * M17Netd                                                                  *
 * Copyright (C) 2024 by Morgan Diepart ON4MOD                              *
 *                       SDR-Engineering SRL                                *
 *                                                                          *
 * This program is free software: you can redistribute it and/or modify     *
 * it under the terms of the GNU Affero General Public License as published *
 * by the Free Software Foundation, either version 3 of the License, or     *
 * (at your option) any later version.                                      *
 *                                                                          *
 * This program is distributed in the hope that it will be useful,          *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 * GNU Affero General Public License for more details.                      *
 *                                                                          *
 * You should have received a copy of the GNU Affero General Public License *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.    *
 ****************************************************************************/

#include <complex>
#include <iostream>
#include <csignal>
#include <fstream>
#include <cstring>
#include <liquid/liquid.h>


using namespace std;

volatile bool running = true;

float taps[] = {
2.847196265065577e-05,
0.00011368729610694572f,0.00023137565585784614f,0.0003731952456291765f,0.0005227993242442608,
0.0006563419010490179f,0.0007444592192769051f,0.0007557355565950274f,0.0006614116718992591,
0.0004408441891428083,8.699936006451026e-05f,-0.00038888040580786765f,-0.0009543407359160483,
-0.0015553210396319628f,-0.002118882490321994f,-0.002559284446761012f,-0.0027871839702129364,
-0.002721251454204321f,-0.002301019849255681f,-0.0014994251541793346f,-0.00033327125129289925,
0.0011301477206870914f,0.0027718008495867252f,0.004426117986440659f,0.005893021821975708,
0.006956371944397688f,0.007407538592815399f,0.0070720030926167965f,0.005836252123117447,
0.003671832149848342f,0.0006533983978442848f,-0.003032081527635455f,-0.007087105419486761,
-0.011117507703602314f,-0.014658285304903984f,-0.017209898680448532f,-0.01828226074576378,
-0.017442580312490463f,-0.014362438581883907f,-0.008859267458319664f,-0.0009275897173210979,
0.009243843145668507f,0.021271638572216034f,0.03459515795111656f,0.048511020839214325,
0.06222226470708847f,0.0748981237411499f,0.08573918789625168f,0.09404204785823822,
0.09925734251737595f,0.1010357216000557f,0.09925734251737595f,0.09404204785823822,
0.08573918789625168f,0.0748981237411499f,0.06222226470708847f,0.048511020839214325,
0.03459515795111656f,0.021271638572216034f,0.009243843145668507f,-0.0009275897173210979,
-0.008859267458319664f,-0.014362438581883907f,-0.017442580312490463f,-0.01828226074576378,
-0.017209898680448532f,-0.014658285304903984f,-0.011117507703602314f,-0.007087105419486761,
-0.003032081527635455f,0.0006533983978442848f,0.003671832149848342f,0.005836252123117447,
0.0070720030926167965f,0.007407538592815399f,0.006956371944397688f,0.005893021821975708,
0.004426117986440659f,0.0027718008495867252f,0.0011301477206870914f,-0.00033327125129289925,
-0.0014994251541793346f,-0.002301019849255681f,-0.002721251454204321f,-0.0027871839702129364,
-0.002559284446761012f,-0.002118882490321994f,-0.0015553210396319628f,-0.0009543407359160483,
-0.00038888040580786765,8.699936006451026e-05f,0.0004408441891428083f,0.0006614116718992591,
0.0007557355565950274f,0.0007444592192769051f,0.0006563419010490179f,0.0005227993242442608,
0.0003731952456291765f,0.00023137565585784614f,0.00011368729610694572,2.847196265065577e-05,
};

void sigint_catcher(int signum)
{
    if(signum == SIGINT)
    {
        std::cout << "Ctrl-C caught, stopping all threads." << std::endl;
        running = false;
    }
}

int main(int argc, char *argv[])
{

    struct sigaction sigint_handler;
    memset(&sigint_handler, 0, sizeof(struct sigaction));
    sigint_handler.sa_handler = sigint_catcher;
    sigemptyset(&sigint_handler.sa_mask);
    sigint_handler.sa_flags = 0;
    sigaction(SIGINT, &sigint_handler, 0);

    // Parse first argument (number of samples to acquire)
    if(argc == 2 && strcmp(argv[1], "help") == 0)
    {
        cout << "Usage: " << argv[0] << " input_file output_file\n"
             << "\tinput_file          is the file from which to read the input I/Q data (as float pairs, in binary).\n"
             << "\toutput_file         is the file to which to write the filtered I/Q data (as float pairs, in binary)."
             << endl;
        return EXIT_SUCCESS;
    }else if(argc != 3)
    {
        cerr << "Incorrect usage, type \"" << argv[0] << " help\" to learn more." << endl;
        return EXIT_FAILURE;
    }

    ifstream iq_in_file;
    ofstream iq_out_file;

    // Open the input file
    try
    {
        iq_in_file.open(argv[1], ios::binary);
    }
    catch(const std::exception &e)
    {
        cerr << "Unable to open input file \"" << argv[1] << "\"." << endl;
        return EXIT_FAILURE;
    }

    // Open the output file
    try
    {
        iq_out_file.open(argv[2], ios::binary | ios::trunc);
    }
    catch(const std::exception &e)
    {
        cerr << "Unable to open output file \"" << argv[2] << "\"." << endl;
        return EXIT_FAILURE;
    }


    size_t cnt = 0;
    constexpr size_t chunk = 960;
    complex<float> input[chunk] = {0};
    complex<float> buffer[chunk] = {0};
    complex<float> output[chunk] = {0};

    // Create DC-Block and low-pass filters
    iirfilt_crcf dcr = iirfilt_crcf_create_dc_blocker((5.0/96000.0));
    firfilt_crcf lpf = firfilt_crcf_create(taps, sizeof(taps)/sizeof(float));


    while(!iq_in_file.eof())
    {

        iq_in_file.read(reinterpret_cast<char *>(input), chunk*sizeof(complex<float>));

        size_t n = iq_in_file.gcount()/sizeof(complex<float>);

        iirfilt_crcf_execute_block(dcr, input, n, buffer);
        firfilt_crcf_execute_block(lpf, buffer, n, output);

        iq_out_file.write(reinterpret_cast<char *>(output), n*sizeof(complex<float>));

        cnt += n;
    }

    iq_out_file.close();
    iq_in_file.close();

    cout << "Processed " << cnt << " samples." << endl;

    iirfilt_crcf_destroy(dcr);
    firfilt_crcf_destroy(lpf);
    return 0;
}
