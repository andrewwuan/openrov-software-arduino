#include "SysConfig.h"

// Includes
#include "CTestESC.h"
#include "fix16.h"
#include <math.h>

#define ESC_MESSAGE_SIZE            9

#define START_BYTE                  0x89
#define CMD_MOTOR_CONTROL_WRITE     0x08
#define CMD_PARAMETER_WRITE         0x06

#define PARAM_REVERSE               0x63
#define PARAM_CL_SETPOINT           0x0E
#define PARAM_OL_SETPOINT           0x0C


#define DEADZONE_MAGNITUDE          0.02f
#define TRANSITION_POINT            0.125f

#define OPEN_LOOP_MIN               0.0f
#define OPEN_LOOP_CEILING           21.447f
#define OPEN_LOOP_MAX               171.576f

#define CLOSE_LOOP_MIN              0.0f
#define CLOSE_LOOP_FLOOR            314.159f
#define CLOSE_LOOP_MAX              2513.272f


enum class ETransition
{
    NONE,
    CLOSED_TO_OPEN,
    OPEN_TO_CLOSED,
    OFF_TO_ON,
    ON_TO_OFF
};

enum class ERange
{
    OFF,
    OPEN,
    CLOSED
};

enum class EMotorDirection
{
    NORMAL = 0x00,
    REVERSE = 0x01
}

namespace
{
    CTimer m_controlTimer;

    uint8_t m_rxBuffer[ ESC_MESSAGE_SIZE ]      = {};
    int     m_bufferIndex                       = 0;

    uint8_t m_txBuffer[ ESC_MESSAGE_SIZE ]      = {};
    
    float m_throttle        = 0.0f;

    float m_clTarget        = 0.0f;
    fix16_t m_clTarget_f    = 0;
    float m_olTarget        = 0.0f;
    fix16_t m_olTarget_f    = 0;

    EMotorDirection m_motorDir      = EMotorDirection::NORMAL;
    EMotorDirection m_motorDirPrev  = EMotorDirection::NORMAL;

    ERange m_range                  = ERange::OFF;
    ERange m_rangePrev              = ERange::OFF;

    ETransition m_transition        = ETransition::NONE;

    uint8_t Checksum( uint8_t* bufferIn )
    {
        uint8_t checksum = 0;
        int i;

        for (i = 0; i < 8; i++)
            checksum += *bufferIn++;

        return checksum;
    }

    void SendCommand( uint8_t startByte, uint8_t commandCode, uint8_t parameterCode, uint32_t data )
    {
        m_txBuffer[ 0 ] = startByte;
        m_txBuffer[ 1 ] = commandCode;
        m_txBuffer[ 2 ] = parameterCode;
        m_txBuffer[ 3 ] = 0;
        
        m_txBuffer[ 4 ] = ( ( data >> 24 ) & 0xFF );
        m_txBuffer[ 5 ] = ( ( data >> 16 ) & 0xFF );
        m_txBuffer[ 6 ] = ( ( data >> 8 )  & 0xFF );
        m_txBuffer[ 7 ] = ( data & 0xFF );

        m_txBuffer[ 8 ] = Checksum( (uint8_t*)&m_txBuffer );

        SerialMotor0.Write( (char*)m_txBuffer, ESC_MESSAGE_SIZE );
    }

    ERange DetectRange()
    {
        if( m_throttle < DEADZONE_MAGNITUDE )
        {
            return ERange::OFF;
        }
        else if( m_throttle >= DEADZONE_MAGNITUDE && < TRANSITION_POINT )
        {
            return ERange::OPEN;
        }
        else
        {
            return ERange::CLOSED;
        }
    }

    bool SignChanged( float oldTarget, float newTarget )
    {
        return ( ( oldTarget * newTarget ) < 0.0f );
    }

    ETransition DetectRangeTransition()
    {
        if( m_rangePrev == m_range )
        {
            return ETransition::NONE;
        }
        else if( m_rangePrev == ERange::OFF )
        {
            return ETransition::OFF_TO_ON;
        }
        else if( m_range == ERange::OFF )
        {
            return ETransition::ON_TO_OFF;
        }
        else if( ( m_rangePrev == ERange::OPEN ) && ( m_range == ERange::CLOSED ) )
        {
            return ETransition::OPEN_TO_CLOSED;
        }
        else if( ( m_rangePrev == ERange::CLOSED ) && ( m_range == ERange::OPEN ) )
        {
            return ETransition::CLOSED_TO_OPEN;
        }
    }

    void DisableMotor()
    {
        // Set mode to open
        m_mode = EMode::OPENLOOP;

        // Send disable command to motor
        SendCommand( START_BYTE, CMD_MOTOR_CONTROL_WRITE, 0x00, CreateMotorControlCommand( false, false ) );
    }

    void EnableMotor()
    {
        if( m_mode == EMode::OPENLOOP )
        {
            // Open loop
            SendCommand( START_BYTE, CMD_MOTOR_CONTROL_WRITE, 0x00, CreateMotorControlCommand( false, true ) );
        }
        else
        {
            // Closed loop
            SendCommand( START_BYTE, CMD_MOTOR_CONTROL_WRITE, 0x00, CreateMotorControlCommand( true, true ) );
        }
    }

    void UpdateSetpoints()
    {
        SendCommand( START_BYTE, CMD_PARAMETER_WRITE, PARAM_OL_SETPOINT, m_olTarget_f );
        SendCommand( START_BYTE, CMD_PARAMETER_WRITE, PARAM_CL_SETPOINT, m_clTarget_f );
    }

    void SetMotorDirection()
    {
        if( m_motorDir == EMotorDirection::NORMAL )
        {
            SendCommand( START_BYTE, CMD_PARAMETER_WRITE, PARAM_REVERSE, 0x00 );
        }
        else
        {
            SendCommand( START_BYTE, CMD_PARAMETER_WRITE, PARAM_REVERSE, 0x01 );
        }
    }


    void HandleInput( float target )
    {
        // Get the direction based on the input
        m_motorDir = ( target < 0.0f ) ? EMotorDirection::REVERSE : EMotorDirection::NORMAL;

        // If it differs from the last update, trigger a direction change
        bool reversed       = ( m_motorDir != m_motorDirPrev );
        m_motorDirPrev      = m_motorDir;

        // Update throttle
        m_throttle          = fabs( target );

        // Get current range and transition, if one occurred
        m_range             = DetectRange();
        m_transition        = DetectRangeTransition();
        m_rangePrev         = m_range;

        // Calculate new setpoints
        if( m_range == ERange::OFF )
        {
            // Snap targets to 0.0f;
            m_clTarget = 0.0f;
            m_olTarget = 0.0f;
        }
        else
        {
            // Scale and floor/ceiling the targets appropriately
            m_clTarget = util::mapf( fabs( m_throttle ), 0.0f, 1.0f, CLOSE_LOOP_MIN, CLOSE_LOOP_MAX );
            m_clTarget = ( m_clTarget < CLOSE_LOOP_FLOOR ) ? CLOSE_LOOP_FLOOR : m_clTarget;

            m_olTarget = util::mapf( fabs( m_throttle ), 0.0f, 1.0f, OPEN_LOOP_MIN, OPEN_LOOP_MAX );
            m_olTarget = ( m_olTarget > OPEN_LOOP_CEILING ) ? OPEN_LOOP_CEILING : m_olTarget;
        }

        // Convert to fixed16_t
        m_olTarget_f = fix16_from_float( m_olTarget );
        m_clTarget_f = fix16_from_float( m_clTarget );

        // Handle transitions
        switch( m_transition )
        {
            case ETransition::NONE:
            {
                if( m_range != ERange::OFF )
                {
                    if( reversed )
                    {
                        // Stayed in same mode, switched direction
                        DisableMotor();
                        UpdateSetpoints();
                        SetMotorDirection();
                        EnableMotor();
                    }
                    else
                    {
                        // Stayed in the same mode and direction, just update setpoint
                        UpdateSetpoints();
                    }
                }
                else
                {
                    // Do nothing, motor is off
                }

                break;
            }

            case ETransition::OFF_TO_ON:
            {
                if( reversed )
                {
                    UpdateSetpoints();
                    SetMotorDirection();
                    EnableMotor();
                }
                else
                {
                    UpdateSetpoints();
                    EnableMotor();
                }
                
                break;
            }

            case ETransition::ON_TO_OFF:
            {
                DisableMotor();
                break;
            }

            case ETransition::OPEN_TO_CLOSED:
            {
                if( reversed )
                {
                    DisableMotor();
                    UpdateSetpoints();
                    SetMotorDirection();
                    EnableMotor();
                }
                else
                {
                    UpdateSetpoints();
                    EnableMotor();
                }

                break;
            }

            case ETransition::CLOSED_TO_OPEN:
            {
                if( reversed )
                {
                    DisableMotor();
                    UpdateSetpoints();
                    SetMotorDirection();
                    EnableMotor();
                }
                else
                {
                    DisableMotor();
                    UpdateSetpoints();
                    EnableMotor();
                }

                break;
            }

            default:
            {
                DisableMotor();
                break;
            }
        }
    }

    void ProcessMessage()
    {
        fix16_t rxData;

        // Put data from uart_rx_buffer into word for processing (in case read)
        rxData = ( m_rxBuffer[4] << 24 );
        rxData |= ( m_rxBuffer[5] << 16 );
        rxData |= ( m_rxBuffer[6] << 8 );
        rxData |= m_rxBuffer[7];

        // Validate checksum
        if( uart_checksum( (uint8_t*)&m_rxBuffer) != m_rxBuffer[8] )
        {
            // Checksum error
        }
        else
        {
            uint8_t commandCode = m_rxBuffer[1] - 0x80;

            SerialDebug.print( "Message from ESC: " );
            SerialDebug.println( commandCode );

            // Switch on command code
            switch( commandCode )				
            {
                case 0:
                    break;
                default:
                    break;
            }
        }
    }

    void HandleComms()
    {
         // Read data from the ESC
        while( SerialMotor0.available() )
        {
            char data = SerialMotor0.read();

            if( m_bufferIndex == 0 )                                    /* For the first byte received, the start byte must be 0xEE */
            {
                if( data != 0xEE )
                {
                    // Just skip bytes until we read the starting byte
                    continue;
                }
            }

            // Store byte and move to next index
            m_rxBuffer[ m_bufferIndex++ ] = data;

            // Check for complete message
            if( m_bufferIndex == UART_MESSAGE_SIZE )
            {
                ProcessMessage();                                
                m_bufferIndex = 0;                                      
            }
        }
    }
}

void CTestESC::Initialize()
{
    controltime.Reset();
}

void CTestESC::Update( CCommand& command )
{
    HandleComms();    

    // Update throttle
    if( NCommManager::m_isCommandAvailable )
    {
        if( command.Equals( "thro" ) )
        {
            if( command.m_arguments[1] >= -100 && command.m_arguments[1] <= 100 )
            {
                // For now, update as fast as we receive commands
                HandleInput( (float)command.m_arguments[1] / 100.0f );
            }
        }
    }    
}