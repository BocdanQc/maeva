////////////////////////////////////////////////////////////////////////////////
// Filename:    SPIUtils.h
//
// Description: Utility procedures for SPI control
//
// Author:      Daniel Emond, IFT and GLO Department, Laval University
////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef _SPIUTILS_H_
#define _SPIUTILS_H_

// -----------------------------------------------------------------------------
// SPI Definitions
// -----------------------------------------------------------------------------
#define SPI_BUFF_SIZE 16

enum T_SLAVE_SELECT
{
   E_SS0,
   E_SS1,
   E_SS2,
   E_SS3,
   E_SS4,
   E_MAX_SLAVE
};

#endif //#ifndef _SPIUTILS_H_
