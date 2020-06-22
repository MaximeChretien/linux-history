/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/init.c,v 1.3 2000/12/02 01:16:16 dawes Exp $ */
/*
 * Mode switching code (CRT1 section) for SiS 300/540/630/730/315/550/650/740
 * (Universal module for Linux kernel framebuffer, XFree86 4.x)
 *
 * Comments and changes marked with "TW" by Thomas Winischhofer <thomer@winischhofer.net>
 *
 * (Version : V 0.80      [ynlai] 04/12/98)
 */

/*
#ifdef WINCE_HEADER
*/
/*
#include "precomp.h"
*/
/*
#endif
*/
#include "init.h"

#ifdef TC
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#endif

#ifdef WIN2000
#include <miniport.h>
#include "dderror.h"
#include "devioctl.h"
#include "miniport.h"
#include "ntddvdeo.h"
#include "video.h"
#include "sisv.h"
#include "tools.h"
#endif

#ifdef SIS300
#include "300vtbl.h"
#endif
#ifdef SIS315H
#include "310vtbl.h"
#endif

#ifdef LINUX_XF86
BOOLEAN SiSBIOSSetMode(PSIS_HW_DEVICE_INFO HwDeviceExtension,
                       ScrnInfoPtr pScrn, DisplayModePtr mode);
#ifdef SISDUALHEAD /* TW: For dual head */
BOOLEAN SiSBIOSSetModeCRT1(PSIS_HW_DEVICE_INFO HwDeviceExtension,
                       ScrnInfoPtr pScrn, DisplayModePtr mode);
BOOLEAN SiSBIOSSetModeCRT2(PSIS_HW_DEVICE_INFO HwDeviceExtension,
                       ScrnInfoPtr pScrn, DisplayModePtr mode);
#endif /* dual head */
#endif /* linux_xf86 */

#ifndef LINUX_XF86
BOOLEAN SiSInit(PSIS_HW_DEVICE_INFO HwDeviceExtension);
#endif

#ifdef LINUX_XF86
BOOLEAN SiSSetMode(PSIS_HW_DEVICE_INFO HwDeviceExtension,
                   ScrnInfoPtr pScrn,USHORT ModeNo);
#else
BOOLEAN SiSSetMode(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT ModeNo);
#endif

#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE,SiSSetMode)
#pragma alloc_text(PAGE,SiSInit)
#endif

void SiS_SetReg1(USHORT, USHORT, USHORT);
void SiS_SetReg2(USHORT, USHORT, USHORT);
void SiS_SetReg3(USHORT, USHORT);
void SiS_SetReg4(USHORT, ULONG);
UCHAR SiS_GetReg1(USHORT, USHORT);
UCHAR SiS_GetReg2(USHORT);
ULONG SiS_GetReg3(USHORT);
void SiS_ClearDAC(ULONG);

void DelaySeconds(int seconds);
void DebugCode(UCHAR code);

#ifdef LINUX_XF86
/* TW: Mode table for X driver */
UShort  ModeIndex_640x480[]   = {0x2E, 0x44, 0x45, 0x62};
UShort  ModeIndex_720x480[]   = {0x31, 0x33, 0x00, 0x35};
UShort  ModeIndex_720x576[]   = {0x32, 0x34, 0x00, 0x36};
UShort  ModeIndex_800x600[]   = {0x30, 0x47, 0x48, 0x63};
UShort  ModeIndex_1024x768[]  = {0x38, 0x4A, 0x4B, 0x64};
UShort  ModeIndex_1280x1024[] = {0x3A, 0x4D, 0x4E, 0x65};
UShort  ModeIndex_1280x960[]  = {0x7C, 0x7D, 0x00, 0x7E};
UShort  ModeIndex_1600x1200[] = {0x3C, 0x3D, 0x3E, 0x66};
UShort  ModeIndex_1920x1440[] = {0x68, 0x69, 0x6A, 0x6B};
#endif

void
DelaySeconds(int seconds)
{
  int i;
#ifdef WIN2000
  int j;
#endif

  for (i=0;i<seconds;i++)
  {
#ifdef TC
    delay(1000);
#endif

#ifdef WIN2000
    for (j=0;j<20000;j++)
      VideoPortStallExecution(50);
#endif

#ifdef WINCE_HEADER
#endif

#ifdef LINUX_KERNEL
#endif
  }
}

void
DebugCode(UCHAR code)
{
  OutPortByte ( 0x80, code);
  /*OutPortByte ( 0x300, code);*/
  DelaySeconds(0x3);
}

#ifdef SIS300
void
InitTo300Pointer(void)
{
   SiS_SModeIDTable = (SiS_StStruct *) SiS300_SModeIDTable;
   SiS_VBModeIDTable = (SiS_VBModeStruct *) SiS300_VBModeIDTable;	/*add for 300 oem util*/
   SiS_StandTable = (SiS_StandTableStruct *) SiS300_StandTable;
   SiS_EModeIDTable = (SiS_ExtStruct *) SiS300_EModeIDTable;
   SiS_RefIndex = (SiS_Ext2Struct *) SiS300_RefIndex;
   SiS_CRT1Table = (SiS_CRT1TableStruct *) SiS300_CRT1Table;
   SiS_MCLKData = (SiS_MCLKDataStruct *) SiS300_MCLKData;
   SiS_ECLKData = (SiS_ECLKDataStruct *) SiS300_ECLKData;
   SiS_VCLKData = (SiS_VCLKDataStruct *) SiS300_VCLKData;
   SiS_VBVCLKData = (SiS_VBVCLKDataStruct *) SiS300_VCLKData;
   SiS_ScreenOffset = SiS300_ScreenOffset;
   SiS_StResInfo = (SiS_StResInfoStruct *) SiS300_StResInfo;
   SiS_ModeResInfo = (SiS_ModeResInfoStruct *) SiS300_ModeResInfo;

   pSiS_OutputSelect = &SiS300_OutputSelect;
   pSiS_SoftSetting = &SiS300_SoftSetting;
   pSiS_SR07 = &SiS300_SR07;
   SiS_SR15 = SiS300_SR15;
   SiS_CR40 = SiS300_CR40;
   SiS_CR49 = SiS300_CR49;
   pSiS_SR1F = &SiS300_SR1F;
   pSiS_SR21 = &SiS300_SR21;
   pSiS_SR22 = &SiS300_SR22;
   pSiS_SR23 = &SiS300_SR23;
   pSiS_SR24 = &SiS300_SR24;
   SiS_SR25 = SiS300_SR25;
   pSiS_SR31 = &SiS300_SR31;
   pSiS_SR32 = &SiS300_SR32;
   pSiS_SR33 = &SiS300_SR33;
   pSiS_CRT2Data_1_2 = &SiS300_CRT2Data_1_2;
   pSiS_CRT2Data_4_D = &SiS300_CRT2Data_4_D;
   pSiS_CRT2Data_4_E = &SiS300_CRT2Data_4_E;
   pSiS_CRT2Data_4_10 = &SiS300_CRT2Data_4_10;
   pSiS_RGBSenseData = &SiS300_RGBSenseData;
   pSiS_VideoSenseData = &SiS300_VideoSenseData;
   pSiS_YCSenseData = &SiS300_YCSenseData;
   pSiS_RGBSenseData2 = &SiS300_RGBSenseData2;
   pSiS_VideoSenseData2 = &SiS300_VideoSenseData2;
   pSiS_YCSenseData2 = &SiS300_YCSenseData2;

   SiS_NTSCPhase = SiS300_NTSCPhase;
   SiS_PALPhase = SiS300_PALPhase;
   SiS_NTSCPhase2 = SiS300_NTSCPhase2;
   SiS_PALPhase2 = SiS300_PALPhase2;
   SiS_PALMPhase = SiS300_PALMPhase;  /*add for PALMN*/
   SiS_PALNPhase = SiS300_PALNPhase;
   
   SiS_StLCD1024x768Data = (SiS_LCDDataStruct *) SiS300_StLCD1024x768Data;
   SiS_ExtLCD1024x768Data = (SiS_LCDDataStruct *) SiS300_ExtLCD1024x768Data;
   SiS_St2LCD1024x768Data = (SiS_LCDDataStruct *) SiS300_St2LCD1024x768Data;
   SiS_StLCD1280x1024Data = (SiS_LCDDataStruct *) SiS300_StLCD1280x1024Data;
   SiS_ExtLCD1280x1024Data = (SiS_LCDDataStruct *) SiS300_ExtLCD1280x1024Data;
   SiS_St2LCD1280x1024Data = (SiS_LCDDataStruct *) SiS300_St2LCD1280x1024Data;
   SiS_NoScaleData = (SiS_LCDDataStruct *) SiS300_NoScaleData;
   SiS_LCD1280x960Data = (SiS_LCDDataStruct *) SiS300_LCD1280x960Data;
   SiS_StPALData = (SiS_TVDataStruct *) SiS300_StPALData;
   SiS_ExtPALData = (SiS_TVDataStruct *) SiS300_ExtPALData;
   SiS_StNTSCData = (SiS_TVDataStruct *) SiS300_StNTSCData;
   SiS_ExtNTSCData = (SiS_TVDataStruct *) SiS300_ExtNTSCData;
   SiS_St1HiTVData = (SiS_TVDataStruct *) SiS300_St1HiTVData;
   SiS_St2HiTVData = (SiS_TVDataStruct *) SiS300_St2HiTVData;
   SiS_ExtHiTVData = (SiS_TVDataStruct *) SiS300_ExtHiTVData;
   SiS_NTSCTiming = SiS300_NTSCTiming;
   SiS_PALTiming = SiS300_PALTiming;
   SiS_HiTVSt1Timing = SiS300_HiTVSt1Timing;
   SiS_HiTVSt2Timing = SiS300_HiTVSt2Timing;
   SiS_HiTVTextTiming = SiS300_HiTVTextTiming;
   SiS_HiTVGroup3Data = SiS300_HiTVGroup3Data;
   SiS_HiTVGroup3Simu = SiS300_HiTVGroup3Simu;
   SiS_HiTVGroup3Text = SiS300_HiTVGroup3Text;

   SiS_PanelDelayTbl = (SiS_PanelDelayTblStruct *) SiS300_PanelDelayTbl;
   SiS_LVDS800x600Data_1 = (SiS_LVDSDataStruct *) SiS300_LVDS800x600Data_1;
   SiS_LVDS800x600Data_2 = (SiS_LVDSDataStruct *) SiS300_LVDS800x600Data_2;
   SiS_LVDS1024x768Data_1 = (SiS_LVDSDataStruct *) SiS300_LVDS1024x768Data_1;
   SiS_LVDS1024x768Data_2 = (SiS_LVDSDataStruct *) SiS300_LVDS1024x768Data_2;
   SiS_LVDS1280x1024Data_1 = (SiS_LVDSDataStruct *) SiS300_LVDS1280x1024Data_1;
   SiS_LVDS1280x1024Data_2 = (SiS_LVDSDataStruct *) SiS300_LVDS1280x1024Data_2;
   SiS_LVDS640x480Data_1 = (SiS_LVDSDataStruct *) SiS300_LVDS640x480Data_1;
   SiS_CHTVUNTSCData = (SiS_LVDSDataStruct *) SiS300_CHTVUNTSCData;
   SiS_CHTVONTSCData = (SiS_LVDSDataStruct *) SiS300_CHTVONTSCData;
   SiS_CHTVUPALData = (SiS_LVDSDataStruct *) SiS300_CHTVUPALData;
   SiS_CHTVOPALData = (SiS_LVDSDataStruct *) SiS300_CHTVOPALData;
   SiS_PanelType00_1 = (SiS_LVDSDesStruct *) SiS300_PanelType00_1;
   SiS_PanelType01_1 = (SiS_LVDSDesStruct *) SiS300_PanelType01_1;
   SiS_PanelType02_1 = (SiS_LVDSDesStruct *) SiS300_PanelType02_1;
   SiS_PanelType03_1 = (SiS_LVDSDesStruct *) SiS300_PanelType03_1;
   SiS_PanelType04_1 = (SiS_LVDSDesStruct *) SiS300_PanelType04_1;
   SiS_PanelType05_1 = (SiS_LVDSDesStruct *) SiS300_PanelType05_1;
   SiS_PanelType06_1 = (SiS_LVDSDesStruct *) SiS300_PanelType06_1;
   SiS_PanelType07_1 = (SiS_LVDSDesStruct *) SiS300_PanelType07_1;
   SiS_PanelType08_1 = (SiS_LVDSDesStruct *) SiS300_PanelType08_1;
   SiS_PanelType09_1 = (SiS_LVDSDesStruct *) SiS300_PanelType09_1;
   SiS_PanelType0a_1 = (SiS_LVDSDesStruct *) SiS300_PanelType0a_1;
   SiS_PanelType0b_1 = (SiS_LVDSDesStruct *) SiS300_PanelType0b_1;
   SiS_PanelType0c_1 = (SiS_LVDSDesStruct *) SiS300_PanelType0c_1;
   SiS_PanelType0d_1 = (SiS_LVDSDesStruct *) SiS300_PanelType0d_1;
   SiS_PanelType0e_1 = (SiS_LVDSDesStruct *) SiS300_PanelType0e_1;
   SiS_PanelType0f_1 = (SiS_LVDSDesStruct *) SiS300_PanelType0f_1;
   SiS_PanelType00_2 = (SiS_LVDSDesStruct *) SiS300_PanelType00_2;
   SiS_PanelType01_2 = (SiS_LVDSDesStruct *) SiS300_PanelType01_2;
   SiS_PanelType02_2 = (SiS_LVDSDesStruct *) SiS300_PanelType02_2;
   SiS_PanelType03_2 = (SiS_LVDSDesStruct *) SiS300_PanelType03_2;
   SiS_PanelType04_2 = (SiS_LVDSDesStruct *) SiS300_PanelType04_2;
   SiS_PanelType05_2 = (SiS_LVDSDesStruct *) SiS300_PanelType05_2;
   SiS_PanelType06_2 = (SiS_LVDSDesStruct *) SiS300_PanelType06_2;
   SiS_PanelType07_2 = (SiS_LVDSDesStruct *) SiS300_PanelType07_2;
   SiS_PanelType08_2 = (SiS_LVDSDesStruct *) SiS300_PanelType08_2;
   SiS_PanelType09_2 = (SiS_LVDSDesStruct *) SiS300_PanelType09_2;
   SiS_PanelType0a_2 = (SiS_LVDSDesStruct *) SiS300_PanelType0a_2;
   SiS_PanelType0b_2 = (SiS_LVDSDesStruct *) SiS300_PanelType0b_2;
   SiS_PanelType0c_2 = (SiS_LVDSDesStruct *) SiS300_PanelType0c_2;
   SiS_PanelType0d_2 = (SiS_LVDSDesStruct *) SiS300_PanelType0d_2;
   SiS_PanelType0e_2 = (SiS_LVDSDesStruct *) SiS300_PanelType0e_2;
   SiS_PanelType0f_2 = (SiS_LVDSDesStruct *) SiS300_PanelType0f_2;
   SiS_CHTVUNTSCDesData = (SiS_LVDSDesStruct *) SiS300_CHTVUNTSCDesData;
   SiS_CHTVONTSCDesData = (SiS_LVDSDesStruct *) SiS300_CHTVONTSCDesData;
   SiS_CHTVUPALDesData = (SiS_LVDSDesStruct *) SiS300_CHTVUPALDesData;
   SiS_CHTVOPALDesData = (SiS_LVDSDesStruct *) SiS300_CHTVOPALDesData;
   SiS_LVDSCRT1800x600_1 = (SiS_LVDSCRT1DataStruct *) SiS300_LVDSCRT1800x600_1;
   SiS_LVDSCRT11024x768_1 = (SiS_LVDSCRT1DataStruct *) SiS300_LVDSCRT11024x768_1;
   SiS_LVDSCRT11280x1024_1 = (SiS_LVDSCRT1DataStruct *) SiS300_LVDSCRT11280x1024_1;
   SiS_LVDSCRT1800x600_1_H = (SiS_LVDSCRT1DataStruct *) SiS300_LVDSCRT1800x600_1_H;
   SiS_LVDSCRT11024x768_1_H = (SiS_LVDSCRT1DataStruct *) SiS300_LVDSCRT11024x768_1_H;
   SiS_LVDSCRT11280x1024_1_H = (SiS_LVDSCRT1DataStruct *) SiS300_LVDSCRT11280x1024_1_H;
   SiS_LVDSCRT1800x600_2 = (SiS_LVDSCRT1DataStruct *) SiS300_LVDSCRT1800x600_2;
   SiS_LVDSCRT11024x768_2 = (SiS_LVDSCRT1DataStruct *) SiS300_LVDSCRT11024x768_2;
   SiS_LVDSCRT11280x1024_2 = (SiS_LVDSCRT1DataStruct *) SiS300_LVDSCRT11280x1024_2;
   SiS_LVDSCRT1800x600_2_H = (SiS_LVDSCRT1DataStruct *) SiS300_LVDSCRT1800x600_2_H;
   SiS_LVDSCRT11024x768_2_H = (SiS_LVDSCRT1DataStruct *) SiS300_LVDSCRT11024x768_2_H;
   SiS_LVDSCRT11280x1024_2_H = (SiS_LVDSCRT1DataStruct *) SiS300_LVDSCRT11280x1024_2_H;
   SiS_CHTVCRT1UNTSC = (SiS_LVDSCRT1DataStruct *) SiS300_CHTVCRT1UNTSC;
   SiS_CHTVCRT1ONTSC = (SiS_LVDSCRT1DataStruct *) SiS300_CHTVCRT1ONTSC;
   SiS_CHTVCRT1UPAL = (SiS_LVDSCRT1DataStruct *) SiS300_CHTVCRT1UPAL;
   SiS_CHTVCRT1OPAL = (SiS_LVDSCRT1DataStruct *) SiS300_CHTVCRT1OPAL;
   SiS_CHTVReg_UNTSC = (SiS_CHTVRegDataStruct *) SiS300_CHTVReg_UNTSC;
   SiS_CHTVReg_ONTSC = (SiS_CHTVRegDataStruct *) SiS300_CHTVReg_ONTSC;
   SiS_CHTVReg_UPAL = (SiS_CHTVRegDataStruct *) SiS300_CHTVReg_UPAL;
   SiS_CHTVReg_OPAL = (SiS_CHTVRegDataStruct *) SiS300_CHTVReg_OPAL;
   SiS_CHTVVCLKUNTSC = SiS300_CHTVVCLKUNTSC;
   SiS_CHTVVCLKONTSC = SiS300_CHTVVCLKONTSC;
   SiS_CHTVVCLKUPAL = SiS300_CHTVVCLKUPAL;
   SiS_CHTVVCLKOPAL = SiS300_CHTVVCLKOPAL;
   /* 300 customization related */
}
#endif

#ifdef SIS315H
void
InitTo310Pointer(void)
{
   SiS_SModeIDTable = (SiS_StStruct *) SiS310_SModeIDTable;
   SiS_StandTable = (SiS_StandTableStruct *) SiS310_StandTable;
   SiS_EModeIDTable = (SiS_ExtStruct *) SiS310_EModeIDTable;
   SiS_RefIndex = (SiS_Ext2Struct *) SiS310_RefIndex;
   SiS_CRT1Table = (SiS_CRT1TableStruct *) SiS310_CRT1Table;
   SiS_MCLKData = (SiS_MCLKDataStruct *) SiS310_MCLKData;
   SiS_ECLKData = (SiS_ECLKDataStruct *) SiS310_ECLKData;
   SiS_VCLKData = (SiS_VCLKDataStruct *) SiS310_VCLKData;
   SiS_VBVCLKData = (SiS_VBVCLKDataStruct *) SiS310_VBVCLKData;
   SiS_ScreenOffset = SiS310_ScreenOffset;
   SiS_StResInfo = (SiS_StResInfoStruct *) SiS310_StResInfo;
   SiS_ModeResInfo = (SiS_ModeResInfoStruct *) SiS310_ModeResInfo;

   pSiS_OutputSelect = &SiS310_OutputSelect;
   pSiS_SoftSetting = &SiS310_SoftSetting;
   pSiS_SR07 = &SiS310_SR07;
   SiS_SR15 = SiS310_SR15;
   SiS_CR40 = SiS310_CR40;
   SiS_CR49 = SiS310_CR49;
   pSiS_SR1F = &SiS310_SR1F;
   pSiS_SR21 = &SiS310_SR21;
   pSiS_SR22 = &SiS310_SR22;
   pSiS_SR23 = &SiS310_SR23;
   pSiS_SR24 = &SiS310_SR24;
   SiS_SR25 = SiS310_SR25;
   pSiS_SR31 = &SiS310_SR31;
   pSiS_SR32 = &SiS310_SR32;
   pSiS_SR33 = &SiS310_SR33;
   pSiS_CRT2Data_1_2 = &SiS310_CRT2Data_1_2;
   pSiS_CRT2Data_4_D = &SiS310_CRT2Data_4_D;
   pSiS_CRT2Data_4_E = &SiS310_CRT2Data_4_E;
   pSiS_CRT2Data_4_10 = &SiS310_CRT2Data_4_10;
   pSiS_RGBSenseData = &SiS310_RGBSenseData;
   pSiS_VideoSenseData = &SiS310_VideoSenseData;
   pSiS_YCSenseData = &SiS310_YCSenseData;
   pSiS_RGBSenseData2 = &SiS310_RGBSenseData2;
   pSiS_VideoSenseData2 = &SiS310_VideoSenseData2;
   pSiS_YCSenseData2 = &SiS310_YCSenseData2;
   SiS_NTSCPhase = SiS310_NTSCPhase;
   SiS_PALPhase = SiS310_PALPhase;
   SiS_NTSCPhase2 = SiS310_NTSCPhase2;
   SiS_PALPhase2 = SiS310_PALPhase2;
   SiS_PALMPhase = SiS310_PALMPhase;  /*add for PALMN*/
   SiS_PALNPhase = SiS310_PALNPhase;

   SiS_StLCD1024x768Data = (SiS_LCDDataStruct *) SiS310_StLCD1024x768Data;
   SiS_ExtLCD1024x768Data = (SiS_LCDDataStruct *) SiS310_ExtLCD1024x768Data;
   SiS_St2LCD1024x768Data = (SiS_LCDDataStruct *) SiS310_St2LCD1024x768Data;
   SiS_StLCD1280x1024Data = (SiS_LCDDataStruct *) SiS310_StLCD1280x1024Data;
   SiS_ExtLCD1280x1024Data = (SiS_LCDDataStruct *) SiS310_ExtLCD1280x1024Data;
   SiS_St2LCD1280x1024Data = (SiS_LCDDataStruct *) SiS310_St2LCD1280x1024Data;
   SiS_NoScaleData = (SiS_LCDDataStruct *) SiS310_NoScaleData;
   SiS_LCD1280x960Data = (SiS_LCDDataStruct *) SiS310_LCD1280x960Data;
   SiS_StPALData = (SiS_TVDataStruct *) SiS310_StPALData;
   SiS_ExtPALData = (SiS_TVDataStruct *) SiS310_ExtPALData;
   SiS_StNTSCData = (SiS_TVDataStruct *) SiS310_StNTSCData;
   SiS_ExtNTSCData = (SiS_TVDataStruct *) SiS310_ExtNTSCData;
   SiS_St1HiTVData = (SiS_TVDataStruct *) SiS310_St1HiTVData;
   SiS_St2HiTVData = (SiS_TVDataStruct *) SiS310_St2HiTVData;
   SiS_ExtHiTVData = (SiS_TVDataStruct *) SiS310_ExtHiTVData;
   SiS_NTSCTiming = SiS310_NTSCTiming;
   SiS_PALTiming = SiS310_PALTiming;
   SiS_HiTVSt1Timing = SiS310_HiTVSt1Timing;
   SiS_HiTVSt2Timing = SiS310_HiTVSt2Timing;
   SiS_HiTVTextTiming = SiS310_HiTVTextTiming;
   SiS_HiTVGroup3Data = SiS310_HiTVGroup3Data;
   SiS_HiTVGroup3Simu = SiS310_HiTVGroup3Simu;
   SiS_HiTVGroup3Text = SiS310_HiTVGroup3Text;

   SiS_PanelDelayTbl = (SiS_PanelDelayTblStruct *) SiS310_PanelDelayTbl;
   SiS_LVDS320x480Data_1 = (SiS_LVDSDataStruct *) SiS310_LVDS320x480Data_1;
   SiS_LVDS800x600Data_1 = (SiS_LVDSDataStruct *) SiS310_LVDS800x600Data_1;
   SiS_LVDS800x600Data_2 = (SiS_LVDSDataStruct *) SiS310_LVDS800x600Data_2;
   SiS_LVDS1024x768Data_1 = (SiS_LVDSDataStruct *) SiS310_LVDS1024x768Data_1;
   SiS_LVDS1024x768Data_2 = (SiS_LVDSDataStruct *) SiS310_LVDS1024x768Data_2;
   SiS_LVDS1280x1024Data_1 = (SiS_LVDSDataStruct *) SiS310_LVDS1280x1024Data_1;
   SiS_LVDS1280x1024Data_2 = (SiS_LVDSDataStruct *) SiS310_LVDS1280x1024Data_2;
   SiS_LVDS640x480Data_1 = (SiS_LVDSDataStruct *) SiS310_LVDS640x480Data_1;
   SiS_CHTVUNTSCData = (SiS_LVDSDataStruct *) SiS310_CHTVUNTSCData;
   SiS_CHTVONTSCData = (SiS_LVDSDataStruct *) SiS310_CHTVONTSCData;
   SiS_CHTVUPALData = (SiS_LVDSDataStruct *) SiS310_CHTVUPALData;
   SiS_CHTVOPALData = (SiS_LVDSDataStruct *) SiS310_CHTVOPALData;
   SiS_PanelType00_1 = (SiS_LVDSDesStruct *) SiS310_PanelType00_1;
   SiS_PanelType01_1 = (SiS_LVDSDesStruct *) SiS310_PanelType01_1;
   SiS_PanelType02_1 = (SiS_LVDSDesStruct *) SiS310_PanelType02_1;
   SiS_PanelType03_1 = (SiS_LVDSDesStruct *) SiS310_PanelType03_1;
   SiS_PanelType04_1 = (SiS_LVDSDesStruct *) SiS310_PanelType04_1;
   SiS_PanelType05_1 = (SiS_LVDSDesStruct *) SiS310_PanelType05_1;
   SiS_PanelType06_1 = (SiS_LVDSDesStruct *) SiS310_PanelType06_1;
   SiS_PanelType07_1 = (SiS_LVDSDesStruct *) SiS310_PanelType07_1;
   SiS_PanelType08_1 = (SiS_LVDSDesStruct *) SiS310_PanelType08_1;
   SiS_PanelType09_1 = (SiS_LVDSDesStruct *) SiS310_PanelType09_1;
   SiS_PanelType0a_1 = (SiS_LVDSDesStruct *) SiS310_PanelType0a_1;
   SiS_PanelType0b_1 = (SiS_LVDSDesStruct *) SiS310_PanelType0b_1;
   SiS_PanelType0c_1 = (SiS_LVDSDesStruct *) SiS310_PanelType0c_1;
   SiS_PanelType0d_1 = (SiS_LVDSDesStruct *) SiS310_PanelType0d_1;
   SiS_PanelType0e_1 = (SiS_LVDSDesStruct *) SiS310_PanelType0e_1;
   SiS_PanelType0f_1 = (SiS_LVDSDesStruct *) SiS310_PanelType0f_1;
   SiS_PanelType00_2 = (SiS_LVDSDesStruct *) SiS310_PanelType00_2;
   SiS_PanelType01_2 = (SiS_LVDSDesStruct *) SiS310_PanelType01_2;
   SiS_PanelType02_2 = (SiS_LVDSDesStruct *) SiS310_PanelType02_2;
   SiS_PanelType03_2 = (SiS_LVDSDesStruct *) SiS310_PanelType03_2;
   SiS_PanelType04_2 = (SiS_LVDSDesStruct *) SiS310_PanelType04_2;
   SiS_PanelType05_2 = (SiS_LVDSDesStruct *) SiS310_PanelType05_2;
   SiS_PanelType06_2 = (SiS_LVDSDesStruct *) SiS310_PanelType06_2;
   SiS_PanelType07_2 = (SiS_LVDSDesStruct *) SiS310_PanelType07_2;
   SiS_PanelType08_2 = (SiS_LVDSDesStruct *) SiS310_PanelType08_2;
   SiS_PanelType09_2 = (SiS_LVDSDesStruct *) SiS310_PanelType09_2;
   SiS_PanelType0a_2 = (SiS_LVDSDesStruct *) SiS310_PanelType0a_2;
   SiS_PanelType0b_2 = (SiS_LVDSDesStruct *) SiS310_PanelType0b_2;
   SiS_PanelType0c_2 = (SiS_LVDSDesStruct *) SiS310_PanelType0c_2;
   SiS_PanelType0d_2 = (SiS_LVDSDesStruct *) SiS310_PanelType0d_2;
   SiS_PanelType0e_2 = (SiS_LVDSDesStruct *) SiS310_PanelType0e_2;
   SiS_PanelType0f_2 = (SiS_LVDSDesStruct *) SiS310_PanelType0f_2;
   /*301b*/
   LVDS1024x768Des_1= (SiS_LVDSDesStruct *) SiS310_PanelType1076_1;
   LVDS1280x1024Des_1= (SiS_LVDSDesStruct *)SiS310_PanelType1210_1;
   LVDS1280x960Des_1= (SiS_LVDSDesStruct *)SiS310_PanelType1296_1 ;
   LVDS1024x768Des_2= (SiS_LVDSDesStruct *) SiS310_PanelType1076_2;
   LVDS1280x1024Des_2= (SiS_LVDSDesStruct *) SiS310_PanelType1210_2;
   LVDS1280x960Des_2= (SiS_LVDSDesStruct *) SiS310_PanelType1296_2;
   /*end 301b*/
   
   SiS_CHTVUNTSCDesData = (SiS_LVDSDesStruct *) SiS310_CHTVUNTSCDesData;
   SiS_CHTVONTSCDesData = (SiS_LVDSDesStruct *) SiS310_CHTVONTSCDesData;
   SiS_CHTVUPALDesData = (SiS_LVDSDesStruct *) SiS310_CHTVUPALDesData;
   SiS_CHTVOPALDesData = (SiS_LVDSDesStruct *) SiS310_CHTVOPALDesData;
   SiS_LVDSCRT1800x600_1 = (SiS_LVDSCRT1DataStruct *) SiS310_LVDSCRT1800x600_1;
   SiS_LVDSCRT11024x768_1 = (SiS_LVDSCRT1DataStruct *) SiS310_LVDSCRT11024x768_1;
   SiS_LVDSCRT11280x1024_1 = (SiS_LVDSCRT1DataStruct *) SiS310_LVDSCRT11280x1024_1;
   SiS_LVDSCRT1800x600_1_H = (SiS_LVDSCRT1DataStruct *) SiS310_LVDSCRT1800x600_1_H;
   SiS_LVDSCRT11024x768_1_H = (SiS_LVDSCRT1DataStruct *) SiS310_LVDSCRT11024x768_1_H;
   SiS_LVDSCRT11280x1024_1_H = (SiS_LVDSCRT1DataStruct *) SiS310_LVDSCRT11280x1024_1_H;
   SiS_LVDSCRT1800x600_2 = (SiS_LVDSCRT1DataStruct *) SiS310_LVDSCRT1800x600_2;
   SiS_LVDSCRT11024x768_2 = (SiS_LVDSCRT1DataStruct *) SiS310_LVDSCRT11024x768_2;
   SiS_LVDSCRT11280x1024_2 = (SiS_LVDSCRT1DataStruct *) SiS310_LVDSCRT11280x1024_2;
   SiS_LVDSCRT1800x600_2_H = (SiS_LVDSCRT1DataStruct *) SiS310_LVDSCRT1800x600_2_H;
   SiS_LVDSCRT11024x768_2_H = (SiS_LVDSCRT1DataStruct *) SiS310_LVDSCRT11024x768_2_H;
   SiS_LVDSCRT11280x1024_2_H = (SiS_LVDSCRT1DataStruct *) SiS310_LVDSCRT11280x1024_2_H;
   SiS_LVDSCRT1320x480_1 = (SiS_LVDSCRT1DataStruct *) SiS310_LVDSCRT1320x480_1;   /*fstn*/
   SiS_CHTVCRT1UNTSC = (SiS_LVDSCRT1DataStruct *) SiS310_CHTVCRT1UNTSC;
   SiS_CHTVCRT1ONTSC = (SiS_LVDSCRT1DataStruct *) SiS310_CHTVCRT1ONTSC;
   SiS_CHTVCRT1UPAL = (SiS_LVDSCRT1DataStruct *) SiS310_CHTVCRT1UPAL;
   SiS_CHTVCRT1OPAL = (SiS_LVDSCRT1DataStruct *) SiS310_CHTVCRT1OPAL;
   SiS_CHTVReg_UNTSC = (SiS_CHTVRegDataStruct *) SiS310_CHTVReg_UNTSC;
   SiS_CHTVReg_ONTSC = (SiS_CHTVRegDataStruct *) SiS310_CHTVReg_ONTSC;
   SiS_CHTVReg_UPAL = (SiS_CHTVRegDataStruct *) SiS310_CHTVReg_UPAL;
   SiS_CHTVReg_OPAL = (SiS_CHTVRegDataStruct *) SiS310_CHTVReg_OPAL;
   /*add for LCDA*/
   SiS_LCDACRT1800x600_1 = (SiS_LCDACRT1DataStruct *) SiS310_LCDACRT1800x600_1;
   SiS_LCDACRT11024x768_1 = (SiS_LCDACRT1DataStruct *) SiS310_LCDACRT11024x768_1;
   SiS_LCDACRT11280x1024_1 = (SiS_LCDACRT1DataStruct *) SiS310_LCDACRT11280x1024_1;
   SiS_LCDACRT1800x600_1_H = (SiS_LCDACRT1DataStruct *) SiS310_LCDACRT1800x600_1_H;
   SiS_LCDACRT11024x768_1_H = (SiS_LCDACRT1DataStruct *) SiS310_LCDACRT11024x768_1_H;
   SiS_LCDACRT11280x1024_1_H = (SiS_LCDACRT1DataStruct *) SiS310_LCDACRT11280x1024_1_H;
   SiS_LCDACRT1800x600_2 = (SiS_LCDACRT1DataStruct *) SiS310_LCDACRT1800x600_2;
   SiS_LCDACRT11024x768_2 = (SiS_LCDACRT1DataStruct *) SiS310_LCDACRT11024x768_2;
   SiS_LCDACRT11280x1024_2 = (SiS_LCDACRT1DataStruct *) SiS310_LCDACRT11280x1024_2;
   SiS_LCDACRT1800x600_2_H = (SiS_LCDACRT1DataStruct *) SiS310_LCDACRT1800x600_2_H;
   SiS_LCDACRT11024x768_2_H = (SiS_LCDACRT1DataStruct *) SiS310_LCDACRT11024x768_2_H;
   SiS_LCDACRT11280x1024_2_H = (SiS_LCDACRT1DataStruct *) SiS310_LCDACRT11280x1024_2_H;
   /*end for 301b*/

   SiS_CHTVVCLKUNTSC = SiS310_CHTVVCLKUNTSC;
   SiS_CHTVVCLKONTSC = SiS310_CHTVVCLKONTSC;
   SiS_CHTVVCLKUPAL = SiS310_CHTVVCLKUPAL;
   SiS_CHTVVCLKOPAL = SiS310_CHTVVCLKOPAL;
   /* 310 customization related */
}
#endif

#ifndef LINUX_XF86
/* -------------- SiSInit -----------------*/
BOOLEAN
SiSInit(PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
   ULONG   ROMAddr  = (ULONG)HwDeviceExtension->pjVirtualRomBase;
   ULONG   FBAddr   = (ULONG)HwDeviceExtension->pjVideoMemoryAddress;
   USHORT  BaseAddr = (USHORT)HwDeviceExtension->ulIOAddress;
   UCHAR   i,temp=0;
   UCHAR   SR11;
#ifdef LINUX_KERNEL
   UCHAR   temp1;
   ULONG   base;
#endif
   UCHAR   SR12=0,SR13=0,SR14=0,SR16=0,SR17=0,SR18=0,SR19=0,SR1A=0;

#ifdef SIS315H
   UCHAR   CR37=0,CR38=0,CR79=0,CR7A=0,CR7B=0,CR7C=0;
   UCHAR   SR1B=0,SR15=0;
   PSIS_DSReg  pSR;
   ULONG   Temp;
#endif
   UCHAR   VBIOSVersion[5];

   if(FBAddr==0)    return (FALSE);
   if(BaseAddr==0)  return (FALSE);

   SiS_SetReg3((USHORT)(BaseAddr+0x12),  0x67);  /* 3c2 <- 67 ,ynlai */

#ifdef SIS315H
   /*if(HwDeviceExtension->jChipType > SIS_315H)*/
   if(HwDeviceExtension->jChipType > SIS_315PRO) {
     if(!HwDeviceExtension->bIntegratedMMEnabled)
     	return (FALSE);  /* alan  */
   }
#endif

   SiS_MemoryCopy(VBIOSVersion,HwDeviceExtension->szVBIOSVer,4);
   VBIOSVersion[4]= 0x0;
   /* 09/07/99 modify by domao */

#ifdef SIS315H
   if((HwDeviceExtension->jChipType == SIS_315H)||
      (HwDeviceExtension->jChipType == SIS_315PRO)||
      (HwDeviceExtension->jChipType == SIS_550)||   /* 05/02/01 ynlai for 550 */
      (HwDeviceExtension->jChipType == SIS_640)||   /* 08/20/01 chiawen for 640/740 */
      (HwDeviceExtension->jChipType == SIS_740)||   /* 09/03/01 chiawen for 640/740 */
      (HwDeviceExtension->jChipType == SIS_650))    /* 09/03/01 chiawen for 650 */
     InitTo310Pointer();
#endif

#ifdef SIS300
   if ((HwDeviceExtension->jChipType == SIS_540)||
       (HwDeviceExtension->jChipType == SIS_630)||
       (HwDeviceExtension->jChipType == SIS_730)||
       (HwDeviceExtension->jChipType == SIS_300))
     InitTo300Pointer();
#endif

   /* TW: Set SiS Register globals */
   SiSRegInit(BaseAddr);

   /* TW: Determine LVDS/CH7005/TRUMPION */
   SiS_Set_LVDS_TRUMPION(HwDeviceExtension);

   SiS_SetReg1(SiS_P3c4,0x05,0x86);                     /* 1.Openkey - unlock registers */

#ifdef LINUX_KERNEL

#ifdef SIS300                                         	/* add to set SR14*/
   if((HwDeviceExtension->jChipType==SIS_540)||
      (HwDeviceExtension->jChipType==SIS_630)||
      (HwDeviceExtension->jChipType==SIS_730)) {
     base=0x80000060;
     OutPortLong(base,0xcf8);
     temp1=InPortLong(0xcfc);
     temp1=temp1>>(16+8+4);
     temp1=temp1&(0x07);
     temp1=temp1+1;
     temp1=1<<temp1;
     SR14=temp1-1;
     base=0x80000064;
     OutPortLong(base,0xcf8);
     temp1=InPortLong(0xcfc);
     temp1=temp1&(0x00000020);
     if(temp1)
      	SR14=(0x10000000)|SR14;
     else
      	SR14=(0x01000000)|SR14;
   }
#endif

#ifdef SIS315H                                         /* add to set SR14*/
   if(HwDeviceExtension->jChipType==SIS_550) {
     base=0x80000060;
     OutPortLong(base,0xcf8);
     temp1=InPortLong(0xcfc);
     temp1=temp1>>(16+8+4);
     temp1=temp1&(0x07);
     temp1=temp1+1;
     temp1=1<<temp1;
     SR14=temp1-1;
     base=0x80000064;
     OutPortLong(base,0xcf8);
     temp1=InPortLong(0xcfc);
     temp1=temp1&(0x00000020);
     if(temp1)
      SR14=(0x10000000)|SR14;
     else
      SR14=(0x01000000)|SR14;
   }

   if((HwDeviceExtension->jChipType == SIS_640)||   /* 08/20/01 chiawen for 640/740 */
      (HwDeviceExtension->jChipType == SIS_740)||   /* 09/03/01 chiawen for 640/740 */
      (HwDeviceExtension->jChipType == SIS_650)) {  /* 09/03/01 chiawen for 650 */
     base=0x80000064;
     OutPortLong(base,0xcf8);
     temp1=InPortLong(0xcfc);
     temp1=temp>>4;
     temp1=temp1&(0x07);
     if(temp1 > 2 ) {
       temp = temp1;
       switch(temp) {
        case 3: temp1=0x07;  break;
        case 4: temp1=0x0F;  break;
        case 5: temp1=0x1F;  break;
        case 6: temp1=0x05;  break;
        case 7: temp1=0x17;  break;
        case 8: break;
        case 9: break;
       }
     }
     SR14=temp1;
     base=0x8000007C;
     OutPortLong(base,0xcf8);
     temp1=InPortLong(0xcfc);
     temp1=temp1&(0x00000020);
     if(temp1)
       SR14=(0x10000000)|SR14;
   }
#endif

#endif  /* Linux kernel */

#ifdef SIS300
   if((HwDeviceExtension->jChipType == SIS_540)||
      (HwDeviceExtension->jChipType == SIS_630)||
      (HwDeviceExtension->jChipType == SIS_730)) {
     SR12 = (UCHAR)SiS_GetReg1(SiS_P3c4,0x12);
     SR13 = (UCHAR)SiS_GetReg1(SiS_P3c4,0x13);
     SR14 = (UCHAR)SiS_GetReg1(SiS_P3c4,0x14);
     SR16 = (UCHAR)SiS_GetReg1(SiS_P3c4,0x16);
     SR17 = (UCHAR)SiS_GetReg1(SiS_P3c4,0x17);
     SR18 = (UCHAR)SiS_GetReg1(SiS_P3c4,0x18);
     SR19 = (UCHAR)SiS_GetReg1(SiS_P3c4,0x19);
     SR1A = (UCHAR)SiS_GetReg1(SiS_P3c4,0x1A);
   } else if(HwDeviceExtension->jChipType == SIS_300){
     SR13 = (UCHAR)SiS_GetReg1(SiS_P3c4,0x13);
     SR14 = (UCHAR)SiS_GetReg1(SiS_P3c4,0x14);
   }
#endif
#ifdef SIS315H
   if((HwDeviceExtension->jChipType == SIS_550)||
      (HwDeviceExtension->jChipType == SIS_640)||   /* 08/20/01 chiawen for 640/740 */
      (HwDeviceExtension->jChipType == SIS_740)||   /* 09/03/01 chiawen for 640/740 */
      (HwDeviceExtension->jChipType == SIS_650)) {  /* 09/03/01 chiawen for 650 */
     SR19 = (UCHAR)SiS_GetReg1(SiS_P3c4,0x19);
     SR19 = (SR19)||0x01;  /* TW: ??? || ??? */
     if(SR19==0x00) {
     	SR13 = 0x22;
     	SR14 = 0x00;
    	SR15 = 0x01;
     	SR16 = 0x00;
     	SR17 = 0x00;
     	SR1A = 0x00;
     	SR1B = 0x00;
     	CR37 = 0x00;
     	CR38 = 0x00;
     	CR79 = 0x00;
     	CR7A = 0x00;
     	CR7B = 0x00;
     	CR7C = 0x00;
     } else {
     	SR13 = (UCHAR)SiS_GetReg1(SiS_P3c4,0x13);
     	SR14 = (UCHAR)SiS_GetReg1(SiS_P3c4,0x14);
     	SR15 = (UCHAR)SiS_GetReg1(SiS_P3c4,0x15);
     	SR16 = (UCHAR)SiS_GetReg1(SiS_P3c4,0x16);
     	SR17 = (UCHAR)SiS_GetReg1(SiS_P3c4,0x17);
     	SR1A = (UCHAR)SiS_GetReg1(SiS_P3c4,0x1A);
     	SR1B = (UCHAR)SiS_GetReg1(SiS_P3c4,0x1B);
     	CR37 = 0x02;/*(UCHAR)SiS_GetReg1(SiS_P3d4,0x37);*/
     	CR38 = (UCHAR)SiS_GetReg1(SiS_P3d4,0x38);
     	CR79 = (UCHAR)SiS_GetReg1(SiS_P3d4,0x79);
     	CR7A = (UCHAR)SiS_GetReg1(SiS_P3d4,0x7A);
     	CR7B = (UCHAR)SiS_GetReg1(SiS_P3d4,0x7B);
     	CR7C = (UCHAR)SiS_GetReg1(SiS_P3d4,0x7C);
     }
   }
#endif

/* ResetExtReg begin: Reset extended registers */

   for(i=0x06;i< 0x20;i++) SiS_SetReg1(SiS_P3c4,i,0);   /* 2.Reset Extended register */
   for(i=0x21;i<=0x27;i++) SiS_SetReg1(SiS_P3c4,i,0);   /*   Reset Extended register */
   for(i=0x31;i<=0x3D;i++) SiS_SetReg1(SiS_P3c4,i,0);

#ifdef SIS300
   if((HwDeviceExtension->jChipType == SIS_540)||
      (HwDeviceExtension->jChipType == SIS_630)||
      (HwDeviceExtension->jChipType == SIS_730)||
      (HwDeviceExtension->jChipType == SIS_300)) {
     	for(i=0x38;i<=0x3F;i++) SiS_SetReg1(SiS_P3d4,i,0);
   }
#endif

#ifdef SIS315H
   if((HwDeviceExtension->jChipType == SIS_315H)||
      (HwDeviceExtension->jChipType == SIS_315PRO)||
      (HwDeviceExtension->jChipType == SIS_550)||   /* 05/02/01 ynlai for 550 */
      (HwDeviceExtension->jChipType == SIS_640)||   /* 08/20/01 chiawen for 640/740 */
      (HwDeviceExtension->jChipType == SIS_740)||   /* 09/03/01 chiawen for 640/740 */
      (HwDeviceExtension->jChipType == SIS_650)) {
   	for(i=0x12;i<=0x1B;i++) SiS_SetReg1(SiS_P3c4,i,0);
   	for(i=0x79;i<=0x7C;i++) SiS_SetReg1(SiS_P3d4,i,0);
   }
#endif
/* ResetExtReg end */

#ifdef SIS300
   if((HwDeviceExtension->jChipType == SIS_540)||
      (HwDeviceExtension->jChipType == SIS_630)||
      (HwDeviceExtension->jChipType == SIS_730)) {
     SiS_SetReg1(SiS_P3c4,0x12,SR12);
     SiS_SetReg1(SiS_P3c4,0x13,SR13);
     SiS_SetReg1(SiS_P3c4,0x14,SR14);
     SiS_SetReg1(SiS_P3c4,0x16,SR16);
     SiS_SetReg1(SiS_P3c4,0x17,SR17);
     SiS_SetReg1(SiS_P3c4,0x18,SR18);
     SiS_SetReg1(SiS_P3c4,0x19,SR19);
     SiS_SetReg1(SiS_P3c4,0x1A,SR1A);
   }
#endif

#ifdef SIS315H
   if((HwDeviceExtension->jChipType == SIS_550)||
      (HwDeviceExtension->jChipType == SIS_640)||   /* 08/20/01 chiawen for 640/740 */
      (HwDeviceExtension->jChipType == SIS_740)||   /* 09/03/01 chiawen for 640/740 */
      (HwDeviceExtension->jChipType == SIS_650)) {  /* 09/03/01 chiawen for 650 */
     SiS_SetReg1(SiS_P3c4,0x13,SR13);
     SiS_SetReg1(SiS_P3c4,0x14,SR14);
     SiS_SetReg1(SiS_P3c4,0x15,SR15);
     SiS_SetReg1(SiS_P3c4,0x16,SR16);
     SiS_SetReg1(SiS_P3c4,0x17,SR17);
     SiS_SetReg1(SiS_P3c4,0x19,SR19);
     SiS_SetReg1(SiS_P3c4,0x1A,SR1A);
     SiS_SetReg1(SiS_P3c4,0x1B,SR1B);
     SiS_SetReg1(SiS_P3d4,0x37,CR37);
     SiS_SetReg1(SiS_P3d4,0x38,CR38);
     SiS_SetReg1(SiS_P3d4,0x79,CR79);
     SiS_SetReg1(SiS_P3d4,0x7A,CR7A);
     SiS_SetReg1(SiS_P3d4,0x7B,CR7B);
     SiS_SetReg1(SiS_P3d4,0x7C,CR7C);
   }
#endif

#ifdef SIS300
   if((HwDeviceExtension->jChipType==SIS_540)||
      (HwDeviceExtension->jChipType==SIS_630)||
      (HwDeviceExtension->jChipType==SIS_730)) {
     	temp=(UCHAR)SR1A;
   } else if(HwDeviceExtension->jChipType==SIS_300) {
        /* TW: Nothing */
   } else
#endif
   {
      	if((*pSiS_SoftSetting&SoftDRAMType)==0){
          	temp=(UCHAR)SiS_GetReg1(SiS_P3c4,0x3A);
        }
   }

   SiS_RAMType=temp&0x03;
   SiS_SetMemoryClock(ROMAddr);

/* SetDefExt1Regs begin: Set default register contents */

   SiS_SetReg1(SiS_P3c4,0x07,*pSiS_SR07); 		/* DAC speed */
   if((HwDeviceExtension->jChipType != SIS_540)&&
      (HwDeviceExtension->jChipType != SIS_630)&&
      (HwDeviceExtension->jChipType != SIS_730)){
     	for(i=0x15;i<0x1C;i++) {
       		SiS_SetReg1(SiS_P3c4,i,SiS_SR15[i-0x15][SiS_RAMType]);
     	}
   }

#ifdef SIS315H
   if ((HwDeviceExtension->jChipType == SIS_315H )||
       (HwDeviceExtension->jChipType == SIS_315PRO)) {
     	for(i=0x40;i<=0x44;i++) {
       		SiS_SetReg1(SiS_P3d4,i,SiS_CR40[i-0x40][SiS_RAMType]);
     	}
     	SiS_SetReg1(SiS_P3d4,0x48,0x23);
     	SiS_SetReg1(SiS_P3d4,0x49,SiS_CR49[0]);
    	/* /SiS_SetReg1(SiS_P3c4,0x25,SiS_SR25[0]);  */
   }
#endif

   SiS_SetReg1(SiS_P3c4,0x1F,*pSiS_SR1F); 	/* DAC pedestal */
   /*SiS_SetReg1(SiS_P3c4,0x20,0x20);*/
   SiS_SetReg1(SiS_P3c4,0x20,0xA0); /* alan, 2001/6/26 Frame buffer can read/write*/
   SiS_SetReg1(SiS_P3c4,0x23,*pSiS_SR23);
   SiS_SetReg1(SiS_P3c4,0x24,*pSiS_SR24);
   SiS_SetReg1(SiS_P3c4,0x25,SiS_SR25[0]);

#ifdef SIS300
   if(HwDeviceExtension->jChipType==SIS_300) {
     	SiS_SetReg1(SiS_P3c4,0x21,0x84);
     	SiS_SetReg1(SiS_P3c4,0x22,0x00);
   }
#endif

   SR11=0x0F;
   SiS_SetReg1(SiS_P3c4,0x11,SR11);	/* Power Management */

   SiS_UnLockCRT2(HwDeviceExtension, BaseAddr);
   SiS_SetReg1(SiS_Part1Port,0x00,0x00);
   SiS_SetReg1(SiS_Part1Port,0x02,*pSiS_CRT2Data_1_2);

#ifdef SIS315H                            /* 05/02/01 ynlai  for sis550 */
   if((HwDeviceExtension->jChipType == SIS_315H) ||
      (HwDeviceExtension->jChipType == SIS_315PRO) ||
      (HwDeviceExtension->jChipType == SIS_550)||
      (HwDeviceExtension->jChipType == SIS_640)||   /* 08/20/01 chiawen for 640/740 */
      (HwDeviceExtension->jChipType == SIS_740)||   /* 09/03/01 chiawen for 640/740 */
      (HwDeviceExtension->jChipType == SIS_650))    /* 09/03/01 chiawen for 650 */
     	SiS_SetReg1(SiS_Part1Port,0x2E,0x08); /* use VB */
#endif

   temp=*pSiS_SR32;
   if(!SiS_BridgeIsOn(BaseAddr)) {
     	temp=temp&0xEF;
   }
   SiS_SetReg1(SiS_P3c4,0x32,temp);

#ifdef SIS315H
   if ((HwDeviceExtension->jChipType == SIS_315H) ||
       (HwDeviceExtension->jChipType == SIS_315PRO)) {
     HwDeviceExtension->pQueryVGAConfigSpace(HwDeviceExtension,0x50,0,&Temp);  /* Get */
     Temp >>= 20;
     Temp &= 0xF;
     if (Temp!=1) {
     	SiS_SetReg1(SiS_P3c4,0x25,SiS_SR25[1]);
     	SiS_SetReg1(SiS_P3d4,0x49,SiS_CR49[1]);
     }

     SiS_SetReg1(SiS_P3c4,0x27,0x1F);

     SiS_SetReg1(SiS_P3c4,0x31,*pSiS_SR31);
     SiS_SetReg1(SiS_P3c4,0x32,*pSiS_SR32);
     SiS_SetReg1(SiS_P3c4,0x33,*pSiS_SR33);
   }
#endif

   if (SiS_BridgeIsOn(BaseAddr)==1) {
     	if(SiS_IF_DEF_LVDS==0) {
       		SiS_SetReg1(SiS_Part2Port,0x00,0x1C);
       		SiS_SetReg1(SiS_Part4Port,0x0D,*pSiS_CRT2Data_4_D);
       		SiS_SetReg1(SiS_Part4Port,0x0E,*pSiS_CRT2Data_4_E);
       		SiS_SetReg1(SiS_Part4Port,0x10,*pSiS_CRT2Data_4_10);
       		SiS_SetReg1(SiS_Part4Port,0x0F,0x3F);
     	}
     	SiS_LockCRT2(HwDeviceExtension, BaseAddr);
   }
   SiS_SetReg1(SiS_P3d4,0x83,0x00);
/*   SetDefExt1Regs end */

#ifdef SIS315H
   if ((HwDeviceExtension->jChipType==SIS_315H)||
       (HwDeviceExtension->jChipType==SIS_315PRO)) { /* 05/02/01 ynlai */
       	if (HwDeviceExtension->bSkipDramSizing==TRUE) {
         	SiS_SetDRAMModeRegister(ROMAddr);
         	pSR = HwDeviceExtension->pSR;
         	if (pSR!=NULL) {
           		while (pSR->jIdx!=0xFF) {
             			SiS_SetReg1(SiS_P3c4,pSR->jIdx,pSR->jVal);
             			pSR++;
           		}
         	}
       } else SiS_SetDRAMSize_310(HwDeviceExtension);
   }
#endif

#ifdef SIS315H
   if((HwDeviceExtension->jChipType==SIS_550)){ /* 05/02/01 ynlai For SiS 550 */
       /* SetDRAMConfig begin */
/*     SiS_SetReg1(SiS_P3c4,0x12,SR12);
       SiS_SetReg1(SiS_P3c4,0x13,SR13);
       SiS_SetReg1(SiS_P3c4,0x14,SR14);
       SiS_SetReg1(SiS_P3c4,0x16,SR16);
       SiS_SetReg1(SiS_P3c4,0x17,SR17);
       SiS_SetReg1(SiS_P3c4,0x18,SR18);
       SiS_SetReg1(SiS_P3c4,0x19,SR19);
       SiS_SetReg1(SiS_P3c4,0x1A,SR1A);   */
       /* SetDRAMConfig end */
   }
#endif

#ifdef SIS300
   if( HwDeviceExtension->jChipType==SIS_300) {       /* For SiS 300 Chip  */
       	if (HwDeviceExtension->bSkipDramSizing==TRUE) {
/*       	SiS_SetDRAMModeRegister(ROMAddr);
         	temp = (HwDeviceExtension->pSR)->jVal;
         	SiS_SetReg1(SiS_P3c4,0x13,temp);
         	temp = (HwDeviceExtension->pSR)->jVal;
         	SiS_SetReg1(SiS_P3c4,0x14,temp);   */
       } else {
#ifdef TC
         	SiS_SetReg1(SiS_P3c4,0x13,SR13);
         	SiS_SetReg1(SiS_P3c4,0x14,SR14);
         	SiS_SetRegANDOR(SiS_P3c4,0x15,0xFF,0x04);
#else
         	SiS_SetDRAMSize_300(HwDeviceExtension);
         	SiS_SetDRAMSize_300(HwDeviceExtension);
#endif
       }
   }
   if((HwDeviceExtension->jChipType==SIS_540)||       /* For SiS 630/540/730 Chip  */
      (HwDeviceExtension->jChipType==SIS_630)||
      (HwDeviceExtension->jChipType==SIS_730)){
       	/* SetDRAMConfig begin */
#if 0
     	SiS_SetReg1(SiS_P3c4,0x12,SR12);
       	SiS_SetReg1(SiS_P3c4,0x13,SR13);
       	SiS_SetReg1(SiS_P3c4,0x14,SR14);
       	SiS_SetReg1(SiS_P3c4,0x16,SR16);
       	SiS_SetReg1(SiS_P3c4,0x17,SR17);
       	SiS_SetReg1(SiS_P3c4,0x18,SR18);
       	SiS_SetReg1(SiS_P3c4,0x19,SR19);
       	SiS_SetReg1(SiS_P3c4,0x1A,SR1A);
#endif
       	/* SetDRAMConfig end */
   }
/* SetDRAMSize end */
#endif /* SIS300 */

/*   SetDefExt2Regs begin  */
#if 0
   AGP=1;
   temp=(UCHAR)SiS_GetReg1(SiS_P3c4,0x3A);
   temp=temp&0x30;
   if(temp==0x30) AGP=0;
   if(AGP==0) *pSiS_SR21=*pSiS_SR21&0xEF;
   SiS_SetReg1(SiS_P3c4,0x21,*pSiS_SR21);
   if(AGP==1) *pSiS_SR22=*pSiS_SR22&0x20;
   SiS_SetReg1(SiS_P3c4,0x22,*pSiS_SR22);
#endif
   SiS_SetReg1(SiS_P3c4,0x21,*pSiS_SR21);
   SiS_SetReg1(SiS_P3c4,0x22,*pSiS_SR22);
/*   SetDefExt2Regs end  */

#if 0
   SiS_SetReg3(SiS_P3c6,0xff);
   SiS_ClearDAC(SiS_P3c8);        /* [ynlai] 05/22/01 */
#endif

   SiS_DetectMonitor(HwDeviceExtension,BaseAddr);
   SiS_GetSenseStatus(HwDeviceExtension,ROMAddr);      /* sense CRT2 */

   return(TRUE);
}

void
SiS_Set_LVDS_TRUMPION(PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT temp=0;

#ifdef SiS300
  if ((HwDeviceExtension->jChipType == SIS_540)||
      (HwDeviceExtension->jChipType == SIS_630)||
      (HwDeviceExtension->jChipType == SIS_730)) {
        /* TW: Read POWER_ON_TRAP and copy to CR37 */
    	temp = (UCHAR)SiS_GetReg1(SiS_P3c4,0x1A);
    	temp=(temp&0xE0)>>4;
   	SiS_SetRegANDOR(SiS_P3d4,0x37,0xF1,temp);
   }
#endif

   /* Set globals SiS_IF_DEF... */
   SiSSetLVDSetc(HwDeviceExtension, 0);
}

/* ===============  for 300 dram sizing begin  =============== */
#ifdef SIS300
void
SiS_SetDRAMSize_300(PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
   /*ULONG   ROMAddr  = (ULONG)HwDeviceExtension->pjVirtualRomBase;*/
   ULONG   FBAddr   = (ULONG)HwDeviceExtension->pjVideoMemoryAddress;
   /*USHORT  BaseAddr = (USHORT)HwDeviceExtension->ulIOAddress;*/
   USHORT  SR13,SR14=0,buswidth,Done;
   SHORT   i,j,k;
   USHORT  data, TotalCapacity, PhysicalAdrOtherPage=0;
   ULONG   Addr;
   UCHAR   temp;

   int PseudoRankCapacity,PseudoTotalCapacity,PseudoAdrPinCount;
   int RankCapacity,AdrPinCount,BankNumHigh,BankNumMid,MB2Bank;
   /*int PageCapacity,PhysicalAdrHigh,PhysicalAdrHalfPage,PhysicalAdrAnotherPage;*/
   int PageCapacity,PhysicalAdrHigh,PhysicalAdrHalfPage;

   SiSSetMode(HwDeviceExtension,0x2e);

   data=SiS_GetReg1(SiS_P3c4,0x1);
   data=data|0x20;
   SiS_SetReg1(SiS_P3c4,0x01,data);                    /* Turn OFF Display  */

   SiS_SetReg1(SiS_P3c4,0x13,0x00);
   SiS_SetReg1(SiS_P3c4,0x14,0xBF);
   buswidth=SiS_ChkBUSWidth_300(FBAddr);

   MB2Bank=16;
   Done=0;
   for(i=6;i>=0;i--) {
      if(Done==1) break;
      PseudoRankCapacity=1<<i;
      for(j=4;j>=1;j--) {
         if(Done==1) break;
         PseudoTotalCapacity=PseudoRankCapacity*j;
         PseudoAdrPinCount=15-j;
         if(PseudoTotalCapacity <= 64) {
            for(k=0;k<=16;k++) {
               if(Done==1) break;
               RankCapacity=buswidth*SiS_DRAMType[k][3];
               AdrPinCount=SiS_DRAMType[k][2]+SiS_DRAMType[k][0];
               if(RankCapacity==PseudoRankCapacity)
                 if(AdrPinCount<=PseudoAdrPinCount) {
                    if(j==3) {             /* Rank No */
                       BankNumHigh=RankCapacity*MB2Bank*3-1;
                       BankNumMid=RankCapacity*MB2Bank*1-1;
                    }
                    else {
                       BankNumHigh=RankCapacity*MB2Bank*j-1;
                       BankNumMid=RankCapacity*MB2Bank*j/2-1;
                    }
                    PageCapacity=(1<<SiS_DRAMType[k][1])*buswidth*4;
                    PhysicalAdrHigh =BankNumHigh;
                    PhysicalAdrHalfPage=(PageCapacity/2+PhysicalAdrHigh)%PageCapacity;
                    PhysicalAdrOtherPage=PageCapacity*SiS_DRAMType[k][2]+PhysicalAdrHigh;
                    /* Write data */
                    /*Test */
                    temp=(UCHAR)SiS_GetReg1(SiS_P3c4,0x15);
                    SiS_SetReg1(SiS_P3c4,0x15,(USHORT)(temp&0xFB));

                    temp=(UCHAR)SiS_GetReg1(SiS_P3c4,0x15);
                    SiS_SetReg1(SiS_P3c4,0x15,(USHORT)(temp|0x04));
                    /*Test */
                    TotalCapacity=SiS_DRAMType[k][3]*buswidth;
                    SR13=SiS_DRAMType[k][4];
                    if(buswidth==4) SR14=(TotalCapacity-1)|0x80;
                    if(buswidth==2) SR14=(TotalCapacity-1)|0x40;
                    if(buswidth==1) SR14=(TotalCapacity-1)|0x00;
                    SiS_SetReg1(SiS_P3c4,0x13,SR13);
                    SiS_SetReg1(SiS_P3c4,0x14,SR14);

                    Addr=FBAddr+(BankNumHigh)*64*1024+PhysicalAdrHigh;
                    *((USHORT *)(Addr)) = (USHORT) PhysicalAdrHigh;
                    Addr=FBAddr+(BankNumMid)*64*1024+PhysicalAdrHigh;
                    *((USHORT *)(Addr)) = (USHORT) BankNumMid;
                    Addr=FBAddr+(BankNumHigh)*64*1024+PhysicalAdrHalfPage;
                    *((USHORT *)(Addr)) = (USHORT) PhysicalAdrHalfPage;
                    Addr=FBAddr+(BankNumHigh)*64*1024+PhysicalAdrOtherPage;
                    *((USHORT *)(Addr))=PhysicalAdrOtherPage;

                    /* Read data */
                    Addr=FBAddr+(BankNumHigh)*64*1024+PhysicalAdrHigh;
                    data=*((USHORT *)(Addr));
                    if(data==PhysicalAdrHigh) Done=1;
                 }  /* if struct */
            }  /* for loop (k) */
         }  /* if struct */
      }  /* for loop (j) */
   }  /* for loop (i) */
}

USHORT
SiS_ChkBUSWidth_300(ULONG FBAddress)
{
   /*USHORT  data;*/
   PULONG  pVideoMemory;

   pVideoMemory = (PULONG) FBAddress;

   pVideoMemory[0] = 0x01234567L;
   pVideoMemory[1] = 0x456789ABL;
   pVideoMemory[2] = 0x89ABCDEFL;
   pVideoMemory[3] = 0xCDEF0123L;
   if (pVideoMemory[3]==0xCDEF0123L) { /*ChannelA128Bit */
     return(4);
   }
   if (pVideoMemory[1]==0x456789ABL) {  /*ChannelB64Bit */
     return(2);
   }
   return(1);
}
#endif

/* ===============  for 300 dram sizing end    =============== */

/* ===============  310 dram sizing begin ==================== */
#ifdef SIS315H

/* TW: Moved Get310DRAMType further down */

void
SiS_Delay15us(ULONG ulMicrsoSec)
{
}

void
SiS_SDR_MRS(void)
{
   USHORT  data;

   data=SiS_GetReg1(SiS_P3c4,0x16);
   data=data & 0x3F;          /*/ SR16 D7=0,D6=0 */
   SiS_SetReg1(SiS_P3c4,0x16,data);   /*/ enable mode register set(MRS) low */
   SiS_Delay15us(0x100);
   data=data | 0x80;          /*/ SR16 D7=1,D6=0 */
   SiS_SetReg1(SiS_P3c4,0x16,data);   /*/ enable mode register set(MRS) high */
   SiS_Delay15us(0x100);
}

void
SiS_DDR_MRS(void)
{
   USHORT  data;

   /* SR16 <- 1F,DF,2F,AF */

   /* enable DLL of DDR SD/SGRAM , SR16 D4=1 */
   data=SiS_GetReg1(SiS_P3c4,0x16);
   data &= 0x0F;
   data |= 0x10;
   SiS_SetReg1(SiS_P3c4,0x16,data);

   if (!(SiS_SR15[1][SiS_RAMType] & 0x10))
   {
     data &= 0x0F;
   }
   /* SR16 D7=1,D6=1 */
   data |= 0xC0;
   SiS_SetReg1(SiS_P3c4,0x16,data);
   
   /* SR16 D7=1,D6=0,D5=1,D4=0 */
   data &= 0x0F;
   data |= 0x20;
   SiS_SetReg1(SiS_P3c4,0x16,data);
   if (!(SiS_SR15[1][SiS_RAMType] & 0x10)) {
     data &= 0x0F;
   }
   /* SR16 D7=1 */
   data |= 0x80;
   SiS_SetReg1(SiS_P3c4,0x16,data);
}

void
SiS_SetDRAMModeRegister(ULONG ROMAddr)
{

    if (SiS_Get310DRAMType(ROMAddr)<2) {
      SiS_SDR_MRS();
    } else {
      /* SR16 <- 0F,CF,0F,8F */
      SiS_DDR_MRS();
    }
}

void
SiS_DisableRefresh(void)
{
   USHORT  data;

   data=SiS_GetReg1(SiS_P3c4,0x17);
   data &= 0xF8;
   SiS_SetReg1(SiS_P3c4,0x17,data);

   data=SiS_GetReg1(SiS_P3c4,0x19);
   data |= 0x03;
   SiS_SetReg1(SiS_P3c4,0x19,data);

}

void
SiS_EnableRefresh(ULONG ROMAddr)
{
   SiS_SetReg1(SiS_P3c4,0x17,SiS_SR15[2][SiS_RAMType]);  /* SR17 */
   SiS_SetReg1(SiS_P3c4,0x19,SiS_SR15[4][SiS_RAMType]);  /* SR19 */
}

void
SiS_DisableChannelInterleaving(int index,USHORT SiS_DDRDRAM_TYPE[][5])
{
   USHORT  data;

   data=SiS_GetReg1(SiS_P3c4,0x15);
   data &= 0x1F;
   switch (SiS_DDRDRAM_TYPE[index][3])
   {
     case 64: data |= 0;
              break;
     case 32: data |= 0x20;
              break;
     case 16: data |= 0x40;
              break;
     case 4:  data |= 0x60;
              break;
   }
   SiS_SetReg1(SiS_P3c4,0x15,data);
}

void
SiS_SetDRAMSizingType(int index,USHORT DRAMTYPE_TABLE[][5])
{
   USHORT  data;

   data = DRAMTYPE_TABLE[index][4];
   SiS_SetReg1(SiS_P3c4,0x13,data);

   /* should delay 50 ns */
}

void
SiS_CheckBusWidth_310(ULONG ROMAddress,ULONG FBAddress)
{
   USHORT  data;
   PULONG volatile pVideoMemory;

   pVideoMemory = (PULONG) FBAddress;
   if (SiS_Get310DRAMType(ROMAddress)<2) {

     SiS_SetReg1(SiS_P3c4,0x13,0x00);
     SiS_SetReg1(SiS_P3c4,0x14,0x12);
     /* should delay */
     SiS_SDR_MRS();

     SiS_ChannelAB = 0;
     SiS_DataBusWidth = 128;
     pVideoMemory[0] = 0x01234567L;
     pVideoMemory[1] = 0x456789ABL;
     pVideoMemory[2] = 0x89ABCDEFL;
     pVideoMemory[3] = 0xCDEF0123L;
     pVideoMemory[4] = 0x55555555L;
     pVideoMemory[5] = 0x55555555L;
     pVideoMemory[6] = 0xFFFFFFFFL;
     pVideoMemory[7] = 0xFFFFFFFFL;
     if ((pVideoMemory[3]!=0xCDEF0123L) || (pVideoMemory[2] != 0x89ABCDEFL)) {
       /*ChannelA64Bit */
       SiS_DataBusWidth = 64;
       SiS_ChannelAB = 0;
       data=SiS_GetReg1(SiS_P3c4,0x14);
       SiS_SetReg1(SiS_P3c4,  0x14, (USHORT) (data & 0xFD));
     }

     if ((pVideoMemory[1]!=0x456789ABL) || (pVideoMemory[0] != 0x01234567L)) {
       /*ChannelB64Bit */
       SiS_DataBusWidth = 64;
       SiS_ChannelAB = 1;
       data=SiS_GetReg1(SiS_P3c4,0x14);
       SiS_SetReg1(SiS_P3c4,0x14,(USHORT)((data&0xFD)|0x01));
     }
     return;

   } else {
     /* DDR Dual channel */
     SiS_SetReg1(SiS_P3c4,0x13,0x00);
     SiS_SetReg1(SiS_P3c4,0x14,0x02); /* Channel A, 64bit */
     /* should delay */
     SiS_DDR_MRS();

     SiS_ChannelAB = 0;
     SiS_DataBusWidth = 64;
     pVideoMemory[0] = 0x01234567L;
     pVideoMemory[1] = 0x456789ABL;
     pVideoMemory[2] = 0x89ABCDEFL;
     pVideoMemory[3] = 0xCDEF0123L;
     pVideoMemory[4] = 0x55555555L;
     pVideoMemory[5] = 0x55555555L;
     pVideoMemory[6] = 0xAAAAAAAAL;
     pVideoMemory[7] = 0xAAAAAAAAL;

     if (pVideoMemory[1] == 0x456789ABL) {
       if (pVideoMemory[0] == 0x01234567L) {
         /* Channel A 64bit */
         return;
       }
     } else {
       if (pVideoMemory[0] == 0x01234567L) {
         /* Channel A 32bit */
         SiS_DataBusWidth = 32;
         SiS_SetReg1(SiS_P3c4,0x14,0x00);
         return;
       }
     }

     SiS_SetReg1(SiS_P3c4,0x14,0x03); /* Channel B, 64bit */
     SiS_DDR_MRS();

     SiS_ChannelAB = 1;
     SiS_DataBusWidth = 64;
     pVideoMemory[0] = 0x01234567L;
     pVideoMemory[1] = 0x456789ABL;
     pVideoMemory[2] = 0x89ABCDEFL;
     pVideoMemory[3] = 0xCDEF0123L;
     pVideoMemory[4] = 0x55555555L;
     pVideoMemory[5] = 0x55555555L;
     pVideoMemory[6] = 0xAAAAAAAAL;
     pVideoMemory[7] = 0xAAAAAAAAL;
     if (pVideoMemory[1] == 0x456789ABL) {
       /* Channel B 64 */
       if (pVideoMemory[0] == 0x01234567L) {
         /* Channel B 64bit */
         return;
       } else {
         /* error */
       }
     } else {
       if (pVideoMemory[0] == 0x01234567L) {
         /* Channel B 32 */
         SiS_DataBusWidth = 32;
         SiS_SetReg1(SiS_P3c4,0x14,0x01);
       } else {
         /* error */
       }
     }
   }
}

int
SiS_SetRank(int index,UCHAR RankNo,UCHAR SiS_ChannelAB,USHORT DRAMTYPE_TABLE[][5])
{
  USHORT  data;
  int RankSize;

  if ((RankNo==2)&&(DRAMTYPE_TABLE[index][0]==2))
         return 0;

  RankSize = DRAMTYPE_TABLE[index][3]/2 * SiS_DataBusWidth/32;

  if (RankNo*RankSize<=128) {
    data = 0;
    while ((RankSize>>=1)>0) {
      data+=0x10;
    }
    data |= (RankNo-1)<<2;
    data |= (SiS_DataBusWidth/64)&2;
    data |= SiS_ChannelAB;
    SiS_SetReg1(SiS_P3c4,0x14,data);
    /* should delay */
    SiS_SDR_MRS();
    return 1;
  } else
    return 0;
}

int
SiS_SetDDRChannel(int index,UCHAR ChannelNo,UCHAR SiS_ChannelAB,
                  USHORT DRAMTYPE_TABLE[][5])
{
  USHORT  data;
  int RankSize;

  RankSize = DRAMTYPE_TABLE[index][3]/2 * SiS_DataBusWidth/32;
  /* RankSize = DRAMTYPE_TABLE[index][3]; */
  if (ChannelNo*RankSize<=128) {
    data = 0;
    while ((RankSize>>=1)>0) {
      data+=0x10;
    }
    if (ChannelNo==2)
      data |= 0x0C;

    data |= (SiS_DataBusWidth/32)&2;
    data |= SiS_ChannelAB;
    SiS_SetReg1(SiS_P3c4,0x14,data);
    /* should delay */
    SiS_DDR_MRS();
    return 1;
  } else
    return 0;
}

int
SiS_CheckColumn(int index,USHORT DRAMTYPE_TABLE[][5],ULONG FBAddress)
{
  int i;
  ULONG Increment,Position;

  /*Increment = 1<<(DRAMTYPE_TABLE[index][2] + SiS_DataBusWidth / 64 + 1); */
  Increment = 1<<(10 + SiS_DataBusWidth / 64);

  for (i=0,Position=0;i<2;i++) {
         *((PULONG)(FBAddress+Position))=Position;
         Position += Increment;
  }

  for (i=0,Position=0;i<2;i++) {
/*    if (FBAddress[Position]!=Position) */
         if ( (*(PULONG) (FBAddress + Position)) !=Position)
                return 0;
         Position += Increment;
  }
  return 1;
}

int
SiS_CheckBanks(int index,USHORT DRAMTYPE_TABLE[][5],ULONG FBAddress)
{
  int i;
  ULONG Increment,Position;
  Increment = 1<<(DRAMTYPE_TABLE[index][2] + SiS_DataBusWidth / 64 + 2);

  for (i=0,Position=0;i<4;i++) {
/*    FBAddress[Position]=Position; */
    *((PULONG)(FBAddress+Position))=Position;
    Position += Increment;
  }

  for (i=0,Position=0;i<4;i++) {
/*    if (FBAddress[Position]!=Position) */
    if ( (*(PULONG) (FBAddress + Position)) !=Position)
      return 0;
    Position += Increment;
  }
  return 1;
}

int
SiS_CheckRank(int RankNo,int index,USHORT DRAMTYPE_TABLE[][5],ULONG FBAddress)
{
  int i;
  ULONG Increment,Position;
  Increment = 1<<(DRAMTYPE_TABLE[index][2] + DRAMTYPE_TABLE[index][1] +
                  DRAMTYPE_TABLE[index][0] + SiS_DataBusWidth / 64 + RankNo);

  for (i=0,Position=0;i<2;i++) {
/*    FBAddress[Position]=Position; */
    *((PULONG)(FBAddress+Position))=Position;
    /* *((PULONG)(FBAddress))=Position; */
    Position += Increment;
  }

  for (i=0,Position=0;i<2;i++) {
/*    if (FBAddress[Position]!=Position) */
         if ( (*(PULONG) (FBAddress + Position)) !=Position)
    /*if ( (*(PULONG) (FBAddress )) !=Position) */
      return 0;
    Position += Increment;
  }
  return 1;
}

int
SiS_CheckDDRRank(int RankNo,int index,USHORT DRAMTYPE_TABLE[][5],ULONG FBAddress)
{
  ULONG Increment,Position;
  USHORT  data;
  
  Increment = 1<<(DRAMTYPE_TABLE[index][2] + DRAMTYPE_TABLE[index][1] +
                  DRAMTYPE_TABLE[index][0] + SiS_DataBusWidth / 64 + RankNo);

  Increment += Increment/2;
  
  Position =0;
  *((PULONG)(FBAddress+Position+0))=0x01234567;
  *((PULONG)(FBAddress+Position+1))=0x456789AB;
  *((PULONG)(FBAddress+Position+2))=0x55555555;
  *((PULONG)(FBAddress+Position+3))=0x55555555;
  *((PULONG)(FBAddress+Position+4))=0xAAAAAAAA;
  *((PULONG)(FBAddress+Position+5))=0xAAAAAAAA;

  if ( (*(PULONG) (FBAddress + 1))==0x456789AB)
    return 1;

  if ( (*(PULONG) (FBAddress + 0))==0x01234567)
    return 0;

  data=SiS_GetReg1(SiS_P3c4,0x14);
  data &= 0xF3;
  data |= 0x08;
  SiS_SetReg1(SiS_P3c4,0x14,data);
  data=SiS_GetReg1(SiS_P3c4,0x15);
  data += 0x20;
  SiS_SetReg1(SiS_P3c4,0x15,data);

  return 1;
}

int
SiS_CheckRanks(int RankNo,int index,USHORT DRAMTYPE_TABLE[][5],ULONG FBAddress)
{
  int r;

  for (r=RankNo;r>=1;r--) {
    if (!SiS_CheckRank(r,index,DRAMTYPE_TABLE,FBAddress))
      return 0;
  }
  if (!SiS_CheckBanks(index,DRAMTYPE_TABLE,FBAddress))
    return 0;

  if (!SiS_CheckColumn(index,DRAMTYPE_TABLE,FBAddress))
    return 0;
  return 1;
}

int
SiS_CheckDDRRanks(int RankNo,int index,USHORT DRAMTYPE_TABLE[][5],ULONG FBAddress)
{
  int r;

  for (r=RankNo;r>=1;r--) {
    if (!SiS_CheckDDRRank(r,index,DRAMTYPE_TABLE,FBAddress))
      return 0;
  }
  if (!SiS_CheckBanks(index,DRAMTYPE_TABLE,FBAddress))
    return 0;

  if (!SiS_CheckColumn(index,DRAMTYPE_TABLE,FBAddress))
    return 0;
  return 1;
}

int
SiS_SDRSizing(ULONG FBAddress)
{
  int    i;
  UCHAR  j;

  for (i=0;i<13;i++) {
    SiS_SetDRAMSizingType(i, SiS_SDRDRAM_TYPE);
    for (j=2;j>0;j--) {
      if (!SiS_SetRank(i,(UCHAR) j, SiS_ChannelAB,SiS_SDRDRAM_TYPE))
        continue;
      else {
        if (SiS_CheckRanks(j,i,SiS_SDRDRAM_TYPE, FBAddress))
          return 1;
      }
    }
  }
  return 0;
}

int
SiS_DDRSizing(ULONG FBAddress)
{

  int    i;
  UCHAR  j;

  for (i=0;i<4;i++){
    SiS_SetDRAMSizingType(i,SiS_DDRDRAM_TYPE);
    SiS_DisableChannelInterleaving(i,SiS_DDRDRAM_TYPE);
    for (j=2;j>0;j--) {
      SiS_SetDDRChannel(i, j, SiS_ChannelAB, SiS_DDRDRAM_TYPE);
      if (!SiS_SetRank(i,(UCHAR) j, SiS_ChannelAB,SiS_DDRDRAM_TYPE))
        continue;
      else {
        if (SiS_CheckDDRRanks(j,i,SiS_DDRDRAM_TYPE, FBAddress))
          return 1;
      }
    }
  }
  return 0;
}

/*
 check if read cache pointer is correct
*/
void
SiS_VerifyMclk(ULONG FBAddr)
{
   PUCHAR  pVideoMemory = (PUCHAR) FBAddr;
   UCHAR i,j;
   USHORT Temp,SR21;


   pVideoMemory[0]=0xaa; /* alan */
   pVideoMemory[16]=0x55; /* note: PCI read cache is off */

   if ((pVideoMemory[0]!=0xaa)||(pVideoMemory[16]!=0x55)) {
     for (i=0,j=16;i<2;i++,j+=16)  {
       SR21 = SiS_GetReg1(SiS_P3c4, 0x21);
       Temp = SR21 & 0xFB;     /* disable PCI post write buffer empty gating */
       SiS_SetReg1(SiS_P3c4, 0x21, Temp);

       Temp = SiS_GetReg1(SiS_P3c4, 0x3C);
       Temp = Temp | 0x01;                 /*MCLK reset */
       SiS_SetReg1(SiS_P3c4, 0x3C, Temp);
       Temp = SiS_GetReg1(SiS_P3c4, 0x3C);
       Temp = Temp & 0xFE;                 /* MCLK normal operation */
       SiS_SetReg1(SiS_P3c4, 0x3C, Temp);
       SiS_SetReg1(SiS_P3c4, 0x21, SR21);

       pVideoMemory[16+j] = j;
       if (pVideoMemory[16+j]==j)  {
         pVideoMemory[j] = j;
         break;
       }
     }
   }
}

/* TW: Is this a 315E? */
int
Is315E(void)
{
   USHORT  data;
  	
   data=SiS_GetReg1(SiS_P3d4,0x5F);
   if (data&0x10) return 1;
   else return 0;
}

/* TW: For 315 only */
void
SiS_SetDRAMSize_310(PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
   ULONG   ROMAddr  = (ULONG)HwDeviceExtension->pjVirtualRomBase;
   ULONG   FBAddr   = (ULONG)HwDeviceExtension->pjVideoMemoryAddress;
   /*USHORT  BaseAddr = (USHORT)HwDeviceExtension->ulIOAddress;*/
   USHORT  data;

#ifdef SIS301	/* TW: SIS301 ??? */
   /*SiS_SetReg1(SiS_P3d4,0x30,0x40);   */
#endif
#ifdef SIS302   /* TW: SIS302 ??? */
   SiS_SetReg1(SiS_P3d4,0x30,0x4D);  /* alan,should change value */
   SiS_SetReg1(SiS_P3d4,0x31,0xc0);  /* alan,should change value */
   SiS_SetReg1(SiS_P3d4,0x34,0x3F);  /* alan,should change value */
#endif

   SiSSetMode(HwDeviceExtension,0x2e);

   data=SiS_GetReg1(SiS_P3c4,0x21);
   SiS_SetReg1(SiS_P3c4,0x21,(USHORT) (data&0xDF));      /* disable read cache */

   data=SiS_GetReg1(SiS_P3c4,0x1);
   data=data|0x20;
   SiS_SetReg1(SiS_P3c4,0x01,data);                    /* Turn OFF Display */

   data=SiS_GetReg1(SiS_P3c4,0x16);
   SiS_SetReg1(SiS_P3c4,0x16,(USHORT) (data|0x0F));      /* assume lowest speed DRAM */

   SiS_SetDRAMModeRegister(ROMAddr);
   SiS_DisableRefresh();
   SiS_CheckBusWidth_310(ROMAddr,FBAddr);

   SiS_VerifyMclk(FBAddr); /* alan 2000/7/3 */

   if (SiS_Get310DRAMType(ROMAddr)<2) {
     SiS_SDRSizing(FBAddr);
   } else {
     SiS_DDRSizing(FBAddr);
   }
   
   if (Is315E()) {
     data=SiS_GetReg1(SiS_P3c4,0x14);
     if ((data&0x0C)==0x0C) /* dual channel */
     {
     	if ((data&0xF0)>0x40)
     	  data = (data & 0x0F) | 0x40;
     } else { /* single channel */
     	if ((data&0xF0)>0x50)
     	  data = (data & 0x0F) | 0x50;
     }
   }
   
   SiS_SetReg1(SiS_P3c4,0x16,SiS_SR15[1][SiS_RAMType]);  /* restore SR16 */

   SiS_EnableRefresh(ROMAddr);
   data=SiS_GetReg1(SiS_P3c4,0x21);
   SiS_SetReg1(SiS_P3c4,0x21,(USHORT) (data|0x20));      /* enable read cache */
}
#endif

void
SiS_SetMemoryClock(ULONG ROMAddr)
{
   SiS_SetReg1(SiS_P3c4,0x28,SiS_MCLKData[SiS_RAMType].SR28);
   SiS_SetReg1(SiS_P3c4,0x29,SiS_MCLKData[SiS_RAMType].SR29);
   SiS_SetReg1(SiS_P3c4,0x2A,SiS_MCLKData[SiS_RAMType].SR2A);
   SiS_SetReg1(SiS_P3c4,0x2E,SiS_ECLKData[SiS_RAMType].SR2E);
   SiS_SetReg1(SiS_P3c4,0x2F,SiS_ECLKData[SiS_RAMType].SR2F);
   SiS_SetReg1(SiS_P3c4,0x30,SiS_ECLKData[SiS_RAMType].SR30);

#ifdef SIS315H
   if (Is315E()) {
     SiS_SetReg1(SiS_P3c4,0x28,0x3B); /* 143 */
     SiS_SetReg1(SiS_P3c4,0x29,0x22);
     SiS_SetReg1(SiS_P3c4,0x2E,0x3B); /* 143 */
     SiS_SetReg1(SiS_P3c4,0x2F,0x22);
   }
#endif
}

#endif /* ifndef LINUX_XF86 */

#ifdef SIS315H
UCHAR
SiS_Get310DRAMType(ULONG   ROMAddr)
{
   UCHAR   data;

   /*
   index=SiS_GetReg1(SiS_P3c4,0x1A);
   index=index&07;
   */
   if (*pSiS_SoftSetting&SoftDRAMType)
     data = *pSiS_SoftSetting & 0x03;
   else
     data=SiS_GetReg1(SiS_P3c4,0x3a) & 0x03;

   return data;
}
#endif

/* SiSInit END */

/* ----------------------------------------- */

void SiSRegInit(USHORT BaseAddr)
{
   SiS_P3c4=BaseAddr+0x14;
   SiS_P3d4=BaseAddr+0x24;
   SiS_P3c0=BaseAddr+0x10;
   SiS_P3ce=BaseAddr+0x1e;
   SiS_P3c2=BaseAddr+0x12;
   SiS_P3ca=BaseAddr+0x1a;
   SiS_P3c6=BaseAddr+0x16;
   SiS_P3c7=BaseAddr+0x17;
   SiS_P3c8=BaseAddr+0x18;
   SiS_P3c9=BaseAddr+0x19;
   SiS_P3da=BaseAddr+0x2A;
   SiS_Part1Port=BaseAddr+SIS_CRT2_PORT_04;
   SiS_Part2Port=BaseAddr+SIS_CRT2_PORT_10;
   SiS_Part3Port=BaseAddr+SIS_CRT2_PORT_12;
   SiS_Part4Port=BaseAddr+SIS_CRT2_PORT_14;
   SiS_Part5Port=BaseAddr+SIS_CRT2_PORT_14+2;
   SiS_DDC_Port=BaseAddr+0x14;  /* 0x3c4; */
}

void
SiSInitPCIetc(PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
#ifdef LINUX_XF86
   if ((HwDeviceExtension->jChipType == SIS_540)||
       (HwDeviceExtension->jChipType == SIS_630)||
       (HwDeviceExtension->jChipType == SIS_730)||
       (HwDeviceExtension->jChipType == SIS_300)) {
       /* TW: Set - PCI LINEAR ADDRESSING ENABLE (0x80)
		  - PCI IO ENABLE  (0x20)
		  - MMIO ENABLE (0x1)
  	*/
       SiS_SetReg1(SiS_P3c4,0x20,0xa1);
       /* TW: Enable 2D (0x42) & 3D accelerator (0x18) */
       SiS_SetReg1(SiS_P3c4,0x1E,0x5A);
   }
   if((HwDeviceExtension->jChipType == SIS_315H)||
      (HwDeviceExtension->jChipType == SIS_315PRO)||
      (HwDeviceExtension->jChipType == SIS_550)||
      (HwDeviceExtension->jChipType == SIS_640)||
      (HwDeviceExtension->jChipType == SIS_740)||
      (HwDeviceExtension->jChipType == SIS_650)) {
      /* TW: This seems to be done the same way on these chipsets */
      SiS_SetReg1(SiS_P3c4,0x20,0xa1);
      SiS_SetReg1(SiS_P3c4,0x1E,0x5A);
   }
#endif
}

void
SiSSetLVDSetc(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT ModeNo)
{
   ULONG   temp;

   SiS_IF_DEF_LVDS=0;
   SiS_IF_DEF_CH7005=0;
   SiS_IF_DEF_HiVision=0;
   SiS_IF_DEF_DSTN=0;
   SiS_IF_DEF_FSTN=0;
   if((ModeNo==0x5a)||(ModeNo==0x5b)) {
   	SiS_IF_DEF_DSTN=1;   /* for 550 dstn */
   	SiS_IF_DEF_FSTN=1;   /* for fstn */
   }
   if((HwDeviceExtension->jChipType == SIS_540)||
      (HwDeviceExtension->jChipType == SIS_630)||
      (HwDeviceExtension->jChipType == SIS_730)||
      (HwDeviceExtension->jChipType == SIS_550)||
      (HwDeviceExtension->jChipType == SIS_640)||   /* 08/20/01 chiawen for 640/740 */
      (HwDeviceExtension->jChipType == SIS_740)||   /* 09/03/01 chiawen for 640/740 */
      (HwDeviceExtension->jChipType == SIS_650))    /* 09/03/01 chiawen for 650 */
    {
      	if (SiS_IF_DEF_FSTN) {                               /*fstn setCR37=0x04;*/
        	SiS_SetReg1(SiS_P3d4,0x37,0x04);
      	}
      	temp=SiS_GetReg1(SiS_P3d4,0x37);
      	temp=(temp&0x0E)>>1;
      	if((temp==0)||(temp==1)) {   /* for 301 */
        	SiS_IF_DEF_LVDS=0;
        	SiS_IF_DEF_CH7005=0;
        	SiS_IF_DEF_TRUMPION=0;
      	}
      	if((temp>=2)&&(temp<=5)) SiS_IF_DEF_LVDS=1;
      	if(temp==3) SiS_IF_DEF_TRUMPION=1;
      	if((temp==4)||(temp==5)) {
		/* TW: Save power status */
		SiS_Backup7005 = SiS_GetCH7005(0x0e);
		SiS_IF_DEF_CH7005=1;
        }
   }
}

void
SiSInitPtr(PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
#ifdef SIS315H
   if((HwDeviceExtension->jChipType == SIS_315H)||  /* 05/02/01 ynlai for sis550 */
      (HwDeviceExtension->jChipType == SIS_315PRO)||
      (HwDeviceExtension->jChipType == SIS_550)||
      (HwDeviceExtension->jChipType == SIS_640)||   /* 08/20/01 chiawen for 640/740 */
      (HwDeviceExtension->jChipType == SIS_740)||   /* 09/03/01 chiawen for 640/740 */
      (HwDeviceExtension->jChipType == SIS_650))    /* 09/03/01 chiawen for 650 */
     InitTo310Pointer();
#endif

#ifdef SIS300
   if ((HwDeviceExtension->jChipType == SIS_540)||
       (HwDeviceExtension->jChipType == SIS_630)||
       (HwDeviceExtension->jChipType == SIS_730)||
       (HwDeviceExtension->jChipType == SIS_300))
     InitTo300Pointer();
#endif
}

/*
 	=========================================
 	======== SiS SetMode Function  ==========
 	=========================================
*/
#ifdef LINUX_XF86
/* TW: This is used for non-Dual-Head mode from X */
BOOLEAN
SiSBIOSSetMode(PSIS_HW_DEVICE_INFO HwDeviceExtension, ScrnInfoPtr pScrn,
               DisplayModePtr mode)
{
   UShort  ModeNo=0;

   ModeNo = SiS_CalcModeIndex(pScrn, mode);
   if (!ModeNo) return FALSE;

   xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Setting mode 0x%x\n", ModeNo);

   return(SiSSetMode(HwDeviceExtension, pScrn, ModeNo));
}

#ifdef SISDUALHEAD
/* TW: Set CRT1 mode (used for dual head) */
BOOLEAN
SiSBIOSSetModeCRT1(PSIS_HW_DEVICE_INFO HwDeviceExtension, ScrnInfoPtr pScrn,
               DisplayModePtr mode)
{
   ULONG   temp;
   USHORT  ModeIdIndex,KeepLockReg;
   ULONG   ROMAddr  = (ULONG)HwDeviceExtension->pjVirtualRomBase;
   USHORT  BaseAddr = (USHORT)HwDeviceExtension->ulIOAddress;
   SISPtr  pSiS = SISPTR(pScrn);
   SISEntPtr pSiSEnt = pSiS->entityPrivate;

   UShort  ModeNo=0;

   ModeNo = SiS_CalcModeIndex(pScrn, mode);
   if (!ModeNo) return FALSE;

   xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Setting mode 0x%x on CRT1\n", ModeNo);

   SiSInitPtr(HwDeviceExtension);

   SiSRegInit(BaseAddr);

   SiSInitPCIetc(HwDeviceExtension);

   SiSSetLVDSetc(HwDeviceExtension, ModeNo);

   /* TW: We don't clear the buffer under X */
   flag_clearbuffer=0;

   /* SiS_PresetScratchregister(SiS_P3d4,HwDeviceExtension);  */      		/* add for CRT2  */
   KeepLockReg = SiS_GetReg1(SiS_P3c4,0x05);
   SiS_SetReg1(SiS_P3c4,0x05,0x86);                             		/* 1.Openkey */

   temp = SiS_SearchModeID(ROMAddr,ModeNo,&ModeIdIndex);        		/* 2.Get ModeID Table  */
   if(temp==0)  return(0);

   /* TW: Determine VBType (301,301B,301LV,302B,302LV) */
   SiS_GetVBType(BaseAddr);

   SiS_GetVBInfo301(BaseAddr,ROMAddr,ModeNo,ModeIdIndex,HwDeviceExtension);   	/*add for CRT2 */
   SiS_GetLCDResInfo301(ROMAddr,SiS_P3d4,ModeNo,ModeIdIndex);   		/*add for CRT2 */

   SiS_SetCRT1Group(ROMAddr,HwDeviceExtension,ModeNo,ModeIdIndex);

   pSiSEnt->CRT1ModeNo = ModeNo;
   pSiSEnt->CRT1DMode = mode;

   /* TW: SetPitch: Adapt to virtual size & position */
   SiS_SetPitchCRT1(pScrn, BaseAddr);

   /* We have to reset CRT2 if changing mode on CRT1 */
   if (pSiSEnt->CRT2ModeNo != -1) {
        xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "(Re-)Setting mode 0x%x on CRT2\n",
				pSiSEnt->CRT2ModeNo);
	SiSBIOSSetModeCRT2(HwDeviceExtension, pSiSEnt->pScrn_1,
				pSiSEnt->CRT2DMode);
   }

   /* Backup/Set ModeNo in MMIO */
   /* SiS_GetSetModeID(pScrn,ModeNo); */

   if(KeepLockReg==0xA1) SiS_SetReg1(SiS_P3c4,0x05,0x86); /* 05/02/01 ynlai */
   else SiS_SetReg1(SiS_P3c4,0x05,0x00);

   return TRUE;
}

/* TW: Set CRT2 mode (used for dual head) */
BOOLEAN
SiSBIOSSetModeCRT2(PSIS_HW_DEVICE_INFO HwDeviceExtension, ScrnInfoPtr pScrn,
               DisplayModePtr mode)
{
   ULONG   temp;
   USHORT  ModeIdIndex,KeepLockReg;
   ULONG   ROMAddr  = (ULONG)HwDeviceExtension->pjVirtualRomBase;
   USHORT  BaseAddr = (USHORT)HwDeviceExtension->ulIOAddress;
   UShort  ModeNo=0;
   SISPtr  pSiS = SISPTR(pScrn);
   SISEntPtr pSiSEnt = pSiS->entityPrivate;

   ModeNo = SiS_CalcModeIndex(pScrn, mode);
   if (!ModeNo) return FALSE;

   SiSInitPtr(HwDeviceExtension);

   SiSRegInit(BaseAddr);

   SiSInitPCIetc(HwDeviceExtension);

   SiSSetLVDSetc(HwDeviceExtension, ModeNo);

   /* TW: We don't clear the buffer under X */
   flag_clearbuffer=0;

   /* TW: Save ModeNo so we can set it from within SetMode for CRT1 */
   pSiSEnt->CRT2ModeNo = ModeNo;
   pSiSEnt->CRT2DMode = mode;

   /* TW: We can't set CRT2 mode before CRT1 mode is set */
   if (pSiSEnt->CRT1ModeNo == -1) {
   	xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		"Setting CRT2 mode delayed until after setting CRT1 mode\n");
   	return TRUE;
   }

   xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Setting mode 0x%x on CRT2\n", ModeNo);

   /* SiS_PresetScratchregister(SiS_P3d4,HwDeviceExtension);      */  		/* add for CRT2  */
   KeepLockReg = SiS_GetReg1(SiS_P3c4,0x05);
   SiS_SetReg1(SiS_P3c4,0x05,0x86);                             		/* 1.Openkey */

   temp = SiS_SearchModeID(ROMAddr,ModeNo,&ModeIdIndex);        		/* 2.Get ModeID Table  */
   if(temp==0)  return(0);

   /* TW: Determine VBType (301,301B,301LV,302B,302LV) */
   SiS_GetVBType(BaseAddr);

   SiS_GetVBInfo301(BaseAddr,ROMAddr,ModeNo,ModeIdIndex,HwDeviceExtension);   	/*add for CRT2 */
   SiS_GetLCDResInfo301(ROMAddr,SiS_P3d4,ModeNo,ModeIdIndex);   		/*add for CRT2 */

   switch (HwDeviceExtension->ujVBChipID) {
     case VB_CHIP_301:
     case VB_CHIP_301B:    /*for Linux & WinCE to open*/
        SiS_SetCRT2Group301(BaseAddr,ROMAddr,ModeNo,HwDeviceExtension);         /*add for CRT2 */
        break;
     case VB_CHIP_302:
        SiS_SetCRT2Group301(BaseAddr,ROMAddr,ModeNo,HwDeviceExtension);
        break;
     case VB_CHIP_303:
/*      SetCRT2Group302(BaseAddr,ROMAddr,ModeNo, HwDeviceExtension);            add for CRT2   */
        break;
     case VB_CHIP_UNKNOWN:                                                                                                 /*add for lvds ch7005*/
        temp=SiS_GetReg1(SiS_P3d4,0x37);
        if(temp&(ExtChipLVDS|ExtChipTrumpion|ExtChipCH7005)){
             	SiS_SetCRT2Group301(BaseAddr,ROMAddr,ModeNo,HwDeviceExtension);
        }
        break;
   }

   /* TW: SetPitch: Adapt to virtual size & position */
   SiS_SetPitchCRT2(pScrn, BaseAddr);

   /* Backup/Set ModeNo in MMIO */
   /* SiS_GetSetModeID(pScrn,ModeNo); */

   if(KeepLockReg==0xA1) SiS_SetReg1(SiS_P3c4,0x05,0x86); /* 05/02/01 ynlai */
   else SiS_SetReg1(SiS_P3c4,0x05,0x00);

   return TRUE;
}
#endif /* Dualhead */
#endif /* Linux_XF86 */

#ifdef LINUX_XF86
/* TW: We need pScrn for setting the pitch correctly */
BOOLEAN
SiSSetMode(PSIS_HW_DEVICE_INFO HwDeviceExtension,ScrnInfoPtr pScrn,USHORT ModeNo)
#else
BOOLEAN
SiSSetMode(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT ModeNo)
#endif
{
   ULONG   temp;
   USHORT  ModeIdIndex,KeepLockReg;
   ULONG   ROMAddr  = (ULONG)HwDeviceExtension->pjVirtualRomBase;
   USHORT  BaseAddr = (USHORT)HwDeviceExtension->ulIOAddress;

   SiSInitPtr(HwDeviceExtension);

   SiSRegInit(BaseAddr);

   SiSInitPCIetc(HwDeviceExtension);

   SiSSetLVDSetc(HwDeviceExtension, ModeNo);

#ifdef LINUX_XF86
   /* TW: We don't clear the buffer under X */
   ModeNo |= 0x80;
#endif

   if(ModeNo&0x80) {
     	ModeNo=ModeNo&0x7F;
     	flag_clearbuffer=0;
   } else {
     	flag_clearbuffer=1;
   }

   SiS_PresetScratchregister(SiS_P3d4,HwDeviceExtension);       		/* add for CRT2  */
   KeepLockReg = SiS_GetReg1(SiS_P3c4,0x05);
   SiS_SetReg1(SiS_P3c4,0x05,0x86);                             		/* 1.Openkey */

   temp = SiS_SearchModeID(ROMAddr,ModeNo,&ModeIdIndex);        		/* 2.Get ModeID Table  */
   if(temp==0)  return(0);

   /* TW: Determine VBType (301,301B,301LV,302B,302LV) */
   SiS_GetVBType(BaseAddr);

   SiS_GetVBInfo301(BaseAddr,ROMAddr,ModeNo,ModeIdIndex,HwDeviceExtension);   	/*add for CRT2 */
   SiS_GetLCDResInfo301(ROMAddr,SiS_P3d4,ModeNo,ModeIdIndex);   		/*add for CRT2 */

   temp=SiS_CheckMemorySize(ROMAddr,HwDeviceExtension,ModeNo,ModeIdIndex);    	/*3.Check memory size */
   if(temp==0) return(0);
   if(SiS_VBInfo&(SetSimuScanMode|SetCRT2ToLCDA)) {				/*301b*/
   	SiS_SetCRT1Group(ROMAddr,HwDeviceExtension,ModeNo,ModeIdIndex);
   } else {
     if(!(SiS_VBInfo&SwitchToCRT2)) {
       	SiS_SetCRT1Group(ROMAddr,HwDeviceExtension,ModeNo,ModeIdIndex);
     }
   }

   if(SiS_VBInfo&(SetSimuScanMode|SwitchToCRT2|SetCRT2ToLCDA)) {		/*301b*/
     switch (HwDeviceExtension->ujVBChipID) {
     case VB_CHIP_301:
     case VB_CHIP_301B:    /*for Linux & WinCE to open*/
        SiS_SetCRT2Group301(BaseAddr,ROMAddr,ModeNo,HwDeviceExtension);        /*add for CRT2 */
        break;
     case VB_CHIP_302:
        SiS_SetCRT2Group301(BaseAddr,ROMAddr,ModeNo,HwDeviceExtension);
        break;
     case VB_CHIP_303:
/*      SetCRT2Group302(BaseAddr,ROMAddr,ModeNo, HwDeviceExtension);             add for CRT2   */
        break;
     case VB_CHIP_UNKNOWN:                                                                                                 /*add for lvds ch7005*/
        temp=SiS_GetReg1(SiS_P3d4,0x37);
        if(temp&(ExtChipLVDS|ExtChipTrumpion|ExtChipCH7005)){
             	SiS_SetCRT2Group301(BaseAddr,ROMAddr,ModeNo,HwDeviceExtension);
        }
        break;
     }
   }

#ifdef LINUX_XF86
   /* TW: SetPitch: Adapt to virtual size & position */
   SiS_SetPitch(pScrn, BaseAddr);
   /* Backup/Set ModeNo in MMIO */
   SiS_GetSetModeID(pScrn,ModeNo);
#endif

#ifndef LINUX_XF86  /* TW: We never lock registers in XF86 */
   if(KeepLockReg==0xA1) SiS_SetReg1(SiS_P3c4,0x05,0x86);
   else SiS_SetReg1(SiS_P3c4,0x05,0x00);
#endif

   return TRUE;
}

void
SetEnableDstn()		/* TW: Called from sis_main.c */
{
   SiS_IF_DEF_DSTN=1;   /*for 550 dstn*/
}

void
SiS_SetCRT1Group(ULONG ROMAddr,PSIS_HW_DEVICE_INFO HwDeviceExtension,
                 USHORT ModeNo,USHORT ModeIdIndex)
{
  USHORT  StandTableIndex,RefreshRateTableIndex;
  USHORT  temp;

  /*SiS_SetReg1(SiS_P3d4,0x34,ModeNo);*/
  SiS_CRT1Mode = ModeNo;
  /* set CR34->CRT1 ModeNo for CRT2 FIFO */
  StandTableIndex = SiS_GetModePtr(ROMAddr,ModeNo,ModeIdIndex); /* 4.GetModePtr  */
  SiS_SetSeqRegs(ROMAddr,StandTableIndex);                      /* 5.SetSeqRegs  */
  SiS_SetMiscRegs(ROMAddr,StandTableIndex);                     /* 6.SetMiscRegs */
  SiS_SetCRTCRegs(ROMAddr,HwDeviceExtension,StandTableIndex);   /* 7.SetCRTCRegs */
  SiS_SetATTRegs(ROMAddr,StandTableIndex);                      /* 8.SetATTRegs  */
  SiS_SetGRCRegs(ROMAddr,StandTableIndex);                      /* 9.SetGRCRegs  */
  SiS_ClearExt1Regs();                                          /* 10.Clear Ext1Regs */
  temp=~ProgrammingCRT2;                                        /* 11.GetRatePtr  */
  SiS_SetFlag=SiS_SetFlag&temp;
  SiS_SelectCRT2Rate=0;

#ifdef LINUX_XF86
  xf86DrvMsg(0, X_INFO, "VBType = %x, LVDS=%d, VBInfo=%x\n",
                    SiS_VBType, SiS_IF_DEF_LVDS, SiS_VBInfo);
#endif

  /*301b*/
  if( (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))) {
      if(SiS_VBInfo&SetCRT2ToLCDA) {
	 SiS_SetFlag=SiS_SetFlag|ProgrammingCRT2;
      }
  }
  /*end 301b*/

  RefreshRateTableIndex=SiS_GetRatePtrCRT2(ROMAddr,ModeNo,ModeIdIndex);   /* 11.GetRatePtr */

  if( (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))) {
      if(!(SiS_VBInfo&SetCRT2ToLCDA)) {
	 SiS_SetFlag=SiS_SetFlag&(~ProgrammingCRT2);
      }
  }

  if (RefreshRateTableIndex!=0xFFFF) {
    SiS_SetSync(ROMAddr,RefreshRateTableIndex);                          /* 12.SetSync */
    SiS_SetCRT1CRTC(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex);   /* 13.SetCRT1CRTC  */
    SiS_SetCRT1Offset(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex,HwDeviceExtension);   /* 14.SetCRT1Offset */
    SiS_SetCRT1VCLK(ROMAddr,ModeNo,ModeIdIndex,HwDeviceExtension,RefreshRateTableIndex);     /* 15.SetCRT1VCLK  */
  }
#ifdef SIS300
  if((HwDeviceExtension->jChipType == SIS_630)||
     (HwDeviceExtension->jChipType == SIS_540)) {
     	SiS_SetCRT1FIFO2(ROMAddr,ModeNo,HwDeviceExtension,RefreshRateTableIndex);
  }
#endif
#ifdef SIS315H
  if(HwDeviceExtension->jChipType >= SIS_315H) {
     	SiS_SetCRT1FIFO(ROMAddr,ModeNo,HwDeviceExtension);
  }
#endif
  SiS_SetCRT1ModeRegs(ROMAddr,HwDeviceExtension,ModeNo,ModeIdIndex,RefreshRateTableIndex);
  SiS_SetVCLKState(ROMAddr,HwDeviceExtension,ModeNo,RefreshRateTableIndex);
#ifdef SIS315H
  if(HwDeviceExtension->jChipType > SIS_315H) {
    if (RefreshRateTableIndex!=0xFFFF)
      	SiS_SetInterlace(ROMAddr,ModeNo,RefreshRateTableIndex);
  }
#endif
  SiS_LoadDAC(ROMAddr,ModeNo,ModeIdIndex);
  if(flag_clearbuffer) SiS_ClearBuffer(HwDeviceExtension,ModeNo);

  if(!(SiS_VBInfo&(SetSimuScanMode|SwitchToCRT2|SetCRT2ToLCDA))) {   /*301b*/
    SiS_LongWait();
    SiS_DisplayOn();
  }
}

#ifdef LINUX_XF86
void
SiS_SetPitch(ScrnInfoPtr pScrn, UShort BaseAddr)
{
   SISPtr pSiS = SISPTR(pScrn);

   if (pSiS->VBFlags & DISPTYPE_DISP1) {
   	SiS_SetPitchCRT1(pScrn, BaseAddr);
   }
   if (pSiS->VBFlags & DISPTYPE_DISP2) {
   	SiS_SetPitchCRT2(pScrn, BaseAddr);
   }
}

void
SiS_SetPitchCRT1(ScrnInfoPtr pScrn, UShort BaseAddr)
{
    SISPtr pSiS = SISPTR(pScrn);
    ULong  HDisplay,temp;

    HDisplay = pSiS->scrnOffset / 8;
    SiS_SetReg1(SiS_P3d4, 0x13, (HDisplay & 0xFF));
    temp = (SiS_GetReg1(SiS_P3c4, 0x0E) & 0xF0) | (HDisplay>>8);
    SiS_SetReg1(SiS_P3c4, 0x0E, temp);
}

void
SiS_SetPitchCRT2(ScrnInfoPtr pScrn, UShort BaseAddr)
{
    SISPtr pSiS = SISPTR(pScrn);
    ULong  HDisplay,temp;

    HDisplay = pSiS->scrnOffset / 8;
    SiS_SetReg1(SiS_Part1Port, 0x24, 1);
    SiS_SetReg1(SiS_Part1Port, 0x07, HDisplay);
    temp = (SiS_GetReg1(SiS_Part1Port, 0x09) & 0xF0) | (HDisplay>>8);
    SiS_SetReg1(SiS_Part1Port, 0x09, temp);
}
#endif

void
SiS_GetVBType(USHORT BaseAddr)
{
  USHORT flag;

  flag=SiS_GetReg1(SiS_Part4Port,0x00);
  /* TW: On pure LVDS, Part4 is not assigned and returns 0xff */
  if (flag >= 4) return;
  /* TW end */
  if (flag >= 2) {
  	flag=SiS_GetReg1(SiS_Part4Port,0x01);
        if(flag>=0xB0){
            	SiS_VBType=VB_SIS302B;
            	if(flag>=0xD0)
               		SiS_VBType=VB_SIS302LV;
        }
  } else {
        flag=SiS_GetReg1(SiS_Part4Port,0x01);
        if(flag>=0xB0){
            	SiS_VBType=VB_SIS301B;
            	if(flag>=0xD0)
               		SiS_VBType=VB_SIS301LV;
        } else
            SiS_VBType=VB_SIS301;
  }
  flag=SiS_GetReg1(SiS_Part4Port,0x23);       /*301dlvds*/
  if(!(flag&0x02))
      	SiS_VBType=SiS_VBType|VB_NoLCD;
}

/* win2000 MM adapter not support standard mode  */
BOOLEAN
SiS_SearchModeID(ULONG ROMAddr, USHORT ModeNo,USHORT  *ModeIdIndex)
{
#ifndef LINUX_XF86
   PUCHAR VGA_INFO="\0x11";
#endif
   
   if (ModeNo<=5) ModeNo |= 1;
   if (ModeNo<=0x13) {
     /* for (*ModeIdIndex=0;*ModeIdIndex<sizeof(SiS_SModeIDTable)/sizeof(SiS_StStruct);(*ModeIdIndex)++) */
     for (*ModeIdIndex=0;;(*ModeIdIndex)++)
     {
       if (SiS_SModeIDTable[*ModeIdIndex].St_ModeID==ModeNo) break;
       if (SiS_SModeIDTable[*ModeIdIndex].St_ModeID==0xFF)  return FALSE;
     }
#ifndef LINUX_XF86
#ifdef TC
     VGA_INFO = (PUCHAR) MK_FP(0,0x489);
#endif
     if (ModeNo==0x07) {
        if ((*VGA_INFO&0x10)!=0) (*ModeIdIndex)++; /* 400 lines */
        /* else 350 lines */
     }
     if (ModeNo<=3) {
       if ((*VGA_INFO&0x80)==0) {
         (*ModeIdIndex)++;
         if ((*VGA_INFO&0x10)!=0)
           (*ModeIdIndex)++;; /* 400 lines  */
         /* else 350 lines  */
       }
       /* else 200 lines  */
     }
#endif  /* NOT X86 */
   }
   else 
   {
     /* for (*ModeIdIndex=0;*ModeIdIndex<sizeof(SiS_EModeIDTable)/sizeof(SiS_ExtStruct);(*ModeIdIndex)++) */
     for (*ModeIdIndex=0;;(*ModeIdIndex)++)
     {
       if (SiS_EModeIDTable[*ModeIdIndex].Ext_ModeID==ModeNo) break;
       if (SiS_EModeIDTable[*ModeIdIndex].Ext_ModeID==0xFF) return FALSE;
     }
   }
   return TRUE;
}

/*add for 300 oem util for search VBModeID*/
BOOLEAN
SiS_SearchVBModeID(ULONG ROMAddr, USHORT ModeNo)
{
   USHORT ModeIdIndex;
#ifdef TC
   PUCHAR VGA_INFO;
#endif
   
   if (ModeNo<=5) ModeNo |= 1;
    /* for (ModeIdIndex=0;ModeIdIndex<sizeof(SiS_SModeIDTable)/sizeof(SiS_StStruct);(*ModeIdIndex)++) */
     for (ModeIdIndex=0;;(ModeIdIndex)++)
     {
       if (SiS_VBModeIDTable[ModeIdIndex].ModeID==ModeNo) break;
       if (SiS_VBModeIDTable[ModeIdIndex].ModeID==0xFF)  return FALSE;
     }
#ifdef TC
     VGA_INFO = (PUCHAR) MK_FP(0,0x489);
     if (ModeNo==0x07) {
	if ((*VGA_INFO&0x10)!=0) (ModeIdIndex)++; /* 400 lines */
	/* else 350 lines */
     }
     if (ModeNo<=3) {
       if ((*VGA_INFO&0x80)==0) {
	 (ModeIdIndex)++;
         if ((*VGA_INFO&0x10)!=0)
	   (ModeIdIndex)++;; /* 400 lines  */
	 /* else 350 lines  */
       }
       /* else 200 lines  */
     }
#endif
  return ((BOOLEAN)ModeIdIndex);
}
/*end oem util */

BOOLEAN
SiS_CheckMemorySize(ULONG ROMAddr,PSIS_HW_DEVICE_INFO HwDeviceExtension,
                    USHORT ModeNo,USHORT ModeIdIndex)
{
  USHORT memorysize,modeflag,temp;

  if((HwDeviceExtension->jChipType == SIS_550)||   /* 05/02/01 ynlai for sis550 */
     (HwDeviceExtension->jChipType == SIS_640)||   /* 08/20/01 chiawen for 640/740 */
     (HwDeviceExtension->jChipType == SIS_740)||   /* 09/03/01 chiawen for 640/740 */
     (HwDeviceExtension->jChipType == SIS_650))    /* 09/03/01 chiawen for 650 */
    return(TRUE);

  if (ModeNo<=0x13) {
    modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
  } else {
    modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
  }

/*  ModeType=modeflag&ModeInfoFlag;                           	   Get mode type  */

  memorysize=modeflag&MemoryInfoFlag;
  memorysize=memorysize>MemorySizeShift;
  memorysize++;                                        		/* Get memory size */

  temp=SiS_GetReg1(SiS_P3c4,0x14);                             	/* Get DRAM Size    */
  if((HwDeviceExtension->jChipType == SIS_315H)||
     (HwDeviceExtension->jChipType == SIS_315PRO)) {
    temp= 1 << ((temp&0x0F0)>>4);				/* 315 */
    if ((temp&0x0c)==0x08) { /* DDR asymetric */
      temp += temp/2;
    } else {
      if ((temp&0x0c)!=0) temp <<= 1;
    }
  } else { 							/* 300, 540, 630, 730 */
    temp=temp&0x3F;
    temp++;
  }

  if(temp<memorysize) return(FALSE);
  else return(TRUE);
}

UCHAR
SiS_GetModePtr(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex)
{
   UCHAR index;

   if(ModeNo<=0x13) {
     	index = SiS_SModeIDTable[ModeIdIndex].St_StTableIndex;
   } else {
     	if(SiS_ModeType <= 0x02) index=0x1B;    /* 02 -> ModeEGA  */
     	else index=0x0F;
   }
   return index;       /* Get SiS_StandTable index  */
}

void
SiS_SetSeqRegs(ULONG ROMAddr,USHORT StandTableIndex)
{
   UCHAR SRdata;
   USHORT i;

   SiS_SetReg1(SiS_P3c4,0x00,0x03);           /* Set SR0  */
   SRdata=SiS_StandTable[StandTableIndex].SR[0];

   if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
      	if(SiS_VBInfo&SetCRT2ToLCDA) {
        	SRdata=SRdata|0x01;
        }
   }

   if(SiS_IF_DEF_LVDS==1) {
     if(SiS_IF_DEF_CH7005==1) {
       if(SiS_VBInfo&SetCRT2ToTV) {
         if(SiS_VBInfo&SetInSlaveMode) {
           SRdata=SRdata|0x01;        					/* 8 dot clock  */
         }
       }
     }
     if(SiS_VBInfo&SetCRT2ToLCD) {
       if(SiS_VBInfo&SetInSlaveMode) {
         SRdata=SRdata|0x01;          					/* 8 dot clock  */
       }
     }
   }

   SRdata=SRdata|0x20;                					/* screen off  */
   SiS_SetReg1(SiS_P3c4,0x01,SRdata);         				/* Set SR1 */
   for(i=02;i<=04;i++) {
     	SRdata=SiS_StandTable[StandTableIndex].SR[i-1];   		/* Get SR2,3,4 from file */
     	SiS_SetReg1(SiS_P3c4,i,SRdata);                       		/* Set SR2 3 4 */
   }
}

void
SiS_SetMiscRegs(ULONG ROMAddr,USHORT StandTableIndex)
{
   UCHAR Miscdata;

   Miscdata = SiS_StandTable[StandTableIndex].MISC;    /* Get Misc from file */
   if( (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))) {
      if(SiS_VBInfo&SetCRT2ToLCDA) {
        Miscdata=Miscdata|0x0C;
      }
    }
   SiS_SetReg3(SiS_P3c2,Miscdata);                         /* Set Misc(3c2) */
}

void
SiS_SetCRTCRegs(ULONG ROMAddr,PSIS_HW_DEVICE_INFO HwDeviceExtension,
                USHORT StandTableIndex)
{
  UCHAR CRTCdata;
  USHORT i;

  CRTCdata=(UCHAR)SiS_GetReg1(SiS_P3d4,0x11);
  CRTCdata=CRTCdata&0x7f;
  SiS_SetReg1(SiS_P3d4,0x11,CRTCdata);                     /* Unlock CRTC */

  for(i=0;i<=0x18;i++) {
     CRTCdata=SiS_StandTable[StandTableIndex].CRTC[i];     /* Get CRTC from file */
     SiS_SetReg1(SiS_P3d4,i,CRTCdata);                     /* Set CRTC(3d4) */
  }
  if((HwDeviceExtension->jChipType==SIS_630)&&
     (HwDeviceExtension->jChipRevision==0x30)) {           /* for 630S0 */
    if(SiS_VBInfo&SetInSlaveMode) {
      if(SiS_VBInfo&(SetCRT2ToLCD|SetCRT2ToTV)) {
        SiS_SetReg1(SiS_P3d4,0x18,0xFE);
      }
    }
  }
}

void
SiS_SetATTRegs(ULONG ROMAddr,USHORT StandTableIndex)
{
   UCHAR ARdata;
   USHORT i;

   for(i=0;i<=0x13;i++) {
    ARdata = SiS_StandTable[StandTableIndex].ATTR[i];     /* Get AR for file  */
    if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
      if(SiS_VBInfo&SetCRT2ToLCDA) {
         if(i==0x13) ARdata=0;
      }
    }
    if (SiS_IF_DEF_LVDS==1) {                             /*for LVDS  */
       if(SiS_IF_DEF_CH7005==1) {
         if(SiS_VBInfo&SetCRT2ToTV) {
           if(SiS_VBInfo&SetInSlaveMode) {
             if(i==0x13) ARdata=0;
           }
         }
       }
       if(SiS_VBInfo&SetCRT2ToLCD) {
         if(SiS_VBInfo&SetInSlaveMode) {
           if(SiS_LCDInfo&LCDNonExpanding) {
             if(i==0x13) ARdata=0;
           }
         }
       }
     }
     SiS_GetReg2(SiS_P3da);                              /* reset 3da  */
     SiS_SetReg3(SiS_P3c0,i);                            /* set index  */
     SiS_SetReg3(SiS_P3c0,ARdata);                       /* set data   */
   }
   SiS_GetReg2(SiS_P3da);                                /* reset 3da  */
   SiS_SetReg3(SiS_P3c0,0x14);                           /* set index  */
   SiS_SetReg3(SiS_P3c0,0x00);                           /* set data   */

   SiS_GetReg2(SiS_P3da);                                /* Enable Attribute  */
   SiS_SetReg3(SiS_P3c0,0x20);
}

void
SiS_SetGRCRegs(ULONG ROMAddr,USHORT StandTableIndex)
{
   UCHAR GRdata;
   USHORT i;

   for(i=0;i<=0x08;i++) {
     GRdata = SiS_StandTable[StandTableIndex].GRC[i]; 	  /* Get GR from file */
     SiS_SetReg1(SiS_P3ce,i,GRdata);                      /* Set GR(3ce) */
   }

   if(SiS_ModeType>ModeVGA) {
     GRdata=(UCHAR)SiS_GetReg1(SiS_P3ce,0x05);
     GRdata=GRdata&0xBF;                                  /* 256 color disable */
     SiS_SetReg1(SiS_P3ce,0x05,GRdata);
   }
}

void
SiS_ClearExt1Regs()
{
  USHORT i;

  for(i=0x0A;i<=0x0E;i++) SiS_SetReg1(SiS_P3c4,i,0x00);      /* Clear SR0A-SR0E */
}

void
SiS_SetSync(ULONG ROMAddr,USHORT RefreshRateTableIndex)
{
  USHORT sync;
  USHORT temp;

  sync = SiS_RefIndex[RefreshRateTableIndex].Ext_InfoFlag >> 8;  /* di+0x00 */

  sync=sync&0xC0;
  temp=0x2F;
  temp=temp|sync;
  SiS_SetReg3(SiS_P3c2,temp);                                 /* Set Misc(3c2) */
}

void
SiS_SetCRT1CRTC(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                USHORT RefreshRateTableIndex)
{
  UCHAR  index;
  UCHAR  data;
  USHORT temp,tempah,i,modeflag,j;
  USHORT ResInfo,DisplayType;
  SiS_LCDACRT1DataStruct *LCDACRT1Ptr=NULL;

  if((SiS_VBType&VB_SIS302B)&&(SiS_VBInfo&SetCRT2ToLCDA)
  	&& (SiS_IF_DEF_LVDS==0) ) { /* TW: Added LVDS check */
     /*add crt1ptr*/
     if(ModeNo<=0x13) {
       modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;      /* si+St_ResInfo */
     } else {
       modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;     /* si+Ext_ResInfo */
     }
     temp= SiS_GetLCDACRT1Ptr(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex,
                       &ResInfo,&DisplayType);
  
     switch(DisplayType) {
      case 0 : LCDACRT1Ptr = SiS_LCDACRT1800x600_1;           break;
      case 1 : LCDACRT1Ptr = SiS_LCDACRT11024x768_1;          break;
      case 2 : LCDACRT1Ptr = SiS_LCDACRT11280x1024_1;         break;
      case 3 : LCDACRT1Ptr = SiS_LCDACRT1800x600_1_H;         break;
      case 4 : LCDACRT1Ptr = SiS_LCDACRT11024x768_1_H;        break;
      case 5 : LCDACRT1Ptr = SiS_LCDACRT11280x1024_1_H;       break;
      case 6 : LCDACRT1Ptr = SiS_LCDACRT1800x600_2;           break;
      case 7 : LCDACRT1Ptr = SiS_LCDACRT11024x768_2;          break;
      case 8 : LCDACRT1Ptr = SiS_LCDACRT11280x1024_2;         break;
      case 9 : LCDACRT1Ptr = SiS_LCDACRT1800x600_2_H;         break;
      case 10: LCDACRT1Ptr = SiS_LCDACRT11024x768_2_H;        break;
      case 11: LCDACRT1Ptr = SiS_LCDACRT11280x1024_2_H;       break;
      /*case 12: LCDACRT1Ptr = SiS_CHTVCRT1UNTSC;             break;
      case 13: LCDACRT1Ptr = SiS_CHTVCRT1ONTSC;               break;
      case 14: LCDACRT1Ptr = SiS_CHTVCRT1UPAL;                break;
      case 15: LCDACRT1Ptr = SiS_CHTVCRT1OPAL;                break;*/
     }

     tempah=(UCHAR)SiS_GetReg1(SiS_P3d4,0x11);                        /*unlock cr0-7  */
     tempah=tempah&0x7F;
     SiS_SetReg1(SiS_P3d4,0x11,tempah);
     tempah = (LCDACRT1Ptr+ResInfo)->CR[0];
     SiS_SetReg1(SiS_P3d4,0x0,tempah);
     for(i=0x01,j=1;i<=0x07;i++,j++){
       tempah = (LCDACRT1Ptr+ResInfo)->CR[j];
       SiS_SetReg1(SiS_P3d4,i,tempah);
     }
     /* for(i=0x06,j=5;i<=0x07;i++,j++){
       tempah = (LCDACRT1Ptr+ResInfo)->CR[j];
       SiS_SetReg1(SiS_P3d4,i,tempah);
     }*/
     for(i=0x10,j=8;i<=0x12;i++,j++){
       tempah = (LCDACRT1Ptr+ResInfo)->CR[j];
       SiS_SetReg1(SiS_P3d4,i,tempah);
     }
     for(i=0x15,j=11;i<=0x16;i++,j++){
       tempah =(LCDACRT1Ptr+ResInfo)->CR[j];
       SiS_SetReg1(SiS_P3d4,i,tempah);
     }
     for(i=0x0A,j=13;i<=0x0C;i++,j++){
       tempah = (LCDACRT1Ptr+ResInfo)->CR[j];
       SiS_SetReg1(SiS_P3c4,i,tempah);
     }
  
     tempah = (LCDACRT1Ptr+ResInfo)->CR[16];
     tempah=tempah&0x0E0;
     SiS_SetReg1(SiS_P3c4,0x0E,tempah);
  
     tempah = (LCDACRT1Ptr+ResInfo)->CR[16];
     tempah=tempah&0x01;
     tempah=tempah<<5;
     if(modeflag&DoubleScanMode){
       tempah=tempah|0x080;
     }
     SiS_SetRegANDOR(SiS_P3d4,0x09,~0x020,tempah);
     if(SiS_ModeType>0x03) SiS_SetReg1(SiS_P3d4,0x14,0x4F);
  /*end 301b*/
  }
  else {  /* LVDS */
     index=SiS_RefIndex[RefreshRateTableIndex].Ext_CRT1CRTC;  		/* Get index */
     index=index&0x3F;

     if(SiS_IF_DEF_FSTN) {						/*fstn*/
        index=index&0x7F;	/* TW: First AND it with 3F, now with 7F ??? */
     }
     data=(UCHAR)SiS_GetReg1(SiS_P3d4,0x11);
     data=data&0x7F;
     SiS_SetReg1(SiS_P3d4,0x11,data);                             	/* Unlock CRTC */

     for(i=0,j=0;i<=07;i++,j++) {
       data=SiS_CRT1Table[index].CR[i];
       SiS_SetReg1(SiS_P3d4,j,data);
     }
     for(j=0x10;i<=10;i++,j++) {
       data=SiS_CRT1Table[index].CR[i];
       SiS_SetReg1(SiS_P3d4,j,data);
     }
     for(j=0x15;i<=12;i++,j++) {
       data=SiS_CRT1Table[index].CR[i];
       SiS_SetReg1(SiS_P3d4,j,data);
     }
     for(j=0x0A;i<=15;i++,j++) {
       data=SiS_CRT1Table[index].CR[i];
       SiS_SetReg1(SiS_P3c4,j,data);
     }

     data=SiS_CRT1Table[index].CR[16];
     data=data&0xE0;
     SiS_SetReg1(SiS_P3c4,0x0E,data);

     data=(UCHAR)SiS_GetReg1(SiS_P3d4,0x09);
     data=data&0xDF;                                      /* clear CR9 D[5] */
     i=SiS_CRT1Table[index].CR[16];
     i=i&0x01;
     i=i<<5;
     data=data|i;

     if (ModeNo<=0x13)
       i = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
     else
       i = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;

     i=i&DoubleScanMode;
     if(i) data=data|0x80;
     SiS_SetReg1(SiS_P3d4,0x09,data);

     if(SiS_ModeType>0x03) SiS_SetReg1(SiS_P3d4,0x14,0x4F);
  }
}

/* TW: Set offset ("pitch") - overruled by SetPitch in XF86 */
void
SiS_SetCRT1Offset(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                  USHORT RefreshRateTableIndex,
		  PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
   USHORT temp,ah,al;
   USHORT temp2,i;
   USHORT DisplayUnit;

   /* Alan */
   temp = SiS_EModeIDTable[ModeIdIndex].Ext_ModeInfo;
   if(HwDeviceExtension->jChipType >= SIS_315H) {
     temp=temp>>8;         /* sis310 */
   } else {
     temp=temp>>4;         /* sis300 */
   }
   temp=SiS_ScreenOffset[temp];

   if((ModeNo>=0x7C)&&(ModeNo<=0x7E)) {
     /* TW: 1280x960 */
     temp=0x6B;  /* TW: Why? Offset for 1280 is normally 0x50! */
     temp2=ModeNo-0x7C;
     /* TW: SiS_ModeType-ModeEGA (below) is 1,3 or 5, not 0,1,or 2 */
     temp2=(temp2*2)+1; /* TW: Maybe this is more correct? */
   }
   else {
     temp2 = SiS_RefIndex[RefreshRateTableIndex].Ext_InfoFlag;
     temp2=temp2&InterlaceMode;
     if(temp2) temp=temp<<1;
     temp2=SiS_ModeType-ModeEGA;
   }

   switch (temp2) {
     case 0 : temp2=1; break;
     case 1 : temp2=2; break;
     case 2 : temp2=4; break;
     case 3 : temp2=4; break;
     case 4 : temp2=6; break;
     case 5 : temp2=8; break;
   }
   temp=temp*temp2;
   DisplayUnit=temp;

   temp2=temp;
   temp=temp>>8;                                         /* ah */
   temp=temp&0x0F;
   i=SiS_GetReg1(SiS_P3c4,0x0E);
   i=i&0xF0;
   i=i|temp;
   SiS_SetReg1(SiS_P3c4,0x0E,i);

   temp=(UCHAR)temp2;
   temp=temp&0xFF;                                        /* al */
   SiS_SetReg1(SiS_P3d4,0x13,temp);

   temp2 = SiS_RefIndex[RefreshRateTableIndex].Ext_InfoFlag;
   temp2=temp2&InterlaceMode;
   if(temp2) DisplayUnit>>=1;

   DisplayUnit=DisplayUnit<<5;
   ah=(DisplayUnit&0xff00)>>8;
   al=DisplayUnit&0x00ff;
   if(al==0) ah=ah+1;
   else ah=ah+2;
   SiS_SetReg1(SiS_P3c4,0x10,ah);
}

void
SiS_SetCRT1VCLK(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                PSIS_HW_DEVICE_INFO HwDeviceExtension,
		USHORT RefreshRateTableIndex)
{
  UCHAR  index,data;
  USHORT vclkindex;

  /* TW: Now I'm confused. Is IF_DEF_LVDS 1 even for 301B/302B/301LV/302lV?
   *     This functions could make one believe that LVDS is 0 for these. But
   *     in this case SetGroup1_LVDS doesn't make sense (see init301.c)
   *     So my conclusion is that LVDS = 1 for some of the SiS bridges as
   *     well, i just don't know which ones...
   */

  if(SiS_IF_DEF_LVDS==1) {

        /* TW: Do this if IF_DEF_LVDS is 1 */

    	index = SiS_RefIndex[RefreshRateTableIndex].Ext_CRTVCLK;

    	/*if(HwDeviceExtension->jChipType < SIS_315H) { */
    	index=index&0x3F;
    	/*}*/
    	data = SiS_GetReg1(SiS_P3c4,0x31) & 0xCF;
	SiS_SetReg1(SiS_P3c4,0x31,data);
	/* SiS_SetReg1(SiS_P3c4,0x31,0x00); 	     */                 /* for300 */

    	SiS_SetReg1(SiS_P3c4,0x2B,SiS_VCLKData[index].SR2B);
    	SiS_SetReg1(SiS_P3c4,0x2C,SiS_VCLKData[index].SR2C);
	
    	if(HwDeviceExtension->jChipType < SIS_315H)
      		SiS_SetReg1(SiS_P3c4,0x2D,0x80);                        /* for300 series */
   	else
      		SiS_SetReg1(SiS_P3c4,0x2D,0x01);                        /* for310 series */
  }

  else if( (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
               && (SiS_VBInfo&SetCRT2ToLCDA)
	       && (SiS_IF_DEF_LVDS==0) ){  /* TW: I did NOT include this here */

        /* TW: Do this if bridge type is 301B/302B/301LV/302LV
	 *     (This can only be called if LVDS is 0...)
	 */
	 
    	vclkindex = SiS_GetVCLK2Ptr(ROMAddr,ModeNo,ModeIdIndex,
	                     RefreshRateTableIndex,HwDeviceExtension);
    	data = SiS_GetReg1(SiS_P3c4,0x31) & 0xCF;
    	SiS_SetReg1(SiS_P3c4,0x31,data);

    	data = SiS_VBVCLKData[vclkindex].Part4_A;
    	SiS_SetReg1(SiS_P3c4,0x2B,data);
    	data = SiS_VBVCLKData[vclkindex].Part4_B;
    	SiS_SetReg1(SiS_P3c4,0x2C,data);

    	if(HwDeviceExtension->jChipType < SIS_315H)
    		SiS_SetReg1(SiS_P3c4,0x2D,0x80);                        /* for300 series */
    	else
    		SiS_SetReg1(SiS_P3c4,0x2D,0x01);
  }
  else {

	/* TW: Do this if LVDS is 0 and the brigde type is NOT
	 *     301B/302B/301LV/302LV. This leaves only 301...
	 *     (or 301B/.. if not LCDA)
	 */

    	index = SiS_RefIndex[RefreshRateTableIndex].Ext_CRTVCLK;
    	/*if(HwDeviceExtension->jChipType < SIS_315H) { */
    	index=index&0x3F;
    	/*}*/
    	data = SiS_GetReg1(SiS_P3c4,0x31) & 0xCF;
	SiS_SetReg1(SiS_P3c4,0x31,data);
	/*SiS_SetReg1(SiS_P3c4,0x31,0x00); */                       	/* for300 */

    	SiS_SetReg1(SiS_P3c4,0x2B,SiS_VCLKData[index].SR2B);
    	SiS_SetReg1(SiS_P3c4,0x2C,SiS_VCLKData[index].SR2C);
    	if(HwDeviceExtension->jChipType < SIS_315H)
      		SiS_SetReg1(SiS_P3c4,0x2D,0x80);                        /* for300 series */
    	else
      		SiS_SetReg1(SiS_P3c4,0x2D,0x01);                        /* for310 series */
  }
}

void
SiS_IsLowResolution(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex)
{
  USHORT data;
  USHORT ModeFlag;

  data = SiS_GetReg1(SiS_P3c4,0x0F);
  data = data&0x7F;
  SiS_SetReg1(SiS_P3c4,0x0F,data);

  if (ModeNo>0x13) {
    ModeFlag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
    if ((ModeFlag & HalfDCLK) && (ModeFlag & DoubleScanMode)) {
      data = SiS_GetReg1(SiS_P3c4,0x0F);
      data = data|0x80;
      SiS_SetReg1(SiS_P3c4,0x0F,data);
      data = SiS_GetReg1(SiS_P3c4,0x01);
      data = data&0xF7;
      SiS_SetReg1(SiS_P3c4,0x01,data);
    }
  }
}

void
SiS_SetCRT1ModeRegs(ULONG ROMAddr,PSIS_HW_DEVICE_INFO HwDeviceExtension,
                    USHORT ModeNo,USHORT ModeIdIndex,USHORT RefreshRateTableIndex)
{
  USHORT data,data2,data3;
  USHORT infoflag=0,modeflag;
  USHORT resindex,xres;

  if (ModeNo>0x13) {
    	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
    	infoflag = SiS_RefIndex[RefreshRateTableIndex].Ext_InfoFlag;
  } else {
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;    /* si+St_ModeFlag */
  }

  SiS_SetRegANDOR(SiS_P3c4,0x1F,0x3F,0x00); 	/* DAC pedestal */

  if(ModeNo>0x13) data=infoflag;
  else data=0;
  data2=SiS_GetReg1(SiS_P3c4,0x06) & 0xC0;  /* TW: Preserve Xv display mode!!! */
  if(ModeNo>0x13) {
    if(SiS_ModeType>0x02) {
       data2=data2|0x02;
       data3=SiS_ModeType-ModeVGA;
       data3=data3<<2;
       data2=data2|data3;
    }
  }
  data=data&InterlaceMode;
  if(data) data2=data2|0x20;
  SiS_SetReg1(SiS_P3c4,0x06,data2);

  if((HwDeviceExtension->jChipType==SIS_630)||
     (HwDeviceExtension->jChipType==SIS_540)||
     (HwDeviceExtension->jChipType==SIS_730)) {
    resindex=SiS_GetResInfo(ROMAddr,ModeNo,ModeIdIndex);
    if(ModeNo<=0x13)
      	xres=SiS_StResInfo[resindex].HTotal;
    else
      	xres=SiS_ModeResInfo[resindex].HTotal;                         /* xres->ax */
    data=0x0000;
    if(infoflag&InterlaceMode) {
      if(xres==1024) data=0x0035;
      if(xres==1280) data=0x0048;
    }
    data2=data&0x00FF;
    SiS_SetRegANDOR(SiS_P3d4,0x19,0xFF,data2);
    data2=(data&0xFF00)>>8;
    SiS_SetRegANDOR(SiS_P3d4,0x19,0xFC,data2);
  }

  if(modeflag&HalfDCLK) {
    SiS_SetRegANDOR(SiS_P3c4,0x01,0xFF,0x01);
  }

  if((HwDeviceExtension->jChipType==SIS_630)||
     (HwDeviceExtension->jChipType==SIS_540)||
     (HwDeviceExtension->jChipType==SIS_730)){
     /* TW: This was missing here for modes > 1024x768: */
    if(modeflag&LineCompareOff) {
      SiS_SetRegANDOR(SiS_P3c4,0x0F,0xF7,0x08);
    } else {
      SiS_SetRegANDOR(SiS_P3c4,0x0F,0xF7,0x00);
    }
  } else {
    if(modeflag&LineCompareOff) {
      SiS_SetRegANDOR(SiS_P3c4,0x0F,0xF7,0x08);
    } else {
      SiS_SetRegANDOR(SiS_P3c4,0x0F,0xF7,0x00);
    }
  }

  data=0x60;
  if(SiS_ModeType!=ModeText) {
      data=data^0x60;
      if(SiS_ModeType!=ModeEGA) {
        data=data^0xA0;
      }
  }
  SiS_SetRegANDOR(SiS_P3c4,0x21,0x1F,data);
}

void
SiS_SetVCLKState(ULONG ROMAddr,PSIS_HW_DEVICE_INFO HwDeviceExtension,
                 USHORT ModeNo,USHORT RefreshRateTableIndex)
{
   USHORT data,data2=0;
   USHORT VCLK;
   UCHAR  index=0;

  if (ModeNo<=0x13) VCLK=0;
  else {
    index = SiS_RefIndex[RefreshRateTableIndex].Ext_CRTVCLK;
    /*if(HwDeviceExtension->jChipType < SIS_315H) { */
    index=index&0x3F;
    /*}*/
    VCLK = SiS_VCLKData[index].CLOCK;
  }

  if(HwDeviceExtension->jChipType < SIS_315H) {
    data2=0x00;
    if(VCLK>150) data2=data2|0x80;
    SiS_SetRegANDOR(SiS_P3c4,0x07,0x7B,data2); 	/* DAC speed */

    data2=0x00;
    if(VCLK>=150) data2=data2|0x08;       	/* VCLK > 150 */
    SiS_SetRegANDOR(SiS_P3c4,0x32,0xF7,data2);
  } else { /* 310 series */
    data=SiS_GetReg1(SiS_P3c4,0x32);
    data=data&0xf3;
    if(VCLK>=200) data=data|0x0c;         	/* VCLK > 200 */
    SiS_SetReg1(SiS_P3c4,0x32,data);
    data=SiS_GetReg1(SiS_P3c4,0x1F);	  	/* DAC pedestal */
    data &= 0xE7;
    if(VCLK<200) data |= 0x10;
    SiS_SetReg1(SiS_P3c4,0x1F,data);	  	/* DAC pedestal */
  }

  if(VCLK<135) data2=0x03;
  if((VCLK>=135)&&(VCLK<160)) data2=0x02;
  if((VCLK>=160)&&(VCLK<260)) data2=0x01;
  if(VCLK>260) data2=0x00;
  /* disable 24bit palette RAM gamma correction  */
  if(HwDeviceExtension->jChipType==SIS_540) {
    	if((VCLK==203)||(VCLK<234)) data2=0x02;
  }
  SiS_SetRegANDOR(SiS_P3c4,0x07,0xFC,data2);  	/* DAC speed */
}

void
SiS_LoadDAC(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex)
{
   USHORT data,data2;
   USHORT time,i,j,k;
   USHORT m,n,o;
   USHORT si,di,bx,dl;
   USHORT al,ah,dh;
   USHORT *table=NULL;

   if (ModeNo<=0x13)
        data = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
   else
        data = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;

   data=data&DACInfoFlag;
   time=64;
   if(data==0x00) table=SiS_MDA_DAC;
   if(data==0x08) table=SiS_CGA_DAC;
   if(data==0x10) table=SiS_EGA_DAC;
   if(data==0x18) {
     time=256;
     table=SiS_VGA_DAC;
   }
   if(time==256) j=16;
   else j=time;

   SiS_SetReg3(SiS_P3c6,0xFF);
   SiS_SetReg3(SiS_P3c8,0x00);

   for(i=0;i<j;i++) {
      data=table[i];
      for(k=0;k<3;k++) {
        data2=0;
        if(data&0x01) data2=0x2A;
        if(data&0x02) data2=data2+0x15;
        SiS_SetReg3(SiS_P3c9,data2);
        data=data>>2;
      }
   }

   if(time==256) {
      for(i=16;i<32;i++) {
         data=table[i];
         for(k=0;k<3;k++) SiS_SetReg3(SiS_P3c9,data);
      }
      si=32;
      for(m=0;m<9;m++) {
         di=si;
         bx=si+0x04;
         dl=0;
         for(n=0;n<3;n++) {
            for(o=0;o<5;o++) {
              dh=table[si];
              ah=table[di];
              al=table[bx];
              si++;
              SiS_WriteDAC(dl,ah,al,dh);
            }         /* for 5 */
            si=si-2;
            for(o=0;o<3;o++) {
              dh=table[bx];
              ah=table[di];
              al=table[si];
              si--;
              SiS_WriteDAC(dl,ah,al,dh);
            }         /* for 3 */
            dl++;
         }            /* for 3 */
         si=si+5;
      }               /* for 9 */
   }
}

void
SiS_WriteDAC(USHORT dl, USHORT ah, USHORT al, USHORT dh)
{
  USHORT temp;
  USHORT bh,bl;

  bh=ah;
  bl=al;
  if(dl!=0) {
    temp=bh;
    bh=dh;
    dh=temp;
    if(dl==1) {
       temp=bl;
       bl=dh;
       dh=temp;
    } else {
       temp=bl;
       bl=bh;
       bh=temp;
    }
  }
  SiS_SetReg3(SiS_P3c9,(USHORT)dh);
  SiS_SetReg3(SiS_P3c9,(USHORT)bh);
  SiS_SetReg3(SiS_P3c9,(USHORT)bl);
}

ULONG
GetDRAMSize(PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  ULONG   AdapterMemorySize=0;
#ifdef SIS315H
  USHORT  counter;
#endif
  
#ifdef SIS315H
  if ((HwDeviceExtension->jChipType == SIS_315H)||
      (HwDeviceExtension->jChipType == SIS_315PRO)) {
    	counter = SiS_GetReg1(SiS_P3c4,0x14)&0xF0; 	/*get memory size*/
    	counter >>= 4;
    	AdapterMemorySize= 1 << counter;
    	AdapterMemorySize *= 1024*1024;
  } else {
    	if ((HwDeviceExtension->jChipType == SIS_550)||
            (HwDeviceExtension->jChipType == SIS_640)||
            (HwDeviceExtension->jChipType == SIS_740)||
            (HwDeviceExtension->jChipType == SIS_650)) {
      		counter = SiS_GetReg1(SiS_P3c4,0x14)&0x3F; /*get memory size*/
      		counter++;
      		AdapterMemorySize = counter * 4;
      		AdapterMemorySize *= 1024*1024;
		/* TW: FIXME: Some 550 BIOSes don't set up SR14 correctly. We
		 * will have to read PCI configuration for a correct memory
		 * size. (However, this is checked in framebuffer driver;
		 * X does not use this function but if it ever will, this will
		 * become an issue.)
		 */
    	}
  }
#endif

#ifdef SIS300
  if ((HwDeviceExtension->jChipType==SIS_300)||
      (HwDeviceExtension->jChipType==SIS_540)||
      (HwDeviceExtension->jChipType==SIS_630)||
      (HwDeviceExtension->jChipType==SIS_730)) {
      	AdapterMemorySize = SiS_GetReg1(SiS_P3c4,0x14); /*get memory size*/
      	AdapterMemorySize = AdapterMemorySize&0x3F;
      	AdapterMemorySize++;
      	AdapterMemorySize *= 1024*1024;
  }
#endif

  return AdapterMemorySize;
}

void
SiS_ClearBuffer(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT ModeNo)
{
  PVOID   VideoMemoryAddress = (PVOID)HwDeviceExtension->pjVideoMemoryAddress;
  ULONG   AdapterMemorySize  = (ULONG)HwDeviceExtension->ulVideoMemorySize;
  PUSHORT pBuffer;
  int i;

  if (SiS_ModeType>=ModeEGA) {
    if (ModeNo>0x13) {
      AdapterMemorySize=GetDRAMSize(HwDeviceExtension);
      SiS_SetMemory(VideoMemoryAddress,AdapterMemorySize,0);
    } else {
      pBuffer = VideoMemoryAddress;
      for (i=0;i<0x4000;i++)
        pBuffer[i]=0x0000;
    }
  } else {
    pBuffer = VideoMemoryAddress;
    if (SiS_ModeType<ModeCGA) {
      for (i=0;i<0x4000;i++)
        pBuffer[i]=0x0720;
    } else {
      SiS_SetMemory(VideoMemoryAddress,0x8000,0);
    }
  }
}

void
SiS_DisplayOn(void)
{
   SiS_SetRegANDOR(SiS_P3c4,0x01,0xDF,0x00);
}

void
SiS_DisplayOff(void)
{
   SiS_SetRegANDOR(SiS_P3c4,0x01,0xDF,0x20);
}


/* ========================================== */
/*  SR CRTC GR */
void
SiS_SetReg1(
   USHORT  port,
   USHORT  index,
   USHORT  data
   )
{
   OutPortByte(port,index);
   OutPortByte(port+1,data);
   
   /*
   _asm
   {
         mov      dx, port                      
         mov      ax, index                     
         mov      bx, data                      
         out      dx, al
         mov      ax, bx
         inc      dx
         out      dx, al
   }
   */

}

/* ========================================== */
/*  AR(3C0) */
void
SiS_SetReg2(
   USHORT  port,
   USHORT  index,
   USHORT  data
   )
{

   InPortByte(port+0x3da-0x3c0);
   OutPortByte(SiS_P3c0,index);
   OutPortByte(SiS_P3c0,data);
   OutPortByte(SiS_P3c0,0x20);

   /*
   _asm
   {
         mov      dx, port                      
         mov      cx, index                     
         mov      bx, data                      

         add      dx, 3dah-3c0h
         in       al, dx

         mov      ax, cx
         mov      dx, 3c0h
         out      dx, al
         mov      ax, bx
         out      dx, al

         mov      ax, 20h
         out      dx, al
   }
   */

}

/* ========================================== */
void
SiS_SetReg3(
   USHORT  port,
   USHORT  data
   )
{

   OutPortByte(port,data);
   
   /*
   _asm
   {
         mov      dx, port                      
         mov      ax, data                      
         out      dx, al

   }
   */

}

/* ========================================== */
void
SiS_SetReg4(
   USHORT  port,
   ULONG   data
   )
{

   OutPortLong(port,data);

   /*
   _asm
   {
         mov      dx, port                      ;; port
         mov      eax, data                      ;; data
         out      dx, eax

   }
   */
}

/* ========================================= */
UCHAR
SiS_GetReg1(
   USHORT  port,
   USHORT  index
   )
{
   UCHAR   data;


   OutPortByte(port,index);
   data = InPortByte(port+1);

   /*
   _asm
   {
         mov      dx, port                      ;; port
         mov      ax, index                     ;; index

         out      dx, al
         mov      ax, bx
         inc      dx
         xor      eax, eax
         in       al, dx
         mov      data, al
   }
   */
   return(data);
}
/* ========================================== */
UCHAR
SiS_GetReg2(
   USHORT  port
   )
{
   UCHAR   data;

   data= InPortByte(port);

   /*
   _asm
   {
         mov      dx, port                      ;; port
         xor      eax, eax
         in       al, dx
         mov      data, al
   }
   */
   return(data);
}

/* ========================================== */
ULONG
SiS_GetReg3(
   USHORT  port
   )
{
   ULONG   data;


   data = InPortLong(port);

   /*
   _asm
   {
         mov      dx, port                      ;; port
         xor      eax, eax
         in       eax, dx
         mov      data, eax
   }
   */
   return(data);
}

/* ========================================== */
void
SiS_ClearDAC(ULONG   port)
{
   int i;

   OutPortByte(port, 0);
   port++;
   for (i=0;i<256*3;i++) {
      OutPortByte(port, 0);
   }

}
/* ========================================== */

void
SiS_SetInterlace(ULONG ROMAddr, USHORT ModeNo,USHORT RefreshRateTableIndex)
{
  ULONG Temp;
  USHORT data,Temp2;

  if (ModeNo<=0x13) return;
  Temp = (ULONG)SiS_GetReg1(SiS_P3d4, 0x01);
  Temp++;
  Temp=Temp*8;

  if(Temp==1024) data=0x0035;
  else if(Temp==1280) data=0x0048;
  else data=0x0000;

  Temp2 = SiS_RefIndex[RefreshRateTableIndex].Ext_InfoFlag;
  Temp2 &= InterlaceMode;
  if(Temp2 == 0) data=0x0000;

  SiS_SetReg1(SiS_P3d4,0x19,data);

  Temp = (ULONG)SiS_GetReg1(SiS_P3d4, 0x1A);
  Temp2= (USHORT)(Temp & 0xFC);
  SiS_SetReg1(SiS_P3d4,0x1A,(USHORT)Temp);

  Temp = (ULONG)SiS_GetReg1(SiS_P3c4, 0x0f);
  Temp2= (USHORT)Temp & 0xBF;
  if(ModeNo==0x37) Temp2=Temp2|0x40;
  SiS_SetReg1(SiS_P3d4,0x1A,(USHORT)Temp2);
}

void
SiS_SetCRT1FIFO(ULONG ROMAddr,USHORT ModeNo,PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT  data;

  data=SiS_GetReg1(SiS_P3c4,0x3D);
  data &= 0xfe;
  SiS_SetReg1(SiS_P3c4,0x3D,data);  /* diable auto-threshold */
  if (ModeNo>0x13) {
    SiS_SetReg1(SiS_P3c4,0x08,0x34);
    data=SiS_GetReg1(SiS_P3c4,0x09);
    data &= 0xF0;
    SiS_SetReg1(SiS_P3c4,0x09,data);

    data=SiS_GetReg1(SiS_P3c4,0x3D);
    data |= 0x01;
    SiS_SetReg1(SiS_P3c4,0x3D,data);
  } else {
    SiS_SetReg1(SiS_P3c4,0x08,0xAE);
    data=SiS_GetReg1(SiS_P3c4,0x09);
    data &= 0xF0;
    SiS_SetReg1(SiS_P3c4,0x09,data);
  }
}

USHORT
SiS_CalcDelay(ULONG ROMAddr,USHORT key)
{
  USHORT data,data2,temp0,temp1;
  UCHAR   ThLowA[]=   {61,3,52,5,68,7,100,11,
                       43,3,42,5,54,7, 78,11,
                       34,3,37,5,47,7, 67,11};
  UCHAR   ThLowB[]=   {81,4,72,6,88,8,120,12,
                       55,4,54,6,66,8, 90,12,
                       42,4,45,6,55,8, 75,12};
  UCHAR   ThTiming[]= {1,2,2,3,0,1,1,2};

  data=SiS_GetReg1(SiS_P3c4,0x16);
  data=data>>6;
  data2=SiS_GetReg1(SiS_P3c4,0x14);
  data2=(data2>>4)&0x0C;
  data=data|data2;
  data=data<1;
  if(key==0) {
    temp0=(USHORT)ThLowA[data];
    temp1=(USHORT)ThLowA[data+1];
  } else {
    temp0=(USHORT)ThLowB[data];
    temp1=(USHORT)ThLowB[data+1];
  }

  data2=0;
  data=SiS_GetReg1(SiS_P3c4,0x18);
  if(data&0x02) data2=data2|0x01;
  if(data&0x20) data2=data2|0x02;
  if(data&0x40) data2=data2|0x04;

  data=temp1*ThTiming[data2]+temp0;
  return(data);
}

void
SiS_SetCRT1FIFO2(ULONG ROMAddr,USHORT ModeNo,PSIS_HW_DEVICE_INFO HwDeviceExtension,
                 USHORT RefreshRateTableIndex)
{
  USHORT  i,index,data,VCLK,data2,MCLK,colorth=0;
  ULONG   B,eax,ah,bl;
  USHORT  ThresholdLow=0;
  UCHAR   FQBQData[]= { 0x01,0x21,0x41,0x61,0x81,  /* TW: in BIOS at 0xf85 - identical */
                        0x31,0x51,0x71,0x91,0xb1,
                        0x00,0x20,0x40,0x60,0x80,
                        0x30,0x50,0x70,0x90,0xb0,0xFF};

  if(ModeNo>=0x13) {
    index=SiS_RefIndex[RefreshRateTableIndex].Ext_CRTVCLK;
    if(HwDeviceExtension->jChipType < SIS_315H) { /* for300 series */
      index=index&0x3F;
    }
    VCLK=SiS_VCLKData[index].CLOCK;           /* Get VCLK  */
    index=SiS_GetReg1(SiS_P3c4,0x1A);
    index=index&07;
    MCLK=SiS_MCLKData[index].CLOCK;           /* Get MCLK  */

    data2=SiS_ModeType-0x02;
      switch (data2) {
        case 0 : colorth=1; break;  /* 1 */
        case 1 : colorth=1; break;  /* 2 */
        case 2 : colorth=2; break;  /* 4 */
        case 3 : colorth=2; break;  /* 4 */
        case 4 : colorth=3; break;  /* 6 */
        case 5 : colorth=4; break;  /* 8 */
      }

    i=0;
    do{
       B=(SiS_CalcDelay2(ROMAddr,FQBQData[i])*VCLK*colorth);

       bl=B/(16*MCLK);
       if (B==bl*16*MCLK) {
         bl=bl+1;
       } else {
         bl=bl+2;  /* TW: was +1 */
       }

       if(bl>0x13) {
          if(FQBQData[i+1]==0xFF) {
             ThresholdLow=0x13;
             break;
          }
          i++;
       } else {
          ThresholdLow=bl;
          break;
       }
    } while(FQBQData[i]!=0xFF);
  }
  else {
    ThresholdLow=0x02;
  }

  data2=FQBQData[i];
  data2=(data2&0xf0)>>4;
  data2=data2<<24;

  SiS_SetReg4(0xcf8,0x80000050);
  eax=SiS_GetReg3(0xcfc);
  eax=eax&0xf0ffffff;
  eax=eax|data2;
  SiS_SetReg4(0xcfc,eax);

  ah=ThresholdLow;
  ah=ah<<4;
  ah=ah|0x0f;
  SiS_SetReg1(SiS_P3c4,0x08,ah);

  data=ThresholdLow;
  data=data&0x10;
  data=data<<1;
  SiS_SetRegANDOR(SiS_P3c4,0x0F,0xDF,data);

  SiS_SetReg1(SiS_P3c4,0x3B,0x09);

  data=ThresholdLow+3;
  if(data>0x0f) data=0x0f;
  SiS_SetRegANDOR(SiS_P3c4,0x09,0x80,data);
}

USHORT
SiS_CalcDelay2(ULONG ROMAddr,UCHAR key)
{
  USHORT data,index;	   /* TW: in BIOS at 0x1029 - identical */
  UCHAR  LatencyFactor[] ={ 97, 88, 86, 79, 77, 00,       /*; 64  bit    BQ=2   */
                            00, 87, 85, 78, 76, 54,       /*; 64  bit    BQ=1   */
                            97, 88, 86, 79, 77, 00,       /*; 128 bit    BQ=2   */
                            00, 79, 77, 70, 68, 48,       /*; 128 bit    BQ=1   */
                            80, 72, 69, 63, 61, 00,       /*; 64  bit    BQ=2   */
                            00, 70, 68, 61, 59, 37,       /*; 64  bit    BQ=1   */
                            86, 77, 75, 68, 66, 00,       /*; 128 bit    BQ=2   */
                            00, 68, 66, 59, 57, 37};      /*; 128 bit    BQ=1   */

  index=(key&0xE0)>>5;
  if(key&0x10) index=index+6;
  if(!(key&0x01)) index=index+24;
  data=SiS_GetReg1(SiS_P3c4,0x14);
  if(data&0x0080) index=index+12;

  data=LatencyFactor[index];
  return(data);
}

void
SiS_CRT2AutoThreshold(USHORT  BaseAddr)
{
  USHORT  temp1;
  USHORT  Part1Port;
  Part1Port=BaseAddr+SIS_CRT2_PORT_04;
  temp1=SiS_GetReg1(SiS_Part1Port,0x01);
  temp1 |= 0x40;
  SiS_SetReg1(SiS_Part1Port,0x01,temp1);
}

/* =============  ynlai ============== */
void
SiS_DetectMonitor(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT BaseAddr)
{
  UCHAR  DAC_TEST_PARMS[]={0x0F,0x0F,0x0F};
  UCHAR  DAC_CLR_PARMS[]={0x00,0x00,0x00};
  USHORT SR1F;

  SR1F=SiS_GetReg1(SiS_P3c4,0x1F);		/* DAC pedestal */
  SiS_SetRegANDOR(SiS_P3c4,0x1F,0xFF,0x04);
  if(SiS_IF_DEF_LVDS==0) {
    if(SiS_BridgeIsOn(BaseAddr)) {
      SiS_SetReg1(SiS_P3d4,0x30,0x41);
    }
  }
  /*SiSSetMode(HwDeviceExtension,0x03);  */ /* ynlai InitMode */

#ifdef LINUX_XF86
  SiSSetMode(HwDeviceExtension,NULL,0x2E); 
#else
  SiSSetMode(HwDeviceExtension,0x2E);   /* alan */
#endif
  SiS_SetReg3(SiS_P3c6,0xff);
  SiS_ClearDAC(SiS_P3c8);
  SiS_LongWait();
  SiS_LongWait();
  SiS_SetRegANDOR(SiS_P3d4,0x32,0xDF,0x00);
  if(SiS_TestMonitorType(DAC_TEST_PARMS[0],DAC_TEST_PARMS[1],DAC_TEST_PARMS[2])) {
    SiS_SetRegANDOR(SiS_P3d4,0x32,0xDF,0x20);
  }
  if(SiS_TestMonitorType(DAC_TEST_PARMS[0],DAC_TEST_PARMS[1],DAC_TEST_PARMS[2])) {
    SiS_SetRegANDOR(SiS_P3d4,0x32,0xDF,0x20);
  }
  SiS_TestMonitorType(DAC_CLR_PARMS[0],DAC_CLR_PARMS[1],DAC_CLR_PARMS[2]);
  SiS_SetReg1(SiS_P3c4,0x1F,SR1F);
}

USHORT
SiS_TestMonitorType(UCHAR R_DAC,UCHAR G_DAC,UCHAR B_DAC)
{
   USHORT temp,tempbx;

   tempbx=R_DAC*0x4d+G_DAC*0x97+B_DAC*0x1c;
   if(tempbx>0x80) tempbx=tempbx+0x100;
   tempbx = (tempbx&0xFF00)>>8;
   R_DAC = (UCHAR) tempbx;
   G_DAC = (UCHAR) tempbx;
   B_DAC = (UCHAR) tempbx;

   SiS_SetReg3(SiS_P3c8,0x00);
   SiS_SetReg3(SiS_P3c9,R_DAC);
   SiS_SetReg3(SiS_P3c9,G_DAC);
   SiS_SetReg3(SiS_P3c9,B_DAC);
   SiS_LongWait();
   temp=SiS_GetReg2(SiS_P3c2);
   if(temp&0x10) return(1);
   else return(0);
}

/* ---- test ----- */
void
SiS_GetSenseStatus(PSIS_HW_DEVICE_INFO HwDeviceExtension,ULONG ROMAddr)
{
  USHORT tempax=0,tempbx,tempcx,temp;
  USHORT P2reg0=0,SenseModeNo=0,OutputSelect=*pSiS_OutputSelect;
  USHORT ModeIdIndex,i;
  USHORT BaseAddr = (USHORT)HwDeviceExtension->ulIOAddress;

  if(SiS_IF_DEF_LVDS==1){
    SiS_GetPanelID();
    temp=LCDSense;
    temp=temp|SiS_SenseCHTV();
    tempbx=~(LCDSense|AVIDEOSense|SVIDEOSense);
    SiS_SetRegANDOR(SiS_P3d4,0x32,tempbx,temp);
  } else {       /* for 301 */
    if(SiS_IF_DEF_HiVision==1) {  /* for HiVision */
      tempax=SiS_GetReg1(SiS_P3c4,0x38);
      temp=tempax&0x01;
      tempax=SiS_GetReg1(SiS_P3c4,0x3A);
      temp=temp|(tempax&0x02);
      SiS_SetRegANDOR(SiS_P3d4,0x32,0xA0,temp);
    } else {
      if(SiS_BridgeIsOn(BaseAddr)) {
        P2reg0 = SiS_GetReg1(SiS_Part2Port,0x00);
        if(!SiS_BridgeIsEnable(BaseAddr,HwDeviceExtension)) {
          SenseModeNo=0x2e;
          temp = SiS_SearchModeID(ROMAddr,SenseModeNo,&ModeIdIndex);
          SiS_SetFlag = 0x00;
          SiS_ModeType = ModeVGA;
          SiS_VBInfo = SetCRT2ToRAMDAC |LoadDACFlag |SetInSlaveMode;
          SiS_SetCRT2Group301(BaseAddr,ROMAddr,SenseModeNo,HwDeviceExtension);
          for(i=0;i<20;i++) {
            SiS_LongWait();
          }
        }
        SiS_SetReg1(SiS_Part2Port,0x00,0x1c);
        tempax=0;
        tempbx=*pSiS_RGBSenseData;
        /*301b*/
	if(!(SiS_Is301B(BaseAddr))){
                tempbx=*pSiS_RGBSenseData2;
        }
        /*end 301b*/	
        tempcx=0x0E08;
        if(SiS_Sense(SiS_Part4Port,tempbx,tempcx)){
          if(SiS_Sense(SiS_Part4Port,tempbx,tempcx)){
            tempax=tempax|Monitor2Sense;
          }
        }

        tempbx=*pSiS_YCSenseData;
         /*301b*/
        if(!(SiS_Is301B(BaseAddr))){
               tempbx=*pSiS_YCSenseData2;  
        }
        /*301b*/
        tempcx=0x0604;
        if(SiS_Sense(SiS_Part4Port,tempbx,tempcx)){
          if(SiS_Sense(SiS_Part4Port,tempbx,tempcx)){
            tempax=tempax|SVIDEOSense;
          }
        }

        if(OutputSelect&BoardTVType){
          tempbx=*pSiS_VideoSenseData;
        /*301b*/
        if(!(SiS_Is301B(BaseAddr))){
             tempbx=*pSiS_VideoSenseData2;
        }
        /*end 301b*/
          tempcx=0x0804;
          if(SiS_Sense(SiS_Part4Port,tempbx,tempcx)){
            if(SiS_Sense(SiS_Part4Port,tempbx,tempcx)){
              tempax=tempax|AVIDEOSense;
            }
          }
        } else {
          if(!(tempax&SVIDEOSense)){
            tempbx=*pSiS_VideoSenseData;
            /*301b*/
            if(!(SiS_Is301B(BaseAddr))){
              tempbx=*pSiS_VideoSenseData2;
            }
            /*end 301b*/
            tempcx=0x0804;
            if(SiS_Sense(SiS_Part4Port,tempbx,tempcx)){
              if(SiS_Sense(SiS_Part4Port,tempbx,tempcx)){
                tempax=tempax|AVIDEOSense;
              }
            }
          }
        }
      }

      if(SiS_SenseLCD(HwDeviceExtension)){
        tempax=tempax|LCDSense;
      }

      tempbx=0;
      tempcx=0;
      SiS_Sense(SiS_Part4Port,tempbx,tempcx);
  
      if((SiS_VBType&VB_SIS301LV)||(SiS_VBType&VB_SIS302LV)){
         tempax=tempax&0x00ef;   				/* 301lv to disable CRT2*/
      }
      SiS_SetRegANDOR(SiS_P3d4,0x32,~0xDF,tempax);
      SiS_SetReg1(SiS_Part2Port,0x00,P2reg0);
      if(!(P2reg0&0x20)) {
        SiS_VBInfo = DisableCRT2Display;
        SiS_SetCRT2Group301(BaseAddr,ROMAddr,SenseModeNo,HwDeviceExtension);
      }
    }
  }
}

BOOLEAN
SiS_Sense(USHORT Part4Port,USHORT tempbx,USHORT tempcx)
{
  USHORT temp,i,tempch;

  temp=tempbx&0xFF;
  SiS_SetReg1(SiS_Part4Port,0x11,temp);
  temp=(tempbx&0xFF00)>>8;
  temp=temp|(tempcx&0x00FF);
  SiS_SetRegANDOR(SiS_Part4Port,0x10,~0x1F,temp);

  for(i=0;i<10;i++) SiS_LongWait();

  tempch=(tempcx&0x7F00)>>8;      /*   ynlai [05/22/2001]  */
  temp=SiS_GetReg1(SiS_Part4Port,0x03);
  temp=temp^(0x0E);
  temp=temp&tempch;               /*   ynlai [05/22/2001]  */
  if(temp>0) return 1;
  else return 0;
}

USHORT
SiS_SenseLCD(PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
/*  USHORT SoftSetting; */
  USHORT temp;

  temp=SiS_GetPanelID();
  if(!temp)  temp=SiS_GetLCDDDCInfo(HwDeviceExtension);
  return(temp);
}

BOOLEAN
SiS_GetLCDDDCInfo(PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT temp;
  /*add lcd sense*/
  if(HwDeviceExtension->ulCRT2LCDType==LCD_UNKNOWN)
    	return 0;
  else{
     	temp=(USHORT)HwDeviceExtension->ulCRT2LCDType;
     	SiS_SetReg1(SiS_P3d4,0x36,temp);
  	return 1;
  }
}

BOOLEAN
SiS_GetPanelID(void)
{
  USHORT PanelTypeTable[16]={ SyncNN | PanelRGB18Bit | Panel800x600  | _PanelType00,
                              SyncNN | PanelRGB18Bit | Panel1024x768 | _PanelType01,
                              SyncPP | PanelRGB18Bit | Panel800x600  | _PanelType02,
                              SyncNN | PanelRGB18Bit | Panel640x480  | _PanelType03,
                              SyncNN | PanelRGB18Bit | Panel1024x768 | _PanelType04,
                              SyncNN | PanelRGB18Bit | Panel1024x768 | _PanelType05,
                              SyncNN | PanelRGB18Bit | Panel1024x768 | _PanelType06,
                              SyncNN | PanelRGB24Bit | Panel1024x768 | _PanelType07,
                              SyncNN | PanelRGB18Bit | Panel800x600  | _PanelType08,
                              SyncNN | PanelRGB18Bit | Panel1024x768 | _PanelType09,
                              SyncNN | PanelRGB18Bit | Panel800x600  | _PanelType0A,
                              SyncNN | PanelRGB18Bit | Panel1024x768 | _PanelType0B,
                              SyncNN | PanelRGB18Bit | Panel1024x768 | _PanelType0C,
                              SyncNN | PanelRGB24Bit | Panel1024x768 | _PanelType0D,
                              SyncNN | PanelRGB18Bit | Panel1024x768 | _PanelType0E,
                              SyncNN | PanelRGB18Bit | Panel1024x768 | _PanelType0F
                              };
  USHORT tempax,tempbx,temp;
/*  USHORT return_flag; */

  tempax=SiS_GetReg1(SiS_P3c4,0x18);
  tempbx=tempax&0x0F;
  if(!(tempax&0x10)){
    if(SiS_IF_DEF_LVDS==1){
      tempbx=0;
      temp=SiS_GetReg1(SiS_P3c4,0x38);
      if(temp&0x40) tempbx=tempbx|0x08;
      if(temp&0x20) tempbx=tempbx|0x02;
      if(temp&0x01) tempbx=tempbx|0x01;
      temp=SiS_GetReg1(SiS_P3c4,0x39);
      if(temp&0x80) tempbx=tempbx|0x04;
    }else{
      return 0;
    }
  }

  tempbx=tempbx<<1;
  tempbx=PanelTypeTable[tempbx];
  tempbx=tempbx|LCDSync;
  temp=tempbx&0x00FF;
  SiS_SetReg1(SiS_P3d4,0x36,temp);
  temp=(tempbx&0xFF00)>>8;
  SiS_SetRegANDOR(SiS_P3d4,0x37,~(LCDSyncBit|LCDRGB18Bit),temp);
  return 1;
}

USHORT
SiS_SenseCHTV(void)
{
  USHORT temp,push0e,status;

  status=0;
  push0e=SiS_GetCH7005(0x0e);
  push0e=(push0e<<8)|0x0e;
  SiS_SetCH7005(0x0b0e);
  SiS_SetCH7005(0x0110);
  SiS_SetCH7005(0x0010);
  temp=SiS_GetCH7005(0x10);
  if(temp&0x08) status=status|SVIDEOSense;
  if(temp&0x02) status=status|AVIDEOSense;
  SiS_SetCH7005(push0e);
  return(status);
}

/*  ================for TC only =================  */
#ifdef TC

int
INT1AReturnCode(union REGS regs)
{
  if (regs.x.cflag)
  {
    /*printf("Error to find pci device!\n"); */
    return 1;
  }

  switch(regs.h.ah)
  {
    case 0: return 0;
            break;
    case 0x81: printf("Function not support\n");
               break;
    case 0x83: printf("bad vendor id\n");
               break;
    case 0x86: printf("device not found\n");
               break;
    case 0x87: printf("bad register number\n");
               break;
    case 0x88: printf("set failed\n");
               break;
    case 0x89: printf("buffer too small");
               break;
  }
  return 1;
}

unsigned
FindPCIIOBase(unsigned index,unsigned deviceid)
{
  union REGS regs;

  regs.h.ah = 0xb1;  /*PCI_FUNCTION_ID */
  regs.h.al = 0x02;  /*FIND_PCI_DEVICE */
  regs.x.cx = deviceid;
  regs.x.dx = 0x1039;
  regs.x.si = index;  /* find n-th device */

  int86(0x1A, &regs, &regs);

  if (INT1AReturnCode(regs)!=0)
    return 0;

  /* regs.h.bh *//* bus number */
  /* regs.h.bl *//* device number */
  regs.h.ah = 0xb1;  /*PCI_FUNCTION_ID */
  regs.h.al = 0x09;  /*READ_CONFIG_WORD */
  regs.x.cx = deviceid;
  regs.x.dx = 0x1039;
  regs.x.di = 0x18;  /* register number */
  int86(0x1A, &regs, &regs);

  if (INT1AReturnCode(regs)!=0)
    return 0;
  return regs.x.cx;
}


void
main(int argc, char *argv[])
{
  SIS_HW_DEVICE_INFO  HwDeviceExtension;
  USHORT temp;
  USHORT ModeNo;

  /*HwDeviceExtension.pjVirtualRomBase =(PUCHAR) MK_FP(0xC000,0); */
  /*HwDeviceExtension.pjVideoMemoryAddress = (PUCHAR)MK_FP(0xA000,0);*/

#ifdef SIS300  
  HwDeviceExtension.ulIOAddress = (FindPCIIOBase(0,0x6300)&0xFF80) + 0x30;
  HwDeviceExtension.jChipType = SIS_630;
#endif

#ifdef SIS315H  
//  HwDeviceExtension.ulIOAddress = (FindPCIIOBase(0,0x5315)&0xFF80) + 0x30;
//  HwDeviceExtension.jChipType = SIS_550;
  HwDeviceExtension.ulIOAddress = (FindPCIIOBase(0,0x325)&0xFF80) + 0x30;
  HwDeviceExtension.jChipType = SIS_315H;
#endif

  HwDeviceExtension.ujVBChipID = VB_CHIP_301;
  strcpy(HwDeviceExtension.szVBIOSVer,"0.84");
  HwDeviceExtension.bSkipDramSizing = FALSE;
  HwDeviceExtension.ulVideoMemorySize = 0;
  if(argc==2) {
    ModeNo=atoi(argv[1]);
  }
  else {
    ModeNo=0x2e;
    /*ModeNo=0x37; */ /* 1024x768x 4bpp */
    /*ModeNo=0x38; *//* 1024x768x 8bpp */
    /*ModeNo=0x4A; *//* 1024x768x 16bpp */
    /*ModeNo=0x47;*/ /* 800x600x 16bpp */
  }
 /* SiSInit(&HwDeviceExtension);*/
#ifdef LINUX_XF86
  SiSSetMode(&HwDeviceExtension,NULL, ModeNo);
#else
  SiSSetMode(&HwDeviceExtension,ModeNo);
#endif
}
#endif /* TC */

/* TW: New for Linux XF86 */
#ifdef LINUX_XF86
USHORT
SiS_CalcModeIndex(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
   UShort i = (pScrn->bitsPerPixel+7)/8 - 1;
   UShort ModeIndex = 0;
   switch(mode->HDisplay)
   {
     case 640:
          ModeIndex = ModeIndex_640x480[i];
          break;
     case 720:
          if(mode->VDisplay == 480)
            ModeIndex = ModeIndex_720x480[i];
          else
            ModeIndex = ModeIndex_720x576[i];
          break;
     case 800:
          ModeIndex = ModeIndex_800x600[i];
          break;
     case 1024:
          ModeIndex = ModeIndex_1024x768[i];
          break;
     case 1280:
          if(mode->VDisplay == 960)
            ModeIndex = ModeIndex_1280x960[i];
          else
            ModeIndex = ModeIndex_1280x1024[i];
          break;
     case 1600:
          ModeIndex = ModeIndex_1600x1200[i];
          break;
     case 1920:
          ModeIndex = ModeIndex_1920x1440[i];
          break;
   }

   return(ModeIndex);
}

#define MODEID_OFF 0x449

unsigned char
SiS_GetSetModeID(ScrnInfoPtr pScrn, unsigned char id)
{
    unsigned char ret;

    unsigned char* base = xf86MapVidMem(pScrn->scrnIndex,
					VIDMEM_MMIO, 0, 0x2000);
    ret = *(base + MODEID_OFF);

    /* id != 0xff means: set mode */
    if (id != 0xff)
	*(base + MODEID_OFF) = id;
    xf86UnMapVidMem(pScrn->scrnIndex,base,0x2000);

    return ret;
}
#endif



