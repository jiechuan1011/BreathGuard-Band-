#ifndef NIMBLE2902_H_
#define NIMBLE2902_H_

#include "NimBLEDescriptor.h"

class NimBLECharacteristic;

class NimBLE2902 : public NimBLEDescriptor {
public:
    NimBLE2902(NimBLECharacteristic* pCharacteristic = nullptr);
};

#endif // NIMBLE2902_H_
