#pragma once

// Includes
#include "CModule.h"

#define SLAVE_ARDUINO_ADDRESS (0x30)

class CSlaveArduino : public CModule
{
public:
    CSlaveArduino( uint8_t addressIn = SLAVE_ARDUINO_ADDRESS );

    void Initialize();
    void Update( CCommand& commandIn );

private:
    bool ReadByte( uint8_t addressIn, uint8_t& dataOut );
    bool ReadNBytes( uint8_t addressIn, uint8_t* dataOut, uint8_t byteCountIn );
    bool WriteByte( uint8_t addressIn, uint8_t dataIn );

    bool m_isInitialized;

    bool m_lastRetcode;

    uint8_t   m_i2cAddress;
};
