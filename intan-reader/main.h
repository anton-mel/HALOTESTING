#ifndef MAIN_H
#define MAIN_H

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <unistd.h>
#include "Engine/API/Hardware/rhxcontroller.h"
#include "Engine/API/Hardware/rhxdatablock.h"
#include "Engine/API/Hardware/rhxregisters.h"

// Print data block contents to console
void printDataBlock(const RHXDataBlock* dataBlock, ControllerType controllerType, int stream);

// Main function for Intan RHX Device Reader
int main(int argc, char* argv[]);

#endif // MAIN_H
