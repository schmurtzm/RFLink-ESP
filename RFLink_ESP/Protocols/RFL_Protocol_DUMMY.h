

#ifndef RFL_Protocol_DUMMY_h
#define RFL_Protocol_DUMMY_h

//20;05;DEBUG_Start;Pulses=32;Pulses(uSec)=1000,10000,500,1000,500,1000,500,1000,500,1000,500,1000,500,1000,1000,500,1000,500,1000,500,1000,500,500,1000,1000,500,1000,500,1000,500,1000,500,500,1000,1000,7008;

//00000011110111101



// ***********************************************************************************
// ***********************************************************************************
class _RFL_Protocol_DUMMY : public _RFL_Protocol_BaseClass {
	
  public:

    // ***********************************************************************
    // Creator, 
    // ***********************************************************************
    _RFL_Protocol_DUMMY () {
      Name = "DUMMY" ;
      NAME = Name ;
      //NAME.toUpperCase () ;
   }
 

    bool RF_Decode (  ) {
      #define PulseCount 32 

      // ****************************************************
      // Check the length of the sequence
      // ****************************************************
      if ( RawSignal.Number != PulseCount + 3 ) {
//        Count = 0 ;
        return false;
      }



      // ****************************************************
      //  Translate the sequence in bit-values and 
      //     jump out this method if errors are detected
      // 0-bit = a short high followed by a long low
      // 1-bit = a long  high followed by a short low
      // ****************************************************
      unsigned long BitStream = 0L ;
      unsigned long P1 ;
      unsigned long P2 ;  
      for ( byte x=2; x < PulseCount + 1 ; x=x+2 ) {
        P1 = RawSignal.Pulses [ x   ] ;
        P2 = RawSignal.Pulses [ x+1 ] ;
        if ( P1 > RawSignal.Mean ) {
          BitStream = ( BitStream << 1 ) | 0x1;  // append "1"
        } else { 
          BitStream =   BitStream << 1;          // append "0"
        }
      }

      //==================================================================================
      // Prevent repeating signals from showing up
      //==================================================================================
//      if ( BitStream == Last_BitStream  ) {
//             Count += 1 ;
//      } else Count  = 0 ;
      
        if ( ( BitStream != Last_BitStream ) ||
             ( millis() > 700 + Last_Detection_Time ) ) {
         // not seen the RF packet recently
         Last_BitStream = BitStream;
      } else {
         // already seen the RF packet recently
//         if ( ! Last_Floating ) {
//           if ( millis() > 700 + Last_Detection_Time ) {
//             Count = 0 ;                                   // <<< lijkt niet te werken
//           } 
           return true ;
//         }
      } 


      String Device = Name ;
 

      unsigned long Id     = BitStream ;
      unsigned long Switch = 1 ;

      Switch        = 1 ;
      String On_Off = "ON" ;

      //sprintf ( pbuffer, "%s;ID=%05X;", Device.c_str(), Id ) ; 
      //sprintf ( pbuffer2, "SWITCH=%02X;CMD=ON;", Switch ) ; 

      return Send_Message ( Device, Id, Switch, On_Off ) ;
      
    }

    // ***********************************************************************************
    // ***********************************************************************************
    bool Home_Command ( String Device, unsigned long ID, int Switch, String On ) {
     
      return true ;
    }
    
    
};
#endif
