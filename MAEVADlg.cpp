////////////////////////////////////////////////////////////////////////////////
// Filename:    MAEVADlg.cpp
//
// Description: Implementation file for the MAEVA application Dialog Window
//
// Author:      Daniel Emond, IFT and GLO Department, Laval University
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MAEVADlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

////////////////////////////////////////////////////////////////////////////////
// GLOBAL Definitions and Declarations
////////////////////////////////////////////////////////////////////////////////
#define INIT_STYLUS       E_STEEL
#define INIT_COMP_VAR     E_SPEED

#define INIT_RPM          45.0f
#define INIT_RADIUS       125.0f 

const LONGLONG CAPTURE_LENGHT[E_MAX_CAPTURE_TIME] =
{
   (LONGLONG)(-1),
   SEC_PER_MIN,
   ( 2 * SEC_PER_MIN),
   ( 5 * SEC_PER_MIN),
   (10 * SEC_PER_MIN),
   (15 * SEC_PER_MIN),
   (20 * SEC_PER_MIN),
   (25 * SEC_PER_MIN),
   (30 * SEC_PER_MIN),
   (45 * SEC_PER_MIN),
   (60 * SEC_PER_MIN)
};

#define INIT_CAPTURE_SEL  3

#define INIT_RANGE        E_RANGE_PLUS_MINUS_4G

#ifdef _LOW_RES_DATA
#define DATA_MASK_2G  E_2G_LO_RES_DATA_MASK
#define DATA_MASK_4G  E_4G_LO_RES_DATA_MASK
#define DATA_MASK_8G  E_8G_LO_RES_DATA_MASK
#define DATA_MASK_16G E_16G_LO_RES_DATA_MASK
#else
#define DATA_MASK_2G  E_2G_HI_RES_DATA_MASK
#define DATA_MASK_4G  E_4G_HI_RES_DATA_MASK
#define DATA_MASK_8G  E_8G_HI_RES_DATA_MASK
#define DATA_MASK_16G E_16G_HI_RES_DATA_MASK
#endif

// -----------------------------------------------------------------------------
// Timing definitions
// -----------------------------------------------------------------------------
#define OUTPUT_DATA_RATE       E_RATE_800_HZ
#define OUTPUT_DATA_RATE_VALUE 800

#define SAFETY_FACTOR        1
#define SEC_TO_MS_FACTOR     1000
#define TIMER_PERIOD_IN_MS   (DWORD)((SEC_TO_MS_FACTOR * ACCEL_FIFO_BUFF_SIZE / OUTPUT_DATA_RATE_VALUE) >> SAFETY_FACTOR)
#define TIMER_DUE_TIME_IN_MS (TIMER_PERIOD_IN_MS >> 1)
#define MAX_REFRESH_COUNT    (((OUTPUT_DATA_RATE_VALUE << SAFETY_FACTOR) / ACCEL_FIFO_BUFF_SIZE) >> 2)

// -----------------------------------------------------------------------------
// Messages definitions between the acquisition timer and the dialog window
// -----------------------------------------------------------------------------
#define WM_DATAACQ_IN_PROGRESS         WM_APP+101
#define WM_DATAACQ_BUFFER_FULL         WM_APP+102
#define WM_DATAACQ_ACCESS_ERROR        WM_APP+103

// -----------------------------------------------------------------------------
// Acquisition buffers
// -----------------------------------------------------------------------------
// Note: The buffer size is fixed for 16 seconds of operations
#define ACQ_BUFF_SIZE (OUTPUT_DATA_RATE_VALUE << 4)

struct T_BUFF_DATA
{
   T_ACCEL_FIFO_ENTRY sData;
   BOOL               bOverrun;
};

static T_BUFF_DATA     g_asAcqBuff[E_MAX_BUFF_NB][ACQ_BUFF_SIZE];
static T_CURR_ACQ_BUFF g_eActBuff;
static BOOL            g_bInactBuffBusy;
static int             g_iBuffCurrIdx;
static LONGLONG        g_uiDataSize;
static LONGLONG        g_uiOverrunOccNb;
static int             g_iRefreshCount;

////////////////////////////////////////////////////////////////////////////////
// Static Timer Procedures (for multi-threading purposes)
////////////////////////////////////////////////////////////////////////////////
// -----------------------------------------------------------------------------
// Acquisition timer
// -----------------------------------------------------------------------------
static BOOL s_bAcqBusy = FALSE;
void CALLBACK AcquisitionTimer(LPVOID i_pParam, BOOL i_TimerOrWaitFired)
{
   CMAEVADlg *v_pCDlg = (CMAEVADlg*)i_pParam;

   int  v_iSPIResult;
   char v_acSPIBuffIn[SPI_BUFF_SIZE];
   char v_acSPIBuffOut[SPI_BUFF_SIZE];

   T_ACCEL_XFER_INFO v_sAccelInfo;
   T_ACCEL_DATA      v_sAccelData;

   // If the SUB-20 Device could be opened, start acquisition.
   if (!s_bAcqBusy && v_pCDlg->m_bAcquiringData && v_pCDlg->m_shHandle)
   {
      s_bAcqBusy = TRUE;

      // Read the INT_SOURCE register and the FIFO Status register.
      v_sAccelInfo.sInfo.bRead          = TRUE;
      v_sAccelInfo.sInfo.bMultipleBytes = FALSE;
      v_sAccelInfo.sInfo.eRegAdr        = E_REG_INT_SOURCE;
      v_acSPIBuffOut[0] = v_sAccelInfo.cRawData;
      v_acSPIBuffOut[1] = NULL;
      v_sAccelInfo.sInfo.bRead          = TRUE;
      v_sAccelInfo.sInfo.bMultipleBytes = FALSE;
      v_sAccelInfo.sInfo.eRegAdr        = E_REG_FIFO_STATUS;
      v_acSPIBuffOut[2] = v_sAccelInfo.cRawData;
      v_acSPIBuffOut[3] = NULL;

      v_iSPIResult = sub_spi_transfer(
                      v_pCDlg->m_shHandle,
                      v_acSPIBuffOut,
                      v_acSPIBuffIn,
                      (ACCEL_READ_BUFF_SIZE * 2),
                      SS_CONF(E_SS0,SS_LO)
                      );

       // Log any FIFO buffer overrun.
      v_sAccelData.cRawData = v_acSPIBuffIn[1];
      if (!v_sAccelData.sIntSource.bOverrun)
      {
         g_asAcqBuff[g_eActBuff][g_iBuffCurrIdx].bOverrun = FALSE;
      }
      else
      {
         g_uiOverrunOccNb++;
         g_asAcqBuff[g_eActBuff][g_iBuffCurrIdx].bOverrun = TRUE;
      }

      // Prepare the buffer to get all entries from the FIFO buffer.
      v_sAccelData.cRawData = v_acSPIBuffIn[3];
      if (v_sAccelData.sFIFOStatus.uiEntries > 0)
      {
         v_sAccelInfo.sInfo.bRead          = TRUE;
         v_sAccelInfo.sInfo.bMultipleBytes = TRUE;
         v_sAccelInfo.sInfo.eRegAdr        = E_REG_DATA_X0;
         v_acSPIBuffOut[0] = v_sAccelInfo.cRawData;
         for (unsigned int v_uiInc = 1; v_uiInc < ACCEL_MB_READ_BUFF_SIZE; v_uiInc++)
         {
            v_acSPIBuffOut[v_uiInc] = NULL;
         }

         // Do a multiple bytes read on DATA X0 register up to DATA Z1 register for data consistency for all entries.
         for (unsigned int v_uiInc = 0; v_uiInc < v_sAccelData.sFIFOStatus.uiEntries; v_uiInc++)
         {
            v_iSPIResult = sub_spi_transfer(
                              v_pCDlg->m_shHandle,
                              v_acSPIBuffOut,
                              v_acSPIBuffIn,
                              ACCEL_MB_READ_BUFF_SIZE,
                              SS_CONF(E_SS0,SS_LO)
                              );

            g_asAcqBuff[g_eActBuff][g_iBuffCurrIdx].sData.acX[0] = v_acSPIBuffIn[1];
            g_asAcqBuff[g_eActBuff][g_iBuffCurrIdx].sData.acX[1] = v_acSPIBuffIn[2];
            g_asAcqBuff[g_eActBuff][g_iBuffCurrIdx].sData.acY[0] = v_acSPIBuffIn[3];
            g_asAcqBuff[g_eActBuff][g_iBuffCurrIdx].sData.acY[1] = v_acSPIBuffIn[4];
            g_asAcqBuff[g_eActBuff][g_iBuffCurrIdx].sData.acZ[0] = v_acSPIBuffIn[5];
            g_asAcqBuff[g_eActBuff][g_iBuffCurrIdx].sData.acZ[1] = v_acSPIBuffIn[6];
            g_iBuffCurrIdx++;

            // Update the data size gathered.
            g_uiDataSize++;

            // Switch buffer when the active buffer is full.
            if (g_iBuffCurrIdx == ACQ_BUFF_SIZE)
            {
               if (g_eActBuff == E_1ST_BUFF)
               {
                  g_eActBuff  = E_2ND_BUFF;
               }
               else
               {
                  g_eActBuff  = E_1ST_BUFF;
               }

               g_iBuffCurrIdx = 0;

               PostMessage(v_pCDlg->m_hWnd,WM_DATAACQ_BUFFER_FULL,(WPARAM)NULL,(LPARAM)NULL);
            }
         }
      }

      // Send a message to the Dialog window to update the display.
      if (g_iRefreshCount > MAX_REFRESH_COUNT)
      {
         PostMessage(v_pCDlg->m_hWnd,WM_DATAACQ_IN_PROGRESS,(WPARAM)NULL,(LPARAM)NULL);
         g_iRefreshCount = 0;
      }
      g_iRefreshCount++;

      s_bAcqBusy = FALSE;
   }
   // Otherwise, stop the data acquisition and indicate the error.
   else
   {
      if (!v_pCDlg->m_shHandle)
      {
         v_pCDlg->m_bAcquiringData = FALSE;
         PostMessage(v_pCDlg->m_hWnd,WM_DATAACQ_ACCESS_ERROR,(WPARAM)NULL,(LPARAM)NULL);
      }
      DeleteTimerQueueTimer(v_pCDlg->m_hTimerQueue, v_pCDlg->m_hTimer, INVALID_HANDLE_VALUE);
      v_pCDlg->m_hTimer = NULL;
   }
}

////////////////////////////////////////////////////////////////////////////////
// CMAEVADlg dialog
////////////////////////////////////////////////////////////////////////////////
// -----------------------------------------------------------------------------
// Dialog Window Constructor method
// -----------------------------------------------------------------------------
CMAEVADlg::CMAEVADlg(CWnd* pParent /*=NULL*/)
	: CDialog(CMAEVADlg::IDD, pParent)
   , m_shHandle(NULL)
   , m_hTimerQueue(NULL)
   , m_hTimer(NULL)
   , m_bAcquiringData(FALSE)
   , m_eStylusType(INIT_STYLUS)
   , m_eComputedVar(INIT_COMP_VAR)
   , m_fRPM(INIT_RPM)
   , m_fRadius(INIT_RADIUS)
   , m_eDataRangeSetting(INIT_RANGE)
   , m_bAccelStarted(FALSE)
   , m_bFileAlreadyOpened(FALSE)
   , m_iFileOverrunEndPtr(0)
   , m_uiElapsedTime(0)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

    // Note: radial speed = 2 x PI x radius x RPM / 60s
    m_fSpeed = (2 * PI * INIT_RPM * INIT_RADIUS) / SEC_PER_MIN;

    m_uiCaptureTime = CAPTURE_LENGHT[INIT_CAPTURE_SEL];
}

// -----------------------------------------------------------------------------
// Dialog Window Data Exchange method
// -----------------------------------------------------------------------------
void CMAEVADlg::DoDataExchange(CDataExchange* pDX)
{
   CDialog::DoDataExchange(pDX);
   DDX_Control(pDX, IDC_LIST_SUB20DEVICES, m_lbSUB20DevicesList);
   DDX_Control(pDX, IDC_STATIC_X_AXIS, m_staticXAxis);
   DDX_Control(pDX, IDC_STATIC_Y_AXIS, m_staticYAxis);
   DDX_Control(pDX, IDC_STATIC_Z_AXIS, m_staticZAxis);
   DDX_Control(pDX, IDC_EDIT_SURFACE_TYPE, m_editSurfaceType);
   DDX_Control(pDX, IDC_COMBO_STYLUS, m_cbStylusType);
   DDX_Control(pDX, IDC_COMBO_VAR, m_cbComputedVar);
   DDX_Control(pDX, IDC_EDIT_RPM, m_editRPM);
   DDX_Control(pDX, IDC_EDIT_RADIUS, m_editRadius);
   DDX_Control(pDX, IDC_EDIT_SPEED, m_editSpeed);
   DDX_Control(pDX, IDC_COMBO_CAPTURE_LENGTH, m_cbCaptureLength);
   DDX_Control(pDX, IDC_COMBO_DATA_RANGE, m_cbDataRangeList);
   DDX_Control(pDX, IDC_STATIC_DATA_SIZE, m_staticDataSize);
   DDX_Control(pDX, IDC_STATIC_ELAPSED_TIME, m_staticElapsedTime);
   DDX_Control(pDX, IDC_STATIC_OVERRUN, m_staticOverrunNb);
   DDX_Control(pDX, IDC_BUTTON_IDLE_TEST, m_buttonIdleTest);
   DDX_Control(pDX, IDC_BUTTON_START_ACQ, m_buttonStartAcq);
   DDX_Control(pDX, IDC_BUTTON_STOP_ACQ, m_buttonStopAcq);
   DDX_Control(pDX, IDC_BUTTON_SHOW_RT_DATA, m_buttonShowRTData);
   DDX_Control(pDX, IDC_BUTTON_SAVE_DATA, m_buttonSaveData);
   DDX_Control(pDX, IDC_BUTTON_EVAL_DATA, m_buttonEvalData);
}

// -----------------------------------------------------------------------------
// WINDOWS MESSAGE MAP
// -----------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CMAEVADlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
   ON_CBN_SELCHANGE(IDC_COMBO_STYLUS,     &CMAEVADlg::OnCbnSelchangeComboStylusType)
   ON_CBN_SELCHANGE(IDC_COMBO_VAR,        &CMAEVADlg::OnCbnSelchangeComboComputedVar)
   ON_EN_KILLFOCUS(IDC_EDIT_RPM,          &CMAEVADlg::OnKillFocusEditRPM)
   ON_EN_KILLFOCUS(IDC_EDIT_RADIUS,       &CMAEVADlg::OnKillFocusEditRadius)
   ON_EN_KILLFOCUS(IDC_EDIT_SPEED,        &CMAEVADlg::OnKillFocusEditSpeed)
   ON_CBN_SELCHANGE(IDC_COMBO_CAPTURE_LENGTH, &CMAEVADlg::OnCbnSelchangeComboCaptureLength)
   ON_CBN_SELCHANGE(IDC_COMBO_DATA_RANGE, &CMAEVADlg::OnCbnSelchangeComboDataRange)
   ON_BN_CLICKED(IDC_BUTTON_SCAN,         &CMAEVADlg::OnBnClickedButtonScan)
   ON_BN_CLICKED(IDC_BUTTON_IDLE_TEST,    &CMAEVADlg::OnBnClickedButtonIdleTest)
   ON_BN_CLICKED(IDC_BUTTON_START_ACQ,    &CMAEVADlg::OnBnClickedButtonStartAcq)
   ON_BN_CLICKED(IDC_BUTTON_STOP_ACQ,     &CMAEVADlg::OnBnClickedButtonStopAcq)
   ON_BN_CLICKED(IDC_BUTTON_SHOW_RT_DATA, &CMAEVADlg::OnBnClickedButtonShowRtData)
   ON_BN_CLICKED(IDC_BUTTON_SAVE_DATA,    &CMAEVADlg::OnBnClickedButtonSaveData)
   ON_BN_CLICKED(IDC_BUTTON_EVAL_DATA,    &CMAEVADlg::OnBnClickedButtonEvalData)
   ON_BN_CLICKED(IDOK,                    &CMAEVADlg::OnBnClickedButtonExit)
   ON_MESSAGE(WM_DATAACQ_IN_PROGRESS,  &CMAEVADlg::OnTimerMsgDataAcqInProgress)
   ON_MESSAGE(WM_DATAACQ_BUFFER_FULL,  &CMAEVADlg::OnTimerMsgDataAcqBufferFull)
   ON_MESSAGE(WM_DATAACQ_ACCESS_ERROR, &CMAEVADlg::OnTimerMsgDataAcqAccessError)
END_MESSAGE_MAP()

// -----------------------------------------------------------------------------
// Methods to access the devices
// -----------------------------------------------------------------------------
// Method used to find all the SUB-20 Devices connected on all the USB ports
BOOL CMAEVADlg::FindAllSUB20Devices(void)
{
   BOOL     v_bDeviceFound = FALSE;
   int      v_iIndex;
   int      v_iMsgResult;
   int      v_iProdTestResult, v_iModeTestResult, v_iSerialNbTestResult;
   int      v_iMode;
   char     v_strProductID[32];
   char     v_strSerialNb[32];
   CString  v_strDescription, v_strVersion;

   sub_device v_sdDevice = NULL;
   sub_handle v_shHandle = NULL;

   // Check for all SUB-20 Devices on USB ports.
   while (v_sdDevice = sub_find_devices(v_sdDevice))
   {
      // If there is a new SUB-20 Device to add, obtain its corresponding handle.
      v_shHandle = sub_open(v_sdDevice);

      // Once you have control over the Device, get its Product ID.
      if (v_shHandle)
      {
         v_iProdTestResult = sub_get_product_id(v_shHandle, v_strProductID, sizeof(v_strProductID));

         // If you can extract the Product ID, add it to the list and
         // extract the Version and the Active Mode.
         if(v_iProdTestResult >= SE_OK)
         {
            v_strDescription =  v_strProductID;
            v_strDescription += ' ';

            const struct sub_version* v_svVersion = sub_get_version(v_shHandle) ;

            v_iModeTestResult = sub_get_mode(v_shHandle, &v_iMode);

            // If the SUB-20 Device is acting Normally, extract the device version
            v_iSerialNbTestResult = -1;
            if (!v_iModeTestResult && (v_iMode != SUB_BOOT_MODE))
            {
               v_iSerialNbTestResult = sub_get_serial_number(v_shHandle, v_strSerialNb, sizeof(v_strSerialNb));
            }

            // If the serial number is valid and the Device is acting Normally,
            // add the remaining information in the list.
            if (v_iSerialNbTestResult >= 0)
            {
               v_strVersion.Format(
                  _T("v%d.%d.%d"),
                  v_svVersion->sub_device.major,
                  v_svVersion->sub_device.minor,
                  v_svVersion->sub_device.micro
                  );

               v_strDescription += "0x";
               v_strDescription += v_strSerialNb;
      
               v_strDescription += " ";
               v_strDescription += v_strVersion;

               v_iIndex = m_lbSUB20DevicesList.GetCount();
               m_lbSUB20DevicesList.AddString(v_strDescription);
               m_lbSUB20DevicesList.SetItemData(v_iIndex, (DWORD_PTR)v_sdDevice);
            }
            //|> LLR Send an error message, if you can't get the current SUB-20 Device Serial Number.
            else
            {
               v_iMsgResult = MessageBox(
                  _T("Error: Cannot obtain the current SUB-20 Device Serial Number."),
                  _T("SUB-20 List Initialization")
                  );
            }
         }
         //|> LLR Send an error message, if you can't get the current SUB-20 Device Product ID.
         else
         {
            v_iMsgResult = MessageBox(
               _T("Error: Cannot obtain he current SUB-20 Product ID."),
               _T("SUB-20 List Initialization")
               );
         }

         sub_close(v_shHandle);
         v_shHandle = NULL;
      }
      //|> LLR Send an error message, if you can't get a hold of the current SUB-20 Device.
      else
      {
         v_iMsgResult = MessageBox(
            _T("Error: Cannot connect to the current SUB-20 Device."),
            _T("SUB-20 List Initialization")
            );
      }
   } // Find Device Loop

   //|>LLR If at least one device was found, return true.
   if (m_lbSUB20DevicesList.GetCount() > 0)
   {
      v_bDeviceFound = TRUE;
   }

   return v_bDeviceFound;
}

// Method that returns the current SUB-20 Device selected in the window list
sub_device CMAEVADlg::GetSelectedSUB20Device(void)
{
	int v_iIndex;

   sub_device v_sdDevice = NULL;

   // Return the handle corresponding to the current SUB-20 Device selected.
   v_iIndex = m_lbSUB20DevicesList.GetCurSel();
	if(v_iIndex != -1)
   {
		v_sdDevice = (sub_device)m_lbSUB20DevicesList.GetItemData(v_iIndex);
   }

   return v_sdDevice;
}

// Method to initialize and start the ADXL345 Accelerometer through the SUB-20 SPI
BOOL CMAEVADlg::StartAccelerometer(sub_handle i_shHandle)
{
   int  v_iSPIResult;
   BOOL v_bSuccess = FALSE;

   char v_acSPIBuffOut[SPI_BUFF_SIZE];

   T_ACCEL_XFER_INFO v_sAccelInfo;
   T_ACCEL_DATA      v_sAccelData;

   // Initialize the accelerometer only if the SUB-20 Device can be accessed.
   if (i_shHandle)
   {
      // Initialize the Data Format register
      v_sAccelInfo.sInfo.bRead          = FALSE;
      v_sAccelInfo.sInfo.bMultipleBytes = FALSE;
      v_sAccelInfo.sInfo.eRegAdr        = E_REG_DATA_FORMAT;

      v_sAccelData.sDataFormat.eRange           = m_eDataRangeSetting;
      v_sAccelData.sDataFormat.bJustifyLeft     = FALSE;
#ifdef _LOW_RES_DATA
      v_sAccelData.sDataFormat.bFullResolution  = FALSE;
#else
      v_sAccelData.sDataFormat.bFullResolution  = TRUE;
#endif
      v_sAccelData.sDataFormat.uiUnused         = NULL;
      v_sAccelData.sDataFormat.bIntInverted     = FALSE;
      v_sAccelData.sDataFormat.bSPI3WireMode    = FALSE;
      v_sAccelData.sDataFormat.bSelfTest        = FALSE;

      v_acSPIBuffOut[0] = v_sAccelInfo.cRawData;
      v_acSPIBuffOut[1] = v_sAccelData.cRawData;

      v_iSPIResult = sub_spi_transfer(
                      i_shHandle,
                      v_acSPIBuffOut,
                      NULL,
                      ACCEL_WRITE_BUFF_SIZE,
                      SS_CONF(E_SS0,SS_LO)
                      );

      // Initialize the Data Rate register
      v_sAccelInfo.sInfo.bRead          = FALSE;
      v_sAccelInfo.sInfo.bMultipleBytes = FALSE;
      v_sAccelInfo.sInfo.eRegAdr        = E_REG_BW_RATE;

      v_sAccelData.sDataRate.eRate     = OUTPUT_DATA_RATE;
      v_sAccelData.sDataRate.bLowPower = FALSE;
      v_sAccelData.sDataRate.uiUnused  = NULL;

      v_acSPIBuffOut[0] = v_sAccelInfo.cRawData;
      v_acSPIBuffOut[1] = v_sAccelData.cRawData;

      v_iSPIResult = sub_spi_transfer(
                      i_shHandle,
                      v_acSPIBuffOut,
                      NULL,
                      ACCEL_WRITE_BUFF_SIZE,
                      SS_CONF(E_SS0,SS_LO)
                      );

      // Initialize the FIFO Control register
      v_sAccelInfo.sInfo.bRead          = FALSE;
      v_sAccelInfo.sInfo.bMultipleBytes = FALSE;
      v_sAccelInfo.sInfo.eRegAdr        = E_REG_FIFO_CTL;

      v_sAccelData.sFIFOCtl.uiSamples = ACCEL_FIFO_BUFF_SIZE - 1;
      v_sAccelData.sFIFOCtl.bTrigger  = FALSE;
      v_sAccelData.sFIFOCtl.eFIFOMode = E_MODE_STREAM;

      v_acSPIBuffOut[0] = v_sAccelInfo.cRawData;
      v_acSPIBuffOut[1] = v_sAccelData.cRawData;

      v_iSPIResult = sub_spi_transfer(
                      i_shHandle,
                      v_acSPIBuffOut,
                      NULL,
                      ACCEL_WRITE_BUFF_SIZE,
                      SS_CONF(E_SS0,SS_LO)
                      );

      // Disable all interrupts by modifying the INT_ENABLE register
      v_sAccelInfo.sInfo.bRead          = FALSE;
      v_sAccelInfo.sInfo.bMultipleBytes = FALSE;
      v_sAccelInfo.sInfo.eRegAdr        = E_REG_INT_ENABLE;

      v_sAccelData.sIntEnable.bOverrun    = FALSE;
      v_sAccelData.sIntEnable.bWatermark  = FALSE;
      v_sAccelData.sIntEnable.bFreeFall   = FALSE;
      v_sAccelData.sIntEnable.bInactivity = FALSE;
      v_sAccelData.sIntEnable.bActivity   = FALSE;
      v_sAccelData.sIntEnable.bDoubleTap  = FALSE;
      v_sAccelData.sIntEnable.bSingleTap  = FALSE;
      v_sAccelData.sIntEnable.bDataReady  = FALSE;

      v_acSPIBuffOut[0] = v_sAccelInfo.cRawData;
      v_acSPIBuffOut[1] = v_sAccelData.cRawData;

      v_iSPIResult = sub_spi_transfer(
                      i_shHandle,
                      v_acSPIBuffOut,
                      NULL,
                      ACCEL_WRITE_BUFF_SIZE,
                      SS_CONF(E_SS0,SS_LO)
                      );

      // Start taking measurements modifying the Power Control register
      v_sAccelInfo.sInfo.bRead          = FALSE;
      v_sAccelInfo.sInfo.bMultipleBytes = FALSE;
      v_sAccelInfo.sInfo.eRegAdr        = E_REG_POWER_CTL;

      v_sAccelData.sPowerCtl.eWakeup      = E_FREQ_1_HZ;
      v_sAccelData.sPowerCtl.bSleep       = FALSE;
      v_sAccelData.sPowerCtl.bMeasure     = TRUE;
      v_sAccelData.sPowerCtl.bAutoSleep   = FALSE;
      v_sAccelData.sPowerCtl.bLink        = FALSE;
      v_sAccelData.sPowerCtl.uiUnused     = NULL;

      v_acSPIBuffOut[0] = v_sAccelInfo.cRawData;
      v_acSPIBuffOut[1] = v_sAccelData.cRawData;

      v_iSPIResult = sub_spi_transfer(
                      i_shHandle,
                      v_acSPIBuffOut,
                      NULL,
                      ACCEL_WRITE_BUFF_SIZE,
                      SS_CONF(E_SS0,SS_LO)
                      );

      // Show that the initialization was a success.
      v_bSuccess = TRUE;
   }

   return v_bSuccess;
}

// Method to stop the ADXL345 Accelerometer through the SUB-20 SPI
BOOL CMAEVADlg::StopAccelerometer(sub_handle i_shHandle)
{
   int  v_iSPIResult;
   BOOL v_bSuccess = FALSE;

   char v_acSPIBuffIn[SPI_BUFF_SIZE];
   char v_acSPIBuffOut[SPI_BUFF_SIZE];

   T_ACCEL_XFER_INFO v_sAccelInfo;
   T_ACCEL_DATA      v_sAccelData;

   // Stop the accelerometer only if the SUB-20 Device can be accessed.
   if (i_shHandle)
   {
      // Stop taking measurements modifying the Power Control register
      v_sAccelInfo.sInfo.bRead          = FALSE;
      v_sAccelInfo.sInfo.bMultipleBytes = FALSE;
      v_sAccelInfo.sInfo.eRegAdr        = E_REG_POWER_CTL;

      v_sAccelData.sPowerCtl.eWakeup      = E_FREQ_1_HZ;
      v_sAccelData.sPowerCtl.bSleep       = FALSE;
      v_sAccelData.sPowerCtl.bMeasure     = FALSE;
      v_sAccelData.sPowerCtl.bAutoSleep   = FALSE;
      v_sAccelData.sPowerCtl.bLink        = FALSE;
      v_sAccelData.sPowerCtl.uiUnused     = NULL;

      v_acSPIBuffOut[0] = v_sAccelInfo.cRawData;
      v_acSPIBuffOut[1] = v_sAccelData.cRawData;

      v_iSPIResult = sub_spi_transfer(
                      i_shHandle,
                      v_acSPIBuffOut,
                      NULL,
                      ACCEL_WRITE_BUFF_SIZE,
                      SS_CONF(E_SS0,SS_LO)
                      );

      // Do a Read Multiple Bytes on the data to force a reset of the overrun flag
      v_sAccelInfo.sInfo.bRead          = TRUE;
      v_sAccelInfo.sInfo.bMultipleBytes = TRUE;
      v_sAccelInfo.sInfo.eRegAdr        = E_REG_DATA_X0;
      v_acSPIBuffOut[0] = v_sAccelInfo.cRawData;
      for (int v_iInc = 1; v_iInc < ACCEL_MB_READ_BUFF_SIZE; v_iInc++)
      {
         v_acSPIBuffOut[v_iInc] = NULL;
      }
      v_iSPIResult = sub_spi_transfer(
                      i_shHandle,
                      v_acSPIBuffOut,
                      v_acSPIBuffIn,
                      ACCEL_MB_READ_BUFF_SIZE,
                      SS_CONF(E_SS0,SS_LO)
                      );

      // Initialize the Interrupt Enable register to ensure complete reset
      v_sAccelInfo.sInfo.bRead          = FALSE;
      v_sAccelInfo.sInfo.bMultipleBytes = FALSE;
      v_sAccelInfo.sInfo.eRegAdr        = E_REG_INT_ENABLE;

      v_sAccelData.sIntEnable.bOverrun    = FALSE;
      v_sAccelData.sIntEnable.bWatermark  = FALSE;
      v_sAccelData.sIntEnable.bFreeFall   = FALSE;
      v_sAccelData.sIntEnable.bInactivity = FALSE;
      v_sAccelData.sIntEnable.bActivity   = FALSE;
      v_sAccelData.sIntEnable.bDoubleTap  = FALSE;
      v_sAccelData.sIntEnable.bSingleTap  = FALSE;
      v_sAccelData.sIntEnable.bDataReady  = FALSE;

      v_acSPIBuffOut[0] = v_sAccelInfo.cRawData;
      v_acSPIBuffOut[1] = v_sAccelData.cRawData;

      v_iSPIResult = sub_spi_transfer(
                      i_shHandle,
                      v_acSPIBuffOut,
                      NULL,
                      ACCEL_WRITE_BUFF_SIZE,
                      SS_CONF(E_SS0,SS_LO)
                      );

      // Show that the accelerometer stopped successfully.
      v_bSuccess = TRUE;
   }

   return v_bSuccess;
}

// -----------------------------------------------------------------------------
// Methods to write data to files
// -----------------------------------------------------------------------------
BOOL CMAEVADlg::WriteDataAcqBufferToFile(T_CURR_ACQ_BUFF i_eBuff, int i_iDataCount)
{
   BOOL           v_bSuccess = FALSE;
   CFile          v_sAcqFile;
   CTime          v_sLocalTime;
   CString        v_strDisplay, v_strTemp;
   char           v_acBufRead[256];
   int            v_iDataCount = 0;
   int            v_iOverrunPtr;
   T_AXIS_CONV    v_sAxisConv;
   T_AXIS_DATA    v_sAxisData;
   unsigned short v_uiDataMask;
   int            v_iDataRangeValue;
   double         v_fConvFactor;

   // Obtain the corresponding data mask and conversion factor as per the data range.
   switch (m_eDataRangeSetting)
   {
   case E_RANGE_PLUS_MINUS_2G:
      v_uiDataMask      = DATA_MASK_2G;
      v_iDataRangeValue = E_2G_VAL;
      break;
   case E_RANGE_PLUS_MINUS_4G:
      v_uiDataMask      = DATA_MASK_4G;
      v_iDataRangeValue = E_4G_VAL;
      break;
   case E_RANGE_PLUS_MINUS_8G:
      v_uiDataMask      = DATA_MASK_8G;
      v_iDataRangeValue = E_8G_VAL;
      break;
   case E_RANGE_PLUS_MINUS_16G:
      v_uiDataMask      = DATA_MASK_16G;
      v_iDataRangeValue = E_16G_VAL;
      break;
   default:
      v_uiDataMask      = 0x0000;
      v_iDataRangeValue = NULL;
   }
   v_fConvFactor = (float)v_iDataRangeValue / (v_uiDataMask + 1);

   // If not already done, create acquisiton file.
   if (!m_bFileAlreadyOpened)
   {
      // Create the acquisition file.
      v_bSuccess = v_sAcqFile.Open(
                      _T("C:\\Temp\\AcqData.txt"),
                      CFile::modeCreate     |
                      CFile::modeReadWrite  |
                      CFile::shareDenyWrite
                      );

      // Write the surface type and the data output rate to the file if created.
      if (v_bSuccess)
      {
         // Date
         v_strDisplay = m_timeAcqStart.Format("%#c\r\n");
         v_strTemp = v_strDisplay;
         // Surface
         m_editSurfaceType.GetWindowText(v_strDisplay);
         v_strTemp.AppendFormat(_T("Surface: %s\r\n"),v_strDisplay);
         // Stylus
         m_cbStylusType.GetWindowText(v_strDisplay);
         v_strTemp.AppendFormat(_T("Stylus: %s\r\n"),v_strDisplay);
         // Speed
         m_editSpeed.GetWindowText(v_strDisplay);
         v_strTemp.AppendFormat(_T("Speed: %s mm\\sec\r\n"),v_strDisplay.Trim());
         // Data rate
         v_strTemp.AppendFormat(_T("Data rate: %d Hz\r\n"),OUTPUT_DATA_RATE_VALUE);
         // Data range
         v_strTemp.AppendFormat(_T("Data range: ±%dG\r\n"),(int)(v_iDataRangeValue/1000));
         // Overrun (initial value of 0)
         v_strTemp.Append(_T("Overruns: 00000\r\n"));

         m_iFileOverrunEndPtr = v_strTemp.GetLength();
         v_sAcqFile.Write(v_strTemp, v_strTemp.GetLength());

         m_bFileAlreadyOpened = TRUE;
      }
   }
   // Open the acquisition file.
   else
   {
      v_bSuccess = v_sAcqFile.Open(
                      _T("C:\\Temp\\AcqData.txt"),
                      CFile::modeReadWrite  |
                      CFile::shareDenyWrite
                      );
      if (v_bSuccess)
      {
         v_sAcqFile.SeekToEnd();
      }
   }

   // Proceed with writing the data only if the file was opened successfully.
   if (v_bSuccess)
   {
      // Extract all entries in the acquisition buffer.
      for (int v_iCount = 0; v_iCount < i_iDataCount; v_iCount++)
      {
         // Convert the current entry of the acquisiton buffer.
         v_sAxisConv.sAxisInChar.acX[0] = g_asAcqBuff[i_eBuff][v_iCount].sData.acX[0];
         v_sAxisConv.sAxisInChar.acX[1] = g_asAcqBuff[i_eBuff][v_iCount].sData.acX[1];
         v_sAxisConv.sAxisInChar.acY[0] = g_asAcqBuff[i_eBuff][v_iCount].sData.acY[0];
         v_sAxisConv.sAxisInChar.acY[1] = g_asAcqBuff[i_eBuff][v_iCount].sData.acY[1];
         v_sAxisConv.sAxisInChar.acZ[0] = g_asAcqBuff[i_eBuff][v_iCount].sData.acZ[0];
         v_sAxisConv.sAxisInChar.acZ[1] = g_asAcqBuff[i_eBuff][v_iCount].sData.acZ[1];

         if ((v_sAxisConv.sAxisInUnsignedShort.uiX & (v_uiDataMask + 1)) == NULL)
         {
            v_sAxisData.fAxisX = (v_sAxisConv.sAxisInUnsignedShort.uiX & v_uiDataMask) * v_fConvFactor;
         }
         else
         {
            v_sAxisData.fAxisX = (~(v_sAxisConv.sAxisInUnsignedShort.uiX - 1) & v_uiDataMask) * -v_fConvFactor;
         }
         if ((v_sAxisConv.sAxisInUnsignedShort.uiY & (v_uiDataMask + 1)) == NULL)
         {
            v_sAxisData.fAxisY = (v_sAxisConv.sAxisInUnsignedShort.uiY & v_uiDataMask) * v_fConvFactor;
         }
         else
         {
            v_sAxisData.fAxisY = (~(v_sAxisConv.sAxisInUnsignedShort.uiY - 1) & v_uiDataMask) * -v_fConvFactor;
         }
         if ((v_sAxisConv.sAxisInUnsignedShort.uiZ & (v_uiDataMask + 1)) == NULL)
         {
            v_sAxisData.fAxisZ = (v_sAxisConv.sAxisInUnsignedShort.uiZ & v_uiDataMask) * v_fConvFactor;
         }
         else
         {
            v_sAxisData.fAxisZ = (~(v_sAxisConv.sAxisInUnsignedShort.uiZ - 1) & v_uiDataMask) * -v_fConvFactor;
         }

         // Write the data into the file
         v_strTemp.Format(
            _T("%.1f, %.1f, %.1f,\r\n"),
            v_sAxisData.fAxisX,
            v_sAxisData.fAxisY,
            v_sAxisData.fAxisZ
            );
         v_sAcqFile.Write(v_strTemp, v_strTemp.GetLength());

         // Log any buffer overrun !
         if (g_asAcqBuff[i_eBuff][v_iCount].bOverrun)
         {
            v_strTemp = _T("NAN\r\n");
            v_sAcqFile.Write(v_strTemp, v_strTemp.GetLength());
         }
      }

      // Write the Buffer Overrun number once the acquisition process is complete
      if (!m_bAcquiringData && (g_uiOverrunOccNb > 0) && (m_iFileOverrunEndPtr > 0))
      {
         v_sAcqFile.SeekToBegin();
         if (v_sAcqFile.Read(v_acBufRead, m_iFileOverrunEndPtr) > 0)
         {
            v_strTemp = v_acBufRead;
            v_iOverrunPtr = v_strTemp.Find(_T("Overruns: 00000"));
            v_sAcqFile.Seek(v_iOverrunPtr, CFile::begin);
            if (g_uiOverrunOccNb < 100000)
            {
               v_strTemp.Format(_T("Overruns: %.5d"), g_uiOverrunOccNb);
            }
            else
            {
               v_strTemp = _T("Overruns: *****");
            }
            v_sAcqFile.Write(v_strTemp, v_strTemp.GetLength());
         }
      }

      // Close the file.
      v_sAcqFile.Close();
   }

   return v_bSuccess;
}
// -----------------------------------------------------------------------------
// Methods to write data to files
// -----------------------------------------------------------------------------
BOOL CMAEVADlg::StopAcquisition(void)
{
   BOOL v_bSuccess = TRUE;

   // Stop the acquisition process.
   m_bAcquiringData = FALSE;

   // Delete the timers.
   if (m_hTimerQueue)
   {
      v_bSuccess = DeleteTimerQueueEx(m_hTimerQueue, INVALID_HANDLE_VALUE);
      m_hTimerQueue = NULL;
   }

   // Stop the accelerometer since it is no longer acquiring data.
   if (m_bAccelStarted && m_shHandle)
   {
      m_bAccelStarted = FALSE;
      v_bSuccess &= StopAccelerometer(m_shHandle);
      sub_close(m_shHandle);
      m_shHandle = NULL;
   }
   else if (m_bAccelStarted)
   {
      m_bAccelStarted = FALSE;
      v_bSuccess = FALSE;
   }

   return v_bSuccess;
}

// -----------------------------------------------------------------------------
// CMEL1Dlg Dialog Window message handlers methods
// -----------------------------------------------------------------------------
// Method to draw the Dialog Window at the application start
BOOL CMAEVADlg::OnInitDialog()
{
	CDialog::OnInitDialog();

   CString v_strTemp;

	// Set the icon for this dialog.  The framework does this automatically
	// when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

   // Initialize the display variables
   m_lbSUB20DevicesList.ResetContent();

   m_staticXAxis.SetWindowText(_T("0.0"));
   m_staticYAxis.SetWindowText(_T("0.0"));
   m_staticZAxis.SetWindowText(_T("0.0"));

   m_editSurfaceType.SetWindowText(_T("Unknown"));

   m_cbStylusType.ResetContent();
   m_cbStylusType.AddString(_T("Aluminium"));
   m_cbStylusType.AddString(_T("Plastic"));
   m_cbStylusType.AddString(_T("Steel"));
   m_cbStylusType.AddString(_T("Wood - Pine"));
   m_cbStylusType.SetCurSel(INIT_STYLUS);

   m_cbComputedVar.ResetContent();
   m_cbComputedVar.AddString(_T("Rev. per min."));
   m_cbComputedVar.AddString(_T("Radius"));
   m_cbComputedVar.AddString(_T("Radial Speed"));
   m_cbComputedVar.SetCurSel(INIT_COMP_VAR);

   v_strTemp.Format(_T("%.1f"), m_fRPM);
   m_editRPM.SetWindowText(v_strTemp);
   v_strTemp.Format(_T("%.1f"), m_fRadius);
   m_editRadius.SetWindowText(v_strTemp);
   v_strTemp.Format(_T("%.1f"), m_fSpeed);
   m_editSpeed.SetWindowText(v_strTemp);

   m_editRPM.EnableWindow(m_eComputedVar != E_RPM);
   m_editRadius.EnableWindow(m_eComputedVar != E_RADIUS);
   m_editSpeed.EnableWindow(m_eComputedVar != E_SPEED);

   m_staticElapsedTime.SetWindowText(_T("  0"));
   m_staticDataSize.SetWindowText(_T("  0"));
   m_staticOverrunNb.SetWindowText(_T("  0"));

   m_cbCaptureLength.ResetContent();
   m_cbCaptureLength.AddString(_T("Illimited"));
   m_cbCaptureLength.AddString(_T("1 min."));
   m_cbCaptureLength.AddString(_T("2 min."));
   m_cbCaptureLength.AddString(_T("5 min."));
   m_cbCaptureLength.AddString(_T("10 min."));
   m_cbCaptureLength.AddString(_T("15 min."));
   m_cbCaptureLength.AddString(_T("20 min."));
   m_cbCaptureLength.AddString(_T("25 min."));
   m_cbCaptureLength.AddString(_T("30 min."));
   m_cbCaptureLength.AddString(_T("45 min."));
   m_cbCaptureLength.AddString(_T("60 min."));
   m_cbCaptureLength.SetCurSel(INIT_CAPTURE_SEL);

   m_cbDataRangeList.ResetContent();
   m_cbDataRangeList.AddString(_T("± 2G"));
   m_cbDataRangeList.AddString(_T("± 4G"));
   m_cbDataRangeList.AddString(_T("± 8G"));
   m_cbDataRangeList.AddString(_T("± 16G"));
   m_cbDataRangeList.SetCurSel(INIT_RANGE);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
// to draw the icon.  For MFC applications using the document/view model,
// this is automatically done for you by the framework.
void CMAEVADlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
// the minimized window.
HCURSOR CMAEVADlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

// Method that updates the Stylus Type.
void CMAEVADlg::OnCbnSelchangeComboStylusType()
{
   // Assign the selected item in the list to the new data range setting
   if ((m_cbStylusType.GetCurSel() >= E_ALUMINIUM) &&
       (m_cbStylusType.GetCurSel() <= E_WOOD_PINE)  )
   {
      m_eStylusType = (T_STYLUS_TYPE)m_cbStylusType.GetCurSel();
   }
   else
   {
      m_eStylusType = E_STEEL;
   }
}

// Method that selects the variable to compute the radial speed equation.
void CMAEVADlg::OnCbnSelchangeComboComputedVar()
{
   // Assign the selected item in the list to the new selected variable.
   if ((m_cbComputedVar.GetCurSel() >= E_RPM)  &&
       (m_cbComputedVar.GetCurSel() <= E_SPEED)  )
   {
      m_eComputedVar = (T_COMPUTED_VAR)m_cbComputedVar.GetCurSel();
   }
   else
   {
      m_eComputedVar = E_SPEED;
   }

   //LLR> Prevent the corresponding edit window from being modified.
   m_editRPM.EnableWindow(m_eComputedVar != E_RPM);
   m_editRadius.EnableWindow(m_eComputedVar != E_RADIUS);
   m_editSpeed.EnableWindow(m_eComputedVar != E_SPEED);
}

// Method that updates the radial speed based upon the new RPM value.
void CMAEVADlg::OnKillFocusEditRPM()
{
   int     v_iLength;
   CString v_strTemp;
   LPTSTR  v_pEndPtr;

   // If the RPM value was modified, update the value of the radial speed.
   if (m_editRPM.GetModify())
   {
      v_iLength = m_editRPM.LineLength(m_editRPM.LineIndex(0));
      m_editRPM.GetLine(0, v_strTemp.GetBuffer(v_iLength), v_iLength);
      m_fRPM = strtod(v_strTemp, &v_pEndPtr);
      v_strTemp.Format(_T("%.1f"), m_fRPM);
      m_editRPM.SetWindowText(v_strTemp);
      v_strTemp.ReleaseBuffer(v_iLength);

      UpdateVariable();

      UpdateWindow();
   }
}

// Method that updates the radial speed based upon the new Radius value.
void CMAEVADlg::OnKillFocusEditRadius()
{
   int     v_iLength;
   CString v_strTemp;
   LPTSTR  v_pEndPtr;

   // If the Radius value was modified, update the value of the radial speed.
   if (m_editRadius.GetModify())
   {
      v_iLength = m_editRadius.LineLength(m_editRadius.LineIndex(0));
      m_editRadius.GetLine(0, v_strTemp.GetBuffer(v_iLength), v_iLength);
      m_fRadius = strtod(v_strTemp, &v_pEndPtr);
      v_strTemp.Format(_T("%.1f"), m_fRadius);
      m_editRadius.SetWindowText(v_strTemp);
      v_strTemp.ReleaseBuffer(v_iLength);

      UpdateVariable();

      UpdateWindow();
   }
}

void CMAEVADlg::OnKillFocusEditSpeed()
{
   int     v_iLength;
   CString v_strTemp;
   LPTSTR  v_pEndPtr;

   // If the Radius value was modified, update the value of the radial speed.
   if (m_editSpeed.GetModify())
   {
      v_iLength = m_editSpeed.LineLength(m_editRadius.LineIndex(0));
      m_editSpeed.GetLine(0, v_strTemp.GetBuffer(v_iLength), v_iLength);
      m_fSpeed = strtod(v_strTemp, &v_pEndPtr);
      v_strTemp.Format(_T("%.1f"), m_fSpeed);
      m_editSpeed.SetWindowText(v_strTemp);
      v_strTemp.ReleaseBuffer(v_iLength);

      UpdateVariable();

      UpdateWindow();
   }
}

// Method that updates the selected computed variable.
void CMAEVADlg::UpdateVariable(void)
{
   CString v_strTemp;

   // Compute the selected variable and update the edit window.
   switch(m_eComputedVar)
   {
   case E_RPM:
      m_fRPM = (m_fSpeed * 60.0f) / (2 * PI * m_fRadius);
      v_strTemp.Format(_T("%.1f"), m_fRPM);
      m_editRPM.SetWindowText(v_strTemp);
      break;
   case E_RADIUS:
      m_fRadius = (m_fSpeed * 60.0f) / (2 * PI * m_fRPM);
      v_strTemp.Format(_T("%.1f"), m_fRadius);
      m_editRadius.SetWindowText(v_strTemp);
      break;
   case E_SPEED:
   default:
      m_fSpeed = (2 * PI * m_fRadius * m_fRPM) / 60.0f;
      v_strTemp.Format(_T("%.1f"), m_fSpeed);
      m_editSpeed.SetWindowText(v_strTemp);
   }
}

// Method that updates the Data Capture Length Setting.
void CMAEVADlg::OnCbnSelchangeComboCaptureLength()
{
   // Assign the selected item in the list to the new data capture length setting and
   // update the new maximum time value for the data capture

   if ((m_cbCaptureLength.GetCurSel() >= E_INFINITE        ) &&
       (m_cbCaptureLength.GetCurSel() <  E_MAX_CAPTURE_TIME)   )
   {
      m_uiCaptureTime = CAPTURE_LENGHT[m_cbCaptureLength.GetCurSel()];
   }
   else
   {
      m_cbCaptureLength.SetCurSel(INIT_CAPTURE_SEL);
      m_uiCaptureTime = CAPTURE_LENGHT[INIT_CAPTURE_SEL];
   }
}

// Method that updates the Data Range Setting.
void CMAEVADlg::OnCbnSelchangeComboDataRange()
{
   // Assign the selected item in the list to the new data range setting
   if ((m_cbDataRangeList.GetCurSel() >= E_RANGE_PLUS_MINUS_2G) &&
       (m_cbDataRangeList.GetCurSel() <= E_RANGE_PLUS_MINUS_16G)  )
   {
      m_eDataRangeSetting = (T_RANGE_G_SET)m_cbDataRangeList.GetCurSel();
   }
   else
   {
      m_eDataRangeSetting = E_RANGE_PLUS_MINUS_4G;
   }
}

// Method called when the "Scan Devices" Button is pressed
void CMAEVADlg::OnBnClickedButtonScan()
{
   BOOL v_bSUB20DevicesFound;
   int  v_iMsgResult;

   // Reset the SUB-20 Devices list
   m_lbSUB20DevicesList.ResetContent();

   // Find all the SUB-20 Devices on all the USB ports.
   v_bSUB20DevicesFound = FindAllSUB20Devices();

   // If at least one device is found update the display with the list
   if (v_bSUB20DevicesFound)
   {
      m_lbSUB20DevicesList.SetCurSel(0);
      m_buttonIdleTest.EnableWindow();
      m_buttonStartAcq.EnableWindow();
   }
   // Otherwise indicate that no devices were found
   else
   {
      m_buttonIdleTest.EnableWindow(FALSE);
      m_buttonStartAcq.EnableWindow(FALSE);
      v_iMsgResult = MessageBox(
         _T("Error: Could not find any SUB-20 devices."),
         _T("SUB-20 List Initialization")
         );
   }
}

// Method called when the "Idle Test" Button is pressed
void CMAEVADlg::OnBnClickedButtonIdleTest()
{
   int v_iMsgResult, v_iSPIResult;

   sub_handle v_shHandle = NULL;

   int  v_iSPIConfig;
   char v_acSPIBuffIn[SPI_BUFF_SIZE];
   char v_acSPIBuffOut[SPI_BUFF_SIZE];

   T_ACCEL_XFER_INFO v_sAccelInfo;
   T_ACCEL_DATA      v_sAccelData;

   LONGLONG       v_uiDataSize = 0;
   T_AXIS_CONV    v_sAxisConv;
   T_AXIS_DATA    v_sAxisData;
   unsigned short v_uiDataMask;
   double         v_fConvFactor = 0.0f;
   double         v_fAvgValueAxisX = 0.0f;
   double         v_fAvgValueAxisY = 0.0f;
   double         v_fAvgValueAxisZ = 0.0f;

   CString v_strTemp;

   // Update the display to disable data capture on the accelerometer
   m_lbSUB20DevicesList.EnableWindow(FALSE);
   m_buttonIdleTest.EnableWindow(FALSE);
   m_buttonStartAcq.EnableWindow(FALSE);

   // Get the handle on the selected SUB-20 Device
   v_shHandle = sub_open(GetSelectedSUB20Device());

   // Proceed with the Idle accelerator test only if the SUB-20 Device is still accessible.
   if (v_shHandle)
   {
      // Configure the selected SUB-20 Device with the following settings:
      // - Enable SPI,
      // - SPI Clock starting with a Falling Edge,
      // - MOSI is valid before SPI Clock Rising edge, i.e.: Leading - Setup,  Traling - Sample
      // - Most Significant Bit first, and
      // - SPI Clock running at 4 MHz
      v_iSPIConfig = SPI_ENABLE | SPI_CPOL_FALL | SPI_SETUP_SMPL | SPI_MSB_FIRST | SPI_CLK_4MHZ;
      if (sub_spi_config(v_shHandle, v_iSPIConfig, NULL) == SE_OK)
      {
         // Reset all the SPI Slave Select and buffers.
         v_acSPIBuffOut[0] = NULL;
         for (int v_iCount = E_SS0; v_iCount < E_MAX_SLAVE; v_iCount++)
         {
            v_iSPIResult = sub_spi_transfer(
                            v_shHandle,
                            v_acSPIBuffOut,
                            NULL,
                            1,
                            SS_CONF(v_iCount,SS_LO)
                            );
         }

         // Validate the Accelerometer connected on the SUB-20 Device by verifying its own Device ID.
         // Note: The Accelerometer is physically connected to the Slave Select 1 pin.
         v_sAccelInfo.sInfo.bRead          = TRUE;
         v_sAccelInfo.sInfo.bMultipleBytes = FALSE;
         v_sAccelInfo.sInfo.eRegAdr        = E_REG_DEVICE_ID;
         v_acSPIBuffOut[0] = v_sAccelInfo.cRawData;
         v_acSPIBuffOut[1] = NULL;
         v_iSPIResult = sub_spi_transfer(
                         v_shHandle,
                         v_acSPIBuffOut,
                         v_acSPIBuffIn,
                         ACCEL_READ_BUFF_SIZE,
                         SS_CONF(E_SS0,SS_LO)
                         );
         v_sAccelData.cRawData = v_acSPIBuffIn[1];

         // If the Accelerometer ID is confirmed and the acquisition file could be created,
         // start the acquisition thread.
         if ((v_sAccelData.cRawData == (char)ADXL345_DEVICE_ID) && (v_iSPIResult == SE_OK))
         {
            // Start the accelerometer
            if(StartAccelerometer(v_shHandle))
            {
               // Obtain the corresponding data mask and conversion factor as per the data range.
               switch (m_eDataRangeSetting)
               {
               case E_RANGE_PLUS_MINUS_2G:
                  v_uiDataMask = DATA_MASK_2G;
                  v_fConvFactor = (float)E_2G_VAL / (v_uiDataMask + 1);
                  break;
               case E_RANGE_PLUS_MINUS_4G:
                  v_uiDataMask = DATA_MASK_4G;
                  v_fConvFactor = (float)E_4G_VAL / (v_uiDataMask + 1);
                  break;
               case E_RANGE_PLUS_MINUS_8G:
                  v_uiDataMask = DATA_MASK_8G;
                  v_fConvFactor = (float)E_8G_VAL / (v_uiDataMask + 1);
                  break;
               case E_RANGE_PLUS_MINUS_16G:
                  v_uiDataMask = DATA_MASK_16G;
                  v_fConvFactor = (float)E_16G_VAL / (v_uiDataMask + 1);
                  break;
               default:
                  v_uiDataMask = 0xFFFF;
                  v_fConvFactor = 0.0f;
               }

               // Proceed with acquiring test data only if the accelerometer was started correctly.
               do
               {

                  // Read the FIFO Status register.
                  v_sAccelInfo.sInfo.bRead          = TRUE;
                  v_sAccelInfo.sInfo.bMultipleBytes = FALSE;
                  v_sAccelInfo.sInfo.eRegAdr        = E_REG_FIFO_STATUS;
                  v_acSPIBuffOut[0] = v_sAccelInfo.cRawData;
                  v_acSPIBuffOut[1] = NULL;

                  v_iSPIResult = sub_spi_transfer(
                                  v_shHandle,
                                  v_acSPIBuffOut,
                                  v_acSPIBuffIn,
                                  ACCEL_READ_BUFF_SIZE,
                                  SS_CONF(E_SS0,SS_LO)
                                  );

                  // Prepare the buffer to get all entries from the FIFO buffer at once.
                  v_sAccelData.cRawData = v_acSPIBuffIn[1];
                  if (v_sAccelData.sFIFOStatus.uiEntries > 0)
                  {
                     v_sAccelInfo.sInfo.bRead          = TRUE;
                     v_sAccelInfo.sInfo.bMultipleBytes = TRUE;
                     v_sAccelInfo.sInfo.eRegAdr        = E_REG_DATA_X0;
                     v_acSPIBuffOut[0] = v_sAccelInfo.cRawData;
                     for (unsigned int v_uiInc = 1; v_uiInc < ACCEL_MB_READ_BUFF_SIZE; v_uiInc++)
                     {
                        v_acSPIBuffOut[v_uiInc] = NULL;
                     }

                     // Do a multiple bytes read on DATA X0 register up to DATA Z1 register for data consistency for all entries.
                     for (unsigned int v_uiInc = 0; v_uiInc < v_sAccelData.sFIFOStatus.uiEntries; v_uiInc++)
                     {
                        v_iSPIResult = sub_spi_transfer(
                                          v_shHandle,
                                          v_acSPIBuffOut,
                                          v_acSPIBuffIn,
                                          ACCEL_MB_READ_BUFF_SIZE,
                                          SS_CONF(E_SS0,SS_LO)
                                          );
 
                        // Update the data size gathered.
                        v_uiDataSize++;

                        // Convert the current entry of the acquisiton buffer.
                        v_sAxisConv.sAxisInChar.acX[0] = v_acSPIBuffIn[1];
                        v_sAxisConv.sAxisInChar.acX[1] = v_acSPIBuffIn[2];
                        v_sAxisConv.sAxisInChar.acY[0] = v_acSPIBuffIn[3];
                        v_sAxisConv.sAxisInChar.acY[1] = v_acSPIBuffIn[4];
                        v_sAxisConv.sAxisInChar.acZ[0] = v_acSPIBuffIn[5];
                        v_sAxisConv.sAxisInChar.acZ[1] = v_acSPIBuffIn[6];

                        if ((v_sAxisConv.sAxisInUnsignedShort.uiX & (v_uiDataMask + 1)) == NULL)
                        {
                           v_sAxisData.fAxisX = (v_sAxisConv.sAxisInUnsignedShort.uiX & v_uiDataMask) * v_fConvFactor;
                        }
                        else
                        {
                           v_sAxisData.fAxisX = (~(v_sAxisConv.sAxisInUnsignedShort.uiX - 1) & v_uiDataMask) * -v_fConvFactor;
                        }
                        if ((v_sAxisConv.sAxisInUnsignedShort.uiY & (v_uiDataMask + 1)) == NULL)
                        {
                           v_sAxisData.fAxisY = (v_sAxisConv.sAxisInUnsignedShort.uiY & v_uiDataMask) * v_fConvFactor;
                        }
                        else
                        {
                           v_sAxisData.fAxisY = (~(v_sAxisConv.sAxisInUnsignedShort.uiY - 1) & v_uiDataMask) * -v_fConvFactor;
                        }
                        if ((v_sAxisConv.sAxisInUnsignedShort.uiZ & (v_uiDataMask + 1)) == NULL)
                        {
                           v_sAxisData.fAxisZ = (v_sAxisConv.sAxisInUnsignedShort.uiZ & v_uiDataMask) * v_fConvFactor;
                        }
                        else
                        {
                           v_sAxisData.fAxisZ = (~(v_sAxisConv.sAxisInUnsignedShort.uiZ - 1) & v_uiDataMask) * -v_fConvFactor;
                        }

                        // Compute the average value for all 3 axis once there are at least 2 data entries.
                        if (v_uiDataSize >= 2)
                        {
                           v_fAvgValueAxisX += v_sAxisData.fAxisX;
                           v_fAvgValueAxisX /= 2;
                           v_fAvgValueAxisY += v_sAxisData.fAxisY;
                           v_fAvgValueAxisY /= 2;
                           v_fAvgValueAxisZ += v_sAxisData.fAxisZ;
                           v_fAvgValueAxisZ /= 2;
                        }
                        // Otherwise, initialize the average value with the first data entry.
                        else
                        {
                           v_fAvgValueAxisX = v_sAxisData.fAxisX;
                           v_fAvgValueAxisY = v_sAxisData.fAxisY;
                           v_fAvgValueAxisZ = v_sAxisData.fAxisZ;
                        }
                     }
                  }
               }
               while ((v_uiDataSize < (OUTPUT_DATA_RATE_VALUE * 2)) && (v_iSPIResult == SE_OK));

               if (StopAccelerometer(v_shHandle))
               {
                  if (v_iSPIResult == SE_OK)
                  {
                     v_strTemp.Format(_T("%.1f"), v_fAvgValueAxisX);
                     m_staticXAxis.SetWindowText(v_strTemp);
                     v_strTemp.Format(_T("%.1f"), v_fAvgValueAxisY);
                     m_staticYAxis.SetWindowText(v_strTemp);
                     v_strTemp.Format(_T("%.1f"), v_fAvgValueAxisZ);
                     m_staticZAxis.SetWindowText(v_strTemp);

                     UpdateWindow();
                  }
                  // Otherwise, indicate a failure to complete the idle test.
                  else
                  {
                     v_iMsgResult = MessageBox(
                        _T("Error: Could not proceed with the accelerometer idle test properly.\r\n(Not enough data gathered)"),
                        _T("Data Acquisition Failed")
                        );
                  }
               }
               // Otherwise, indicate that the ADXL345 Accelerometer could not be stopped.
               else
               {
                  v_iMsgResult = MessageBox(
                     _T("Error: Could not stop the accelerometer."),
                     _T("Data Acquisition Failed")
                     );
               }

            }
            // Otherwise, indicate that the ADXL345 Accelerometer could not be started.
            else
            {
               v_iMsgResult = MessageBox(
                  _T("Error: Could not start the accelerometer."),
                  _T("Data Acquisition Failed")
                  );
            }
         }
         // Otherwise indicate that the SPI Device could not be configured.
         else
         {
            v_iMsgResult = MessageBox(
               _T("Error: The SPI port could not be configured."),
               _T("Accelerometer Identification")
               );
         }
      }
      // Otherwise indicate that the SPI Device could not be configured.
      else
      {
         v_iMsgResult = MessageBox(
            _T("Error: The SPI port could not be configured."),
            _T("Accelerometer Identification")
            );
      }

      // Close the handle on the SUB-20 Device
      sub_close(v_shHandle);
      v_shHandle = NULL;
   }
   // Otherwise indicate that the SUB-20 Device could not be accessed.
   else
   {
      v_iMsgResult = MessageBox(
         _T("Error: The SUB-20 device could not be accessed."),
         _T("SUB-20 Access Denied")
         );
   }

   // Update the display to enable data capture on the accelerometer
   m_lbSUB20DevicesList.EnableWindow();
   m_buttonIdleTest.EnableWindow();
   m_buttonStartAcq.EnableWindow();
}

// Method called when the "Start Data Acquisition" Button is pressed
void CMAEVADlg::OnBnClickedButtonStartAcq()
{
   int  v_iMsgResult, v_iSPIResult;
   BOOL v_bSuccess = FALSE;

   sub_handle v_shHandle = NULL;

   int  v_iSPIConfig;
   char v_acSPIBuffIn[SPI_BUFF_SIZE];
   char v_acSPIBuffOut[SPI_BUFF_SIZE];

   T_ACCEL_XFER_INFO v_sAccelInfo;
   T_ACCEL_DATA      v_sAccelData;

   // Reset the File Opened and the Accelerometer Started flags.
   m_bFileAlreadyOpened = FALSE;
   m_iFileOverrunEndPtr = 0;
   m_bAccelStarted      = FALSE;

   // Get the handle on the selected SUB-20 Device
   v_shHandle = sub_open(GetSelectedSUB20Device());

   // Pursue Data acquiring only if the SUB-20 Device is still accessible,
   if (v_shHandle)
   {
      m_shHandle = v_shHandle;
      // Configure the selected SUB-20 Device with the following settings:
      // - Enable SPI,
      // - SPI Clock starting with a Falling Edge,
      // - MOSI is valid before SPI Clock Rising edge, i.e.: Leading - Setup,  Traling - Sample
      // - Most Significant Bit first, and
      // - SPI Clock running at 4 MHz
      v_iSPIConfig = SPI_ENABLE | SPI_CPOL_FALL | SPI_SETUP_SMPL | SPI_MSB_FIRST | SPI_CLK_4MHZ;
      if (sub_spi_config(m_shHandle, v_iSPIConfig, NULL) == SE_OK)
      {
         // Validate the Accelerometer connected on the SUB-20 Device by verifying its own Device ID.
         // Note: The Accelerometer is physically connected to the Slave Select 1 pin.
         v_sAccelInfo.sInfo.bRead          = TRUE;
         v_sAccelInfo.sInfo.bMultipleBytes = FALSE;
         v_sAccelInfo.sInfo.eRegAdr        = E_REG_DEVICE_ID;
         v_acSPIBuffOut[0] = v_sAccelInfo.cRawData;
         v_acSPIBuffOut[1] = NULL;
         v_iSPIResult = sub_spi_transfer(
                         m_shHandle,
                         v_acSPIBuffOut,
                         v_acSPIBuffIn,
                         ACCEL_READ_BUFF_SIZE,
                         SS_CONF(E_SS0,SS_LO)
                         );
         v_sAccelData.cRawData = v_acSPIBuffIn[1];

         // If the Accelerometer ID is confirmed and the acquisition file could be created,
         // start the acquisition thread.
         if ((v_sAccelData.cRawData == (char)ADXL345_DEVICE_ID) && (v_iSPIResult == SE_OK))
         {
            // Create the timer queue.
            m_hTimerQueue = CreateTimerQueue();
            if (m_hTimerQueue)
            {
               // Reset the acquisition buffer 
               g_iBuffCurrIdx   = 0;
               g_eActBuff       = E_1ST_BUFF;
               g_bInactBuffBusy = FALSE;
               g_uiDataSize     = 0;
               g_uiOverrunOccNb = 0;
               g_iRefreshCount  = 0;
               
               for (int v_eBuff = E_1ST_BUFF; v_eBuff < E_MAX_BUFF_NB; v_eBuff++)
               {
                  for (unsigned int v_uiIndex = 0; v_uiIndex < ACQ_BUFF_SIZE; v_uiIndex++)
                  {
                     g_asAcqBuff[v_eBuff][v_uiIndex].bOverrun = FALSE;
                     for (unsigned int v_uiInc = 0; v_uiInc < ACCEL_DATA_SIZE_BY_AXIS; v_uiInc++)
                     {
                        g_asAcqBuff[v_eBuff][v_uiIndex].sData.acX[v_uiInc] = NULL;
                        g_asAcqBuff[v_eBuff][v_uiIndex].sData.acY[v_uiInc] = NULL;
                        g_asAcqBuff[v_eBuff][v_uiIndex].sData.acZ[v_uiInc] = NULL;
                     }
                  }
               }

               // Create a timer to call the acquisition timer routine.
               m_bAcquiringData = TRUE;
               v_bSuccess = CreateTimerQueueTimer(
                               &m_hTimer,
                               m_hTimerQueue,
                               (WAITORTIMERCALLBACK)AcquisitionTimer,
                               (PVOID)this,
                               TIMER_DUE_TIME_IN_MS,
                               TIMER_PERIOD_IN_MS,
                               WT_EXECUTEINTIMERTHREAD
                               );

               // If the timer is valid, start the accelerometer
               if (v_bSuccess)
               {
                  // Initialize and start the accelerometer
                  if (StartAccelerometer(m_shHandle))
                  {
                     // The acceleromete rwas started successfully, update the new display settings.
                     m_bAccelStarted = TRUE;

                     // Keep the acquisition procedure starting time.
                     m_timeAcqStart = CTime::GetCurrentTime();

                     // New display settings
                     m_lbSUB20DevicesList.EnableWindow(FALSE);
                     m_editSurfaceType.EnableWindow(FALSE);
                     m_cbStylusType.EnableWindow(FALSE);
                     m_editRPM.EnableWindow(FALSE);
                     m_editRadius.EnableWindow(FALSE);
                     m_editSpeed.EnableWindow(FALSE);
                     m_cbCaptureLength.EnableWindow(FALSE);
                     m_cbDataRangeList.EnableWindow(FALSE);
                     m_buttonIdleTest.EnableWindow(FALSE);
                     m_buttonStartAcq.EnableWindow(FALSE);
                     m_buttonStopAcq.EnableWindow();
                     m_buttonSaveData.EnableWindow(FALSE);

                     UpdateWindow();
                  }
                  // Otherwise, indicate that the ADXL345 Accelerometer could not be started.
                  else
                  {
                     m_bAcquiringData = FALSE;
                     v_iMsgResult = MessageBox(
                        _T("Error: Could not start the accelerometer."),
                        _T("Data Acquisition Failed")
                        );
                  }

               }
               // Otherwise, the timer could not be started, indicate the error
               else
               {
                  m_bAcquiringData = FALSE;
                  v_iMsgResult = MessageBox(
                     _T("Error: Could not start the acquisition timer."),
                     _T("Data Acquisition Failed")
                     );
               }
            }
            // Otherwise, indicate that the timer queue could not be created.
            else
            {
               v_iMsgResult = MessageBox(
                  _T("Error: Could not create the timer queue."),
                  _T("Data Acquisition Failed")
                  );
            }
         }
         // Otherwise an error was detected.
         else
         {
            v_iMsgResult = MessageBox(
               _T("Error: SPI device does not match the ADXL345 accelerometer ID."),
               _T("Accelerometer Identification")
               );
         }
      }
      // Otherwise indicate that the SPI Device could not be configured.
      else
      {
         v_iMsgResult = MessageBox(
            _T("Error: The SPI port could not be configured."),
            _T("Accelerometer Identification")
            );
      }
      // Close the handle on the SUB-20 Device, if the accelerometer was not started.
      if (!m_bAccelStarted)
      {
         sub_close(m_shHandle);
         m_shHandle = NULL;
      }
   }
   // Otherwise indicate that the SUB-20 Device could not be accessed.
   else
   {
      v_iMsgResult = MessageBox(
         _T("Error: The SUB-20 device could not be accessed."),
         _T("SUB-20 Access Denied")
         );
   }
}

// Method called when the "Stop Data Acquisition" Button is pressed
void CMAEVADlg::OnBnClickedButtonStopAcq()
{
   int         v_iMsgResult;
   CTimeSpan   v_timespanCurr;
   CString     v_strTemp;

   // Stop the acquisition process, it was not stopped successfully, show it.
   if (!StopAcquisition())
   {
      v_iMsgResult = MessageBox(
         _T("Error: The acquisition process could not be stopped properly."),
         _T("Data Acquisition Failed")
         );
   }

   // Update the new display
   m_lbSUB20DevicesList.EnableWindow();
   m_editSurfaceType.EnableWindow();
   m_cbStylusType.EnableWindow();
   m_editRPM.EnableWindow();
   m_editRadius.EnableWindow();
   m_editSpeed.EnableWindow();
   m_cbCaptureLength.EnableWindow();
   m_cbDataRangeList.EnableWindow();
   m_buttonIdleTest.EnableWindow();
   m_buttonStartAcq.EnableWindow();
   m_buttonStopAcq.EnableWindow(FALSE);

   // Get the acquisition time, data size and overrun number
   m_timeAcqStop   = CTime::GetCurrentTime();
   v_timespanCurr  = m_timeAcqStop - m_timeAcqStart;
   m_uiElapsedTime = v_timespanCurr.GetTotalSeconds();
   v_strTemp.Format(_T("  %d"), m_uiElapsedTime);
   m_staticElapsedTime.SetWindowText(v_strTemp);

   v_strTemp.Format(_T("  %d"), g_uiDataSize);
   m_staticDataSize.SetWindowText(v_strTemp);

   v_strTemp.Format(_T("  %d"), g_uiOverrunOccNb);
   m_staticOverrunNb.SetWindowText(v_strTemp);

   // Verify that some data was obtained.
   if (g_iBuffCurrIdx > 0)
   {
      // Copy the data from the active buffer to the acquisition file and
      // enable the data export if there was no error while acquiring data.
      if (WriteDataAcqBufferToFile(g_eActBuff, g_iBuffCurrIdx))
      {
         m_buttonSaveData.EnableWindow();
      }
      // Otherwise, indicate a problem with the data file
      else
      {
         v_iMsgResult = MessageBox(
            _T("Error: The data file was not created."),
            _T("Data File Manipulation Failed")
            );
      }
   }

   UpdateWindow();
}

// Method called when the "Show Real-Time Capture" Button is pressed
void CMAEVADlg::OnBnClickedButtonShowRtData()
{
   // TODO : ajoutez ici le code de votre gestionnaire de notification de contrôle
}

// Method called when the "Save Acquired Data" Button is pressed
void CMAEVADlg::OnBnClickedButtonSaveData()
{
   int          v_iMsgResult;
   CFile        v_sSourceFile, v_sDestFile;
   BOOL         v_bSuccess = FALSE;
   TCHAR        v_strFilters[] = _T("Data Acquisition Files (*.dat)|*.dat|All Files (*.*)|*.*||");
   char         v_acBufRead[256];
   CString      v_strFileName;
   unsigned int v_uiNameBegin, v_uiNameSize;

   // Open the acquisition file
   v_bSuccess = v_sSourceFile.Open(
                   _T("C:\\Temp\\AcqData.txt"),
                   CFile::modeRead       |
                   CFile::shareDenyWrite
                   );
   v_sSourceFile.SeekToBegin();

   // Create an Open File Dialog Window with ".dat" as the filter and the surface type as the name
   if (v_sSourceFile.Read(v_acBufRead, sizeof(v_acBufRead)))
   {
      v_strFileName = v_acBufRead;
      v_uiNameBegin = v_strFileName.Find(_T("Surface: ")) + 9;
      v_uiNameSize = v_strFileName.Find(_T("\r\nStylus: ")) - v_uiNameBegin;
      if ((v_uiNameBegin > 0) && (v_uiNameSize > 0))
      {
         v_strFileName = v_strFileName.Mid(v_uiNameBegin, v_uiNameSize);
         v_strFileName.Append(_T(".dat"));
      }
      else
      {
         v_strFileName = _T("Unknown.dat");
      }
   }
   else
   {
      v_strFileName = _T("Unknown.dat");
   }

   CFileDialog v_sFileDlg(
                  FALSE,
                  _T("dat"),
                  v_strFileName,
                  OFN_HIDEREADONLY     |
                  OFN_OVERWRITEPROMPT  |
                  OFN_NOTESTFILECREATE |
                  OFN_NOVALIDATE,
                  v_strFilters
                  );

   // Reset the pointer at the begining of the source file.
   v_sSourceFile.SeekToBegin();

   // Open the window for the user and make a copy on OK only
   if (v_bSuccess && (v_sFileDlg.DoModal() == IDOK))
   {
      // Create the file specified by the user.
      v_bSuccess &= v_sDestFile.Open(
                       v_sFileDlg.GetPathName(),
                       CFile::modeCreate     |
                       CFile::modeWrite      |
                       CFile::shareExclusive
                       );

      // Ensure that the files were opened
      if (v_bSuccess)
      {
         // Copy the content of the file
         BYTE  v_cIOBuffer[4096];
         DWORD v_dwReadCount = 0;
         do
         {
            v_dwReadCount = v_sSourceFile.Read(v_cIOBuffer, 4096);
            v_sDestFile.Write(v_cIOBuffer, v_dwReadCount);
         }
         while (v_dwReadCount > 0);

         // Close the files.
         v_sSourceFile.Close();
         v_sDestFile.Close();
      }
      // Otherwise, specify that the acquisiton file was not exported.
      else
      {
         v_iMsgResult = MessageBox(
            _T("Error: The acquisition data file was not exported."),
            _T("Data File Manipulation Failed")
            );
      }
   }
}

// Method called when the "Identify Surface" Button is pressed
void CMAEVADlg::OnBnClickedButtonEvalData()
{
   // TODO : ajoutez ici le code de votre gestionnaire de notification de contrôle
}

// Method called when the "Exit" Button is pressed
void CMAEVADlg::OnBnClickedButtonExit()
{
   BOOL v_bSuccess;

   // Stop the acquisition process.
   v_bSuccess = StopAcquisition();

   // Update the display to show that the Acquisition process was stopped.
   // Update the new display
   m_buttonIdleTest.EnableWindow(FALSE);
   m_buttonStartAcq.EnableWindow(FALSE);
   m_buttonStopAcq.EnableWindow(FALSE);
   m_buttonSaveData.EnableWindow(FALSE);
   m_buttonEvalData.EnableWindow(FALSE);

   // Close the application window.
   OnOK();
}

// Method that processes the WM_DATAACQ_IN_PROGRESS message from the Acquisition Process
LRESULT CMAEVADlg::OnTimerMsgDataAcqInProgress(WPARAM wParam,LPARAM lParam)
{
   CTime       v_timeCurr;
   CTimeSpan   v_timespanCurr; 
   CString     v_strTemp;

   // Update the display for the elapsed time, data size and overrun number.
   v_timeCurr      = CTime::GetCurrentTime();
   v_timespanCurr  = v_timeCurr - m_timeAcqStart;
   m_uiElapsedTime = v_timespanCurr.GetTotalSeconds();
   v_strTemp.Format(_T("  %d"), m_uiElapsedTime);
   m_staticElapsedTime.SetWindowText(v_strTemp);

   v_strTemp.Format(_T("  %d"), g_uiDataSize);
   m_staticDataSize.SetWindowText(v_strTemp);

   v_strTemp.Format(_T("  %d"), g_uiOverrunOccNb);
   m_staticOverrunNb.SetWindowText(v_strTemp);

   UpdateWindow();

   // Stop the acquisition process when the maximum time for the data capture has been reached.
   if ((m_cbCaptureLength.GetCurSel() != E_INFINITE) && (m_uiElapsedTime >= m_uiCaptureTime))
   {
      OnBnClickedButtonStopAcq();
   }

   return TRUE;
}

// Method that processes the WM_DATAACQ_BUFFER_FULL message from the Acquisition Process
LRESULT CMAEVADlg::OnTimerMsgDataAcqBufferFull(WPARAM wParam,LPARAM lParam)
{
   int  v_iMsgResult;
   BOOL v_bSuccess = TRUE;

   T_CURR_ACQ_BUFF v_eInactBuff;

   // If the inactive buffer is not already being emptied into a file, transfer it into a file.
   if (!g_bInactBuffBusy)
   {
      // Show that the inactive buffer is currently being emptied.
      g_bInactBuffBusy = TRUE;

      // Select the inactive buffer.
      if (g_eActBuff == E_1ST_BUFF)
      {
         v_eInactBuff = E_2ND_BUFF;
      }
      else
      {
         v_eInactBuff = E_1ST_BUFF;
      }

      // Write the full inactive buffer to the acquisition file
      v_bSuccess = WriteDataAcqBufferToFile(v_eInactBuff, ACQ_BUFF_SIZE);

      // If the file transfer wasn't a success indicate it
      if (!v_bSuccess)
      {
         // Update the display to show that the Acquisition process was stopped.
         OnBnClickedButtonStopAcq();

         // Indicate that the data acquisition was stopped due to a failure in the data transfer
         // into a file (wasn't fast enough).
         v_iMsgResult = MessageBox(
            _T("Error: The data file was not created."),
            _T("Data File Manipulation Failed")
            );
      }

      // Show that the inactive buffer was emptied.
      g_bInactBuffBusy = FALSE;
   }
   // Otherwise, stop acquiring new data and indicate an error
   else
   {
      // Update the display to show that the Acquisition process was stopped.
      OnBnClickedButtonStopAcq();

      // Indicate that the data acquisition was stopped due to a failure in the data transfer
      // into a file (wasn't fast enough).
      v_iMsgResult = MessageBox(
         _T("Error: Acquisition Buffer overflow."),
         _T("Data Acquisition Failed")
         );
   }

   return v_bSuccess;
}

// Method that processes the WM_DATAACQ_ACCESS_ERROR message from the Acquisition Process
LRESULT CMAEVADlg::OnTimerMsgDataAcqAccessError(WPARAM wParam,LPARAM lParam)
{
   // Update the display to show that the Acquisition process was stopped.
   OnBnClickedButtonStopAcq();

   // Indicate that the data acquisition was stopped due to the handle on
   // the accelerometer being closed or invalid.
   int v_iMsgResult = MessageBox(
      _T("Error: Cannot access the accelerometer on the SUB-20 device."),
      _T("Data Acquisition Failed")
      );

   return TRUE;
}

