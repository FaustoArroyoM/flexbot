#include "serialib.h"

#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <stdint.h>
#include <fstream>
using namespace std;

#define SERIAL_PORT "COM3"

int main() {
    int userlen;
    
    std::cout << "Give simulation length as an integer: ";
    std::cin >> userlen;

    const int len = userlen;
    
    float nums[len][7] = {0};
    float outs[len][2] = {0};
    int out1d = 0;
    int out2d = 0;
    float times[len] = {0};
    unsigned char buffer_write[3] = {0};
    unsigned char buffer_read[16] = {0};
    int identifier_read = 0;
    const int flag_control = 99;
    const int flag_startstop = 120;
    const int flag_send = 109;
    float randwalk1 = 0;
    float randwalk2 = 0;

    const float K[2][6] = {-2, 0, 0.1, 0, 0, 0,
                            0, -3, 0, 0.05, 0, 0
                        };
    // const float K[2][6] = {0, 0, 0.1, 0, 0, 0,
    //                         0, 0, 0, 0.1, 0, 0
    //                     };
    // const float K[2][6] = {-0.0356,	-1.1374, 0.018,	0.1872, 0 , 0,
    //                        -0.0362,	-1.2301, 0.0177, 0.1939, 0, 0
    //                     };
                        

    // Serial object
    serialib serial;

    // Connection to serial port
    char errorOpening = serial.openDevice(SERIAL_PORT, 115200);


    // If connection fails, return the error code otherwise, display a success message
    if (errorOpening!=1){
        std::cout << "No connection to " << SERIAL_PORT << " possible.." << std::endl;
        sleep(1);
        return errorOpening;
    } 
    std::cout << "Successful connection to " << SERIAL_PORT << std::endl;

    sleep(1);
    serial.flushReceiver();
    sleep(1);

    // Write the string on the serial device
    buffer_write[0] = flag_startstop;
    buffer_write[1] = 0;
    buffer_write[2] = 0;
    serial.writeBytes(buffer_write, 3);

    for(int i = 0; i < len; i++){
        // Read the string:
        serial.readBytes(buffer_read, 16, 2000);

        identifier_read = int16_t(buffer_read[0] | buffer_read[1] << 8);
        if(identifier_read == flag_send){
            // store data:
            nums[i][0] = float(int16_t(buffer_read[2] | buffer_read[3] << 8 )/100.0);   // pos1
            nums[i][1] = float(int16_t(buffer_read[4] | buffer_read[5] << 8)/100.0);    // pos2
            nums[i][2] = float(int16_t(buffer_read[6] | buffer_read[7] << 8)/1000.0);   // strain1
            nums[i][3] = float(int16_t(buffer_read[8] | buffer_read[9] << 8)/1000.0);   // strain2
            nums[i][4] = float(int16_t(buffer_read[10] | buffer_read[11] << 8)/100.0);  // strain1div
            nums[i][5] = float(int16_t(buffer_read[12] | buffer_read[13] << 8)/100.0);  // strain2div
            nums[i][6] = float(int16_t(buffer_read[14] | buffer_read[15] << 8)/100.0);   // cycle time in ms
            // std::cout << "Cycle in ms: " << nums[i][6] << std::endl;
            
            // compute controls:
            for (int j=0;j<2;j++){
                for (int k=0;k<6;k++){
                    outs[i][j] += (K[j][k]*nums[i][k]);
                }
            }
            
            // random walk for PE:
            // randwalk1 += 0.08*(float(rand())/RAND_MAX - 0.5);
            // randwalk2 += 0.04*(float(rand())/RAND_MAX - 0.5);
            // outs[i][0] += randwalk1;
            // outs[i][1] += randwalk2;

            // discretize control to uint8:
            out1d = int(256/3.3*outs[i][0] + 127);
            out2d = int(256/3.3*outs[i][1] + 127 - 2);
            if(out1d > 255){out1d = 255;}
            if(out1d < 0){out1d = 0;}
            if(out2d > 255){out2d = 255;}
            if(out2d < 0){out2d = 0;}

            buffer_write[0] = flag_control;
            buffer_write[1] = out1d;
            buffer_write[2] = out2d;
            serial.writeBytes(buffer_write, 3);
        } else{
            std::cout << "Error!" << std::endl;
            break;
        }
    }


    buffer_write[0] = flag_startstop;
    buffer_write[1] = 1;
    buffer_write[2] = 1;
    serial.writeBytes(buffer_write, 3);
    // Close the serial device
    sleep(1);
    serial.closeDevice();
    std::cout << "Done!" << std::endl;

    // for(int i = 0; i < len; i++){
    //     std::cout << "Cycle in ms: " << nums[i][6] << std::endl;
    // }

    // Store data in text file:
    ofstream file("data.txt");
    if(file.is_open()){
        for(int i = 0; i < len; i++){
            for(int j = 0; j <= 6; j++){
                file << double(nums[i][j]) << " ";
            }
            for(int j = 0; j <= 1; j++){
                file << double(outs[i][j]) << " ";
            }
            file << "\n";
        }
        file.close();
    } else cout << "Unable to open file";
    return 0 ;
}