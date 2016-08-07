#include "Arduino.h"

#include "CI2C.h"
#include "CSlaveArduino.h"

CSlaveArduino::CSlaveArduino( uint8_t addressIn )
{
    m_isInitialized = false;
    m_lastRetcode = true;
    m_i2cAddress = addressIn;
}

void CSlaveArduino::Initialize()
{
    m_isInitialized = false;

    delay( 500 );

    m_isInitialized = true;

    while (true) {
        WriteByte(0, ( uint8_t )1);
        delay(1000);
        WriteByte(1, ( uint8_t )0);
        delay(1000);
    }

    delay( 100 );

    m_isInitialized = true;
}

void Update( CCommand& commandIn ) {

}

/***************************************************************************
 PRIVATE FUNCTIONS
 ***************************************************************************/

bool CSlaveArduino::WriteByte( uint8_t addressIn, uint8_t dataIn )
{
    uint8_t ret = I2c.write( m_i2cAddress, ( uint8_t )addressIn, dataIn );

	// Non-zero is failure
    if( ret )
    {
        return false;
    }

    return true;
}

bool CSlaveArduino::ReadByte( uint8_t addressIn, uint8_t& dataOut )
{

    // Set address to request from
    uint8_t ret = I2c.read( m_i2cAddress, ( uint8_t )addressIn, ( uint8_t )1 );

    // Non-zero failure
    if( ret )
    {
        return false;
    }

    // Request single byte from slave
    dataOut = I2c.receive();

    return true;
}

bool CSlaveArduino::ReadNBytes( uint8_t addressIn, uint8_t* dataOut, uint8_t byteCountIn )
{
    // Set address to request from
    uint8_t ret = I2c.read( m_i2cAddress, ( uint8_t )addressIn, byteCountIn, dataOut );

    // Non-zero failure
    if( ret )
    {
        return false;
    }

    return true;
}
