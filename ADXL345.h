////////////////////////////////////////////////////////////////////////////////
// Filename:    ADXL345.h
//
// Description: ADXL345 Accelerometer header file for type definitions 
//
// Author:      Daniel Emond, IFT and GLO Department, Laval University
////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef _ADXL345_H_
#define _ADXL345_H_

typedef unsigned int T_UNUSED;
typedef unsigned int T_DATA;

#define ADXL345_DEVICE_ID 0xE5

#define ACCEL_READ_BUFF_SIZE       2
#define ACCEL_WRITE_BUFF_SIZE      2
#define ACCEL_MB_READ_BUFF_SIZE    7
#define ACCEL_FIFO_BUFF_SIZE       32

struct T_AXIS_DATA
{
   double fAxisX;
   double fAxisY;
   double fAxisZ;
};

// Defines the address of each registers inside the Accelerometer
enum T_REGISTER_MAP
{
   E_REG_DEVICE_ID,       //READ ONLY
   E_REG_01,              //RESERVED
   E_REG_02,              //RESERVED
   E_REG_03,              //RESERVED
   E_REG_04,              //RESERVED
   E_REG_05,              //RESERVED
   E_REG_06,              //RESERVED
   E_REG_07,              //RESERVED
   E_REG_08,              //RESERVED
   E_REG_09,              //RESERVED
   E_REG_10,              //RESERVED
   E_REG_11,              //RESERVED
   E_REG_12,              //RESERVED
   E_REG_13,              //RESERVED
   E_REG_14,              //RESERVED
   E_REG_15,              //RESERVED
   E_REG_16,              //RESERVED
   E_REG_17,              //RESERVED
   E_REG_18,              //RESERVED
   E_REG_19,              //RESERVED
   E_REG_20,              //RESERVED
   E_REG_21,              //RESERVED
   E_REG_22,              //RESERVED
   E_REG_23,              //RESERVED
   E_REG_24,              //RESERVED
   E_REG_25,              //RESERVED
   E_REG_26,              //RESERVED
   E_REG_27,              //RESERVED
   E_REG_28,              //RESERVED
   E_REG_THRESH_TAP,      //READ AND WRITE
   E_REG_OFSX,            //READ AND WRITE
   E_REG_OFSY,            //READ AND WRITE
   E_REG_OFSZ,            //READ AND WRITE
   E_REG_TAP_DURATION,    //READ AND WRITE
   E_REG_TAP_LATENCY,     //READ AND WRITE
   E_REG_TAP_WINDOW,      //READ AND WRITE
   E_REG_THRESH_ACT,      //READ AND WRITE
   E_REG_THRESH_INACT,    //READ AND WRITE
   E_REG_TIME_INACT,      //READ AND WRITE
   E_REG_ACT_INACT_CTL,   //READ AND WRITE
   E_REG_THRESH_FF,       //READ AND WRITE
   E_REG_TIME_FF,         //READ AND WRITE
   E_REG_TAP_AXES,        //READ AND WRITE
   E_REG_ACT_TAP_STATUS,  //READ ONLY
   E_REG_BW_RATE,         //READ AND WRITE
   E_REG_POWER_CTL,       //READ AND WRITE
   E_REG_INT_ENABLE,      //READ AND WRITE
   E_REG_INT_MAP,         //READ AND WRITE
   E_REG_INT_SOURCE,      //READ ONLY
   E_REG_DATA_FORMAT,     //READ AND WRITE
   E_REG_DATA_X0,         //READ ONLY
   E_REG_DATA_X1,         //READ ONLY
   E_REG_DATA_Y0,         //READ ONLY
   E_REG_DATA_Y1,         //READ ONLY
   E_REG_DATA_Z0,         //READ ONLY
   E_REG_DATA_Z1,         //READ ONLY
   E_REG_FIFO_CTL,        //READ AND WRITE
   E_REG_FIFO_STATUS      //READ ONLY
};

// Accelerometer Output Data rate possible values
enum T_OUT_DATA_RATE
{
   E_RATE_6_25_HZ = 0x06,
   E_RATE_12_5_HZ,
   E_RATE_25_HZ,
   E_RATE_50_HZ,
   E_RATE_100_HZ,
   E_RATE_200_HZ,
   E_RATE_400_HZ,
   E_RATE_800_HZ,
   E_RATE_1600_HZ,
   E_RATE_3200_HZ
};

// Accelerometer wakeup values
enum T_WAKEUP_FREQ
{
   E_FREQ_8_HZ,
   E_FREQ_4_HZ,
   E_FREQ_2_HZ,
   E_FREQ_1_HZ
};

// Accelerometer data range setting values
enum T_RANGE_G_SET
{
   E_RANGE_PLUS_MINUS_2G,
   E_RANGE_PLUS_MINUS_4G,
   E_RANGE_PLUS_MINUS_8G,
   E_RANGE_PLUS_MINUS_16G
};

enum
{
   E_2G_VAL  = 2000,
   E_4G_VAL  = 4000,
   E_8G_VAL  = 8000,
   E_16G_VAL = 16000
};

enum
{
   E_2G_LO_RES_DATA_MASK  = 0x01FF,
   E_4G_LO_RES_DATA_MASK  = 0x01FF,
   E_8G_LO_RES_DATA_MASK  = 0x01FF,
   E_16G_LO_RES_DATA_MASK = 0x01FF,
   E_2G_HI_RES_DATA_MASK  = 0x01FF,
   E_4G_HI_RES_DATA_MASK  = 0x03FF,
   E_8G_HI_RES_DATA_MASK  = 0x07FF,
   E_16G_HI_RES_DATA_MASK = 0x0FFF
};


// Accelerometer FIFO buffer mode values
enum T_FIFO_MODE
{
   E_MODE_BYPASS,
   E_MODE_FIFO,
   E_MODE_STREAM,
   E_MODE_TRIGGER
};

#define ACCEL_DATA_SIZE_BY_AXIS 2

#pragma pack(1)

// Accelerometer Multiple bytes entries data structure
struct T_ACCEL_FIFO_ENTRY
{
   char acX[ACCEL_DATA_SIZE_BY_AXIS];
   char acY[ACCEL_DATA_SIZE_BY_AXIS];
   char acZ[ACCEL_DATA_SIZE_BY_AXIS];
};

union T_AXIS_CONV
{
   T_ACCEL_FIFO_ENTRY sAxisInChar;
   struct
   {
      unsigned short uiX;
      unsigned short uiY;
      unsigned short uiZ;
   } sAxisInUnsignedShort;
};

// Accelerometer ACT_INACT Control register data structure
struct T_ACT_INACT_CTL
{
   BOOL  bINACT_Z_Enable         : 1;
   BOOL  bINACT_Y_Enable         : 1;
   BOOL  bINACT_X_Enable         : 1;
   BOOL  bINACT_AC_Coupled       : 1;
   BOOL  bACT_Z_Enable           : 1;
   BOOL  bACT_Y_Enable           : 1;
   BOOL  bACT_X_Enable           : 1;
   BOOL  bACT_AC_Coupled         : 1;
};

// Accelerometer TAP_AXES register data structure
struct T_TAP_AXES
{
   BOOL     bTAP_Z_Enable        : 1;
   BOOL     bTAP_Y_Enable        : 1;
   BOOL     bTAP_X_Enable        : 1;
   BOOL     bSuppress            : 1;
   T_UNUSED uiUnused             : 4;
};

// Accelerometer TAP_AXES Status register data structure
struct T_ACT_TAP_STATUS
{
   BOOL     bTAP_Z_Source        : 1;
   BOOL     bTAP_Y_Source        : 1;
   BOOL     bTAP_X_Source        : 1;
   BOOL     bAsleep              : 1;
   BOOL     bACT_Z_Source        : 1;
   BOOL     bACT_Y_Source        : 1;
   BOOL     bACT_X_Source        : 1;
   T_UNUSED uiUnused             : 1;
};

// Accelerometer Output Data Rate register data structure
struct T_BW_RATE
{
   T_OUT_DATA_RATE   eRate       : 4;
   BOOL              bLowPower   : 1;
   T_UNUSED          uiUnused    : 3;
};

// Accelerometer Power Control register data structure
struct T_POWER_CTL
{
   T_WAKEUP_FREQ  eWakeup        : 2;
   BOOL           bSleep         : 1;
   BOOL           bMeasure       : 1;
   BOOL           bAutoSleep     : 1;
   BOOL           bLink          : 1;
   T_UNUSED       uiUnused       : 2;
};

// Accelerometer INT_ENABLE, INT_MAP and INT_SOURCE registers data structure
struct T_INT_ENABLE
{
   BOOL        bOverrun          : 1;
   BOOL        bWatermark        : 1;
   BOOL        bFreeFall         : 1;
   BOOL        bInactivity       : 1;
   BOOL        bActivity         : 1;
   BOOL        bDoubleTap        : 1;
   BOOL        bSingleTap        : 1;
   BOOL        bDataReady        : 1;
};

typedef T_INT_ENABLE T_INT_MAP;
typedef T_INT_ENABLE T_INT_SOURCE;

// Accelerometer DATA_FORMAT register data structure
struct T_DATA_FORMAT
{
   T_RANGE_G_SET  eRange            : 2;
   BOOL           bJustifyLeft      : 1;
   BOOL           bFullResolution   : 1;
   T_UNUSED       uiUnused          : 1;
   BOOL           bIntInverted      : 1;
   BOOL           bSPI3WireMode     : 1;
   BOOL           bSelfTest         : 1;
};

// Accelerometer FIFO Buffer Control register data structure
struct T_FIFO_CTL
{
   T_DATA      uiSamples            : 5;
   BOOL        bTrigger             : 1;
   T_FIFO_MODE eFIFOMode            : 2;
};

// Accelerometer FIFO Buffer Status register data structure
struct T_FIFO_STATUS
{
   T_DATA   uiEntries               : 6;
   T_UNUSED uiUnused                : 1;
   BOOL     bFIFOTriggered          : 1;
};

// Accelerometer register access structure
union T_ACCEL_XFER_INFO
{
   struct
   {
      T_REGISTER_MAP eRegAdr        : 6;
      BOOL           bMultipleBytes : 1;
      BOOL           bRead          : 1;
   } sInfo;
   char cRawData;
};

// Accelerometer data access structure
union T_ACCEL_DATA
{
      T_ACT_INACT_CTL   sActInacCtl;
      T_TAP_AXES        sTAPAxes;
      T_ACT_TAP_STATUS  sActTAPAStatus;
      T_BW_RATE         sDataRate;
      T_POWER_CTL       sPowerCtl;
      T_INT_ENABLE      sIntEnable;
      T_INT_MAP         sIntMap;
      T_INT_SOURCE      sIntSource;
      T_DATA_FORMAT     sDataFormat;
      T_FIFO_CTL        sFIFOCtl;
      T_FIFO_STATUS     sFIFOStatus;
      char              cRawData;
};

#pragma pack()

#endif //#ifndef _ADXL345_H_
                                           