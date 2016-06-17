#include "SysConfig.h"

// Includes
#include "CTestESC.h"
#include "fix16.h"

namespace
{
    CTimer controltime;
    
    float m_targetThrottle = 0.0f;
    
    bool enabled;
}

void CTestESC::Initialize()
{
    controltime.Reset();
    
    // Set setpoint 
}

void CTestESC::Update( CCommand& command )
{
    
    
    if( command.Equals( "thro" ) )
    {
        if( command.m_arguments[1] >= -100 && command.m_arguments[1] <= 100 )
        {
            m_targetThrottle = command.m_arguments[1] / 100.0f;
        }
    }
    
    
}