/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/init301.c,v 1.3 2000/12/02 01:16:16 dawes Exp $ */

/*
 * Mode switching code (CRT2 section) for SiS 300/540/630/730/315/550/650/740
 * (Universal module for Linux kernel framebuffer, XFree86 4.x)
 *
 * Comments and changes marked with "TW" by Thomas Winischhofer <thomer@winischhofer.net>
 *
 */

/* DEBUG: GetVCLK2Ptr (Mitac bug) - reverted */

/*
#ifdef WINCE_HEADER
#include "precomp.h"
#endif
*/
/*#include "precomp.h"*/

#include "init301.h"
#ifdef SIS300
#include "oem300.h"
#endif
#ifdef SIS315H
#include "oem310.h"
#endif

BOOLEAN
SiS_SetCRT2Group301(USHORT BaseAddr,ULONG ROMAddr,USHORT ModeNo,
                    PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
   USHORT ModeIdIndex;
   USHORT RefreshRateTableIndex;

   SiS_SetFlag=SiS_SetFlag|ProgrammingCRT2;
   SiS_SearchModeID(ROMAddr,ModeNo,&ModeIdIndex);
   SiS_SelectCRT2Rate=4;
   RefreshRateTableIndex = SiS_GetRatePtrCRT2(ROMAddr,ModeNo,ModeIdIndex);
   SiS_SaveCRT2Info(ModeNo);
   SiS_DisableBridge(HwDeviceExtension,BaseAddr);
   SiS_UnLockCRT2(HwDeviceExtension, BaseAddr);
   SiS_SetCRT2ModeRegs(BaseAddr,ModeNo,HwDeviceExtension);
   if(SiS_VBInfo&DisableCRT2Display) {
     SiS_LockCRT2(HwDeviceExtension, BaseAddr);
     SiS_DisplayOn();
     return(FALSE);
   }
   /* SetDefCRT2ExtRegs(BaseAddr);   */
   SiS_GetCRT2Data(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex);
   /*301b*/
   if( ((SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS301LV))
           && (SiS_VBInfo&SetCRT2ToLCDA)) ){
	SiS_GetLVDSDesData(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex);
   }
   /*end 301b*/
   if(SiS_IF_DEF_LVDS==1) {
   	SiS_GetLVDSDesData(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex);
   }

   SiS_SetGroup1(BaseAddr,ROMAddr,ModeNo,ModeIdIndex,
                 HwDeviceExtension,RefreshRateTableIndex);

   /*301b*/
   if( (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS301LV))
   		&& (SiS_VBInfo&SetCRT2ToLCDA)
		&& (SiS_IF_DEF_LVDS == 0) ) {
     	if( (SiS_VBType&(VB_SIS301LV|VB_SIS302LV)) && (SiS_VBInfo&SetCRT2ToLCDA) ){
             	SiS_SetReg1(SiS_Part4Port,0x24,0x0e);
    	}
   /* end 301b */
   } else if((SiS_IF_DEF_LVDS==0) && (!(SiS_VBInfo&SetCRT2ToLCDA))) {
      	SiS_SetGroup2(BaseAddr,ROMAddr,ModeNo,ModeIdIndex,
	              RefreshRateTableIndex,HwDeviceExtension);
      	SiS_SetGroup3(BaseAddr,ROMAddr,ModeNo,ModeIdIndex,
	              HwDeviceExtension);
      	SiS_SetGroup4(BaseAddr,ROMAddr,ModeNo,ModeIdIndex,
	              RefreshRateTableIndex,HwDeviceExtension);
      	SiS_SetGroup5(BaseAddr,ROMAddr,ModeNo,ModeIdIndex);
   } else {
     	if(SiS_IF_DEF_CH7005==1) {
	    if(SiS_VBInfo&SetCRT2ToTV)
	        /* TW: Set Chrontel registers only if CRT2 is TV */
       		SiS_SetCHTVReg(ROMAddr,ModeNo,ModeIdIndex,
		               RefreshRateTableIndex);
     	}
     	if(!(SiS_LCDResInfo==Panel640x480)){
    	 	SiS_ModCRT1CRTC(ROMAddr,ModeNo,ModeIdIndex,
		                RefreshRateTableIndex);
         	if(!SiS_IF_DEF_FSTN)  /*fstn*/
     	     		SiS_SetCRT2ECLK(ROMAddr,ModeNo,ModeIdIndex,
			          RefreshRateTableIndex,HwDeviceExtension);
     	}
   }

#ifdef SIS300
   if ( (HwDeviceExtension->jChipType==SIS_540)||
        (HwDeviceExtension->jChipType==SIS_630)||
        (HwDeviceExtension->jChipType==SIS_730)||
        (HwDeviceExtension->jChipType==SIS_300) )
     	SiS_OEM300Setting(HwDeviceExtension,BaseAddr,ROMAddr,ModeNo);
#endif

#ifdef SIS315H
   if ( (HwDeviceExtension->jChipType==SIS_315H)||   /* 05/02/01 ynlai for sis550 */
        (HwDeviceExtension->jChipType==SIS_315PRO)||
        (HwDeviceExtension->jChipType==SIS_550) ||   /* 05/02/01 ynlai for 550 */
        (HwDeviceExtension->jChipType==SIS_640) ||   /* 08/20/01 chiawen for 640/740 */
        (HwDeviceExtension->jChipType==SIS_740) ||   /* 09/03/01 chiawen for 640/740 */
        (HwDeviceExtension->jChipType==SIS_650))     /* 09/03/01 chiawen for 650 */
   {
        SiS_OEM310Setting(HwDeviceExtension,BaseAddr,ROMAddr,ModeNo,ModeIdIndex);
        SiS_CRT2AutoThreshold(BaseAddr);
   }
#endif

   SiS_EnableBridge(HwDeviceExtension,BaseAddr);
   if(SiS_IF_DEF_CH7005==1) {
	if(SiS_VBInfo&SetCRT2ToTV) {
	     /* TW: Disable LCD panel when using TV */
	     SiS_SetRegANDOR(SiS_P3c4,0x11,0xFF,0x0C);
	} else {
	     /* TW: Disable TV when using LCD */
	     SiS_SetCHTVRegANDOR(0x010E,0xF8);
	}
   }
   SiS_DisplayOn();
   SiS_LockCRT2(HwDeviceExtension, BaseAddr);
   return 1;
}

void
SiS_SetGroup1(USHORT BaseAddr,ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
              PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT RefreshRateTableIndex)
{
  USHORT  temp=0,tempax=0,tempbx=0,tempcx=0;
  USHORT  pushbx=0,CRT1Index=0;
  USHORT  modeflag,resinfo=0;

  if(ModeNo<=0x13) {
  	/* TW: Do nothing for std modes */
  } else {
    	CRT1Index = SiS_RefIndex[RefreshRateTableIndex].Ext_CRT1CRTC;
    	CRT1Index=CRT1Index&0x3F;
    	resinfo = SiS_EModeIDTable[ModeIdIndex].Ext_RESINFO;
  }

  /*301b*/ 
  if( (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
         && (SiS_VBInfo&SetCRT2ToLCDA) ) {
	   /* TW: Do nothing for these bridge types here */
  } else {
     SiS_SetCRT2Offset(SiS_Part1Port,ROMAddr,ModeNo,ModeIdIndex,
      		       RefreshRateTableIndex,HwDeviceExtension);
     if (HwDeviceExtension->jChipType < SIS_315H ) /* 300 series */
    	   SiS_SetCRT2FIFO(SiS_Part1Port,ROMAddr,ModeNo,
	                   HwDeviceExtension);
     else 					   /* 310 series */
           SiS_SetCRT2FIFO2(SiS_Part1Port,ROMAddr,ModeNo,
	                   HwDeviceExtension);

     SiS_SetCRT2Sync(BaseAddr,ROMAddr,ModeNo,
                     RefreshRateTableIndex);
  }

  if (ModeNo<=0x13)
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
  else
    	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;

  /*301b*/
  if( (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
   					&& (SiS_VBInfo&SetCRT2ToLCDA) ) {
     	SiS_SetGroup1_LCDA(BaseAddr,ROMAddr,ModeNo,ModeIdIndex,
     	                   HwDeviceExtension,RefreshRateTableIndex);
  /*end 301b*/
  } else if (HwDeviceExtension->jChipType < SIS_315H ) {
    /* ------------- 300 series --------------*/
    temp=(SiS_VGAHT-1)&0x0FF;   					/* BTVGA2HT 0x08,0x09 */
    SiS_SetReg1(SiS_Part1Port,0x08,temp);
    temp=(((SiS_VGAHT-1)&0xFF00)>>8)<<4;
    SiS_SetRegANDOR(SiS_Part1Port,0x09,~0x0F0,temp); 	/* <------------------- */

    temp=(SiS_VGAHDE+12)&0x0FF;                               		/* BTVGA2HDEE 0x0A,0x0C */
    SiS_SetReg1(SiS_Part1Port,0x0A,temp); 		/* <------------------- */
        
    pushbx=SiS_VGAHDE+12;                                     		/* bx  BTVGA@HRS 0x0B,0x0C */
    tempcx=(SiS_VGAHT-SiS_VGAHDE)>>2;                           	/* cx */
    tempbx=pushbx+tempcx;
    tempcx=tempcx<<1;
    tempcx=tempcx+tempbx;
    
    if(SiS_IF_DEF_LVDS==0) {
      if(SiS_VBInfo&SetCRT2ToRAMDAC){
        tempbx = SiS_CRT1Table[CRT1Index].CR[4];
        tempbx = tempbx|((SiS_CRT1Table[CRT1Index].CR[14]&0xC0)<<2);
        tempbx = (tempbx-1)<<3;
        tempcx = SiS_CRT1Table[CRT1Index].CR[5];
        tempcx = tempcx&0x1F;
        temp = SiS_CRT1Table[CRT1Index].CR[15];
        temp = (temp&0x04)<<(6-2);
        tempcx=((tempcx|temp)-1)<<3;
      }
    }
   /*add for hardware request*/
    if((SiS_VBInfo&SetCRT2ToTV)&&(resinfo==0x08)){
        if(SiS_VBInfo&SetPALTV){
      		tempbx=1040;
      		tempcx=1042;
      	} else {
      		tempbx=1040;
      		tempcx=1042;
      	}
    }

    temp=tempbx&0x00FF;
    SiS_SetReg1(SiS_Part1Port,0x0B,temp); 		/* <------------------- */
    
  } else {
     /* ---------------------- 310 series ------------------*/
     if (modeflag&HalfDCLK) { /* for low resolution mode */
         temp=(SiS_VGAHT/2-1)&0x0FF;                             /* BTVGA2HT 0x08,0x09 */
         SiS_SetReg1(SiS_Part1Port,0x08,temp);
         temp=(((SiS_VGAHT/2-1)&0xFF00)>>8)<<4;
         SiS_SetRegANDOR(SiS_Part1Port,0x09,~0x0F0,temp);
         temp=(SiS_VGAHDE/2+16)&0x0FF;                           /* BTVGA2HDEE 0x0A,0x0C */
         SiS_SetReg1(SiS_Part1Port,0x0A,temp);

         pushbx=SiS_VGAHDE/2+16;
         tempcx=((SiS_VGAHT-SiS_VGAHDE)/2)>>2;                   /* cx */
         tempbx=pushbx+tempcx;                                   /* bx  BTVGA@HRS 0x0B,0x0C */
         tempcx=tempcx+tempbx;

         if(SiS_IF_DEF_LVDS==0) {
             if(SiS_VBInfo&SetCRT2ToRAMDAC){
                tempbx = SiS_CRT1Table[CRT1Index].CR[4];
                tempbx = tempbx|((SiS_CRT1Table[CRT1Index].CR[14]&0xC0)<<2);
                tempbx = (tempbx-3)<<3;         		/*(VGAHRS-3)*8 */
                tempcx = SiS_CRT1Table[CRT1Index].CR[5];
                tempcx = tempcx&0x1F;
                temp = SiS_CRT1Table[CRT1Index].CR[15];
                temp = (temp&0x04)<<(5-2);      		/*VGAHRE D[5]*/
                tempcx=((tempcx|temp)-3)<<3;    		/* (VGAHRE-3)*8 */
             }
         }
         tempbx += 4;
         tempcx += 4;
         if (tempcx>(SiS_VGAHT/2))
              tempcx = SiS_VGAHT/2;
         temp=tempbx&0x00FF;
         SiS_SetReg1(SiS_Part1Port,0x0B,temp);
    } else {
         temp=(SiS_VGAHT-1)&0x0FF;                             	/* BTVGA2HT 0x08,0x09 */
         SiS_SetReg1(SiS_Part1Port,0x08,temp);
         temp=(((SiS_VGAHT-1)&0xFF00)>>8)<<4;
         SiS_SetRegANDOR(SiS_Part1Port,0x09,~0x0F0,temp);
         temp=(SiS_VGAHDE+16)&0x0FF;                            /* BTVGA2HDEE 0x0A,0x0C */
         SiS_SetReg1(SiS_Part1Port,0x0A,temp);

         pushbx=SiS_VGAHDE+16;
         tempcx=(SiS_VGAHT-SiS_VGAHDE)>>2;                      /* cx */
         tempbx=pushbx+tempcx;                                  /* bx  BTVGA@HRS 0x0B,0x0C */
         tempcx=tempcx+tempbx;
      
         if(SiS_IF_DEF_LVDS==0) {
             if(SiS_VBInfo&SetCRT2ToRAMDAC){
                tempbx = SiS_CRT1Table[CRT1Index].CR[4];
                tempbx = tempbx|((SiS_CRT1Table[CRT1Index].CR[14]&0xC0)<<2);
                tempbx = (tempbx-3)<<3;         		/*(VGAHRS-3)*8 */
                tempcx = SiS_CRT1Table[CRT1Index].CR[5];
                tempcx = tempcx&0x1F;
                temp = SiS_CRT1Table[CRT1Index].CR[15];
                temp = (temp&0x04)<<(5-2);      		/*VGAHRE D[5]*/
                tempcx=((tempcx|temp)-3)<<3;    		/* (VGAHRE-3)*8 */
                tempbx += 16;
                tempcx += 16;
             }
         }
         if (tempcx>SiS_VGAHT)
        	tempcx = SiS_VGAHT;
        /*add for hardware request*/
         if((SiS_VBInfo&SetCRT2ToTV)&&(resinfo==0x08)){
             if(SiS_VBInfo&SetPALTV){
      		 tempbx=1040;
      		 tempcx=1042;
      	     } else {
      		 tempbx=1040;
      		 tempcx=1042;
      	     }
         }
         temp=tempbx&0x00FF;
         SiS_SetReg1(SiS_Part1Port,0x0B,temp);
     }
  }

  /* TW: The following is done for all bridge/chip types/series */

#if 1
  tempax = (tempax&0x00FF)|(tempbx&0xFF00); /* TW: tempax not used above!!!*/
  tempbx=pushbx;
  tempbx=(tempbx&0x00FF)|((tempbx&0xFF00)<<4);
  tempax=tempax|(tempbx&0xFF00);
  temp=(tempax&0xFF00)>>8;
#endif
#if 0
  /* TW: new code: */
  tempax=(tempbx&0xFF00)>>8;
  tempbx=(((pushbx&0xFF00)>>8)<<4)&0xFF;
  temp=tempax|tempbx;
#endif
  SiS_SetReg1(SiS_Part1Port,0x0C,temp); 		/* <------------------- */
  temp=tempcx&0x00FF;
  SiS_SetReg1(SiS_Part1Port,0x0D,temp); 		/* <------------------- */
  tempcx=SiS_VGAVT-1;
  temp=tempcx&0x00FF;
  if(SiS_IF_DEF_CH7005==1) {
    	if(SiS_VBInfo&SetCRT2ToTV) {
      		temp--;
    	}
  }
  SiS_SetReg1(SiS_Part1Port,0x0E,temp); 		/* <------------------- */
  tempbx=SiS_VGAVDE-1;
  temp=tempbx&0x00FF;
  /* [Do the same if CH7005==1 as above?] */
  SiS_SetReg1(SiS_Part1Port,0x0F,temp); 		/* <------------------- */
  temp=((tempbx&0xFF00)<<3)>>8;
  temp=temp|((tempcx&0xFF00)>>8);
  SiS_SetReg1(SiS_Part1Port,0x12,temp); 		/* <------------------- */

  tempax=SiS_VGAVDE;
  tempbx=SiS_VGAVDE;
  tempcx=SiS_VGAVT;
  tempbx=(SiS_VGAVT+SiS_VGAVDE)>>1;                     	/*  BTVGA2VRS     0x10,0x11   */
  tempcx=((SiS_VGAVT-SiS_VGAVDE)>>4)+tempbx+1;          	/*  BTVGA2VRE     0x11        */
  if(SiS_IF_DEF_LVDS==0) {
    	if(SiS_VBInfo & SetCRT2ToRAMDAC){
      		tempbx = SiS_CRT1Table[CRT1Index].CR[8];
      		temp = SiS_CRT1Table[CRT1Index].CR[7];
      		if(temp&0x04) tempbx=tempbx|0x0100;
      		if(temp&0x80) tempbx=tempbx|0x0200;
      		temp = SiS_CRT1Table[CRT1Index].CR[13];
      		if(temp&0x08) tempbx=tempbx|0x0400;
      		temp = SiS_CRT1Table[CRT1Index].CR[9];
      		tempcx=(tempcx&0xFF00)|(temp&0x00FF);
    	}
  }
  temp=tempbx&0x00FF;
  SiS_SetReg1(SiS_Part1Port,0x10,temp); 		/* <------------------- */
  temp=((tempbx&0xFF00)>>8)<<4;
  temp=((tempcx&0x000F)|(temp));
  SiS_SetReg1(SiS_Part1Port,0x11,temp); 		/* <------------------- */
  if(SiS_IF_DEF_LVDS==0) {
    	temp=0x20;
    	if(SiS_LCDResInfo==Panel1280x1024) temp=0x20;
    	if(SiS_LCDResInfo==Panel1280x960) temp=0x24;
    	if(SiS_VBInfo&SetCRT2ToTV) temp=0x08;
    	if(SiS_VBInfo&SetCRT2ToHiVisionTV) {
      		if(SiS_VBInfo&SetInSlaveMode) temp=0x2c;
      		else temp=0x20;
    	}
  } else {
    	temp=0x20;
  }

  if (HwDeviceExtension->jChipType < SIS_315H ) {
    /* ---------- 300 series -------------- */

    SiS_SetRegANDOR(SiS_Part1Port,0x13,~0x03C,temp); 	/* <----------------- */
	/* TW: This register will be adapted according to LCD
	 *     panel type later in the OEM setup functions.
	 *     (Various panel types require a different delay
	 *     such as Clevo 2202; however, on most panels,
	 *     0x20 does nicely.)
	 */

  } else {
    /* ----------- 310 series ---------------*/
    temp >>=2;    /* TW: This is typical SiS code: First calulate temp, then overwrite it... */
    temp = 0x11;    						/*  ynlai 05/30/2001 for delay compenation  */
    SiS_SetReg1(SiS_Part1Port,0x2D,temp);
    /*SiS_SetRegANDOR(SiS_Part1Port,0x2D,~0x00F,temp);*/
    SiS_SetRegAND(SiS_Part1Port,0x13,0xEF);  			/* BDirectLCD=0 for lcd ?? */
    tempax=0;

    if (modeflag&DoubleScanMode) tempax |= 0x80;
    if (modeflag&HalfDCLK) tempax |= 0x40;
    SiS_SetRegANDOR(SiS_Part1Port,0x2C,~0x0C0,tempax);
  }

  if(SiS_IF_DEF_LVDS==0) {
    	SiS_SetGroup1_301(BaseAddr,ROMAddr,ModeNo,ModeIdIndex,
	                  HwDeviceExtension,RefreshRateTableIndex);
  } else {
    	SiS_SetGroup1_LVDS(BaseAddr,ROMAddr,ModeNo,ModeIdIndex,
	                   HwDeviceExtension,RefreshRateTableIndex);
  }
}

void
SiS_SetGroup1_301(USHORT  BaseAddr,ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                  PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT RefreshRateTableIndex)
{
  USHORT  push1,push2;
  USHORT  tempax,tempbx,tempcx,temp;
  USHORT  resinfo,modeflag;
  USHORT  CRT1Index;

  if(ModeNo<=0x13) {
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;      /* si+St_ResInfo */
    	resinfo = SiS_SModeIDTable[ModeIdIndex].St_ResInfo;
  } else {
    	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;     /* si+Ext_ResInfo */
    	resinfo = SiS_EModeIDTable[ModeIdIndex].Ext_RESINFO;
    	CRT1Index = SiS_RefIndex[RefreshRateTableIndex].Ext_CRT1CRTC;
    	CRT1Index = CRT1Index &0x3F;
  }

  if(!(SiS_VBInfo&SetInSlaveMode))  return;

  tempax=0xFFFF;
  if(!(SiS_VBInfo&SetCRT2ToTV)) {
    	tempax=SiS_GetVGAHT2();
  }
  if(modeflag&Charx8Dot) tempcx=0x08;
  else tempcx=0x09;
  if(tempax>=SiS_VGAHT) {
    	tempax=SiS_VGAHT;
  }
  if(modeflag&HalfDCLK) {
    	tempax=tempax>>1;
  }
  tempax=(tempax/tempcx)-5;
  tempbx=tempax;
  temp=0xFF;                                            	/* set MAX HT */
  SiS_SetReg1(SiS_Part1Port,0x03,temp);

  tempax=SiS_VGAHDE;                                           	/* 0x04 Horizontal Display End */
  if(modeflag&HalfDCLK) tempax=tempax>>1;
  tempax=(tempax/tempcx)-1;
  tempbx=tempbx|((tempax&0x00FF)<<8);
  temp=tempax&0x00FF;
  SiS_SetReg1(SiS_Part1Port,0x04,temp);

  temp=(tempbx&0xFF00)>>8;
  if(SiS_VBInfo&SetCRT2ToTV){
    	temp=temp+2;
    	if(SiS_VBInfo&SetCRT2ToHiVisionTV) {
      		if(resinfo==7) temp=temp-2;
    	}
  }
  SiS_SetReg1(SiS_Part1Port,0x05,temp);                         /* 0x05 Horizontal Display Start */
  SiS_SetReg1(SiS_Part1Port,0x06,0x03);                       	/* 0x06 Horizontal Blank end     */
                                                           	/* 0x07 horizontal Retrace Start */
  if(SiS_VBInfo&SetCRT2ToHiVisionTV) {
    temp=(tempbx&0x00FF)-1;
    if(!(modeflag&HalfDCLK)) {
      temp=temp-6;
      if(SiS_SetFlag&TVSimuMode) {
        temp=temp-4;
        if(ModeNo>0x13) temp=temp-10;
      }
    }
  } else {
    tempcx=tempbx&0x00FF;
    tempbx=(tempbx&0xFF00)>>8;
    tempcx=(tempcx+tempbx)>>1;
    temp=(tempcx&0x00FF)+2;
    if(SiS_VBInfo&SetCRT2ToTV){
       temp=temp-1;
       if(!(modeflag&HalfDCLK)){
          if((modeflag&Charx8Dot)){
            temp=temp+4;
            if(SiS_VGAHDE>=800){
              temp=temp-6;
            }
          }
        }
    } else {
       if(!(modeflag&HalfDCLK)){
         temp=temp-4;
         if(SiS_LCDResInfo!=Panel1280x960) {
           if(SiS_VGAHDE>=800){
             temp=temp-7;
             if(SiS_ModeType==ModeEGA){
               if(SiS_VGAVDE==1024){
                 temp=temp+15;
                 if(SiS_LCDResInfo!=Panel1280x1024){
                    temp=temp+7;
                 }
               }
             }
             if(SiS_VGAHDE>=1280){
               if(SiS_LCDResInfo!=Panel1280x960) {
                 if(SiS_LCDInfo&LCDNonExpanding) {
                   temp=temp+28;
                 }
               }
             }
           }
         }
       }
    }
  }
 
  SiS_SetReg1(SiS_Part1Port,0x07,temp);                       	/* 0x07 Horizontal Retrace Start */
  SiS_SetReg1(SiS_Part1Port,0x08,0);                            /* 0x08 Horizontal Retrace End   */
  
  if(SiS_VBInfo&SetCRT2ToTV) {
        if(SiS_SetFlag&TVSimuMode) {
            if((ModeNo==0x06)||(ModeNo==0x10)||(ModeNo==0x11)||(ModeNo==0x13)||(ModeNo==0x0F)){
             	SiS_SetReg1(SiS_Part1Port,0x07,0x5b);
             	SiS_SetReg1(SiS_Part1Port,0x08,0x03);
            }
            if((ModeNo==0x00)||(ModeNo==0x01)) {
             	if(SiS_VBInfo&SetNTSCTV) {
             		SiS_SetReg1(SiS_Part1Port,0x07,0x2A);
             		SiS_SetReg1(SiS_Part1Port,0x08,0x61);
              	} else {
             		SiS_SetReg1(SiS_Part1Port,0x07,0x2A);
             		SiS_SetReg1(SiS_Part1Port,0x08,0x41);
             		SiS_SetReg1(SiS_Part1Port,0x0C,0xF0);
            	}
           }
           if((ModeNo==0x02)||(ModeNo==0x03)||(ModeNo==0x07)){
            	if(SiS_VBInfo&SetNTSCTV) {
           		SiS_SetReg1(SiS_Part1Port,0x07,0x54);
           		SiS_SetReg1(SiS_Part1Port,0x08,0x00);
            	} else {
           		SiS_SetReg1(SiS_Part1Port,0x07,0x55);
           		SiS_SetReg1(SiS_Part1Port,0x08,0x00);
           		SiS_SetReg1(SiS_Part1Port,0x0C,0xF0);
           	}
           }
           if((ModeNo==0x04)||(ModeNo==0x05)||(ModeNo==0x0D)||(ModeNo==0x50)){
            	if(SiS_VBInfo&SetNTSCTV) {
            		SiS_SetReg1(SiS_Part1Port,0x07,0x30);
            		SiS_SetReg1(SiS_Part1Port,0x08,0x03);
            	} else {
           		SiS_SetReg1(SiS_Part1Port,0x07,0x2f);
           		SiS_SetReg1(SiS_Part1Port,0x08,0x02);
                }
           }
       } 
  }
  
  SiS_SetReg1(SiS_Part1Port,0x18,0x03);                         	/* 0x18 SR08    */
  SiS_SetRegANDOR(SiS_Part1Port,0x19,0xF0,0x00);
  SiS_SetReg1(SiS_Part1Port,0x09,0xFF);                         	/* 0x09 Set Max VT    */

  tempbx=SiS_VGAVT;
  push1=tempbx;
  tempcx=0x121;
  tempbx=SiS_VGAVDE;                                        		/* 0x0E Virtical Display End */
  if(tempbx==357) tempbx=350;
  if(tempbx==360) tempbx=350;
  if(tempbx==375) tempbx=350;
  if(tempbx==405) tempbx=400;
  if(tempbx==420) tempbx=400;
  if(tempbx==525) tempbx=480;
  push2=tempbx;
  if(SiS_VBInfo&SetCRT2ToLCD) {
    	if(SiS_LCDResInfo==Panel1024x768) {
      		if(!(SiS_SetFlag&LCDVESATiming)) {
        		if(tempbx==350) tempbx=tempbx+5;
        		if(tempbx==480) tempbx=tempbx+5;
      		}
    	}
  }
  tempbx--;
  temp=tempbx&0x00FF;
  tempbx--;
  temp=tempbx&0x00FF;
  SiS_SetReg1(SiS_Part1Port,0x10,temp);                    		/* 0x10 vertical Blank Start */
  tempbx=push2;
  tempbx--;
  temp=tempbx&0x00FF;
  SiS_SetReg1(SiS_Part1Port,0x0E,temp);
  if(tempbx&0x0100){
    	tempcx=tempcx|0x0002;
  }
  tempax=0x000B;
  if(modeflag&DoubleScanMode){
    	tempax=tempax|0x08000;
  }
  if(tempbx&0x0200){
    	tempcx=tempcx|0x0040;
  }

  temp=(tempax&0xFF00)>>8;
  SiS_SetReg1(SiS_Part1Port,0x0B,temp);
  if(tempbx&0x0400){
    	tempcx=tempcx|0x0600;
  }
  SiS_SetReg1(SiS_Part1Port,0x11,0x00);                         	/* 0x11 Vertival Blank End */

  tempax=push1;
  tempax=tempax-tempbx;                              			/* 0x0C Vertical Retrace Start */
  tempax=tempax>>2;
  push1=tempax;

  if(resinfo!=0x09) {
    	tempax=tempax<<1;
    	tempbx=tempax+tempbx;
  }
  if(SiS_VBInfo&SetCRT2ToHiVisionTV) {
    	tempbx=tempbx-10;
  } else {
    	if(SiS_SetFlag&TVSimuMode) {
      		if(SiS_VBInfo&SetPALTV) {
        		tempbx=tempbx+40;
      		}
    	}
  }
  tempax=push1;
  tempax=tempax>>2;
  tempax++;
  tempax=tempax+tempbx;
  push1=tempax;
  if((SiS_VBInfo&SetPALTV)) {
    	if(tempbx<=513)  {
      		if(tempax>=513) {
        		tempbx=513;
      		}
    	}
  }
  temp=(tempbx&0x00FF);
  SiS_SetReg1(SiS_Part1Port,0x0C,temp);
  tempbx--;
  temp=tempbx&0x00FF;
  SiS_SetReg1(SiS_Part1Port,0x10,temp);
  if(tempbx&0x0100){
    	tempcx=tempcx|0x0008;
  }
  if(tempbx&0x0200){
    	SiS_SetRegANDOR(SiS_Part1Port,0x0B,0x0FF,0x20);
  }
  tempbx++;
  if(tempbx&0x0100){
    	tempcx=tempcx|0x0004;
  }
  if(tempbx&0x0200){
    	tempcx=tempcx|0x0080;
  }
  if(tempbx&0x0400){
    	tempcx=tempcx|0x0C00;
  }
  tempbx=push1;
  temp=tempbx&0x00FF;
  temp=temp&0x0F;
  SiS_SetReg1(SiS_Part1Port,0x0D,temp);                    		/* 0x0D vertical Retrace End */
  if(tempbx&0x0010) {
    	tempcx=tempcx|0x2000;
  }

  temp=tempcx&0x00FF;
  SiS_SetReg1(SiS_Part1Port,0x0A,temp);                    		/* 0x0A CR07 */
  temp=(tempcx&0x0FF00)>>8;
  SiS_SetReg1(SiS_Part1Port,0x17,temp);                    		/* 0x17 SR0A */
  tempax=modeflag;
  temp=(tempax&0xFF00)>>8;

  temp=(temp>>1)&0x09;
  SiS_SetReg1(SiS_Part1Port,0x16,temp);                    		/* 0x16 SR01 */
  SiS_SetReg1(SiS_Part1Port,0x0F,0);                         		/* 0x0F CR14 */
  SiS_SetReg1(SiS_Part1Port,0x12,0);                         		/* 0x12 CR17 */
  if(SiS_LCDInfo&LCDRGB18Bit) temp=0x80;
  else temp=0x00;
  SiS_SetReg1(SiS_Part1Port,0x1A,temp);                         	/* 0x1A SR0E */
  return;
}

/* TW: This seems to be for some of 301B/302B/301LV/302LV as well
 *     which confuses me. In init.c, there is code for these bridges
 *     which is executed ONLY if IF_DEF_LVDS is 0. The following
 *     is being called only if IF_DEF_LVDS is NOT 0, and it contains
 *     code for 301B et al as well. Is IF_DEF_LVDS now 0 or 1 on
 *     301B/302B/301LV/302LV?
 */
void SiS_SetGroup1_LVDS(USHORT  BaseAddr,ULONG ROMAddr,USHORT ModeNo,
		        USHORT ModeIdIndex,
                        PSIS_HW_DEVICE_INFO HwDeviceExtension,
			USHORT RefreshRateTableIndex)
{
  USHORT modeflag,resinfo;
  USHORT push1,push2,tempax,tempbx,tempcx,temp,pushcx;
  ULONG tempeax=0,tempebx,tempecx,tempvcfact=0;

  if(ModeNo<=0x13) {
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;      	/* si+St_ResInfo */
    	resinfo = SiS_SModeIDTable[ModeIdIndex].St_ResInfo;
  } else {
    	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;     	/* si+Ext_ResInfo */
    	resinfo = SiS_EModeIDTable[ModeIdIndex].Ext_RESINFO;
  }

  tempax=SiS_LCDHDES;
  tempbx=SiS_HDE;
  tempcx=SiS_HT;

  tempcx=tempcx-tempbx;                                    		/* HT-HDE  */
  if(SiS_LCDInfo&LCDNonExpanding) {
    	if(SiS_LCDResInfo==Panel640x480)  tempbx=640;
    	if(SiS_LCDResInfo==Panel800x600)  tempbx=800;
    	if(SiS_LCDResInfo==Panel1024x768) tempbx=1024;
  }
  push1=tempax;
  tempax=tempax+tempbx;                                    		/* lcdhdee  */
  tempbx=SiS_HT;
  if(tempax>=tempbx){
    	tempax=tempax-tempbx;
  }
  push2=tempax;	                                                       	/* push ax   lcdhdee  */
  tempcx=tempcx>>2;                                        		/* temp  */
  tempcx=tempcx+tempax;                                    		/* lcdhrs  */
  if(tempcx>=tempbx){
    	tempcx=tempcx-tempbx;
  }									/* v ah,cl  */
  tempax=tempcx;
  tempax=tempax>>3;                                        		/* BPLHRS */
  temp=tempax&0x00FF;
  if(SiS_LCDResInfo==Panel640x480){                         		/*adjust for panel 640x480*/
      	temp=temp-4;
  }
  SiS_SetReg1(SiS_Part1Port,0x14,temp);		/* <------------------ */ 	/* Part1_14h  */
  temp=(tempax&0x00FF)+10;
  temp=temp&0x01F;
  temp=temp|(((tempcx&0x00ff)&0x07)<<5);
  /*fstn*/
  if(SiS_IF_DEF_FSTN){
      	temp=0x20;
  }
  SiS_SetReg1(SiS_Part1Port,0x15,temp);    	/* <------------------- */      /* Part1_15h  */
  tempbx=push2;                                          			/* lcdhdee  */
  tempcx=push1;                                         			/* lcdhdes  */
  temp=(tempcx&0x00FF);
  temp=temp&0x07;                                      				/* BPLHDESKEW  */
  SiS_SetReg1(SiS_Part1Port,0x1A,temp);   	/* <------------------- */      /* Part1_1Ah  */
  tempcx=tempcx>>3;                                        			/* BPLHDES */
  temp=(tempcx&0x00FF);
  if(ModeNo==0x5b){                            					/*fix fstn mode=5b*/
      	temp--;
  }
  SiS_SetReg1(SiS_Part1Port,0x16,temp);    	/* <------------------- */      /* Part1_16h  */
  if(tempbx&0x07) tempbx=tempbx+8;
  tempbx=tempbx>>3;                                        			/* BPLHDEE  */
  temp=tempbx&0x00FF;
  if(ModeNo==0x5b){                            					/*fix fstn mode=5b*/
      	temp--;
  }
  SiS_SetReg1(SiS_Part1Port,0x17,temp);   	/* <------------------- */      /* Part1_17h  */

  tempcx=SiS_VGAVT;
  tempbx=SiS_VGAVDE;
  tempcx=tempcx-tempbx;                                    			/* GAVT-VGAVDE  */
  tempbx=SiS_LCDVDES;			                    			/* VGAVDES  */

  push1=tempbx;                                        				/* push bx temppush1 */
  if(SiS_IF_DEF_TRUMPION==0){
    	if(SiS_IF_DEF_CH7005==1) {
      		if(SiS_VBInfo&SetCRT2ToTV) {
        		tempax=SiS_VGAVDE;
      		}
    	}
    	if(SiS_VBInfo&SetCRT2ToLCD) {
      		if(SiS_LCDResInfo==Panel640x480)  tempax=480;
      		if(SiS_LCDResInfo==Panel800x600)  tempax=600;
      		if(SiS_LCDResInfo==Panel1024x768) tempax=768;
      		if(SiS_LCDResInfo==Panel320x480)  tempax=480;
    	}
  } else tempax=SiS_VGAVDE;

  tempbx=tempbx+tempax;
  tempax=SiS_VT;                                               			/* VT  */
  if(tempbx>=SiS_VT){
    	tempbx=tempbx-tempax;
  }
  push2=tempbx;                                        				/* push bx  temppush2  */
  tempcx=tempcx>>1;
  tempbx=tempbx+tempcx;
  tempbx++;                                                			/* BPLVRS  */
  if(tempbx>=tempax){
    	tempbx=tempbx-tempax;
  }
  temp=tempbx&0x00FF;
  SiS_SetReg1(SiS_Part1Port,0x18,temp);       	/* <------------------- */      /* Part1_18h  */
  tempcx=tempcx>>3;
  tempcx=tempcx+tempbx;
  tempcx++;                                                			/* BPLVRE  */
  temp=tempcx&0x00FF;
  temp=temp&0x0F;
  SiS_SetRegANDOR(SiS_Part1Port,0x19,~0x00F,temp);   /* <-------------- */      /* Part1_19h  */

  temp=(tempbx&0xFF00)>>8;
  temp=temp&0x07;
  temp=temp<<3;                                 				/* BPLDESKEW =0 */
  tempbx=SiS_VGAVDE;
  if(tempbx!=SiS_VDE){
    	temp=temp|0x40;
  }
  if(SiS_SetFlag&EnableLVDSDDA) {
    	temp=temp|0x40;
  }
  if(SiS_LCDInfo&LCDRGB18Bit) {
    	temp=temp|0x80;
  }
  SiS_SetRegANDOR(SiS_Part1Port,0x1A,0x07,temp);  /* <----------------- */    	/* Part1_1Ah */

  tempecx=SiS_VGAVT;
  tempebx=SiS_VDE;
  tempeax=SiS_VGAVDE;
  tempecx=tempecx-tempeax;                                 			/* VGAVT-VGAVDE  */
  tempeax=tempeax<<6;
  temp=(USHORT)(tempeax%tempebx);
  tempeax=tempeax/tempebx;
  if(temp!=0){
    	tempeax++;
  }
  tempebx=tempeax;                                        			/* BPLVCFACT  */

  /* TW: Modified according to info from SiS - might be wrong */
  /* if (!(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))) { */
  if ((HwDeviceExtension->jChipType == SIS_540)||
      (HwDeviceExtension->jChipType == SIS_630)||
      (HwDeviceExtension->jChipType == SIS_730)||
      (HwDeviceExtension->jChipType == SIS_300)) {

  	if(SiS_SetFlag&EnableLVDSDDA) {
    		tempebx=tempebx&0x003F;
  	}
  	temp=(USHORT)(tempebx&0x00FF);
  	SiS_SetReg1(SiS_Part1Port,0x1E,temp);                       		/* Part1_1Eh */

  }
  /*add for 301b different 301*/
  else if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
    	tempecx=SiS_VGAVT;
    	tempebx=SiS_VDE;
    	tempeax=SiS_VGAVDE;
    	tempecx=tempecx-tempeax;                                 		/* VGAVT-VGAVDE  */
    	tempeax=tempeax<<18;
    	temp=(USHORT)(tempeax%tempebx);
    	tempeax=tempeax/tempebx;
    	if(temp!=0) tempeax++;
    	tempebx=tempeax;                                        		/* BPLVCFACT  */
    	tempvcfact=tempeax;       /*301b*/
    	temp=(USHORT)(tempebx&0x00FF);
    	SiS_SetReg1(SiS_Part1Port,0x37,temp);
    	temp=(USHORT)((tempebx&0x00FF00)>>8);
    	SiS_SetReg1(SiS_Part1Port,0x36,temp);
    	temp=(USHORT)((tempebx&0x00030000)>>16);
    	if(SiS_VDE==SiS_VGAVDE)	temp=temp|0x04;
    	SiS_SetReg1(SiS_Part1Port,0x35,temp);
  }
  /*end for 301b*/
  
  tempbx=push2;                                        				/* p bx temppush2 BPLVDEE  */
  tempcx=push1;			/* pop cx temppush1 NPLVDES */
  push1=(USHORT)(tempeax&0xFFFF);
  if(!(SiS_VBInfo&SetInSlaveMode)) {
   	if(!SiS_IF_DEF_DSTN){
    		if(SiS_LCDResInfo==Panel800x600) {
      			if(resinfo==7) tempcx++;
    		} else {
      			if(SiS_LCDResInfo==Panel1024x768) {
        			if(resinfo==8) tempcx++;
       			}
     		}
   	}
  }
  temp=(tempbx&0xFF00)>>8;
  temp=temp&0x07;
  temp=temp<<3;
  temp=temp|(((tempcx&0xFF00)>>8)&0x07);
  SiS_SetReg1(SiS_Part1Port,0x1D,temp);     	/* <------------------- */     	/* Part1_1Dh */
  temp=tempbx&0x00FF;
  /*fstn*/
  if(SiS_IF_DEF_FSTN){
      	temp=temp+1;
  }
  SiS_SetReg1(SiS_Part1Port,0x1C,temp);      	/* <------------------- */   	/* Part1_1Ch  */
  temp=tempcx&0x00FF;
  SiS_SetReg1(SiS_Part1Port,0x1B,temp);      	/* <------------------- */     	/* Part1_1Bh  */

  tempecx=SiS_VGAHDE;
  tempebx=SiS_HDE;
  tempeax=tempecx;
  tempeax=tempeax<<16;
  tempeax=tempeax/tempebx;
  if(tempebx==tempecx){
    	tempeax=0xFFFF;
  }
  tempecx=tempeax;
  tempeax=SiS_VGAHT; /* TW: Was SiS_VGAHDE;  - WRONG! - old (wrong) comment: "change VGAHT->VGAHDE"*/
  tempeax=tempeax<<16;
  tempeax=tempeax/tempecx;
  tempecx=tempecx<<16;
  tempeax=tempeax-1;
  tempecx=tempecx|(tempeax&0x00FFFF);
  temp=(USHORT)(tempecx&0x00FF);
  SiS_SetReg1(SiS_Part1Port,0x1F,temp);  	/* <------------------- */    	/* Part1_1Fh  */

  tempbx = SiS_VDE; /* TW: added this and if statement */
  if (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
  	tempeax=SiS_VGAVDE;
  	tempeax=tempeax<<18;          /*301b*/
  	tempeax=tempeax/tempvcfact;
  	tempbx=(USHORT) (tempeax&0x0FFFF);
  }

  if(SiS_LCDResInfo==Panel1024x768) tempbx--;

  if(SiS_SetFlag&EnableLVDSDDA) tempbx=1;

  temp=((tempbx&0xFF00)>>8)<<3;
  temp=temp|(USHORT)(((tempecx&0x0000FF00)>>8)&0x07);
  SiS_SetReg1(SiS_Part1Port,0x20,temp);  	/* <------------------- */    	/* Part1_20h */
  temp=tempbx&0x00FF;
  SiS_SetReg1(SiS_Part1Port,0x21,temp);  	/* <------------------- */     	/* Part1_21h */
  tempecx=tempecx>>16;                                     			/* BPLHCFACT  */
  if(modeflag&HalfDCLK) {
    	tempecx=tempecx>>1;
  }
  temp=(USHORT)((tempecx&0x0000FF00)>>8);
  SiS_SetReg1(SiS_Part1Port,0x22,temp);     	/* <------------------- */      /* Part1_22h */
  temp=(USHORT)(tempecx&0x000000FF);
  SiS_SetReg1(SiS_Part1Port,0x23,temp); 	/* <------------------- */
 /*add dstn new register*/
  if(SiS_IF_DEF_DSTN){
     	SiS_SetReg1(SiS_Part1Port,0x1E,0x01);
     	SiS_SetReg1(SiS_Part1Port,0x25,0x00);
     	SiS_SetReg1(SiS_Part1Port,0x26,0x00);
     	SiS_SetReg1(SiS_Part1Port,0x27,0x00);
     	SiS_SetReg1(SiS_Part1Port,0x28,0x87);
     	SiS_SetReg1(SiS_Part1Port,0x29,0x5A);
     	SiS_SetReg1(SiS_Part1Port,0x2A,0x4B);
     	SiS_SetRegANDOR(SiS_Part1Port,0x44,~0x007,0x03);
     	tempbx=SiS_HDE;                              				/*Blps=lcdhdee(lcdhdes+HDE) +64*/
     	tempbx=tempbx+64;
     	temp=tempbx&0x00FF;
     	SiS_SetReg1(SiS_Part1Port,0x38,temp);
     	temp=((tempbx&0xFF00)>>8)<<3;
     	SiS_SetRegANDOR(SiS_Part1Port,0x35,~0x078,temp);
     	tempbx=tempbx+32;		       					/*Blpe=lBlps+32*/
     	temp=tempbx&0x00FF;
     	/*fstn*/
     	if(SiS_IF_DEF_FSTN){
         	temp=0;
     	}
     	SiS_SetReg1(SiS_Part1Port,0x39,temp);
     	SiS_SetReg1(SiS_Part1Port,0x3A,0x00);  /*Bflml=0*/
     	SiS_SetRegANDOR(SiS_Part1Port,0x3C,~0x007,0x00);
     	tempbx=SiS_VDE;
     	tempbx=tempbx/2;
     	temp=tempbx&0x00FF;
     	SiS_SetReg1(SiS_Part1Port,0x3B,temp);
     	temp=((tempbx&0xFF00)>>8)<<3;
     	SiS_SetRegANDOR(SiS_Part1Port,0x3C,~0x038,temp);
     	tempeax=SiS_HDE;                       					/* BDxFIFOSTOP= (HDE*4)/128 */
     	tempeax=tempeax*4;
     	tempebx=128;
     	temp=(USHORT)(tempeax%tempebx);
     	tempeax=tempeax/tempebx;
     	if(temp!=0){
      		tempeax++;
     	}
     	temp=(USHORT)(tempeax&0x0000003F);
     	SiS_SetRegANDOR(SiS_Part1Port,0x45,~0x0FF,temp);
     	SiS_SetReg1(SiS_Part1Port,0x3F,0x00);           			/*BDxWadrst0*/
     	SiS_SetReg1(SiS_Part1Port,0x3E,0x00);
     	SiS_SetReg1(SiS_Part1Port,0x3D,0x10);
     	SiS_SetRegANDOR(SiS_Part1Port,0x3C,~0x040,0x00);
     	tempax=SiS_HDE;
     	tempax=tempax>>4;                                			/*BDxWadroff = HDE*4/8/8  */
     	pushcx=tempax;
     	temp=tempax&0x00FF;
     	SiS_SetReg1(SiS_Part1Port,0x43,temp);
     	temp=((tempax&0xFF00)>>8)<<3;
     	SiS_SetRegANDOR(SiS_Part1Port,0x44,~0x0F8,temp);
     	tempax=SiS_VDE;                        					/*BDxWadrst1 = BDxWadrst0+BDxWadroff*VDE*/
     	tempeax=(tempax*pushcx);
     	tempebx=0x00100000+tempeax;
     	temp=(USHORT)tempebx&0x000000FF;
     	SiS_SetReg1(SiS_Part1Port,0x42,temp);
     	temp=(USHORT)((tempebx&0x0000FF00)>>8);
     	SiS_SetReg1(SiS_Part1Port,0x41,temp);
     	temp=(USHORT)((tempebx&0x00FF0000)>>16);
     	SiS_SetReg1(SiS_Part1Port,0x40,temp);
     	temp=(USHORT)((tempebx&0x01000000)>>24);
     	temp=temp<<7;
     	SiS_SetRegANDOR(SiS_Part1Port,0x3C,~0x080,temp);
     	SiS_SetReg1(SiS_Part1Port,0x2F,0x03);
     	SiS_SetReg1(SiS_Part1Port,0x03,0x50);
     	SiS_SetReg1(SiS_Part1Port,0x04,0x00);
     	SiS_SetReg1(SiS_Part1Port,0x2F,0x01);
     	SiS_SetReg1(SiS_Part1Port,0x13,0x00);
     	SiS_SetReg1(SiS_P3c4,0x05,0x86);
     	SiS_SetReg1(SiS_P3c4,0x1e,0x62);
     	/*fstn*/
     	if(SiS_IF_DEF_FSTN){
         	SiS_SetReg1(SiS_P3c4,0x2b,0x1b);
         	SiS_SetReg1(SiS_P3c4,0x2c,0xe3);
         	SiS_SetReg1(SiS_P3c4,0x1e,0x62);
         	SiS_SetReg1(SiS_P3c4,0x2e,0x04);
         	SiS_SetReg1(SiS_P3c4,0x2f,0x42);
         	SiS_SetReg1(SiS_P3c4,0x32,0x01);
         	SiS_SetReg1(SiS_Part1Port,0x2b,0x02);
         	SiS_SetReg1(SiS_Part1Port,0x2c,0x00);
         	SiS_SetReg1(SiS_Part1Port,0x2d,0x00);
     	}
     	/*end add fstn*/
     	SiS_SetRegANDOR(SiS_Part1Port,0x19,0x00f,0x30);
     	SiS_SetReg1(SiS_Part1Port,0x1e,0x7d);
     	SiS_SetReg1(SiS_Part1Port,0x2e,0xe0);
  }
  /*end add dstn*/
  return;
}

/*301b*/
void
SiS_SetGroup1_LCDA(USHORT  BaseAddr,ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                   PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT RefreshRateTableIndex)
{
  USHORT modeflag,resinfo;
  USHORT push1,push2,tempax,tempbx,tempcx,temp;
  ULONG tempeax=0,tempebx,tempecx,tempvcfact;/*301b*/

  SiS_SetRegOR(SiS_Part1Port,0x2D,0x20);
  
  if(ModeNo<=0x13) {
    modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;      /* si+St_ResInfo */
    resinfo = SiS_SModeIDTable[ModeIdIndex].St_ResInfo;
  } else {
    modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;     /* si+Ext_ResInfo */
    resinfo = SiS_EModeIDTable[ModeIdIndex].Ext_RESINFO;
  }

  tempax=SiS_LCDHDES;
  tempbx=SiS_HDE;
  tempcx=SiS_HT;
  
  if(SiS_LCDInfo&LCDNonExpanding) {
    	if(SiS_LCDResInfo==Panel1280x1024) tempbx=1280;
    	if(SiS_LCDResInfo==Panel1024x768) tempbx=1024;
  }
  tempcx=tempcx-tempbx;                                    	/* HT-HDE  */
  push1=tempax;
  tempax=tempax+tempbx;                                    	/* lcdhdee  */
  tempbx=SiS_HT;
  if(tempax>=tempbx){
   	tempax=tempax-tempbx;
  }
  push2=tempax;
                                                           	/* push ax   lcdhdee  */
  tempcx=tempcx>>2;                                        	/* temp  */
  tempcx=tempcx+tempax;                                    	/* lcdhrs  */
  if(tempcx>=tempbx){
    	tempcx=tempcx-tempbx;
  }
                                                           	/* v ah,cl  */
  tempax=tempcx;
  tempax=tempax>>3;                                        	/* BPLHRS */
  temp=tempax&0x00FF;
  SiS_SetReg1(SiS_Part1Port,0x14,temp);                         /* Part1_14h  */
  temp=(tempax&0x00FF)+10;
  temp=temp&0x01F;
  temp=temp|(((tempcx&0x00ff)&0x07)<<5);
  SiS_SetReg1(SiS_Part1Port,0x15,temp);                         /* Part1_15h  */
  tempbx=push2;                                          	/* lcdhdee  */
  tempcx=push1;                                          	/* lcdhdes  */
  temp=(tempcx&0x00FF);
  temp=temp&0x07;                                      		/* BPLHDESKEW  */
  SiS_SetReg1(SiS_Part1Port,0x1A,temp);                         /* Part1_1Ah  */
  tempcx=tempcx>>3;                                        	/* BPLHDES */
  temp=(tempcx&0x00FF);
  SiS_SetReg1(SiS_Part1Port,0x16,temp);                         /* Part1_16h  */
  if(tempbx&0x07) tempbx=tempbx+8;
  tempbx=tempbx>>3;                                        	/* BPLHDEE  */
  temp=tempbx&0x00FF;
  SiS_SetReg1(SiS_Part1Port,0x17,temp);                        	/* Part1_17h  */

  tempcx=SiS_VGAVT;
  tempbx=SiS_VGAVDE;
  tempcx=tempcx-tempbx;                                    	/* GAVT-VGAVDE  */
  tempbx=SiS_LCDVDES;                                          	/* VGAVDES  */
  push1=tempbx;                                        		/* push bx temppush1 */
  if(SiS_IF_DEF_TRUMPION==0){
    if(SiS_IF_DEF_CH7005==1) {
      if(SiS_VBInfo&SetCRT2ToTV) {
        tempax=SiS_VGAVDE;
      }
    }
    if(SiS_LCDResInfo==Panel1024x768) tempax=768;
    if(SiS_LCDResInfo==Panel1280x1024)  tempax=1024;
  }
  else tempax=SiS_VGAVDE;
  tempbx=tempbx+tempax;
  tempax=SiS_VT;                                               	/* VT  */
  if(tempbx>=SiS_VT){
    tempbx=tempbx-tempax;
  }
  push2=tempbx;                                        		/* push bx  temppush2  */
  tempcx=tempcx>>1;
  tempbx=tempbx+tempcx;
  tempbx++;                                                	/* BPLVRS  */
  if(tempbx>=tempax){
    tempbx=tempbx-tempax;
  }
  temp=tempbx&0x00FF;
  SiS_SetReg1(SiS_Part1Port,0x18,temp);                         /* Part1_18h  */
  tempcx=tempcx>>3;
  tempcx=tempcx+tempbx;
  tempcx++;                                                	/* BPLVRE  */
  temp=tempcx&0x00FF;
  temp=temp&0x0F;
  SiS_SetRegANDOR(SiS_Part1Port,0x19,~0x00F,temp);               /* Part1_19h  */
  temp=(tempbx&0xFF00)>>8;
  temp=temp&0x07;
  temp=temp<<3;                                 		/* BPLDESKEW =0 */
  tempbx=SiS_VGAVDE;
  if(tempbx!=SiS_VDE){
    temp=temp|0x40;
  }
  if(SiS_SetFlag&EnableLVDSDDA) {
    temp=temp|0x40;
  }
  if(SiS_LCDInfo&LCDRGB18Bit) {
    temp=temp|0x80;
  }
  SiS_SetRegANDOR(SiS_Part1Port,0x1A,0x07,temp);             	/* Part1_1Ah */
  
  tempbx=push2;                                        		/* p bx temppush2 BPLVDEE  */
  tempcx=push1;                                        		/* pop cx temppush1 NPLVDES */
  push1=(USHORT)(tempeax&0xFFFF);
    
  if(!(SiS_VBInfo&SetInSlaveMode)) {
    if(SiS_LCDResInfo==Panel800x600) {
      if(resinfo==7) tempcx++;
    } else {
      if(SiS_LCDResInfo==Panel1024x768) {
        if(resinfo==8) tempcx++;
      }
    }
  }

  temp=(tempbx&0xFF00)>>8;
  temp=temp&0x07;
  temp=temp<<3;
  temp=temp|(((tempcx&0xFF00)>>8)&0x07);
  SiS_SetReg1(SiS_Part1Port,0x1D,temp);                          /* Part1_1Dh */
  temp=tempbx&0x00FF;
  SiS_SetReg1(SiS_Part1Port,0x1C,temp);                          /* Part1_1Ch  */
  temp=tempcx&0x00FF;
  SiS_SetReg1(SiS_Part1Port,0x1B,temp);                          /* Part1_1Bh  */ 

  tempecx=SiS_VGAVT;
  tempebx=SiS_VDE;
  tempeax=SiS_VGAVDE;
  tempecx=tempecx-tempeax;                                 	/* VGAVT-VGAVDE  */
  tempeax=tempeax<<18;
  temp=(USHORT)(tempeax%tempebx);
  tempeax=tempeax/tempebx;
  if(temp!=0){
    tempeax++;
  }
  tempebx=tempeax;                                        	/* BPLVCFACT  */
  tempvcfact=tempeax;       /*301b*/
  temp=(USHORT)(tempebx&0x00FF);
  SiS_SetReg1(SiS_Part1Port,0x37,temp);   
  temp=(USHORT)((tempebx&0x00FF00)>>8);
  SiS_SetReg1(SiS_Part1Port,0x36,temp);   
  temp=(USHORT)((tempebx&0x00030000)>>16);
  if(SiS_VDE==SiS_VGAVDE) {
      temp=temp|0x04;
  }
  
  SiS_SetReg1(SiS_Part1Port,0x35,temp);   
  
  tempecx=SiS_VGAHDE;
  tempebx=SiS_HDE;
  tempeax=tempecx;
  tempeax=tempeax<<6;
  tempeax=tempeax<<10;
  tempeax=tempeax/tempebx;
  if(tempebx==tempecx){
    	tempeax=0xFFFF;
  }
  tempecx=tempeax;
  tempeax=SiS_VGAHDE;  /*301b to change HT->HDE*/
  tempeax=tempeax<<6;
  tempeax=tempeax<<10;
  tempeax=tempeax/tempecx;
  tempecx=tempecx<<16;
  tempeax=tempeax-1;
  tempecx=tempecx|(tempeax&0x00FFFF);
  temp=(USHORT)(tempecx&0x00FF);
  SiS_SetReg1(SiS_Part1Port,0x1F,temp);                          /* Part1_1Fh  */

  tempeax=SiS_VGAVDE;
  tempeax=tempeax<<18;          /*301b*/
  tempeax=tempeax/tempvcfact;
  tempbx=(USHORT) (tempeax&0x0FFFF);
  if(SiS_LCDResInfo==Panel1024x768) tempbx--;
  if(SiS_SetFlag&EnableLVDSDDA){
    	tempbx=1;
  }
  temp=((tempbx&0xFF00)>>8)<<3;
  temp=temp|(USHORT)(((tempecx&0x0000FF00)>>8)&0x07);
  SiS_SetReg1(SiS_Part1Port,0x20,temp);                         /* Part1_20h */
  temp=tempbx&0x00FF;
  SiS_SetReg1(SiS_Part1Port,0x21,temp);                         /* Part1_21h */
  tempecx=tempecx>>16;                                     	/* BPLHCFACT  */
 
  temp=(USHORT)((tempecx&0x0000FF00)>>8);
  SiS_SetReg1(SiS_Part1Port,0x22,temp);                         /* Part1_22h */
  temp=(USHORT)(tempecx&0x000000FF);
  SiS_SetReg1(SiS_Part1Port,0x23,temp);
  
  if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
  	SiS_SetReg1(SiS_Part1Port,0x1e,0x20);  /* for 650 & 550 lvds part */
  }
  return;
}
/*end 301b*/

void SiS_SetTPData()
{
  return;
}

void SiS_SetCRT2Offset(USHORT SiS_Part1Port,ULONG ROMAddr,USHORT ModeNo,
                       USHORT ModeIdIndex ,USHORT RefreshRateTableIndex,
		       PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT offset;
  UCHAR temp;

  if(SiS_VBInfo&SetInSlaveMode) return;

  offset=SiS_GetOffset(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex,
                       HwDeviceExtension);
  temp=(UCHAR)(offset&0xFF);
  SiS_SetReg1(SiS_Part1Port,0x07,temp);
  temp=(UCHAR)((offset&0xFF00)>>8);
  SiS_SetReg1(SiS_Part1Port,0x09,temp);
  temp=(UCHAR)(((offset>>3)&0xFF)+1);
  SiS_SetReg1(SiS_Part1Port,0x03,temp);
}

USHORT
SiS_GetOffset(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
              USHORT RefreshRateTableIndex,PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT temp,colordepth;
  USHORT modeinfo,index,infoflag;
  USHORT ColorDepth[]={0x02,0x04,0x08};  /* TW: Was 1,2,4 */

  modeinfo = SiS_EModeIDTable[ModeIdIndex].Ext_ModeInfo;
  infoflag = SiS_RefIndex[RefreshRateTableIndex].Ext_InfoFlag;
  if (HwDeviceExtension->jChipType < SIS_315H ) {
  	/* 300 series */
    	index=(modeinfo>>4)&0xFF;
  } else {
  	/* 310 series */
    	index=(modeinfo>>8)&0xFF;
  }
  temp=SiS_ScreenOffset[index];
  if(infoflag&InterlaceMode){
    	temp=temp<<1;
  }
  colordepth=SiS_GetColorDepth(ROMAddr,ModeNo,ModeIdIndex);

  if((ModeNo>=0x7C)&&(ModeNo<=0x7E)) {
  	/* TW: For 1280x960 */
    	temp=ModeNo-0x7C;
    	colordepth=ColorDepth[temp];
    	temp=0x6B;
    	if(infoflag&InterlaceMode){
      		temp=temp<<1;
    	}
   	return(temp*colordepth);
  }
  else return(temp*colordepth);
}

USHORT
SiS_GetColorDepth(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex)
{
  USHORT ColorDepth[6]={1,2,4,4,6,8};
  SHORT  index;
  USHORT modeflag;

  if(ModeNo<=0x13) {
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
  } else {
    	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
  }
  index=(modeflag&ModeInfoFlag)-ModeEGA;
  if(index<0) index=0;
  return(ColorDepth[index]);
}

void
SiS_SetCRT2Sync(USHORT BaseAddr,ULONG ROMAddr,USHORT ModeNo,
                USHORT RefreshRateTableIndex)
{
  USHORT tempah=0,infoflag,flag;

  flag=0;
  infoflag = SiS_RefIndex[RefreshRateTableIndex].Ext_InfoFlag;
  if(SiS_IF_DEF_LVDS==1){
    if(SiS_VBInfo&SetCRT2ToLCD){
      tempah=SiS_LCDInfo;
      if(tempah&LCDSync){
        flag=1;
      }
    }
  }
  if(flag!=1) tempah=infoflag>>8;
  tempah=tempah&0xC0;
  tempah=tempah|0x20;
  if(!(SiS_LCDInfo&LCDRGB18Bit)) tempah=tempah|0x10;
  if(SiS_IF_DEF_CH7005==1) tempah=tempah|0xC0;
  SiS_SetRegANDOR(SiS_Part1Port,0x19,0x3F,tempah);
}

void
SiS_SetCRT2FIFO(USHORT  SiS_Part1Port,ULONG ROMAddr,USHORT ModeNo,
                PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT temp,index;
  USHORT modeidindex,refreshratetableindex;
  USHORT VCLK,MCLK,colorth=0,data2;
  ULONG  data,eax;
  UCHAR  LatencyFactor[] ={
  	97, 88, 86, 79, 77, 00,       /*; 64  bit    BQ=2   */
        00, 87, 85, 78, 76, 54,       /*; 64  bit    BQ=1   */
        97, 88, 86, 79, 77, 00,       /*; 128 bit    BQ=2   */
        00, 79, 77, 70, 68, 48,       /*; 128 bit    BQ=1   */
        80, 72, 69, 63, 61, 00,       /*; 64  bit    BQ=2   */
        00, 70, 68, 61, 59, 37,       /*; 64  bit    BQ=1   */
        86, 77, 75, 68, 66, 00,       /*; 128 bit    BQ=2   */
        00, 68, 66, 59, 57, 37};      /*; 128 bit    BQ=1   */

  SiS_SearchModeID(ROMAddr,ModeNo,&modeidindex);
  SiS_SetFlag=SiS_SetFlag&(~ProgrammingCRT2);
  SiS_SelectCRT2Rate=0;
  refreshratetableindex=SiS_GetRatePtrCRT2(ROMAddr,ModeNo,modeidindex);   /* 11.GetRatePtr */
  if(ModeNo>=0x13) {
    index=SiS_RefIndex[refreshratetableindex].Ext_CRTVCLK;
    index=index&0x3F;
    VCLK=SiS_VCLKData[index].CLOCK;           /* Get VCLK  */
    index=SiS_GetReg1(SiS_P3c4,0x1A);
    index=index&07;
    MCLK=SiS_MCLKData[index].CLOCK;           /* Get MCLK  */
    data2=SiS_ModeType-0x02;
    switch (data2) {
      case 0 : 	colorth=1; break;
      case 1 : 	colorth=1; break;
      case 2 : 	colorth=2; break;
      case 3 : 	colorth=2; break;
      case 4 : 	colorth=3; break;
      case 5 : 	colorth=4; break;
    }
    /* data2=(data2*VCLK)/MCLK;   */  /*  bx */
    data2=(colorth*VCLK)/MCLK;  /* TW */

    temp = SiS_GetReg1(SiS_P3c4,0x14);
    temp = ((temp&0x00FF)>>6)<<1;
    if(temp==0) temp=1;
    temp=temp<<2;

    data2=temp-data2;

/*  if(data2%(28*16)) {		 TW: WRONG
      	data2=data2/(28*16);
      	data2++;
    } else {
      	data2=data2/(28*16);
    } */
    if((28*16)%data2) {		/* TW */
      	data2=(28*16)/data2;
      	data2++;
    } else {
      	data2=(28*16)/data2;
    }

    index=0;
    temp = SiS_GetReg1(SiS_P3c4,0x14);
    if(temp&0x0080) index=index+12;

    SiS_SetReg4(0xcf8,0x800000A0);
    eax=SiS_GetReg3(0xcfc);
    temp=(USHORT)(eax>>24);
    if(!(temp&0x01)) index=index+24;

    SiS_SetReg4(0xcf8,0x80000050);
    eax=SiS_GetReg3(0xcfc);
    temp=(USHORT)(eax>>24);
    if(temp&0x01) index=index+6;

    temp=(temp&0x0F)>>1;
    index=index+temp;
    data=LatencyFactor[index];
    data=data+15;
    temp = SiS_GetReg1(SiS_P3c4,0x14);
    if(!(temp&0x80)) data=data+5;

    data=data+data2;

    SiS_SetFlag=SiS_SetFlag|ProgrammingCRT2;
    data=data*VCLK*colorth;
    if(data%(MCLK<<4)) {
      	data=data/(MCLK<<4);
      	data++;
    } else {
      	data=data/(MCLK<<4);
    }

/* TW: The following made all the calculations above void... */

#if 0
    temp=0x16;
/*  Revision ID  */
    temp=0x13;
/*  Revision ID  */
    SiS_SetRegANDOR(SiS_Part1Port,0x01,~0x01F,temp);
    SiS_SetRegANDOR(SiS_Part1Port,0x02,~0x01F,temp);
#endif

   /* TW: We do this instead: */

   temp = SiS_GetReg1(SiS_Part1Port,0x01);
   if( (HwDeviceExtension->jChipType == SIS_630 ) &&
	    ((HwDeviceExtension->jChipRevision & 0xf0) == 0x30) ) /* 630s */
   {
	temp = (temp & (~0x1F)) | 0x1b;  	/* TW: VESA sets 1b, prev. 0x19 */
   } else {
	temp = (temp & (~0x1F)) | 0x16;
   }
   SiS_SetReg1(SiS_Part1Port,0x01,temp);  	/* FIFO HIGH? */

   if(data <= 6) data = 6;
   if(data > 0x14) data = 0x14;
   if (SiS_IF_DEF_LVDS==1) {			/* TW: LVDS doesn't like 0x14 */
	if(data > 0x13) data = 0x13;
   }
   SiS_SetRegANDOR(SiS_Part1Port,0x02,~0x01F,data);  /* FIFO LOW? */
  /* TW end */
  }
}

void
SiS_SetCRT2FIFO2(USHORT SiS_Part1Port,ULONG ROMAddr,USHORT ModeNo,
                 PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
#ifdef SIS315H
 UCHAR CombCode[]={1,1,1,4,3,1,3,4,4,1,4,4,5,1,5,4};
 UCHAR CRT2ThLow[]={39,63,55,79,78,102,90,114,55,87,84,116,103,135,119,151};
#endif
  USHORT temp,temp1,temp2,temp3;
  USHORT index;
  USHORT CRT1ModeNo,CRT2ModeNo;
  USHORT ModeIdIndex;
  USHORT RefreshRateTableIndex;

  SiS_SetReg1(SiS_Part1Port,0x1,0x3B);
  /* CRT1ModeNo=(UCHAR)SiS_GetReg1(SiS_P3d4,0x34); *//* get CRT1 ModeNo */
  CRT1ModeNo = SiS_CRT1Mode;
  /* CRT1ModeNo =ModeNo; */
  SiS_SearchModeID(ROMAddr,CRT1ModeNo,&ModeIdIndex);    /* Get ModeID Table */
  SiS_SetFlag=SiS_SetFlag& (~ProgrammingCRT2);

  RefreshRateTableIndex=SiS_GetRatePtrCRT2(ROMAddr,CRT1ModeNo,
                            ModeIdIndex); /* Set REFIndex-> for crt1 refreshrate */
  index = SiS_GetVCLK2Ptr(ROMAddr,CRT1ModeNo,ModeIdIndex,
                          RefreshRateTableIndex,HwDeviceExtension);
  temp1=SiS_VCLKData[index].CLOCK;           /* Get VCLK  */

  temp2= SiS_GetColorDepth(ROMAddr,CRT1ModeNo,ModeIdIndex);
#ifdef SIS315H
  index = SiS_Get310DRAMType(ROMAddr);
#endif
  temp3=SiS_MCLKData[index].CLOCK;           /* Get MCLK  */

  temp=SiS_GetReg1(SiS_P3c4,0x14);
  if (temp&0x02)
   	temp=16;
  else
    	temp=8;

  temp = temp - temp1*temp2/temp3; /* 16-DRamBus - DCLK*BytePerPixel/MCLK */

  if ((52*16 % temp)==0)
    	temp = 52*16/temp +40;
  else
    	temp = 52*16/temp +40 + 1;

  /* get DRAM latency */
  temp1=(SiS_GetReg1(SiS_P3c4,0x17)>>3)&0x7; /* SR17[5:3] DRAM Queue depth */
  temp2=(SiS_GetReg1(SiS_P3c4,0x17)>>6)&0x3; /* SR17[7:6] DRAM Grant length */

#ifdef SIS315H
  if (SiS_Get310DRAMType(ROMAddr)<2)
  {
    for (temp3=0;temp3<16;temp3+=2)
    {
      if ((CombCode[temp3]==temp1) && (CombCode[temp3+1]==temp2))
      {
        temp3 = CRT2ThLow[temp3>>1];
      }
    }
  }
  else
  {
    for (temp3=0;temp3<16;temp3+=2)
    {
      if ((CombCode[temp3]==temp1) && (CombCode[temp3+1]==temp2))
      {
        temp3 = CRT2ThLow[8+(temp3>>1)];
      }
    }
  }
#endif

  temp +=  temp3; /* CRT1 Request Period */

  CRT2ModeNo = ModeNo; /* get CRT2 ModeNo */
  SiS_SearchModeID(ROMAddr,CRT2ModeNo,&ModeIdIndex);    /* Get ModeID Table */
  SiS_SetFlag=SiS_SetFlag|ProgrammingCRT2;
  RefreshRateTableIndex=SiS_GetRatePtrCRT2(ROMAddr,CRT1ModeNo,
                        ModeIdIndex);/* Set REFIndex-> for crt1 refreshrate */
  index = SiS_GetVCLK2Ptr(ROMAddr,CRT2ModeNo,ModeIdIndex,
                          RefreshRateTableIndex,HwDeviceExtension);
  temp1=SiS_VCLKData[index].CLOCK;           /* Get VCLK  */

  temp2= SiS_GetColorDepth(ROMAddr,CRT2ModeNo,ModeIdIndex);
#ifdef SIS315H
  index = SiS_Get310DRAMType(ROMAddr);
#endif
  temp3=SiS_MCLKData[index].CLOCK;           /* Get MCLK  */
 
  if ((temp*temp1*temp2)%(16*temp3)==0)
    temp = temp*temp1*temp2/(16*temp3);   /* CRT1 Request period * TCLK*BytePerPixel/(MCLK*16) */
  else 
    temp = temp*temp1*temp2/(16*temp3)+1; /* CRT1 Request period * TCLK*BytePerPixel/(MCLK*16) */

  if (temp>0x37)
    temp = 0x37;

  SiS_SetRegANDOR(SiS_Part1Port,0x02,~0x3F,temp);
}

void
SiS_GetLVDSDesData(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                   USHORT RefreshRateTableIndex)
{
  USHORT modeflag;
  USHORT PanelIndex,ResIndex;
  SiS_LVDSDesStruct  *PanelDesPtr=NULL;

  if((SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
       && (SiS_IF_DEF_LVDS == 0) ) {  /*301b*//*for test*/
     SiS_GetLVDSDesPtrA(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex,
                        &PanelIndex,&ResIndex);
     switch (PanelIndex)
     {
     	case  0: PanelDesPtr = LVDS1024x768Des_1;   break;
     	case  1: PanelDesPtr = LVDS1280x1024Des_1;  break;
     	case  2: PanelDesPtr = LVDS1280x960Des_1;   break;
     	case  3: PanelDesPtr = LVDS1024x768Des_2;   break;
     	case  4: PanelDesPtr = LVDS1280x1024Des_2;  break;
     	case  5: PanelDesPtr = LVDS1280x960Des_2;   break;
     }
  } else {
     SiS_GetLVDSDesPtr(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex,
                       &PanelIndex,&ResIndex);
#ifdef LINUX_KERNEL
     if (PanelIndex < 32)	/* >=32 = TV */
         printk(KERN_INFO "sisfb: LVDS-LCD panel type %d (Resindex %d)\n", PanelIndex, ResIndex);
#endif
     switch (PanelIndex)
     {
     	case  0: PanelDesPtr = SiS_PanelType00_1;   break;
     	case  1: PanelDesPtr = SiS_PanelType01_1;   break;
     	case  2: PanelDesPtr = SiS_PanelType02_1;   break;
     	case  3: PanelDesPtr = SiS_PanelType03_1;   break;
     	case  4: PanelDesPtr = SiS_PanelType04_1;   break;
     	case  5: PanelDesPtr = SiS_PanelType05_1;   break;
     	case  6: PanelDesPtr = SiS_PanelType06_1;   break;
     	case  7: PanelDesPtr = SiS_PanelType07_1;   break;
     	case  8: PanelDesPtr = SiS_PanelType08_1;   break;
     	case  9: PanelDesPtr = SiS_PanelType09_1;   break;
     	case 10: PanelDesPtr = SiS_PanelType0a_1;   break;
     	case 11: PanelDesPtr = SiS_PanelType0b_1;   break;
     	case 12: PanelDesPtr = SiS_PanelType0c_1;   break;	/* TW: Clevo 2202   */
     	case 13: PanelDesPtr = SiS_PanelType0d_1;   break;
     	case 14: PanelDesPtr = SiS_PanelType0e_1;   break;	/* TW: Uniwill N271S2 */
     	case 15: PanelDesPtr = SiS_PanelType0f_1;   break;
     	case 16: PanelDesPtr = SiS_PanelType00_2;   break;
     	case 17: PanelDesPtr = SiS_PanelType01_2;   break;
     	case 18: PanelDesPtr = SiS_PanelType02_2;   break;
     	case 19: PanelDesPtr = SiS_PanelType03_2;   break;
     	case 20: PanelDesPtr = SiS_PanelType04_2;   break;
     	case 21: PanelDesPtr = SiS_PanelType05_2;   break;
     	case 22: PanelDesPtr = SiS_PanelType06_2;   break;
     	case 23: PanelDesPtr = SiS_PanelType07_2;   break;
     	case 24: PanelDesPtr = SiS_PanelType08_2;   break;
     	case 25: PanelDesPtr = SiS_PanelType09_2;   break;
     	case 26: PanelDesPtr = SiS_PanelType0a_2;   break;
     	case 27: PanelDesPtr = SiS_PanelType0b_2;   break;
     	case 28: PanelDesPtr = SiS_PanelType0c_2;   break;
     	case 29: PanelDesPtr = SiS_PanelType0d_2;   break;
     	case 30: PanelDesPtr = SiS_PanelType0e_2;   break;
     	case 31: PanelDesPtr = SiS_PanelType0f_2;   break;
     	case 32: PanelDesPtr = SiS_CHTVUNTSCDesData;   break;
     	case 33: PanelDesPtr = SiS_CHTVONTSCDesData;   break;
     	case 34: PanelDesPtr = SiS_CHTVUPALDesData;    break;
     	case 35: PanelDesPtr = SiS_CHTVOPALDesData;    break;
     }
  }
  SiS_LCDHDES = (PanelDesPtr+ResIndex)->LCDHDES;
  SiS_LCDVDES = (PanelDesPtr+ResIndex)->LCDVDES;

  if(SiS_LCDInfo&LCDNonExpanding){
    if(SiS_LCDResInfo>=Panel1024x768){
      if(ModeNo<=0x13){
        modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
        if(!(modeflag&HalfDCLK)) {
          SiS_LCDHDES=320;
        }
      }
    }
  }
  return;
}

void
SiS_GetLVDSDesPtr(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                  USHORT RefreshRateTableIndex,USHORT *PanelIndex,
		  USHORT *ResIndex)
{
  USHORT tempbx,tempal;

  tempbx=0;
  if(SiS_IF_DEF_CH7005==1) {
    if(!(SiS_VBInfo&SetCRT2ToLCD)) {
      tempbx=32;
      if(SiS_VBInfo&SetPALTV) tempbx=tempbx+2;
      if(SiS_VBInfo&SetCHTVOverScan) tempbx=tempbx+1;
    }
  } 
  if(SiS_VBInfo&SetCRT2ToLCD) {
    tempbx=SiS_LCDTypeInfo;
    if(SiS_LCDInfo&LCDNonExpanding){
      tempbx=tempbx+16;
    }
  }
  if(ModeNo<=0x13){
    	tempal = SiS_SModeIDTable[ModeIdIndex].St_CRT2CRTC;
  }else{
    	tempal = SiS_RefIndex[RefreshRateTableIndex].Ext_CRT2CRTC;
  }
  tempal=tempal&0x1F;
  *PanelIndex=tempbx;
  *ResIndex=tempal;
}

/*301b*/
void
SiS_GetLVDSDesPtrA(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                   USHORT RefreshRateTableIndex,USHORT *PanelIndex,
		   USHORT *ResIndex)
{
  USHORT tempbx=0,tempal;

  tempbx=SiS_LCDResInfo-Panel1024x768;
  if(SiS_LCDInfo&LCDNonExpanding){
      tempbx=tempbx+3;
  }

  if(ModeNo<=0x13){
    	tempal = SiS_SModeIDTable[ModeIdIndex].St_CRT2CRTC;
  } else {
    	tempal = SiS_RefIndex[RefreshRateTableIndex].Ext_CRT2CRTC;
  }
  tempal=tempal&0x1F;
  *PanelIndex=tempbx;
  *ResIndex=tempal;
}
/*end 301b*/

void
SiS_SetCRT2ModeRegs(USHORT BaseAddr,USHORT ModeNo,
                    PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT i,j;
  USHORT tempcl,tempah;

  SiS_SetReg1(SiS_Part1Port,0x03,0x00);   /*fix write part1 index 0  BTDRAM bit Bug*/

  /*301b*/
  if( (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
  				&& (SiS_VBInfo&SetCRT2ToLCDA) ) {

	/* TW:   1. for 301B/302B/301LV/302LV (on some of which
	 *          IF_DEF_LVDS seems to be 1 as well)
	 */

      SiS_SetRegANDOR(SiS_Part1Port,0x00,~0x050,0x40); /* FUNCTION CONTROL */
      SiS_SetRegAND(SiS_Part1Port,0x2E,0xF7);
      SiS_SetRegANDOR(SiS_Part1Port,0x13,0xFB,0x04);
      SiS_SetRegANDOR(SiS_Part1Port,0x2c,0xCF,0x30);
      SiS_SetRegANDOR(SiS_Part4Port,0x21,0x3F,0xC0);
      SiS_SetRegANDOR(SiS_Part4Port,0x23,0x7F,0x00);
  /*end 301b*/
  } else {
    for(i=0,j=4;i<3;i++,j++) SiS_SetReg1(SiS_Part1Port,j,0);

    tempcl=SiS_ModeType;
    if(HwDeviceExtension->jChipType < SIS_315H) {
      /* ---- 300 series ---- */
      if(ModeNo>0x13){
        tempcl=tempcl-ModeVGA;
        if((tempcl>0)||(tempcl==0)){
           tempah=((0x010>>tempcl)|0x080);
        }
      } else {
        tempah=0x080;
      }
      if(SiS_VBInfo&SetInSlaveMode){
         tempah=(tempah^0x0A0);
      }
    } else {
    /* ---- 310 series ---- */
      if(ModeNo>0x13) {
        tempcl=tempcl-ModeVGA;
        if((tempcl>0)||(tempcl==0)){
           tempah=(0x008>>tempcl);
           if (tempah==0) tempah=1;
              tempah |= 0x040;
        }
      } else {
       tempah=0x040;
      }
  
      if(SiS_VBInfo&SetInSlaveMode){
          tempah=(tempah^0x050);
      }
    }

    if(SiS_VBInfo&CRT2DisplayFlag){
      	tempah=0;
    }
    SiS_SetReg1(SiS_Part1Port,0x00,tempah);  /* FUNCTION CONTROL */

    if(SiS_IF_DEF_LVDS==0) {    /* ifdef 301*/

	/* TW:   2. for 301 */

    	tempah=0x01;
    	if(!(SiS_VBInfo&SetInSlaveMode)) {
      		tempah=(tempah|0x02);
    	}
    	if(!(SiS_VBInfo&SetCRT2ToRAMDAC)) {
      		tempah=(tempah^0x05);
      		if(!(SiS_VBInfo&SetCRT2ToLCD)) {
        		tempah=(tempah^0x01);
      		}
    	}

    	tempcl=tempah;     /* 05/03/01 ynlai for TV display bug */

    	if(HwDeviceExtension->jChipType < SIS_315H) {
      		/* 300 series */
      		tempah=(tempah<<5)&0xFF;
      		if(SiS_VBInfo&CRT2DisplayFlag){
        		tempah=0;
      		}
      		SiS_SetReg1(SiS_Part1Port,0x01,tempah);

      		tempah=tempah>>5;
    	} else {
      		/* 310 series */
      		if(SiS_VBInfo&CRT2DisplayFlag){
        		tempah=0;
      		}
      		tempah = (SiS_GetReg1(SiS_Part1Port,0x2E)&0xF8)|tempah;
      		SiS_SetReg1(SiS_Part1Port,0x2E,tempah);
      		tempah=tempcl;
    	}

    	if((SiS_ModeType==ModeVGA)&&(!(SiS_VBInfo&SetInSlaveMode))){
      		tempah=tempah|0x010;
    	}
      
     	if(SiS_LCDResInfo==Panel1024x768)
         	tempah=tempah|0x080;
  
    	if((SiS_LCDResInfo==Panel1280x1024)||(SiS_LCDResInfo==Panel1280x960)){
      		tempah=tempah|0x080;
    	}
    	if(SiS_VBInfo&SetCRT2ToTV){
      		if(SiS_VBInfo&SetInSlaveMode){
        		if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {   /*301b*/
          			if(SiS_SetFlag&TVSimuMode)
                			tempah=tempah|0x020;
        		} else
          			tempah=tempah|0x020;
      		}
    	}
    	SiS_SetRegANDOR(SiS_Part4Port,0x0D,~0x0BF,tempah);
    	tempah=0;
    	if(SiS_VBInfo&SetCRT2ToTV) {
      		if(SiS_VBInfo&SetInSlaveMode) {
       			if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) { /*301b*/
           		{
            			SiS_SetFlag=SiS_SetFlag|RPLLDIV2XO;
            			tempah=tempah|0x40;
          		}
       			} else {
        			if(!(SiS_SetFlag&TVSimuMode)) {
          				if(!(SiS_VBInfo&SetCRT2ToHiVisionTV)) {
            					SiS_SetFlag=SiS_SetFlag|RPLLDIV2XO;
            					tempah=tempah|0x40;
          				}
        			}
      			}
     		}
      		else {
        		SiS_SetFlag=SiS_SetFlag|RPLLDIV2XO;
        		tempah=tempah|0x40;
      		}
    	}
    	if(SiS_LCDResInfo==Panel1280x1024) tempah=tempah|0x80;
    	if(SiS_LCDResInfo==Panel1280x960) tempah=tempah|0x80;
    	SiS_SetReg1(SiS_Part4Port,0x0C,tempah);
    } else {

    	/* TW: 3. for LVDS, _not_ 301B/302B/301LV/302LV */

    	tempah=0;

    	if((!(SiS_VBInfo&SetInSlaveMode)) &&
          	    (ModeNo!=0x2e)&&(ModeNo!=0x30)&&(ModeNo!=0x38)&&(ModeNo!=0x3A)) {
      			tempah=tempah|0x02;
    	}
    	SiS_SetRegANDOR(SiS_Part1Port,0x2e,0xF0,tempah); /* <------------------- */
    }
  }

  /*301b*/
  if( (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
 				&& (!(SiS_VBInfo&SetCRT2ToLCDA))){
      if(SiS_IsDualEdge(BaseAddr))
         SiS_SetRegANDOR(SiS_Part1Port,0x13,0xFB,0x00);
      else
         SiS_SetRegANDOR(SiS_Part1Port,0x13,0xFF,0x00);
      if(SiS_IsDualEdge(BaseAddr))
         SiS_SetRegANDOR(SiS_Part1Port,0x2c,0xCF,0x00);
      else
         SiS_SetRegANDOR(SiS_Part1Port,0x2c,0xFF,0x00);
      if(SiS_IsDualEdge(BaseAddr))
         SiS_SetRegANDOR(SiS_Part4Port,0x21,0x3F,0x00);
      else
         SiS_SetRegANDOR(SiS_Part4Port,0x21,0xFF,0x00);

      if(SiS_IsDualEdge(BaseAddr))
         SiS_SetRegANDOR(SiS_Part4Port,0x23,0xFF,0x80);
       else
         SiS_SetRegANDOR(SiS_Part4Port,0x23,0xFF,0x00);     
  }
  /*end 301b*/
}

void
SiS_GetCRT2Data(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                USHORT RefreshRateTableIndex)
{
  if(SiS_IF_DEF_LVDS==0) { /* 301  */
     if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
        if(SiS_VBInfo&SetCRT2ToLCDA)
          SiS_GetCRT2DataLVDS(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex);
        else
          SiS_GetCRT2Data301(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex);
     } else
     	SiS_GetCRT2Data301(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex);
     return;
  } else {  /* LVDS */
     SiS_GetCRT2DataLVDS(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex);
     return;
  }
}

void
SiS_GetCRT2DataLVDS(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                    USHORT RefreshRateTableIndex)
{
   USHORT tempax,tempbx;
   USHORT CRT2Index,ResIndex;
   SiS_LVDSDataStruct *LVDSData=NULL;

   SiS_GetCRT2ResInfo(ROMAddr,ModeNo,ModeIdIndex);
   /*301b*/
   if((SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
   	 && (SiS_VBInfo&SetCRT2ToLCDA)) {
      SiS_GetCRT2PtrA(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex,
                      &CRT2Index,&ResIndex);
      switch (CRT2Index) {
      	case  0:  LVDSData=SiS_LVDS1024x768Data_1;    break;
      	case  1:  LVDSData=SiS_LVDS1280x1024Data_1;   break;
      	case  2:  LVDSData=SiS_LVDS1280x1024Data_1;   break;
    	/*  case  2:  LVDSData=SiS_LVDS1280x960Data_1;  break;*/
      	case  3:  LVDSData=SiS_LVDS1024x768Data_2;    break;
      	case  4:  LVDSData=SiS_LVDS1280x1024Data_2;   break;
      	case  5:  LVDSData=SiS_LVDS1280x1024Data_2;   break;
    	/*  case  5:  LVDSData=SiS_LVDS1280x960Data_2;  break; */
       }    
   } else {
      SiS_GetCRT2Ptr(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex,&CRT2Index,&ResIndex);
      switch (CRT2Index) {
      	case  0:  LVDSData=SiS_LVDS800x600Data_1;    break;
      	case  1:  LVDSData=SiS_LVDS1024x768Data_1;   break;
      	case  2:  LVDSData=SiS_LVDS1280x1024Data_1;  break;
      	case  3:  LVDSData=SiS_LVDS800x600Data_2;    break;
      	case  4:  LVDSData=SiS_LVDS1024x768Data_2;   break;
      	case  5:  LVDSData=SiS_LVDS1280x1024Data_2;  break;
      	case  6:  LVDSData=SiS_LVDS640x480Data_1;    break;
      	case  7:  LVDSData=SiS_CHTVUNTSCData;        break;
      	case  8:  LVDSData=SiS_CHTVONTSCData;        break;
      	case  9:  LVDSData=SiS_CHTVUPALData;         break;
      	case 10:  LVDSData=SiS_CHTVOPALData;         break;
      	case 11:  LVDSData=SiS_LVDS320x480Data_1;    break;
     }
   }
   SiS_VGAHT = (LVDSData+ResIndex)->VGAHT;
   SiS_VGAVT = (LVDSData+ResIndex)->VGAVT;
   SiS_HT = (LVDSData+ResIndex)->LCDHT;
   SiS_VT = (LVDSData+ResIndex)->LCDVT;

  /*301b*/ /* TW: I have NOT added LVDS check here */
  if( (SiS_IF_DEF_LVDS==0) &&
      (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))) {/*for test*/
    if(!(SiS_LCDInfo&LCDNonExpanding)){
         if(SiS_LCDResInfo==Panel1024x768){
           tempax=1024;
           tempbx=768;
         } else {
           tempax=1280;
           tempbx=1024;
         }
         SiS_HDE=tempax;
         SiS_VDE=tempbx;
    }
  } else {
   if(SiS_IF_DEF_TRUMPION==0){
     if(SiS_VBInfo&SetCRT2ToLCD){
       if(!(SiS_LCDInfo&LCDNonExpanding)){
         if(SiS_LCDResInfo==Panel640x480){
           tempax=640;
           tempbx=480;
         } else if(SiS_LCDResInfo==Panel800x600){
           tempax=800;
           tempbx=600;
         } else if(SiS_LCDResInfo==Panel1024x768){
           tempax=1024;
           tempbx=768;
         } else {
           tempax=1280;
           tempbx=1024;
         }
         if(SiS_IF_DEF_FSTN){           /*fstn*/
           tempax=320;
           tempbx=480;
         }
         SiS_HDE=tempax;
         SiS_VDE=tempbx;
       }
     }
   }
  } 
  return;
}

void
SiS_GetCRT2Data301(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                        USHORT RefreshRateTableIndex)
{
  USHORT tempax,tempbx,modeflag;
  USHORT resinfo;
  USHORT CRT2Index,ResIndex;
  SiS_LCDDataStruct *LCDPtr=NULL;
  SiS_TVDataStruct  *TVPtr=NULL;

  if(ModeNo<=0x13) {
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;      /* si+St_ResInfo */
    	resinfo = SiS_SModeIDTable[ModeIdIndex].St_ResInfo;
  } else {
    	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;     /* si+Ext_ResInfo */
    	resinfo = SiS_EModeIDTable[ModeIdIndex].Ext_RESINFO;
  }
  SiS_NewFlickerMode=0;
  SiS_RVBHRS=50;
  SiS_RY1COE=0;
  SiS_RY2COE=0;
  SiS_RY3COE=0;
  SiS_RY4COE=0;

  SiS_GetCRT2ResInfo(ROMAddr,ModeNo,ModeIdIndex);
  if(SiS_VBInfo&SetCRT2ToRAMDAC){
    	SiS_GetRAMDAC2DATA(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex);
    	return;
  }

  if(SiS_VBInfo&SetCRT2ToTV){
    SiS_GetCRT2Ptr(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex,
                   &CRT2Index,&ResIndex);
    switch (CRT2Index) {
      case  2:  TVPtr=SiS_ExtHiTVData;   break;
      case  3:  TVPtr=SiS_ExtPALData;    break;
      case  4:  TVPtr=SiS_ExtNTSCData;   break;
      case  7:  TVPtr=SiS_St1HiTVData;   break;
      case  8:  TVPtr=SiS_StPALData;     break;
      case  9:  TVPtr=SiS_StNTSCData;    break;
      case 12:  TVPtr=SiS_St2HiTVData;   break;
      default:  TVPtr=SiS_StPALData;     break;  /* TW: Just to avoid a crash */
    }

    SiS_RVBHCMAX = (TVPtr+ResIndex)->RVBHCMAX;
    SiS_RVBHCFACT = (TVPtr+ResIndex)->RVBHCFACT;
    SiS_VGAHT = (TVPtr+ResIndex)->VGAHT;
    SiS_VGAVT = (TVPtr+ResIndex)->VGAVT;
    SiS_HDE = (TVPtr+ResIndex)->TVHDE;
    SiS_VDE = (TVPtr+ResIndex)->TVVDE;
    SiS_RVBHRS = (TVPtr+ResIndex)->RVBHRS;
    SiS_NewFlickerMode = (TVPtr+ResIndex)->FlickerMode;
    if(SiS_VBInfo&SetCRT2ToHiVisionTV ) {
      	if(resinfo==0x08) SiS_NewFlickerMode=0x40;
      	if(resinfo==0x09) SiS_NewFlickerMode=0x40;
	if(resinfo==0x10) SiS_NewFlickerMode=0x40;
    }
    if(SiS_VBInfo&SetCRT2ToHiVisionTV ) {
      if(SiS_VGAVDE==350) SiS_SetFlag=SiS_SetFlag|TVSimuMode;
      tempax=ExtHiTVHT;
      tempbx=ExtHiTVVT;
      if(SiS_VBInfo&SetInSlaveMode) {
        if(SiS_SetFlag&TVSimuMode) {
          tempax=StHiTVHT;
          tempbx=StHiTVVT;
          if(!(modeflag&Charx8Dot)){
            tempax=StHiTextTVHT;
            tempbx=StHiTextTVVT;
          }
        }
      }
    }
    if(!(SiS_VBInfo&SetCRT2ToHiVisionTV)) {
      SiS_RY1COE = (TVPtr+ResIndex)->RY1COE;
      SiS_RY2COE = (TVPtr+ResIndex)->RY2COE;
      if(modeflag&HalfDCLK) {
        SiS_RY1COE = 0x00;
        SiS_RY2COE = 0xf4;
      }
      SiS_RY3COE = (TVPtr+ResIndex)->RY3COE;
      SiS_RY4COE = (TVPtr+ResIndex)->RY4COE;
      if(modeflag&HalfDCLK) {
        SiS_RY3COE = 0x10;
        SiS_RY4COE = 0x38;
      }
      if(!(SiS_VBInfo&SetPALTV)){
        tempax=NTSCHT;
        tempbx=NTSCVT;
      }
      else{
        tempax=PALHT;
        tempbx=PALVT;
      }
    }
    SiS_HT=tempax;
    SiS_VT=tempbx;
    return;
  }

  if(SiS_VBInfo&SetCRT2ToLCD){
    SiS_GetCRT2Ptr(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex,
                   &CRT2Index,&ResIndex);
    switch (CRT2Index) {
      case  0: LCDPtr = SiS_ExtLCD1024x768Data;        break;
      case  1: LCDPtr = SiS_ExtLCD1280x1024Data;       break;
      case  5: LCDPtr = SiS_StLCD1024x768Data;         break;
      case  6: LCDPtr = SiS_StLCD1280x1024Data;        break;
      case 10: LCDPtr = SiS_St2LCD1024x768Data;        break;
      case 11: LCDPtr = SiS_St2LCD1280x1024Data;       break;
      case 13: LCDPtr = SiS_NoScaleData;               break;
      case 14: LCDPtr = SiS_LCD1280x960Data;           break;
      default: LCDPtr = SiS_ExtLCD1024x768Data;	       break; /* TW: Just to avoid a crash */
    }

    SiS_RVBHCMAX = (LCDPtr+ResIndex)->RVBHCMAX;
    SiS_RVBHCFACT = (LCDPtr+ResIndex)->RVBHCFACT;
    SiS_VGAHT = (LCDPtr+ResIndex)->VGAHT;
    SiS_VGAVT = (LCDPtr+ResIndex)->VGAVT;
    SiS_HT = (LCDPtr+ResIndex)->LCDHT;
    SiS_VT = (LCDPtr+ResIndex)->LCDVT;
    tempax=1024;
    if(SiS_SetFlag&LCDVESATiming) {
      if(SiS_VGAVDE==350) tempbx=560;
      else if(SiS_VGAVDE==400) tempbx=640;
      else tempbx=768;
    }
    else {
      if(SiS_VGAVDE==357) tempbx=527;
      else if(SiS_VGAVDE==420) tempbx=620;
      else if(SiS_VGAVDE==525) tempbx=775;
      else if(SiS_VGAVDE==600) tempbx=775;
      else if(SiS_VGAVDE==350) tempbx=560;
      else if(SiS_VGAVDE==400) tempbx=640;
      else tempbx=768;
    }
    if(SiS_LCDResInfo==Panel1280x1024){
      tempax=1280;
      if(SiS_VGAVDE==360) tempbx=768;
      else if(SiS_VGAVDE==375) tempbx=800;
           else if(SiS_VGAVDE==405) tempbx=864;
                else tempbx=1024;
    }
    if(SiS_LCDResInfo==Panel1280x960){
      tempax=1280;
      if(SiS_VGAVDE==350) tempbx=700;
      else if(SiS_VGAVDE==400) tempbx=800;
           else if(SiS_VGAVDE==1024) tempbx=960;
                else tempbx=960;
    }
    if(SiS_LCDInfo&LCDNonExpanding) {
       tempax=SiS_VGAHDE;
       tempbx=SiS_VGAVDE;
    }
    SiS_HDE=tempax;
    SiS_VDE=tempbx;
    return;
  }
}

USHORT
SiS_GetResInfo(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex)
{
  USHORT resindex;

  if(ModeNo<=0x13){
    	resindex=SiS_SModeIDTable[ModeIdIndex].St_ResInfo;              /* si+St_ResInfo */
  } else {
    	resindex=SiS_EModeIDTable[ModeIdIndex].Ext_RESINFO;            	/* si+Ext_ResInfo */
  }
  return(resindex);
}

void
SiS_GetCRT2ResInfo(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex)
{
  USHORT xres,yres,modeflag,resindex;

  resindex=SiS_GetResInfo(ROMAddr,ModeNo,ModeIdIndex);
  if(ModeNo<=0x13){
    	xres=SiS_StResInfo[resindex].HTotal;
    	yres=SiS_StResInfo[resindex].VTotal;
  } else {
    	xres=SiS_ModeResInfo[resindex].HTotal;                         		/* xres->ax */
    	yres=SiS_ModeResInfo[resindex].VTotal;                         		/* yres->bx */
    	modeflag=SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;      		/* si+St_ModeFlag */
    	if(SiS_IF_DEF_FSTN){							/*fstn*/
       		xres=xres*2;
       		yres=yres*2;
    	} else {
      		if(modeflag&HalfDCLK) { xres=xres*2;}
      		if(modeflag&DoubleScanMode) {yres=yres*2;}
    	}
  }
  if(SiS_IF_DEF_LVDS==0) {
    	if(SiS_LCDResInfo==Panel1280x1024) {
      		if(yres==400) yres=405;
      		if(yres==350) yres=360;
      		if(SiS_SetFlag&LCDVESATiming) {
        		if(yres==360) yres=375;
      		}
   	 }
    	if(SiS_LCDResInfo==Panel1024x768){
      		if(!(SiS_SetFlag&LCDVESATiming)) {
        		if(!(SiS_LCDInfo&LCDNonExpanding)) {
          			if(yres==350) yres=357;
          			if(yres==400) yres=420;
/*          			if(!OldBios)             */
            			if(yres==480) yres=525;
        		}
      		}
    	}
  } else { /* LVDS - does not support 720x??? */
    	if(xres==720) xres=640;
  }
  SiS_VGAHDE=xres;
  SiS_HDE=xres;
  SiS_VGAVDE=yres;
  SiS_VDE=yres;
}

void
SiS_GetCRT2Ptr(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
	       USHORT RefreshRateTableIndex,USHORT *CRT2Index,USHORT *ResIndex)
{
  USHORT tempbx,tempal=0;
  USHORT Flag;
  if(SiS_IF_DEF_LVDS==0) {
    	if(SiS_VBInfo&SetCRT2ToLCD){              /* LCD */
      		tempbx=SiS_LCDResInfo;
      		tempbx=tempbx-Panel1024x768;
      		if(!(SiS_SetFlag&LCDVESATiming)) {
        		tempbx+=5;
/*      		GetRevisionID();  */
        		tempbx+=5;
       		}
     	} else {
       		if(SiS_VBInfo&SetCRT2ToHiVisionTV){              /* TV */
         		if(SiS_VGAVDE>480) SiS_SetFlag=SiS_SetFlag&(!TVSimuMode);
         		tempbx=2;
         		if(SiS_VBInfo&SetInSlaveMode) {
            			if(!(SiS_SetFlag&TVSimuMode)) tempbx=10;
         		}
       		} else {
         		if(SiS_VBInfo&SetPALTV){
           			tempbx=3;
         		} else {
           			tempbx=4;
         		}
         		if(SiS_SetFlag&TVSimuMode){
           			tempbx=tempbx+5;
         		}
       		}
     	}

     	if(ModeNo<=0x13){
       		tempal = SiS_SModeIDTable[ModeIdIndex].St_CRT2CRTC;
     	} else {
       		tempal = SiS_RefIndex[RefreshRateTableIndex].Ext_CRT2CRTC;
     	}
     	tempal=tempal&0x3F;
     	/*301b*/
      	if((SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
       					&& (SiS_VBInfo&SetCRT2ToTV)) {
        	/*look*/
      		if(tempal==0x06) tempal=0x07;
        }
   	/*end 301b*/
     	if((0x31<=ModeNo)&&(ModeNo<=0x35)) tempal=6;
     	if(SiS_LCDInfo&LCDNonExpanding) tempbx=0x0D;
     	if(SiS_LCDResInfo==Panel1280x960) tempbx=0x0E;	/* TW !!!! */
     	*CRT2Index=tempbx;
     	*ResIndex=tempal;
  } else {   /* LVDS */
    	Flag=1;
    	tempbx=0;
    	if(SiS_IF_DEF_CH7005==1) {
      		if(!(SiS_VBInfo&SetCRT2ToLCD)) {
        		Flag=0;
        		tempbx=7;
        		if(SiS_VBInfo&SetPALTV) tempbx=tempbx+2;
        		if(SiS_VBInfo&SetCHTVOverScan) tempbx=tempbx+1;
      		}
    	}
    	if(Flag==1) {
      		tempbx=SiS_LCDResInfo-Panel800x600;
      		if(SiS_LCDInfo&LCDNonExpanding){
        		tempbx=tempbx+3;
      		}
    	}
     	if(SiS_LCDResInfo==Panel640x480){
        	tempbx=6;
      	}
     	/*fstn*/
     	if(SiS_IF_DEF_FSTN){
       	 	if(SiS_LCDResInfo==Panel320x480){
         		tempbx=11;                        /* not same with asmber code */
         		tempal=6;	/* TW: That can't work - is being overwritten below... */
        	}
    	}

	else 				/* TW: .... unless I place an "else" here */
     
    	if(ModeNo<=0x13){
      		tempal = SiS_SModeIDTable[ModeIdIndex].St_CRT2CRTC;
    	} else {
      		tempal = SiS_RefIndex[RefreshRateTableIndex].Ext_CRT2CRTC;
    	}
    	tempal=tempal&0x1F;
    	*CRT2Index=tempbx;
    	*ResIndex=tempal;
  }
}

void
SiS_GetCRT2PtrA(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
		USHORT RefreshRateTableIndex,USHORT *CRT2Index,
		USHORT *ResIndex)
{
  USHORT tempbx,tempal;

  tempbx=SiS_LCDResInfo-Panel1024x768;
  if(SiS_LCDInfo&LCDNonExpanding){
        tempbx=tempbx+3;
  }
  if(ModeNo<=0x13){
      	tempal = SiS_SModeIDTable[ModeIdIndex].St_CRT2CRTC;
  } else {
      	tempal = SiS_RefIndex[RefreshRateTableIndex].Ext_CRT2CRTC;
  }
  tempal=tempal&0x1F;
  *CRT2Index=tempbx;
  *ResIndex=tempal;
}
/*end 301b*/

USHORT
SiS_GetRatePtrCRT2(ULONG ROMAddr, USHORT ModeNo, USHORT ModeIdIndex)
{
  /* TW: These tables are being indexed with SiS_LCDResInfo.
   *     This is equal to the Panel???x??? constants in initdef.h
   *     and is eg. 2 for 1024x768; these tables were each one
   *     value too short!
   */
  SHORT  LCDRefreshIndex[]  = {0x00,0x00,0x03,0x01,0x01};
  SHORT  LCDARefreshIndex[] = {0x00,0x00,0x03,0x01,0x01,0x01,0x01,0x01};
  USHORT RefreshRateTableIndex,i,backup_i;
  USHORT modeflag,index,temp;

  if (ModeNo<=0x13) {
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
  }else {
    	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
  }

  if(SiS_IF_DEF_CH7005==1) {
    	if(SiS_VBInfo&SetCRT2ToTV) {
      		if(modeflag&HalfDCLK) return(0);
    	}
  }

  if(ModeNo<0x14) return(0xFFFF);

 /* TW: CR33 holds refresh rate index for CRT1 (0-3) and CRT2 (4-7).
  *     On LVDS machines, CRT2 index is always 0 and will be
  *     set to 0 by the following code; this causes the function
  *     to take the first non-interlaced mode in SiS300_Ext2Struct
  *     (which is the second 1024x768 mode in case the resolution
  *     is 1024x768; the first mode in the list is interlaced and
  *     therefore skipped).
  */

  index=SiS_GetReg1(SiS_P3d4,0x33);
  index=index>>SiS_SelectCRT2Rate;
  index=index&0x0F;
  if(SiS_LCDInfo&LCDNonExpanding) index=0;
  if(index>0) index--;

  if(SiS_SetFlag & ProgrammingCRT2) {
    	if(SiS_IF_DEF_CH7005==1) {
      		if(SiS_VBInfo&SetCRT2ToTV) {
        		index=0;
      		}
    	}
    	if(SiS_VBInfo&(SetCRT2ToLCD|SetCRT2ToLCDA)) {
      		if(SiS_IF_DEF_LVDS==0) {
        		if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
           			temp=LCDARefreshIndex[SiS_LCDResInfo];     /* 301b */
        		else
           			temp=LCDRefreshIndex[SiS_LCDResInfo];	   /* 301  */
        		if(index>temp){
          			index=temp;
        		}
      		} else {  						   /* LVDS */
        		index=0;
      		}
    	}
  }

  RefreshRateTableIndex = SiS_EModeIDTable[ModeIdIndex].REFindex;
  ModeNo = SiS_RefIndex[RefreshRateTableIndex].ModeID;

  i=0;
  do {
    	if (SiS_RefIndex[RefreshRateTableIndex+i].ModeID!=ModeNo) break;
    	temp = SiS_RefIndex[RefreshRateTableIndex+i].Ext_InfoFlag;
    	temp=temp&ModeInfoFlag;
    	if(temp<SiS_ModeType) break;
    	i++;
    	index--;
  } while(index!=0xFFFF);

  if(!(SiS_VBInfo&SetCRT2ToRAMDAC)) {
    	if(SiS_VBInfo&SetInSlaveMode) {
      		temp=SiS_RefIndex[RefreshRateTableIndex+i-1].Ext_InfoFlag;
      			if(temp&InterlaceMode) {
        			i++;
      			}
    	}
  }
  i--;

  if((SiS_SetFlag & ProgrammingCRT2) && (!(SiS_VBInfo & DisableCRT2Display))) {
    	backup_i = i;
    	if (!(SiS_AdjustCRT2Rate(ROMAddr,ModeNo,ModeIdIndex,
	                             RefreshRateTableIndex,&i))) {
		/* TW: This is for avoiding random data to be used; i is
		 *     in an undefined state if no matching CRT2 mode is
		 *     found.
		 */
		i = backup_i;
#ifdef LINUX_KERNEL
		printk("sisfb: WARNING: No matching CRT2 mode found\n");
#endif
	}
  }

  return(RefreshRateTableIndex+i);                			/*return(0x01|(temp1<<1));   */
}

BOOLEAN
SiS_AdjustCRT2Rate(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                   USHORT RefreshRateTableIndex,USHORT *i)
{
  USHORT tempax,tempbx,resinfo;
  USHORT modeflag,infoflag;

  if (ModeNo<=0x13) {
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;    	/* si+St_ModeFlag */
  } else {
    	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
  }

  resinfo = SiS_EModeIDTable[ModeIdIndex].Ext_RESINFO;
  tempbx = SiS_RefIndex[RefreshRateTableIndex+(*i)].ModeID;
  tempax=0;
  if(SiS_IF_DEF_LVDS==0) {
  	/* TW: For 301 (and which ones of 301B, 302B, 301LV, 302LV ?) */
    	if(SiS_VBInfo&SetCRT2ToRAMDAC) {
      		tempax=tempax|SupportRAMDAC2;
    	}
    	if(SiS_VBInfo&(SetCRT2ToLCD|SetCRT2ToLCDA)) {        /* 301b */
      		tempax=tempax|SupportLCD;
      		if(SiS_LCDResInfo!=Panel1280x1024) {
        		if(SiS_LCDResInfo!=Panel1280x960) {
           			if(SiS_LCDInfo&LCDNonExpanding) {
             				if(resinfo>=9) {
               					tempax=0;
               					return(0);
             				}
           			}
        		}
      		}
    	}
    	if(SiS_VBInfo&SetCRT2ToHiVisionTV) {    /* for HiTV */
      		tempax=tempax|SupportHiVisionTV;
      		if(SiS_VBInfo&SetInSlaveMode){
        		if(resinfo==4) return(0);
        		if(resinfo==3) {
          			if(SiS_SetFlag&TVSimuMode) return(0);
        		}
        		if(resinfo>7) return(0);
      		}
    	} else {
      		if(SiS_VBInfo&(SetCRT2ToAVIDEO|SetCRT2ToSVIDEO|SetCRT2ToSCART)) {
        		tempax=tempax|SupportTV;
       			/*301b*/
         		if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
             			tempax=tempax|SupportTV1024;
         		}
        		/*end 301b*/
       
        		if(!(SiS_VBInfo&SetPALTV)) {
          			if(modeflag&NoSupportSimuTV) {
            				if(SiS_VBInfo&SetInSlaveMode) {
              					if(!(SiS_VBInfo&SetNotSimuMode)) {
                					return 0;
              					}
            				}
          			}
        		}
      		}
    	}
  } else {
  	/* TW: for LVDS (and which ones of the SiS bridges?) */
    	if(SiS_IF_DEF_CH7005==1) {
      		if(SiS_VBInfo&SetCRT2ToTV) {
        		tempax=tempax|SupportCHTV;
      		}
    	}
    	if(SiS_VBInfo&SetCRT2ToLCD) {
      		tempax=tempax|SupportLCD;
      		if(resinfo>0x08) return(0);       	  /* 1024x768  */
      		if(SiS_LCDResInfo<Panel1024x768) {
        		if(resinfo>0x07) return(0);       /*  800x600  */
        		if(resinfo==0x04) return(0);      /*  512x384  */
      		}
    	}
  }
  /* TW: Look backwards in table for matching CRT2 mode */
  for(;SiS_RefIndex[RefreshRateTableIndex+(*i)].ModeID==tempbx;(*i)--) {
     	infoflag = SiS_RefIndex[RefreshRateTableIndex+(*i)].Ext_InfoFlag;
     	if(infoflag&tempax) {
       		return(1);
     	}
     	if ((*i)==0) break;
  }
  /* TW: Look through the whole mode-section of the table from the beginning
   *     for a matching CRT2 mode if no mode was found yet.
   */
  for((*i)=0;;(*i)++) {
     	infoflag = SiS_RefIndex[RefreshRateTableIndex+(*i)].Ext_InfoFlag;
     	if(SiS_RefIndex[RefreshRateTableIndex+(*i)].ModeID!=tempbx) {
       		return(0);
     	}
     	if(infoflag&tempax) {
       		return(1);
     	}
  }
  return(1);
}

void
SiS_SaveCRT2Info(USHORT ModeNo)
{
  USHORT temp1,temp2;

  SiS_SetReg1(SiS_P3d4,0x34,ModeNo);  /* reserve CR34 for CRT1 Mode No */
  temp1=(SiS_VBInfo&SetInSlaveMode)>>8;
  temp2=~(SetInSlaveMode>>8);
  SiS_SetRegANDOR(SiS_P3d4,0x31,temp2,temp1);
}

void
SiS_GetVBInfo301(USHORT BaseAddr,ULONG ROMAddr,USHORT ModeNo,
                 USHORT ModeIdIndex,PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT tempax,tempbx,temp;
  USHORT modeflag;
  UCHAR  OutputSelect=*pSiS_OutputSelect;

  if (ModeNo<=0x13) {
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
  } else {
   	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
  }
  SiS_SetFlag=0;

  SiS_ModeType=modeflag&ModeInfoFlag;
  
  tempbx=0;
  if(SiS_BridgeIsOn(BaseAddr)) {
    	temp=SiS_GetReg1(SiS_P3d4,0x30);
    	if((SiS_IF_DEF_LVDS==0) && (SiS_VBType&(VB_SIS301LV|VB_SIS302LV))) {
       		temp=temp&0xbf;                                        	/*301lvds disable CRT2*/
    	}
    	if(SiS_IF_DEF_FSTN){                             		/*fstn must set CR30=0x21 */
       		temp=0x21;
       		SiS_SetReg1(SiS_P3d4,0x30,temp);
    	}
    	tempbx=tempbx|temp;
    	temp=SiS_GetReg1(SiS_P3d4,0x31);
    	tempax=temp<<8;
    	tempbx=tempbx|tempax;
    	temp=SetCHTVOverScan|SetInSlaveMode|DisableCRT2Display;  	/* ynlai */
   	temp=0xFFFF^temp;
    	tempbx=tempbx&temp;
#ifdef SIS315H
     	/*301b*/
    	if((SiS_IF_DEF_LVDS==0) && (SiS_VBType&(VB_SIS302B|VB_SIS302LV))){
       		temp=SiS_GetReg1(SiS_P3d4,0x38);
       		if(temp==0x03)
          		tempbx=tempbx|(SetCRT2ToLCDA);
    	}
    	/*end301b*/
#endif
    	if(SiS_IF_DEF_LVDS==0) {
      		if(SiS_IF_DEF_HiVision) temp=0x80FC;
      		else temp=0x807C;
    	} else {
      		if(SiS_IF_DEF_CH7005==1) {
        		temp = SetCRT2ToTV|SetCRT2ToLCD;
      		} else {
        		temp = SetCRT2ToLCD;
      		}
    	}
    	if(!(tempbx&temp)) {
      		tempax=tempax|DisableCRT2Display;
      		tempbx=0;
    	}
   
   	if(SiS_IF_DEF_LVDS==0) {
      		if(tempbx&SetCRT2ToLCDA) {                                    /*301b*/
        		tempbx=tempbx&(0xFF00|SwitchToCRT2|SetSimuScanMode);
      		} else if(tempbx&SetCRT2ToRAMDAC) {
        		tempbx=tempbx&(0xFF00|SetCRT2ToRAMDAC|SwitchToCRT2|SetSimuScanMode);
      		} else if((tempbx&SetCRT2ToLCD)&&(!(SiS_VBType&VB_NoLCD)) ){              /*301dlvds*/
        		tempbx=tempbx&(0xFF00|SetCRT2ToLCD|SwitchToCRT2|SetSimuScanMode);
      		} else if(tempbx&SetCRT2ToSCART){
        		tempbx=tempbx&(0xFF00|SetCRT2ToSCART|SwitchToCRT2|SetSimuScanMode);
        		tempbx=tempbx|SetPALTV;
      		} else if(tempbx&SetCRT2ToHiVisionTV){
        		tempbx=tempbx&(0xFF00|SetCRT2ToHiVisionTV|SwitchToCRT2|SetSimuScanMode);
  			/* ynlai begin */
        		tempbx=tempbx|SetPALTV;
  			/* ynlai end */
      		}
   	} else { /* LVDS */
      		if(SiS_IF_DEF_CH7005==1) {
        		if(tempbx&SetCRT2ToTV)
          			tempbx=tempbx&(0xFF00|SetCRT2ToTV|SwitchToCRT2|SetSimuScanMode);
      		}
      		if(tempbx&SetCRT2ToLCD)
        		tempbx=tempbx&(0xFF00|SetCRT2ToLCD|SwitchToCRT2|SetSimuScanMode);
	}
    	if(tempax&DisableCRT2Display) {
      		if(!(tempbx&(SwitchToCRT2|SetSimuScanMode))) {
        		tempbx=SetSimuScanMode|DisableCRT2Display;
      		}
    	}
    	if(!(tempbx&DriverMode)){
      		tempbx=tempbx|SetSimuScanMode;
    	}
	/* TW: LVDS bridge can only be slave in 8bpp modes */
	if ((SiS_IF_DEF_LVDS==1) &&
		(ModeNo == 0x2e || ModeNo == 0x30 ||
			ModeNo == 0x38 || ModeNo == 0x3a)) {
		tempbx=tempbx|SetSimuScanMode|SetInSlaveMode;
	}
	/* TW end */
    	if(!(tempbx&SetSimuScanMode)){
      		if(tempbx&SwitchToCRT2) {
        		if(!(modeflag&CRT2Mode)) {
            			tempbx=tempbx|SetSimuScanMode;
        		}

      		} else {
        		if(!(SiS_BridgeIsEnable(BaseAddr,HwDeviceExtension))) {
          			if(!(tempbx&DriverMode)) {
            				if(SiS_BridgeInSlave()) {
              					tempbx=tempbx|SetInSlaveMode;
            				}
          			}
        		}
      		}
    	}
    	if(!(tempbx&DisableCRT2Display)) {
     		 if(tempbx&DriverMode) {
        		if(tempbx&SetSimuScanMode) {
          			if(!(modeflag&CRT2Mode)) {
            				tempbx=tempbx|SetInSlaveMode;
            				if(SiS_IF_DEF_LVDS==0) {
              					if(tempbx&SetCRT2ToTV) {
                					if(!(tempbx&SetNotSimuMode))
								SiS_SetFlag=SiS_SetFlag|TVSimuMode;
              					}
            				}
          			}
        		}
      		} else {
        		tempbx=tempbx|SetInSlaveMode;
        		if(SiS_IF_DEF_LVDS==0) {
          			if(tempbx&SetCRT2ToTV) {
            				if(!(tempbx&SetNotSimuMode))
						SiS_SetFlag=SiS_SetFlag|TVSimuMode;
          			}
        		}
      		}
    	}
    	if(SiS_IF_DEF_CH7005==1) {
      		temp=SiS_GetReg1(SiS_P3d4,0x35);
      		if(temp&TVOverScan) tempbx=tempbx|SetCHTVOverScan;
    	}
  }
  /*add PALMN*/
  if(SiS_IF_DEF_LVDS==0) {
#ifdef SIS300
     	if((HwDeviceExtension->jChipType==SIS_630)||
           (HwDeviceExtension->jChipType==SIS_730)) {
           	if(!(OutputSelect&EnablePALMN))
             		SiS_SetRegAND(SiS_P3d4,0x35,0x3F);
           	if(tempbx&SetCRT2ToTV) {
              		if(tempbx&SetPALTV) {
                  		temp=SiS_GetReg1(SiS_P3d4,0x35);
                  		temp=temp&0xC0;
                  		if(temp==0x40)
                    			tempbx=tempbx&(~SetPALTV);
             		}
          	}
      	}
#endif
#ifdef SIS315H
     	if((HwDeviceExtension->jChipType == SIS_315H)||
           (HwDeviceExtension->jChipType == SIS_315PRO)||
           (HwDeviceExtension->jChipType == SIS_550)||   /* 05/02/01 ynlai for 550 */
           (HwDeviceExtension->jChipType == SIS_640)||   /* 08/20/01 chiawen for 640/740 */
           (HwDeviceExtension->jChipType == SIS_740)||   /* 09/03/01 chiawen for 640/740 */
           (HwDeviceExtension->jChipType == SIS_650)) {
		if(!(OutputSelect&EnablePALMN))
        		SiS_SetRegAND(SiS_P3d4,0x38,0x3F);
   		if(tempbx&SetCRT2ToTV) {
    			if(tempbx&SetPALTV) {
               			temp=SiS_GetReg1(SiS_P3d4,0x38);
               			temp=temp&0xC0;
               			if(temp==0x40)
               				tempbx=tempbx&(~SetPALTV);
              		}
        	}
  	}
#endif
 /*end add*/
  }
  SiS_VBInfo=tempbx;
#ifdef LINUX_KERNEL
  printk(KERN_INFO "sisfb: (VBInfo = %x)\n", SiS_VBInfo);
#endif
#ifdef LINUX_XF86
  xf86DrvMsg(0, X_INFO, "(init301.c: VBInfo = %x)\n", SiS_VBInfo);
#endif
}

void
SiS_GetRAMDAC2DATA(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                   USHORT RefreshRateTableIndex)
{
  USHORT tempax,tempbx,temp;
  USHORT temp1,temp2,modeflag=0,tempcx;
  USHORT StandTableIndex,CRT1Index;
  USHORT ResInfo,DisplayType;
  SiS_LVDSCRT1DataStruct *LVDSCRT1Ptr=NULL;

  SiS_RVBHCMAX=1;
  SiS_RVBHCFACT=1;
  if(ModeNo<=0x13){
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
    	StandTableIndex = SiS_GetModePtr(ROMAddr,ModeNo,ModeIdIndex);
    	tempax = SiS_StandTable[StandTableIndex].CRTC[0];
    	tempbx = SiS_StandTable[StandTableIndex].CRTC[6];
    	temp1=SiS_StandTable[StandTableIndex].CRTC[7];
  } else {
    if( (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
        			&& (SiS_VBInfo&SetCRT2ToLCDA) ) {
    	/*add crt1ptr*/
    	temp=SiS_GetLVDSCRT1Ptr(ROMAddr,ModeNo,ModeIdIndex,
			RefreshRateTableIndex,&ResInfo,&DisplayType);
    	if(temp==0){
    		return;
    	}
    	switch(DisplayType) {
    		case 0 : LVDSCRT1Ptr = SiS_LVDSCRT1800x600_1;		break;
    		case 1 : LVDSCRT1Ptr = SiS_LVDSCRT11024x768_1;          break;
    		case 2 : LVDSCRT1Ptr = SiS_LVDSCRT11280x1024_1;         break;
    		case 3 : LVDSCRT1Ptr = SiS_LVDSCRT1800x600_1_H;         break;
    		case 4 : LVDSCRT1Ptr = SiS_LVDSCRT11024x768_1_H;        break;
    		case 5 : LVDSCRT1Ptr = SiS_LVDSCRT11280x1024_1_H;       break;
    		case 6 : LVDSCRT1Ptr = SiS_LVDSCRT1800x600_2;           break;
    		case 7 : LVDSCRT1Ptr = SiS_LVDSCRT11024x768_2;          break;
    		case 8 : LVDSCRT1Ptr = SiS_LVDSCRT11280x1024_2;         break;
    		case 9 : LVDSCRT1Ptr = SiS_LVDSCRT1800x600_2_H;         break;
    		case 10: LVDSCRT1Ptr = SiS_LVDSCRT11024x768_2_H;        break;
    		case 11: LVDSCRT1Ptr = SiS_LVDSCRT11280x1024_2_H;       break;
    		case 12: LVDSCRT1Ptr = SiS_CHTVCRT1UNTSC;               break;
    		case 13: LVDSCRT1Ptr = SiS_CHTVCRT1ONTSC;               break;
    		case 14: LVDSCRT1Ptr = SiS_CHTVCRT1UPAL;                break;
    		case 15: LVDSCRT1Ptr = SiS_CHTVCRT1OPAL;                break;
    		case 16: LVDSCRT1Ptr = SiS_LVDSCRT1320x480_1;           break;
    	}
    	temp1=(LVDSCRT1Ptr+ResInfo)->CR[0];
    	temp2=(LVDSCRT1Ptr+ResInfo)->CR[14];
    	tempax=(temp1&0xFF)|((temp2&0x03)<<8);
    	tempbx=(LVDSCRT1Ptr+ResInfo)->CR[6];
    	tempcx=(LVDSCRT1Ptr+ResInfo)->CR[13]<<8;
    	tempcx = tempcx&0x0100;
    	tempcx = tempcx << 2;
    	tempbx = tempbx | tempcx;
    	temp1=(LVDSCRT1Ptr+ResInfo)->CR[7];
    	/*add 301b*/
    } else {
    	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
    	CRT1Index = SiS_RefIndex[RefreshRateTableIndex].Ext_CRT1CRTC;
    	CRT1Index=CRT1Index&0x3F;
    	temp1 = (USHORT)SiS_CRT1Table[CRT1Index].CR[0];
    	temp2 = (USHORT)SiS_CRT1Table[CRT1Index].CR[14];
    	tempax=(temp1&0xFF)|((temp2&0x03)<<8);
    	tempbx = (USHORT)SiS_CRT1Table[CRT1Index].CR[6];
    	tempcx = (USHORT)SiS_CRT1Table[CRT1Index].CR[13]<<8;
    	tempcx = tempcx&0x0100;
    	tempcx = tempcx << 2;
    	tempbx = tempbx | tempcx;
    	temp1 = (USHORT)SiS_CRT1Table[CRT1Index].CR[7];
   }
  }
  if(temp1&0x01) tempbx=tempbx|0x0100;
  if(temp1&0x20) tempbx=tempbx|0x0200;
  tempax=tempax+5;
  if(modeflag&Charx8Dot) tempax=tempax*8;
  else tempax=tempax*9;

  SiS_VGAHT=tempax;
  SiS_HT=tempax;
  tempbx++;
  SiS_VGAVT=tempbx;
  SiS_VT=tempbx;
}

void
SiS_UnLockCRT2(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT BaseAddr)
{
  if(HwDeviceExtension->jChipType >= SIS_315H) {
    	SiS_SetRegANDOR(SiS_Part1Port,0x2f,0xFF,0x01);
  } else {
    	SiS_SetRegANDOR(SiS_Part1Port,0x24,0xFF,0x01);
  }
}

void
SiS_LockCRT2(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT BaseAddr)
{
  if(HwDeviceExtension->jChipType >= SIS_315H) {
    	SiS_SetRegANDOR(SiS_Part1Port,0x2F,0xFE,0x00);
  } else {
     	SiS_SetRegANDOR(SiS_Part1Port,0x24,0xFE,0x00);
  }
}

void
SiS_EnableCRT2()
{
  SiS_SetRegANDOR(SiS_P3c4,0x1E,0xFF,0x20);
}

void
SiS_DisableBridge(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT  BaseAddr)
{
  USHORT temp1,tempah,temp;

  SiS_SetRegANDOR(SiS_P3c4,0x11,0xF7,0x08);

  /*SetPanelDelay(1); */
  temp1=0x01;
  if( (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)))  {  /*301b*/
    	if((SiS_IsVAMode(BaseAddr)))
       		temp1=0x00;			/*no disable vb*/
  }

  if(SiS_IF_DEF_LVDS==0) {
   if(!(temp1)){              /*301b*/
    SiS_SetRegANDOR(SiS_Part2Port,0x00,0x0DF,0x00);   /* disable VB */
    SiS_DisplayOff();
    if(HwDeviceExtension->jChipType >= SIS_315H) { /* 310 series */
      SiS_SetRegOR(SiS_Part1Port,0x00,0x80);  /* FUNCTION CTRL | alan,BScreenOff */
    }
    SiS_SetRegANDOR(SiS_P3c4,0x32,0xDF,0x00);
    
    temp = SiS_GetReg1(SiS_Part1Port,0); 
    SiS_SetRegOR(SiS_Part1Port,0x00,0x10);  /* FUNCTION CTRL | alan,BScreenOff */
/*
     if(HwDeviceExtension->jChipType >= SIS_315H)
     {
      	SiS_SetRegAND(SiS_Part1Port,0x2E,0x7F);
     }
*/
     SiS_SetRegANDOR(SiS_P3c4,0x1E,0xDF,0x00);
     SiS_SetReg1(SiS_Part1Port,0,temp);
    }
    else {
    if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))  /*301b*/
      {
        if(!(SiS_Is301B(BaseAddr))) {
          SiS_SetRegAND(SiS_P3c4,0x32,0xDF);
          if((!(SiS_IsDualEdge(BaseAddr)))&&(!(SiS_IsVAMode(BaseAddr))))
            tempah=0x7F;
          else if((!(SiS_IsDualEdge(BaseAddr)))&&(SiS_IsVAMode(BaseAddr)))
            tempah=0xBF;
          else
            tempah=0x3F;
          SiS_SetRegAND(SiS_Part4Port,0x1F,tempah);
        }
      }
    }
  }
  else { /* LVDS */
    if(SiS_IF_DEF_CH7005==1) {
      SiS_Backup7005 = SiS_GetCH7005(0x0e);
      SiS_SetCH7005(0x090E);
    }
    SiS_DisplayOff();
    SiS_SetRegANDOR(SiS_P3c4,0x32,0xDF,0x00);
    SiS_SetRegANDOR(SiS_P3c4,0x1E,0xDF,0x00);
    SiS_UnLockCRT2(HwDeviceExtension,BaseAddr);
    SiS_SetRegANDOR(SiS_Part1Port,0x01,0xFF,0x80);
    SiS_SetRegANDOR(SiS_Part1Port,0x02,0xFF,0x40);
  }
  /*SetPanelDelay(0);  */
  SiS_SetRegANDOR(SiS_P3c4,0x11,0xFB,0x04);
}

void
SiS_EnableBridge(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT  BaseAddr)
{
  USHORT temp,tempah;

  SiS_SetRegANDOR(SiS_P3c4,0x11,0xFB,0x00);

  /*SetPanelDelay(0);        */
  if(SiS_IF_DEF_LVDS==0) {
    if( (!(SiS_IsVAMode(BaseAddr))) &&
        (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))) {
    		SiS_SetRegANDOR(SiS_Part2Port,0x00,0x1F,0x20);
    } else {
      temp=SiS_GetReg1(SiS_P3c4,0x32);
      temp=temp&0xDF;
      if(SiS_BridgeInSlave()) {
        tempah=SiS_GetReg1(SiS_P3d4,0x30);
        if(!(tempah&SetCRT2ToRAMDAC)) {
         temp=temp|0x20;
        }
      }
      SiS_SetReg1(SiS_P3c4,0x32,temp);
      SiS_SetRegANDOR(SiS_P3c4,0x1E,0xFF,0x20);

      if(HwDeviceExtension->jChipType >= SIS_315H) { /* 310 series */
        temp=SiS_GetReg1(SiS_Part1Port,0x2E);
        if (!(temp&0x80))
          SiS_SetRegOR(SiS_Part1Port,0x2E,0x80);          /* by alan,BVBDOENABLE=1 */
      }

      SiS_SetRegANDOR(SiS_Part2Port,0x00,0x1F,0x20);

      if(HwDeviceExtension->jChipType >= SIS_315H) { /* 310 series */
      	temp=SiS_GetReg1(SiS_Part1Port,0x2E);
     	if (!(temp&0x80))
            SiS_SetRegOR(SiS_Part1Port,0x2E,0x80);          /* by alan,BVBDOENABLE=1 */
      }
   
      SiS_SetRegANDOR(SiS_Part2Port,0x00,0x1F,0x20);
      SiS_VBLongWait();
      SiS_DisplayOn();
      SiS_VBLongWait();
    }  
  /*add301b*/
    if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
      if(!(SiS_Is301B(BaseAddr))){  
        temp=SiS_GetReg1(SiS_Part1Port,0x2E);
        if(!(temp&0x80))
           SiS_SetRegOR(SiS_Part1Port,0x2E,0x80);
        if((!(SiS_IsDualEdge(BaseAddr)))&&(!(SiS_IsVAMode(BaseAddr))))
           tempah=0x80;
        else if((!(SiS_IsDualEdge(BaseAddr)))&&(SiS_IsVAMode(BaseAddr)))
           tempah=0x40;
        else
           tempah=0xC0;
        SiS_SetRegOR(SiS_Part4Port,0x1F,tempah);
      }
     }  
  /*end 301b*/
  } else {                /*LVDS*/
    SiS_EnableCRT2();
    SiS_DisplayOn();
    SiS_UnLockCRT2(HwDeviceExtension, BaseAddr);
    SiS_SetRegANDOR(SiS_Part1Port,0x02,0xBF,0x00);
    if(SiS_BridgeInSlave()) {
      	SiS_SetRegANDOR(SiS_Part1Port,0x01,0x1F,0x00);
    } else {
      	SiS_SetRegANDOR(SiS_Part1Port,0x01,0x1F,0x40);
    }
    if(SiS_IF_DEF_CH7005) {
        if (SiS_Backup7005 != 0xff) {
		SiS_SetCH7005(((SiS_Backup7005<<8)|0x0E));
		SiS_Backup7005 = 0xff;
	} else SiS_SetCH7005(0x0B0E);
    }
  }
  /*SetPanelDelay(1);  */
  SiS_SetRegANDOR(SiS_P3c4,0x11,0xF7,0x00);
}

void
SiS_SetPanelDelay(USHORT DelayTime)
{
  USHORT PanelID;

  PanelID=SiS_GetReg1(SiS_P3d4,0x36);
  PanelID=PanelID>>4;

  if(DelayTime==0) SiS_LCD_Wait_Time(SiS_PanelDelayTbl[PanelID].timer[0]);
  else SiS_LCD_Wait_Time(SiS_PanelDelayTbl[PanelID].timer[1]);
}

void
SiS_LCD_Wait_Time(UCHAR DelayTime)
{
  USHORT i,j;
  ULONG  temp,flag;

  flag=0;
  for(i=0;i<DelayTime;i++) {
    for(j=0;j<66;j++) {
      temp=SiS_GetReg3(0x61);
      temp=temp&0x10;
      if(temp==flag) continue;
      flag=temp;
    }
  }
}

/*301b*/
BOOLEAN
SiS_Is301B(USHORT BaseAddr)
{
  USHORT flag;

    flag=SiS_GetReg1(SiS_Part4Port,0x01);
    if(flag>(0x0B0)) return(0); /*301b*/
    else return(1);
 }

BOOLEAN
SiS_IsDualEdge(USHORT BaseAddr)
{
#ifdef SIS315H
    USHORT flag;

     flag=SiS_GetReg1(SiS_P3d4,0x38);
      if(flag&EnableDualEdge)
        return (0);
      else
#endif
      	return(1);
}

BOOLEAN
SiS_IsVAMode(USHORT BaseAddr)
{
  USHORT flag;
         flag=SiS_GetReg1(SiS_P3d4,0x38);
#ifdef SIS315H
      if((flag&EnableDualEdge)&&(flag&SetToLCDA))
        return (0);
      else
#endif
      	return(1);
 }

BOOLEAN
SiS_IsDisableCRT2(USHORT BaseAddr)
{
  USHORT flag;

  flag=SiS_GetReg1(SiS_P3d4,0x30);
  if(flag&0x20) return(0); /*301b*/
  else return(1);
}
/*end 301b*/

BOOLEAN
SiS_BridgeIsOn(USHORT BaseAddr)
{
  USHORT flag;

  if(SiS_IF_DEF_LVDS==1){
    return(1);
  }
  else {
    flag=SiS_GetReg1(SiS_Part4Port,0x00);
      if((flag==1)||(flag==2)) return(1); /*301b*/
    else return(0);
  }
}


BOOLEAN
SiS_BridgeIsEnable(USHORT BaseAddr,PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT flag;

  if(SiS_BridgeIsOn(BaseAddr)==0) 
  {
    flag=SiS_GetReg1(SiS_Part1Port,0x0);
    if(HwDeviceExtension->jChipType < SIS_315H) {
      /* 300 series */
      if(flag&0x0a0) return 1;
      else	     return 0;
    } else {
      /* 310 series */
      if(flag&0x050) return 1;
      else           return 0;
    }
  }
  return 0;
}

BOOLEAN
SiS_BridgeInSlave()
{
  USHORT flag1;

  flag1=SiS_GetReg1(SiS_P3d4,0x31);
  if(flag1&(SetInSlaveMode>>8)) return 1;
  else  return 0;
}

BOOLEAN
SiS_GetLCDResInfo301(ULONG ROMAddr,USHORT SiS_P3d4,
                     USHORT ModeNo,USHORT ModeIdIndex)
{
  USHORT temp,modeflag,resinfo=0;

  SiS_LCDResInfo=0;
  SiS_LCDTypeInfo=0;
  SiS_LCDInfo=0;

  if (ModeNo<=0x13) {
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;  	/* si+St_ModeFlag  */
  } else {
    	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
    	resinfo = SiS_EModeIDTable[ModeIdIndex].Ext_RESINFO; 	/*si+Ext_ResInfo*/
  }

  if(!(SiS_VBInfo&(SetCRT2ToLCD|SetCRT2ToLCDA))){
    	return 0;
  }
  if(!(SiS_VBInfo&(SetSimuScanMode|SwitchToCRT2))) {
    	return 0;
  }
  temp=SiS_GetReg1(SiS_P3d4,0x36);
  /*fstn*/
  if(SiS_IF_DEF_FSTN){
   	temp=0x27;
   	SiS_SetReg1(SiS_P3d4,0x36,temp);
  } 
  SiS_LCDTypeInfo=temp>>4;
  SiS_LCDResInfo=temp&0x0F;
  if(SiS_IF_DEF_LVDS==0) {
    	if(SiS_LCDResInfo<Panel1024x768) SiS_LCDResInfo=Panel1024x768;
  } else {
    	if(SiS_LCDResInfo<Panel800x600) SiS_LCDResInfo=Panel800x600;
  }
#if 0
  /* TW: No 1600x1200 panels? Don't think so... */
  if(SiS_LCDResInfo>Panel640x480) SiS_LCDResInfo=Panel1024x768;
#endif
  if(SiS_LCDResInfo>Panel1600x1200) SiS_LCDResInfo=Panel1024x768;
  /*fstn*/
  if(SiS_IF_DEF_FSTN){
       	SiS_LCDResInfo=Panel320x480;
  }
  temp=SiS_GetReg1(SiS_P3d4,0x37);
  if(SiS_IF_DEF_FSTN){
      	temp=0x04;
      	SiS_SetReg1(SiS_P3d4,0x37,temp);
  }
  SiS_LCDInfo=temp;

#ifdef LINUX_KERNEL
  printk(KERN_INFO "sisfb: (LCDInfo = 0x%x LCDResInfo = 0x%x LCDTypeInfo = 0x%x)\n",
                   SiS_LCDInfo, SiS_LCDResInfo, SiS_LCDTypeInfo);
#endif
#ifdef LINUX_XF86
  xf86DrvMsg(0, X_INFO, "(init301.c: LCDInfo = 0x%x LCDResInfo = 0x%x LCDTypeInfo = 0x%x)\n",
			SiS_LCDInfo, SiS_LCDResInfo, SiS_LCDTypeInfo);
#endif

  if(SiS_IF_DEF_LVDS==1){
     if(modeflag&HalfDCLK){
        if(SiS_IF_DEF_TRUMPION==0){
            if(!(SiS_LCDInfo&LCDNonExpanding)){
               if(ModeNo>0x13) {
                  if(SiS_LCDResInfo==Panel1024x768){
                      if(resinfo==4){                                /* 512x384  */
                           SiS_SetFlag=SiS_SetFlag|EnableLVDSDDA;
                      }
                  } else {
                      if(SiS_LCDResInfo==Panel800x600){
                           if(resinfo==3){                             /*400x300  */
                                SiS_SetFlag=SiS_SetFlag|EnableLVDSDDA;
                           }
                      }
                  }
               }
           } else { /* NonExpanding */
              SiS_SetFlag=SiS_SetFlag|EnableLVDSDDA;
           }
        } else { /* TRUMPION */
          SiS_SetFlag=SiS_SetFlag|EnableLVDSDDA;
        }
     }
  }

  if(SiS_VBInfo&SetInSlaveMode){
    	if(SiS_VBInfo&SetNotSimuMode){
      		SiS_SetFlag=SiS_SetFlag|LCDVESATiming;
    	}
  } else {
    	SiS_SetFlag=SiS_SetFlag|LCDVESATiming;
  }
  return 1;
}

void
SiS_PresetScratchregister(USHORT SiS_P3d4,PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  /*SiS_SetReg1(SiS_P3d4,0x30,0x21);  */
  /*SiS_SetReg1(SiS_P3d4,0x31,0x41);  */
  /*SiS_SetReg1(SiS_P3d4,0x32,0x28);  */
  /*SiS_SetReg1(SiS_P3d4,0x33,0x22);  */
  /*SiS_SetReg1(SiS_P3d4,0x35,0x43);  */
  /*SiS_SetReg1(SiS_P3d4,0x36,0x01);  */
  /*SiS_SetReg1(SiS_P3d4,0x37,0x00);  */
}

void
SiS_LongWait()
{
  USHORT i;

  i=SiS_GetReg1(SiS_P3c4,0x1F);
  if(!(i&0xC0)) {

    for(i=0; i<0xFFFF; i++) {
       if(!(SiS_GetReg2(SiS_P3da) & 0x08))
         break;
    }
    for(i=0; i<0xFFFF; i++) {
       if((SiS_GetReg2(SiS_P3da) & 0x08))
         break;
    }
  }
}

void
SiS_VBLongWait()
{
  USHORT tempal,temp,i,j;

  if(!(SiS_VBInfo&SetCRT2ToTV)) {
    temp=0;
    for(i=0;i<3;i++) {
      for(j=0;j<100;j++) {
        tempal=SiS_GetReg2(SiS_P3da);
        if(temp&0x01) {        /* VBWaitMode2  */
          if((tempal&0x08)) {
            continue;
          }
          if(!(tempal&0x08)) {
            break;
          }
        } else {                 /* VBWaitMode1  */
          if(!(tempal&0x08)) {
            continue;
          }
          if((tempal&0x08)) {
            break;
          }
        }
      }
      temp=temp^0x01;
    }
  } else {
    SiS_LongWait();
  }
  return;
}

BOOLEAN
SiS_WaitVBRetrace(USHORT  BaseAddr)
{
  USHORT temp;

return 0;

  temp=SiS_GetReg1(SiS_Part1Port,0x00); /* FUNCTION CTRL */
  if(!(temp&0x80)){
    return 0;
  }

  for(temp=0;temp==0;){
     temp=SiS_GetReg1(SiS_Part1Port,0x25);
     temp=temp&0x01;
  }
  for(;temp>0;){
     temp=SiS_GetReg1(SiS_Part1Port,0x25);
     temp=temp&0x01;
  }
  return 1;
}

void
SiS_SetRegANDOR(USHORT Port,USHORT Index,USHORT DataAND,USHORT DataOR)
{
  USHORT temp;

  temp=SiS_GetReg1(Port,Index);     /* SiS_Part1Port index 02 */
  temp=(temp&(DataAND))|DataOR;
  SiS_SetReg1(Port,Index,temp);
}

void
SiS_SetRegAND(USHORT Port,USHORT Index,USHORT DataAND)
{
  USHORT temp;

  temp=SiS_GetReg1(Port,Index);     /* SiS_Part1Port index 02 */
  temp=temp&DataAND;
  SiS_SetReg1(Port,Index,temp);
}

void SiS_SetRegOR(USHORT Port,USHORT Index,USHORT DataOR)
{
  USHORT temp;

  temp=SiS_GetReg1(Port,Index);     /* SiS_Part1Port index 02 */
  temp=temp|DataOR;
  SiS_SetReg1(Port,Index,temp);
}

void
SiS_SetGroup2(USHORT BaseAddr,ULONG ROMAddr, USHORT ModeNo,
                   USHORT ModeIdIndex,USHORT RefreshRateTableIndex,
		   PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT i,j,tempax,tempbx,tempcx,temp,temp3;
  USHORT push1,push2,temp1;
  UCHAR *PhasePoint;
  UCHAR *TimingPoint;
  USHORT  modeflag,resinfo,crt2crtc,resindex,xres;
  ULONG longtemp,tempeax,tempebx,temp2,tempecx;
  USHORT  SiS_RY1COE=0,SiS_RY2COE=0,SiS_RY3COE=0,SiS_RY4COE=0;
  USHORT  SiS_RY5COE=0,SiS_RY6COE=0,SiS_RY7COE=0;

  if(ModeNo<=0x13) {
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;      /* si+St_ResInfo */
    	resinfo = SiS_SModeIDTable[ModeIdIndex].St_ResInfo;
    	crt2crtc = SiS_SModeIDTable[ModeIdIndex].St_CRT2CRTC;
  } else {
    	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;     /* si+Ext_ResInfo */
    	resinfo = SiS_EModeIDTable[ModeIdIndex].Ext_RESINFO;
    	crt2crtc = SiS_RefIndex[RefreshRateTableIndex].Ext_CRT2CRTC;
  }

  tempcx=SiS_VBInfo;
  tempax=(tempcx&0x00FF)<<8;
  tempbx=(tempcx&0x00FF)|((tempcx&0x00FF)<<8);
  tempbx=tempbx&0x0410;
  temp=(tempax&0x0800)>>8;
  temp=temp>>1;
  temp=temp|(((tempbx&0xFF00)>>8)<<1);
  temp=temp|((tempbx&0x00FF)>>3);
  temp=temp^0x0C;

  PhasePoint = SiS_PALPhase;
  if(SiS_VBInfo&SetCRT2ToHiVisionTV) {        /* PALPhase */
    temp=temp^0x01;
    if(SiS_VBInfo&SetInSlaveMode) {
      TimingPoint = SiS_HiTVSt2Timing;
      if(SiS_SetFlag&TVSimuMode) {
        if(modeflag&Charx8Dot) TimingPoint = SiS_HiTVSt1Timing;
        else TimingPoint = SiS_HiTVTextTiming;
      }
    } else TimingPoint = SiS_HiTVExtTiming;
  } else {
    if(SiS_VBInfo&SetPALTV){
      if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
         PhasePoint = SiS_PALPhase2;              /* PALPhase */
      else 
         PhasePoint = SiS_PALPhase;  
     
         TimingPoint = SiS_PALTiming;
    } else {
        temp=temp|0x10;
        if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
        	PhasePoint = SiS_NTSCPhase2;             /* PALPhase */
        else
        	PhasePoint = SiS_NTSCPhase;
      
        TimingPoint = SiS_NTSCTiming;
    }
  }
  SiS_SetReg1(SiS_Part2Port,0x00,temp);

#ifdef SIS300
  /*add PALMN*/
  if((HwDeviceExtension->jChipType==SIS_630)||
     (HwDeviceExtension->jChipType==SIS_730)) {
     	if(SiS_VBInfo&SetCRT2ToTV) {
             temp=SiS_GetReg1(SiS_P3d4,0x31);   
             temp=temp&0x01; 
             if(temp) {
                  temp1=SiS_GetReg1(SiS_P3d4,0x35);        
                  temp1=temp1&0x40; 
                  if(temp1)
                     	PhasePoint = SiS_PALMPhase;
                  temp1=SiS_GetReg1(SiS_P3d4,0x35);        
                  temp1=temp1&0x80;
                  if(temp1)
                     	PhasePoint = SiS_PALNPhase;
             }
        }
  }
  /*end add*/
#endif

#ifdef SIS315H
  /*add PALMN*/
  if ( (HwDeviceExtension->jChipType==SIS_315H)||
       (HwDeviceExtension->jChipType==SIS_315PRO)||
       (HwDeviceExtension->jChipType==SIS_550) ||
       (HwDeviceExtension->jChipType==SIS_640) ||
       (HwDeviceExtension->jChipType==SIS_740) ||
       (HwDeviceExtension->jChipType==SIS_650)) {
           if(SiS_VBInfo&SetCRT2ToTV) {
              temp=SiS_GetReg1(SiS_P3d4,0x31);
              temp=temp&0x01;
              if(temp) {
                  temp1=SiS_GetReg1(SiS_P3d4,0x38);
                  temp1=temp1&0x40;
                  if(temp1)
                     PhasePoint = SiS_PALMPhase;
                  temp1=SiS_GetReg1(SiS_P3d4,0x38);
                  temp1=temp1&0x80;
                  if(temp1)
                     PhasePoint = SiS_PALNPhase;
              }
           }
  }
  /*end add*/
#endif

  for(i=0x31,j=0;i<=0x34;i++,j++){
     SiS_SetReg1(SiS_Part2Port,i,PhasePoint[j]);
  }
  for(i=0x01,j=0;i<=0x2D;i++,j++){
     SiS_SetReg1(SiS_Part2Port,i,TimingPoint[j]);
  }
  for(i=0x39;i<=0x45;i++,j++){
     SiS_SetReg1(SiS_Part2Port,i,TimingPoint[j]);      /* di->temp2[j] */
  }
  if(SiS_VBInfo&SetCRT2ToTV) {
    SiS_SetRegANDOR(SiS_Part2Port,0x3A,0x1F,0x00);
  }
  temp=SiS_NewFlickerMode;
  SiS_SetRegANDOR(SiS_Part2Port,0x0A,0xFF,temp);

  SiS_SetReg1(SiS_Part2Port,0x35,0x00); /*301b*/
  SiS_SetReg1(SiS_Part2Port,0x36,0x00);
  SiS_SetReg1(SiS_Part2Port,0x37,0x00);
  SiS_SetReg1(SiS_Part2Port,0x38,SiS_RY1COE);
  SiS_SetReg1(SiS_Part2Port,0x48,SiS_RY2COE);
  SiS_SetReg1(SiS_Part2Port,0x49,SiS_RY3COE);
  SiS_SetReg1(SiS_Part2Port,0x4a,SiS_RY4COE);

  /*add to change 630+301b filter*/
  resindex=SiS_GetResInfo(ROMAddr,ModeNo,ModeIdIndex);
  if(ModeNo<=0x13) {
      xres=SiS_StResInfo[resindex].HTotal;
  } else {
      xres=SiS_ModeResInfo[resindex].HTotal;                         /* xres->ax */
  }
  if(xres==640){SiS_RY1COE=0xFF;SiS_RY2COE=0x03;SiS_RY3COE=0x02;SiS_RY4COE=0xF6;
                         SiS_RY5COE=0xFC;SiS_RY6COE=0x27;SiS_RY7COE=0x46;}
  if(xres==800){SiS_RY1COE=0x01;SiS_RY2COE=0x01;SiS_RY3COE=0xFC;SiS_RY4COE=0xF8;
                         SiS_RY5COE=0x08;SiS_RY6COE=0x26;SiS_RY7COE=0x38;}                        
  if(xres==1024){SiS_RY1COE=0xFF;SiS_RY2COE=0xFF;SiS_RY3COE=0xFC;SiS_RY4COE=0x00;
                         SiS_RY5COE=0x0F;SiS_RY6COE=0x22;SiS_RY7COE=0x28;}
  if(xres==720){SiS_RY1COE=0x01;SiS_RY2COE=0x02;SiS_RY3COE=0xFE;SiS_RY4COE=0xF7;
                         SiS_RY5COE=0x03;SiS_RY6COE=0x27;SiS_RY7COE=0x3c;}
  SiS_SetReg1(SiS_Part2Port,0x35,SiS_RY1COE); /*301b*/
  SiS_SetReg1(SiS_Part2Port,0x36,SiS_RY2COE);
  SiS_SetReg1(SiS_Part2Port,0x37,SiS_RY3COE);
  SiS_SetReg1(SiS_Part2Port,0x38,SiS_RY4COE);
  SiS_SetReg1(SiS_Part2Port,0x48,SiS_RY5COE);
  SiS_SetReg1(SiS_Part2Port,0x49,SiS_RY6COE);
  SiS_SetReg1(SiS_Part2Port,0x4a,SiS_RY7COE);
  /*end add*/

  if(SiS_VBInfo&SetCRT2ToHiVisionTV) tempax=950;
  else {
    if(SiS_VBInfo&SetPALTV) tempax=520;
    else tempax=440;
  }
  if(SiS_VDE<=tempax) {
    tempax=tempax-SiS_VDE;
    tempax=tempax>>2;
    tempax=(tempax&0x00FF)|((tempax&0x00FF)<<8);
    push1=tempax;
    temp=(tempax&0xFF00)>>8;
    temp=temp+(USHORT)TimingPoint[0];
    SiS_SetReg1(SiS_Part2Port,0x01,temp);
    tempax=push1;
    temp=(tempax&0xFF00)>>8;
    temp=temp+TimingPoint[1];
    SiS_SetReg1(SiS_Part2Port,0x02,temp);
  }
  /*301b*/
  if( (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
        && (SiS_VBInfo&SetCRT2ToTV) && (SiS_VGAHDE==1024) ) {
      if(SiS_VBInfo&SetPALTV) {
       SiS_SetReg1(SiS_Part2Port,0x01,0x19);
       SiS_SetReg1(SiS_Part2Port,0x02,0x52);
      } else {
       SiS_SetReg1(SiS_Part2Port,0x01,0x0B);
       SiS_SetReg1(SiS_Part2Port,0x02,0x11);
      } 
    }
    
  tempcx=SiS_HT-1;
  /*301b*/
  if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
     tempcx=tempcx-1;   
  }
  temp=tempcx&0x00FF;
  SiS_SetReg1(SiS_Part2Port,0x1B,temp);
  temp=(tempcx&0xFF00)>>8;
  SiS_SetRegANDOR(SiS_Part2Port,0x1D,~0x0F,temp);

  tempcx=SiS_HT>>1;
  push1=tempcx;                           /* push cx */
  tempcx=tempcx+7;
  if(SiS_VBInfo&SetCRT2ToHiVisionTV) {
    tempcx=tempcx-4;
  }
  temp=(tempcx&0x00FF);
  temp=temp<<4;
  SiS_SetRegANDOR(SiS_Part2Port,0x22,0x0F,temp);

  tempbx=TimingPoint[j]|((TimingPoint[j+1])<<8);
  tempbx=tempbx+tempcx;
  push2=tempbx;
  temp=tempbx&0x00FF;
  SiS_SetReg1(SiS_Part2Port,0x24,temp);
  temp=(tempbx&0xFF00)>>8;
  temp=temp<<4;
  SiS_SetRegANDOR(SiS_Part2Port,0x25,0x0F,temp);

  tempbx=push2;
  tempbx=tempbx+8;
  if(SiS_VBInfo&SetCRT2ToHiVisionTV) {
    tempbx=tempbx-4;
    tempcx=tempbx;
  }
  temp=(tempbx&0x00FF)<<4;
  SiS_SetRegANDOR(SiS_Part2Port,0x29,0x0F,temp);

  j=j+2;
  tempcx=tempcx+(TimingPoint[j]|((TimingPoint[j+1])<<8));
  temp=tempcx&0x00FF;
  SiS_SetReg1(SiS_Part2Port,0x27,temp);
  temp=((tempcx&0xFF00)>>8)<<4;
  SiS_SetRegANDOR(SiS_Part2Port,0x28,0x0F,temp);

  tempcx=tempcx+8;
  if(SiS_VBInfo&SetCRT2ToHiVisionTV) {
    tempcx=tempcx-4;
  }
  temp=tempcx&0xFF;
  temp=temp<<4;
  SiS_SetRegANDOR(SiS_Part2Port,0x2A,0x0F,temp);

  tempcx=push1;                                       /* pop cx */
  j=j+2;
  temp=TimingPoint[j]|((TimingPoint[j+1])<<8);
  tempcx=tempcx-temp;
  temp=tempcx&0x00FF;
  temp=temp<<4;
  SiS_SetRegANDOR(SiS_Part2Port,0x2D,0x0F,temp);

  tempcx=tempcx-11;
  if(!(SiS_VBInfo&SetCRT2ToTV)){
    tempax=SiS_GetVGAHT2();
    tempcx=tempax-1;
  }
  temp=tempcx&0x00FF;
  SiS_SetReg1(SiS_Part2Port,0x2E,temp);	/* TW: BIOS 0x48, here 0x3f @@@ */

  tempbx=SiS_VDE;
  if(SiS_VGAVDE==360) tempbx=746;
  if(SiS_VGAVDE==375) tempbx=746;
  if(SiS_VGAVDE==405) tempbx=853;
  if(SiS_VBInfo&SetCRT2ToTV) {
    tempbx=tempbx>>1;
  }
  tempbx=tempbx-2;
  temp=tempbx&0x00FF;
  if(SiS_VBInfo&SetCRT2ToHiVisionTV) {
    if(SiS_VBInfo&SetInSlaveMode) {
      if(ModeNo==0x2f) temp=temp+1;
    }
  }
  SiS_SetReg1(SiS_Part2Port,0x2F,temp);

  temp=(tempcx&0xFF00)>>8;
  temp=temp|(((tempbx&0xFF00)>>8)<<6);
  if(!(SiS_VBInfo&SetCRT2ToHiVisionTV)) {
    temp=temp|0x10;
    if(!(SiS_VBInfo&SetCRT2ToSVIDEO)){
      temp=temp|0x20;
    }
  }
  SiS_SetReg1(SiS_Part2Port,0x30,temp);  /* TW: BIOS sets 0xb8, here (or somewhere else) 0xb5 */

  /*301b*/
  if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {      /*tv gatingno*/
    tempbx=SiS_VDE;
    if(SiS_VBInfo&SetCRT2ToTV) {
       tempbx=tempbx>>1;
    }
    temp=(((tempbx-3)&0x0300)>>8)<<5;
    SiS_SetReg1(SiS_Part2Port,0x46,temp);
    temp=(tempbx-3)&0x00FF;
    SiS_SetReg1(SiS_Part2Port,0x47,temp);
  }
  /*end 301b*/

  tempbx=tempbx&0x00FF;
  if(!(modeflag&HalfDCLK)){
    tempcx=SiS_VGAHDE;
    if(tempcx>=SiS_HDE){
      tempbx=tempbx|0x2000;
      tempax=tempax&0x00FF;
    }
  }
  tempcx=0x0101;

  if(SiS_VBInfo&(SetCRT2ToHiVisionTV|SetCRT2ToTV)) { /*301b*/
    if(SiS_VGAHDE>=1024) {
      tempcx=0x1920;
      if(SiS_VGAHDE>=1280) {
        tempcx=0x1420;
        tempbx=tempbx&0xDFFF;
      }
    }
  }
  if(!(tempbx&0x2000)){
    if(modeflag&HalfDCLK){
      tempcx=(tempcx&0xFF00)|((tempcx&0x00FF)<<1);
    }
    push1=tempbx;
    tempeax=SiS_VGAHDE;
    tempebx=(tempcx&0xFF00)>>8;
    longtemp=tempeax*tempebx;
    tempecx=tempcx&0x00FF;
    longtemp=longtemp/tempecx;
    /*301b*/
    tempecx=8*1024;
    if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
     	tempecx=tempecx*8;
    }
    longtemp=longtemp*tempecx;
    tempecx=SiS_HDE;
    temp2=longtemp%tempecx;
    tempeax=longtemp/tempecx;
    if(temp2!=0){
      tempeax=tempeax+1;
    }
    tempax=(USHORT)tempeax;
    /*301b*/
    if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
     	tempcx=((tempax&0xFF00)>>5)>>8;
    }
    /*end 301b*/
    tempbx=push1;
    tempbx=(USHORT) ( ((tempeax&0x0000FF00)&0x1F00)|(tempbx&0x00FF) );
    tempax=(USHORT) ( ((tempeax&0x000000FF)<<8)|(tempax&0x00FF) );
    temp=(tempax&0xFF00)>>8;
  } else {
    temp=(tempax&0x00FF)>>8;
  }
  SiS_SetReg1(SiS_Part2Port,0x44,temp);
  temp=(tempbx&0xFF00)>>8;
  SiS_SetRegANDOR(SiS_Part2Port,0x45,~0x03F,temp);
  /*301b*/
  if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
       if((tempcx&0x00FF)==0x01)
    		tempcx=0x00;
       SiS_SetRegANDOR(SiS_Part2Port,0x46,~0x007,tempcx);
       SiS_SetRegOR(SiS_Part2Port,0x46,0x18);
               if(SiS_VBInfo&SetPALTV) {
                   tempbx=0x0364;
                   tempcx=0x009c;
               } else {
                   tempbx=0x0346;
                   tempcx=0x0078;
               }
       temp=(tempbx&0x00FF) ;
       SiS_SetReg1(SiS_Part2Port,0x4B,temp);
       temp=(tempcx&0x00FF) ;
       SiS_SetReg1(SiS_Part2Port,0x4C,temp);
       tempbx=(tempbx&0x0300);
       temp=(tempcx&0xFF00)>>8 ;
       temp=(temp&0x0003)<<2;
       temp=temp|(tempbx>>8);
       SiS_SetReg1(SiS_Part2Port,0x4D,temp);

       temp=SiS_GetReg1(SiS_Part2Port,0x43); /*301b change */
       SiS_SetReg1(SiS_Part2Port,0x43,(USHORT)(temp-3));
  }
  /*end 301b*/

#ifdef SIS300
  /*add PALMN*/
  if((HwDeviceExtension->jChipType==SIS_630)||
     (HwDeviceExtension->jChipType==SIS_730)) {
          if(SiS_VBInfo&SetCRT2ToTV) {
             temp=SiS_GetReg1(SiS_P3d4,0x31);
             temp=temp&0x01;
             if(temp) {
                  temp1=SiS_GetReg1(SiS_P3d4,0x35);
                  temp1=temp1&0x40;
                  if(temp1){
                           SiS_SetRegANDOR(SiS_Part2Port,0x00,0xEF,0x00);
                           temp3=SiS_GetReg1(SiS_Part2Port,0x01);
                           temp3=temp3-1;
                           SiS_SetReg1(SiS_Part2Port,0x01,temp3);
                  }
             }
          }
  }
  /*end add*/
#endif

#ifdef SIS315H
  /*add PALMN*/
  if ( (HwDeviceExtension->jChipType==SIS_315H)||
       (HwDeviceExtension->jChipType==SIS_315PRO)||
       (HwDeviceExtension->jChipType==SIS_550) ||
       (HwDeviceExtension->jChipType==SIS_640) ||
       (HwDeviceExtension->jChipType==SIS_740) ||
       (HwDeviceExtension->jChipType==SIS_650)) {
        	if(SiS_VBInfo&SetCRT2ToTV) {
             		temp=SiS_GetReg1(SiS_P3d4,0x31);
            		temp=temp&0x01;
              		if(temp) {
                  		temp1=SiS_GetReg1(SiS_P3d4,0x38);
                 		temp1=temp1&0x40;
                   		if(temp1){
                           		SiS_SetRegANDOR(SiS_Part2Port,0x00,0xEF,0x00);
                           		temp3=SiS_GetReg1(SiS_Part2Port,0x01);
                           		temp3=temp3-1;
                           		SiS_SetReg1(SiS_Part2Port,0x01,temp3);
                		}
             		}
          	}
  }
  /*end add*/
#endif

  if(SiS_VBInfo&SetCRT2ToHiVisionTV) {
    if(!(SiS_VBInfo&SetInSlaveMode)) {
      SiS_SetReg1(SiS_Part2Port,0x0B,0x00);
    }
  }
  if(SiS_VBInfo&SetCRT2ToTV){
    return;
  }

  /* TW: From here: LCD Part2 group */

  tempbx=SiS_HDE-1;         /* RHACTE=HDE-1 */
  temp=tempbx&0x00FF;
  SiS_SetReg1(SiS_Part2Port,0x2C,temp);
  temp=(tempbx&0xFF00)>>8;
  temp=temp<<4;
  SiS_SetRegANDOR(SiS_Part2Port,0x2B,0x0F,temp);
  temp=0x01;
  if(SiS_LCDResInfo==Panel1280x1024) {
    if(SiS_ModeType==ModeEGA) {
      if(SiS_VGAHDE>=1024) {
        temp=0x02;
        if (SiS_SetFlag&LCDVESATiming)
          temp=0x01;
      }
    }
  }
  SiS_SetReg1(SiS_Part2Port,0x0B,temp);

  tempbx=SiS_VDE;         /* RTVACTEO=(VDE-1)&0xFF */
  push1=tempbx;
  tempbx--;
  temp=tempbx&0x00FF;
  SiS_SetReg1(SiS_Part2Port,0x03,temp);
  temp=((tempbx&0xFF00)>>8)&0x07;
  SiS_SetRegANDOR(SiS_Part2Port,0x0C,~0x07,temp);

  tempcx=SiS_VT-1;
  push2=tempcx+1;
  temp=tempcx&0x00FF;   /* RVTVT=VT-1 */
  SiS_SetReg1(SiS_Part2Port,0x19,temp);
  temp=(tempcx&0xFF00)>>8;
  temp=temp<<5;
  if(SiS_LCDInfo&LCDRGB18Bit){
    temp=temp|0x10;
  }
  if(SiS_VBInfo&SetCRT2ToLCD) {
    tempbx=(tempbx&0xFF00)|(SiS_LCDInfo&0x0FF);
    if(tempbx&LCDSync) {
      tempbx=tempbx&LCDSyncBit;
      tempbx=(tempbx&0xFF00)|((tempbx&0x00FF)>>LCDSyncShift);
      temp=temp|(tempbx&0x00FF);
    }
  }
  SiS_SetReg1(SiS_Part2Port,0x1A,temp);

  tempcx++;
  tempbx=768;
  if(SiS_LCDResInfo!=Panel1024x768) {
    tempbx=1024;
    if(SiS_LCDResInfo!=Panel1280x1024) {
         tempbx=1200;     /*301b */
         if(SiS_LCDResInfo!=Panel1600x1200) {
            if(tempbx!=SiS_VDE) {
               tempbx=960;
            }
         }
    }
  }
  if(SiS_LCDInfo&LCDNonExpanding) {
    tempbx=SiS_VDE;
    tempbx--;
    tempcx--;
  }
  tempax=1;
  if(!(SiS_LCDInfo&LCDNonExpanding)) {
    if(tempbx!=SiS_VDE){
      tempax=tempbx;
      if(tempax<SiS_VDE) {
        tempax=0;
        tempcx=0;
      } else {
        tempax=tempax-SiS_VDE;
      }
      tempax=tempax>>1;
    }
    tempcx=tempcx-tempax; /* lcdvdes */
    tempbx=tempbx-tempax; /* lcdvdee */
  } else {
    tempax=tempax>>1;
    tempcx=tempcx-tempax; /* lcdvdes */
    tempbx=tempbx-tempax; /* lcdvdee */
  }

  temp=tempcx&0x00FF;   /* RVEQ1EQ=lcdvdes */
  SiS_SetReg1(SiS_Part2Port,0x05,temp);
  temp=tempbx&0x00FF;   /* RVEQ2EQ=lcdvdee */
  SiS_SetReg1(SiS_Part2Port,0x06,temp);

  /* TW new @@@ */
  if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
        /* TW: BIOS sets Part2 0x02 to 0x18. The old caluculation (in else statement)
	 *     resulted in 0x13. Do it this way for 301B?
	 */
        temp=(((tempcx&0xFF00)>>8)<<3);   /* 0x325 & ff00 = 300 >> 8 = 3 << 3 = 0x18 */
	SiS_SetReg1(SiS_Part2Port,0x02,temp);
  } else {
  /* TW end */
  	temp=(tempbx&0xFF00)>>8;
  	temp=temp<<3;
  	temp=temp|((tempcx&0xFF00)>>8);
  	SiS_SetReg1(SiS_Part2Port,0x02,temp);
  }

  tempbx=push2;  	/* SiS_VT = 0x326  - - - (TW DEBUG REMARKS)*/
  tempax=push1;  	/* SiS_VDE = 0x300 */
  tempcx=tempbx;
  tempcx=tempcx-tempax; /* -> 0x26 */
  tempcx=tempcx>>4;     /* -> 0x02 */
  tempbx=tempbx+tempax; /* -> 0x626 */
  tempbx=tempbx>>1;     /* -> 0x313 */
  if(SiS_LCDInfo&LCDNonExpanding) {
     tempbx=tempbx-10;
  }
  /* TW new @@@ */
  if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
  	/* TW: BIOS sets Part2 0x04 to 0x02. The old caluculation (in else statement)
	 *     resulted in 0x13. Do it this way for 301B?
	 */
        temp=tempcx&0x00FF;
	SiS_SetReg1(SiS_Part2Port,0x04,temp);
  } else {
  /* TW end */
    	temp=tempbx&0x00FF;   		/* RTVACTEE=lcdvrs */
  	SiS_SetReg1(SiS_Part2Port,0x04,temp);   /* TW: BIOS: 0x02, here 0x13 */
  }

 temp=(tempbx&0xFF00)>>8;
 temp=temp<<4;
 tempbx=tempbx+tempcx+1;
 temp=temp|(tempbx&0x000F);
 SiS_SetReg1(SiS_Part2Port,0x01,temp);

 if(SiS_LCDResInfo==Panel1024x768) {
   if(!(SiS_SetFlag&LCDVESATiming)) {
     if(!(SiS_LCDInfo&LCDNonExpanding)) {
       if(ModeNo==0x13) {
         SiS_SetReg1(SiS_Part2Port,0x04,0xB9);
         SiS_SetReg1(SiS_Part2Port,0x05,0xCC);
         SiS_SetReg1(SiS_Part2Port,0x06,0xA6);
       } else {
         temp=crt2crtc&0x3F;
         if(temp==4) {
           SiS_SetReg1(SiS_Part2Port,0x01,0x2B);
           SiS_SetReg1(SiS_Part2Port,0x02,0x13);
           SiS_SetReg1(SiS_Part2Port,0x04,0xE5);
           SiS_SetReg1(SiS_Part2Port,0x05,0x08);
           SiS_SetReg1(SiS_Part2Port,0x06,0xE2);
         }
       }
     }
   }
 }

 SiS_SetRegANDOR(SiS_Part2Port,0x09,0xF0,0x00);
 SiS_SetRegANDOR(SiS_Part2Port,0x0A,0xF0,0x00);

 tempcx=(SiS_HT-SiS_HDE)>>2;    		/* (HT-HDE)>>2     */
 tempbx=(SiS_HDE+7);            		/* lcdhdee         */
 /*301b*/
 if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
    tempbx=tempbx+2;
 }
 push1=tempbx;
 temp=tempbx&0x00FF;    			/* RHEQPLE=lcdhdee */
 SiS_SetReg1(SiS_Part2Port,0x23,temp);
 temp=(tempbx&0xFF00)>>8;
 SiS_SetRegANDOR(SiS_Part2Port,0x25,~0x0F,temp);
 /*301b*/
 temp=7;
 if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
    temp=temp+2;
 }
 SiS_SetReg1(SiS_Part2Port,0x1F,temp);  	/* RHBLKE=lcdhdes */
 SiS_SetRegANDOR(SiS_Part2Port,0x20,0x0F,0x00);

 tempbx=tempbx+tempcx;
 push2=tempbx;
 temp=tempbx&0xFF;            			/* RHBURSTS=lcdhrs */
 if(SiS_LCDResInfo==Panel1280x1024) {
   if(!(SiS_LCDInfo&LCDNonExpanding)) {
     if(SiS_HDE==1280) {
       temp=0x47;
     }
   }
 }
 SiS_SetReg1(SiS_Part2Port,0x1C,temp);
 temp=(tempbx&0xFF00)>>8;
 temp=temp<<4;
 SiS_SetRegANDOR(SiS_Part2Port,0x1D,~0x0F0,temp);

 tempbx=push2;
 tempcx=tempcx<<1;
 tempbx=tempbx+tempcx;
 temp=tempbx&0x00FF;            		/* RHSYEXP2S=lcdhre */
 SiS_SetReg1(SiS_Part2Port,0x21,temp);

 SiS_SetRegANDOR(SiS_Part2Port,0x17,0xFB,0x00);
 SiS_SetRegANDOR(SiS_Part2Port,0x18,0xDF,0x00);

 if(!(SiS_SetFlag&LCDVESATiming)) {
   if(SiS_VGAVDE==525) {
     if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))   /*301b*/
    	temp=0xC6;
     else
       	temp=0xC4;
     SiS_SetReg1(SiS_Part2Port,0x2f,temp);
     SiS_SetReg1(SiS_Part2Port,0x30,0xB3);
   }
   if(SiS_VGAVDE==420) {
     if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
    	temp=0x4F;
     else
       	temp=0x4E;
     SiS_SetReg1(SiS_Part2Port,0x2f,temp);
   }
 }
}

USHORT
SiS_GetVGAHT2()
{
  ULONG tempax,tempbx;

  tempbx=((SiS_VGAVT-SiS_VGAVDE)*SiS_RVBHCMAX)&0xFFFF;
  tempax=(SiS_VT-SiS_VDE)*SiS_RVBHCFACT;
  tempax=(tempax*SiS_HT)/tempbx;
  return((USHORT) tempax);
}

void
SiS_SetGroup3(USHORT  BaseAddr,ULONG ROMAddr,USHORT ModeNo,
              USHORT ModeIdIndex,PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT i;
  UCHAR *tempdi;
  USHORT  modeflag,temp,temp1;

  if(ModeNo<=0x13) {
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;      /* si+St_ResInfo */
  } else {
    	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;     /* si+Ext_ResInfo */
  }

  SiS_SetReg1(SiS_Part3Port,0x00,0x00);
  if(SiS_VBInfo&SetPALTV) {
    SiS_SetReg1(SiS_Part3Port,0x13,0xFA);
    SiS_SetReg1(SiS_Part3Port,0x14,0xC8);
  } else {
    SiS_SetReg1(SiS_Part3Port,0x13,0xF6);
    SiS_SetReg1(SiS_Part3Port,0x14,0xBF);
  }
#ifdef SIS300
  /*add PALMN*/
  if((HwDeviceExtension->jChipType==SIS_630)||
     (HwDeviceExtension->jChipType==SIS_730)) {
           if(SiS_VBInfo&SetCRT2ToTV) {
              temp=SiS_GetReg1(SiS_P3d4,0x31);
              temp=temp&0x01;
              if(temp) {
                  temp1=SiS_GetReg1(SiS_P3d4,0x35);
                  temp1=temp1&0x40;
                   if(temp1){
                          SiS_SetReg1(SiS_Part3Port,0x13,0xFA);
                          SiS_SetReg1(SiS_Part3Port,0x14,0xC8);
                          SiS_SetReg1(SiS_Part3Port,0x3D,0xA8);
                }
             }
          } 
      }	
    /*end add*/
#endif
#ifdef SIS315H
/*add PALMN*/
  if ( (HwDeviceExtension->jChipType==SIS_315H)||
       (HwDeviceExtension->jChipType==SIS_315PRO)||
       (HwDeviceExtension->jChipType==SIS_550) ||
       (HwDeviceExtension->jChipType==SIS_640) ||
       (HwDeviceExtension->jChipType==SIS_740) ||
       (HwDeviceExtension->jChipType==SIS_650)) {
         if(SiS_VBInfo&SetCRT2ToTV) {
             temp=SiS_GetReg1(SiS_P3d4,0x31);   
             temp=temp&0x01; 
              if(temp) {
                  temp1=SiS_GetReg1(SiS_P3d4,0x38);        
                  temp1=temp1&0x40; 
                   if(temp1){
                          SiS_SetReg1(SiS_Part3Port,0x13,0xFA); 
                          SiS_SetReg1(SiS_Part3Port,0x14,0xC8); 
                          SiS_SetReg1(SiS_Part3Port,0x3D,0xA8);
                   }
              }
         }
  }
 /*end add*/
#endif
  if(SiS_VBInfo&SetCRT2ToHiVisionTV) {
    tempdi=SiS_HiTVGroup3Data;
    if(SiS_SetFlag&TVSimuMode) {
      tempdi=SiS_HiTVGroup3Simu;
      if(!(modeflag&Charx8Dot)) {
        tempdi=SiS_HiTVGroup3Text;
      }
    }
    for(i=0;i<=0x3E;i++){
       SiS_SetReg1(SiS_Part3Port,i,tempdi[i]);
    }
  }
  return;
}

void
SiS_SetGroup4(USHORT  BaseAddr,ULONG ROMAddr,USHORT ModeNo,
              USHORT ModeIdIndex,USHORT RefreshRateTableIndex,
	      PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT tempax,tempcx,tempbx,modeflag,temp,temp2,push1;
  ULONG tempebx,tempeax,templong;

  if(ModeNo<=0x13) {
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;      /* si+St_ResInfo */
  } else {
    	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;     /* si+Ext_ResInfo */
  }
  temp=SiS_RVBHCFACT;
  SiS_SetReg1(SiS_Part4Port,0x13,temp);

  tempbx=SiS_RVBHCMAX;
  temp=tempbx&0x00FF;
  SiS_SetReg1(SiS_Part4Port,0x14,temp);
  temp2=((tempbx&0xFF00)>>8)<<7;
  
  tempcx=SiS_VGAHT-1;
  temp=tempcx&0x00FF;
  SiS_SetReg1(SiS_Part4Port,0x16,temp);
  temp=((tempcx&0xFF00)>>8)<<3;
  temp2=temp|temp2;

  tempcx=SiS_VGAVT-1;
  if(!(SiS_VBInfo&SetCRT2ToTV)){
    tempcx=tempcx-5;
  }
  temp=tempcx&0x00FF;
  SiS_SetReg1(SiS_Part4Port,0x17,temp);
  temp=temp2|((tempcx&0xFF00)>>8);
  SiS_SetReg1(SiS_Part4Port,0x15,temp);

  tempcx=SiS_VBInfo;
  tempbx=SiS_VGAHDE;
  if(modeflag&HalfDCLK){
    tempbx=tempbx>>1;
  }
  if(tempcx&SetCRT2ToHiVisionTV) {
       temp=0xA0;
       if(tempbx!=1024) {
           temp=0xC0;
           if(tempbx!=1280) temp=0;
       }
  } else if((tempcx&SetCRT2ToTV)&&(SiS_VGAHDE==1024)) {       /*301b*/
       temp=0xA0;
  } else {
       temp=0x80;
       if(SiS_VBInfo&SetCRT2ToLCD){
          temp=0;
          if(tempbx>800) temp=0x60;
       }
  }
  if(SiS_LCDResInfo!=Panel1280x1024) temp=temp|0x0A;
  SiS_SetRegANDOR(SiS_Part4Port,0x0E,~0xEF,temp);

  tempebx=SiS_VDE;
  if(tempcx&SetCRT2ToHiVisionTV) {
    /* if(!(tempax&0xE000)) tempbx=tempbx>>1; */
    if(!(temp&0xE000)) tempbx=tempbx>>1;               /* alan ???? */

  }

  tempcx=SiS_RVBHRS;
  temp=tempcx&0x00FF;
  SiS_SetReg1(SiS_Part4Port,0x18,temp);

  tempebx=tempebx;
  tempeax=SiS_VGAVDE;
  tempcx=tempcx|0x04000;
/*tempeax=tempeax-tempebx;  */
  if(tempeax<=tempebx){
    tempcx=((tempcx&0xFF00)^0x4000)|(tempcx&0x00ff);
    tempeax=SiS_VGAVDE;
  }

  else {
    tempeax=tempeax-tempebx;
  }

  push1=tempcx;
  templong=(tempeax*256*1024)%tempebx;
  tempeax=(tempeax*256*1024)/tempebx;
  tempebx=tempeax;
  if(templong!=0){
    tempebx++;
  }
  tempcx=push1;
  temp=(USHORT)(tempebx&0x000000FF);
  SiS_SetReg1(SiS_Part4Port,0x1B,temp);
  temp=(USHORT)((tempebx&0x0000FF00)>>8);
  SiS_SetReg1(SiS_Part4Port,0x1A,temp);
  tempbx=(USHORT) (tempebx>>16);
  temp=tempbx&0x00FF;
  temp=temp<<4;
  temp=temp|((tempcx&0xFF00)>>8);
  SiS_SetReg1(SiS_Part4Port,0x19,temp);

  /*301b*/
  if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
         temp=0x0028;
         SiS_SetReg1(SiS_Part4Port,0x1C,temp);
         tempax=SiS_VGAHDE;
          if(modeflag&HalfDCLK){
           tempax=tempax>>1;
           }
          if(SiS_VBInfo&(SetCRT2ToLCD)) {
             if(tempax>800)
                tempax=tempax-800;
          }
          tempax=tempax-1;                
                 
         if(SiS_VBInfo&(SetCRT2ToTV|SetCRT2ToHiVisionTV)) {
          if(SiS_VGAHDE>800)
            {
              if(SiS_VGAHDE==1024)
                tempax=(tempax*25/32)-1;
              else
                  tempax=(tempax*20/32)-1;          
            }
         }
         temp=(tempax&0xFF00)>>8;
         temp=((temp&0x0003)<<4);
         SiS_SetReg1(SiS_Part4Port,0x1E,temp);
         temp=(tempax&0x00FF);
         SiS_SetReg1(SiS_Part4Port,0x1D,temp);
   
         if(SiS_VBInfo&(SetCRT2ToTV|SetCRT2ToHiVisionTV)) {
            if(SiS_VGAHDE>800){
               SiS_SetRegOR(SiS_Part4Port,0x1E,0x08);
            }
         }
         temp=0x0036;
         if(SiS_VBInfo&SetCRT2ToTV) {
             temp=temp|0x0001;
         }
         SiS_SetRegANDOR(SiS_Part4Port,0x1F,0x00C0,temp);
         tempbx=(SiS_HT/2)-2;
         temp=((tempbx&0x0700)>>8)<<3;
         SiS_SetRegANDOR(SiS_Part4Port,0x21,0x00C0,temp);
         temp=tempbx&0x00FF;
         SiS_SetReg1(SiS_Part4Port,0x22,temp);
         if( (SiS_VBType&(VB_SIS301LV|VB_SIS302LV)) && (SiS_VBInfo&SetCRT2ToLCD) ) {
             SiS_SetReg1(SiS_Part4Port,0x24,0x0e);
	 }
  }
  /*end 301b*/

  SiS_SetCRT2VCLK(BaseAddr,ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex,HwDeviceExtension);
}

void
SiS_SetCRT2VCLK(USHORT BaseAddr,ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                 USHORT RefreshRateTableIndex,PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT vclkindex;
  USHORT tempah,temp1;

  vclkindex = SiS_GetVCLK2Ptr(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex,HwDeviceExtension);
  /*301b*/
  if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
   	tempah = SiS_VBVCLKData[vclkindex].Part4_A;
   	SiS_SetReg1(SiS_Part4Port,0x0A,tempah);
   	tempah = SiS_VBVCLKData[vclkindex].Part4_B;
   	SiS_SetReg1(SiS_Part4Port,0x0B,tempah);
  } else {
   	SiS_SetReg1(SiS_Part4Port,0x0A,0x01);
   	tempah = SiS_VBVCLKData[vclkindex].Part4_B;
   	SiS_SetReg1(SiS_Part4Port,0x0B,tempah);
   	tempah = SiS_VBVCLKData[vclkindex].Part4_A;
   	SiS_SetReg1(SiS_Part4Port,0x0A,tempah);
  }
  SiS_SetReg1(SiS_Part4Port,0x12,0x00);
  tempah=0x08;
  if(SiS_VBInfo&SetCRT2ToRAMDAC){
    	tempah=tempah|0x020;
  }
  temp1=SiS_GetReg1(SiS_Part4Port,0x12);
  tempah=tempah|temp1;
  SiS_SetReg1(SiS_Part4Port,0x12,tempah);
}

USHORT
SiS_GetVCLK2Ptr(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
        USHORT RefreshRateTableIndex,PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT tempbx;
#ifdef SIS300
  USHORT LCDXlat1VCLK300[4] = {VCLK65,VCLK65,VCLK65,VCLK65};
  USHORT LCDXlat2VCLK300[4] = {VCLK108_2,VCLK108_2,VCLK108_2,VCLK108_2};
  USHORT LVDSXlat2VCLK300[4]= {VCLK65,VCLK65,VCLK65,VCLK65};
  USHORT LVDSXlat3VCLK300[4]= {VCLK65,VCLK65,VCLK65,VCLK65};
#endif
#ifdef SIS315H
  USHORT LCDXlat1VCLK310[4] = {VCLK65+2,VCLK65+2,VCLK65+2,VCLK65+2};
  USHORT LCDXlat2VCLK310[4] = {VCLK108_2+5,VCLK108_2+5,VCLK108_2+5,VCLK108_2+5};
  USHORT LVDSXlat2VCLK310[4]= {VCLK65+2,VCLK65+2,VCLK65+2,VCLK65+2};
  USHORT LVDSXlat3VCLK310[4]= {VCLK65+2,VCLK65+2,VCLK65+2,VCLK65+2};
#endif
  USHORT LVDSXlat1VCLK[4]   = {VCLK40,VCLK40,VCLK40,VCLK40};
  USHORT CRT2Index,VCLKIndex;
  USHORT modeflag,resinfo;
  UCHAR *CHTVVCLKPtr=NULL;
  USHORT *LCDXlatVCLK1=NULL;
  USHORT *LCDXlatVCLK2=NULL;
  USHORT *LVDSXlatVCLK2=NULL;
  USHORT *LVDSXlatVCLK3=NULL;

#ifdef SIS315H
  if ( (HwDeviceExtension->jChipType==SIS_315H)||   
       (HwDeviceExtension->jChipType==SIS_315PRO)||
       (HwDeviceExtension->jChipType==SIS_550) ||
       (HwDeviceExtension->jChipType==SIS_640) ||
       (HwDeviceExtension->jChipType==SIS_740) ||
       (HwDeviceExtension->jChipType==SIS_650)) {
		LCDXlatVCLK1 = LCDXlat1VCLK310;
		LCDXlatVCLK2 = LCDXlat2VCLK310;
		LVDSXlatVCLK2 = LVDSXlat2VCLK310;
		LVDSXlatVCLK3 = LVDSXlat3VCLK310;
  } else {
#endif
#ifdef SIS300
		LCDXlatVCLK1 = LCDXlat1VCLK300;
		LCDXlatVCLK2 = LCDXlat2VCLK300;
		LVDSXlatVCLK2 = LVDSXlat2VCLK300;
		LVDSXlatVCLK3 = LVDSXlat3VCLK300;
#endif
#ifdef SIS315H
  }
#endif

  if(ModeNo<=0x13) {
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;      /* si+St_ResInfo */
    	resinfo = SiS_SModeIDTable[ModeIdIndex].St_ResInfo;
    	CRT2Index = SiS_SModeIDTable[ModeIdIndex].St_CRT2CRTC;
  } else {
    	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;     /* si+Ext_ResInfo */
    	resinfo = SiS_EModeIDTable[ModeIdIndex].Ext_RESINFO;
    	CRT2Index = SiS_RefIndex[RefreshRateTableIndex].Ext_CRT2CRTC;
  }

  if(SiS_IF_DEF_LVDS==0) {
     CRT2Index = CRT2Index>>6;        /*  for LCD */
     if((SiS_VBInfo&SetCRT2ToLCD)||(SiS_VBInfo&SetCRT2ToLCDA)){   /*301b*/
      	if(SiS_LCDResInfo!=Panel1024x768){
        	VCLKIndex = LCDXlatVCLK2[CRT2Index];
      	} else {
        	VCLKIndex = LCDXlatVCLK1[CRT2Index];
      	}
     } else {        /* for TV */
      	if(SiS_VBInfo&SetCRT2ToTV) {
        	if(SiS_IF_DEF_HiVision==1) {
          		if(SiS_SetFlag&RPLLDIV2XO) {
            			VCLKIndex=HiTVVCLKDIV2;
            			if(HwDeviceExtension->jChipType >= SIS_315H) { /* 310 series */
					/* VCLKIndex += 11;    for chip310  0x2E */
              				VCLKIndex += 25;    /* for chip315  */
            			}
     			} else {
            			VCLKIndex=HiTVVCLK;
            			if(HwDeviceExtension->jChipType >= SIS_315H) { /* 310 series */
					/*  VCLKIndex += 11;    for chip310  0x2E */
              				VCLKIndex += 25;    /* for chip315  */
            			}
          		}
          		if(SiS_SetFlag&TVSimuMode) {
            			if(modeflag&Charx8Dot) {
              				VCLKIndex=HiTVSimuVCLK;
              					if(HwDeviceExtension->jChipType >= SIS_315H) { /* 310 series */
							/*  VCLKIndex += 11;    for chip310  0x2E */
              						VCLKIndex += 25;    /* for chip315  */
              					}
            			} else {
              				VCLKIndex=HiTVTextVCLK;
              				if(HwDeviceExtension->jChipType >= SIS_315H) { /* 310 series */
						/*  VCLKIndex += 11;    for chip310  0x2E */
              					VCLKIndex += 25;    /* for chip315  */
              				}
            			}
          		}
        	} else {
          		if(SiS_VBInfo&SetCRT2ToTV) {
            			if(SiS_SetFlag&RPLLDIV2XO) {
              				VCLKIndex=TVVCLKDIV2;
              					if(HwDeviceExtension->jChipType >= SIS_315H) { /* 310 series */
							/* VCLKIndex += 11;    for chip310  0x2E */
              						VCLKIndex += 25;    /* for chip315  */
              					}
            			} else {
              				VCLKIndex=TVVCLK;
              				if(HwDeviceExtension->jChipType >= SIS_315H) { /* 310 series */
						/* VCLKIndex += 11;    for chip310  0x2E */
              					VCLKIndex += 25;    /* for chip315  */
              				}
            			}
          		}
        	}
      } else {    /* for CRT2 */
        	VCLKIndex = (UCHAR)SiS_GetReg2((USHORT)(SiS_P3ca+0x02));      /*  Port 3cch */
        	VCLKIndex =((VCLKIndex>>2)&0x03);
        	if(ModeNo>0x13) {
          		VCLKIndex =
			    SiS_RefIndex[RefreshRateTableIndex].Ext_CRTVCLK;    /* di+Ext_CRTVCLK */
          		VCLKIndex = VCLKIndex&0x3f;
        	}
      }
    }
  } else {       /*   LVDS  */
    	if(ModeNo<=0x13) VCLKIndex = CRT2Index;
    	else VCLKIndex = CRT2Index;
#ifdef LINUX_XF86
	xf86DrvMsg(0, X_INFO, "CRT2Index = %d\n", CRT2Index);
#endif
	/* TW: This does not make sense: If CH7005 is there but CRT2 is set to
	 *     LCD, VCLKindex is returned untouched...
	 *     (Inserted SlaveMode check for 8bpp modes later)
	 */

#if 1
    	if((SiS_IF_DEF_CH7005==1) && (!(SiS_VBInfo&SetInSlaveMode))) {
      		if(!(SiS_VBInfo&SetCRT2ToLCD)) {
#else

/* 	TW: If we do it this way instead, some machines (MITAC) have problems 		*/
	if((SiS_IF_DEF_CH7005==1) && (SiS_VBInfo&SetCRT2ToTV)) {
#endif

        		VCLKIndex = VCLKIndex&0x1f;
        		tempbx=0;
        		if(SiS_VBInfo&SetPALTV) tempbx=tempbx+2;
        		if(SiS_VBInfo&SetCHTVOverScan) tempbx=tempbx+1;
       			switch(tempbx) {
          			case 0: CHTVVCLKPtr=SiS_CHTVVCLKUNTSC;
					break;
          			case 1: CHTVVCLKPtr=SiS_CHTVVCLKONTSC;
					break;
          			case 2: CHTVVCLKPtr=SiS_CHTVVCLKUPAL;
					break;
          			case 3: CHTVVCLKPtr=SiS_CHTVVCLKOPAL;
					break;
        		}
        		VCLKIndex=CHTVVCLKPtr[VCLKIndex];
      		}  
    	} else {
     		VCLKIndex = VCLKIndex>>6;
     		if((SiS_LCDResInfo==Panel800x600)||(SiS_LCDResInfo==Panel320x480))
     			VCLKIndex = LVDSXlat1VCLK[VCLKIndex]; /*fstn*/
     		else if(SiS_LCDResInfo==Panel1024x768)
     			VCLKIndex = LVDSXlatVCLK2[VCLKIndex];
     		else 	VCLKIndex = LVDSXlatVCLK3[VCLKIndex];
    	}
  }
  if(HwDeviceExtension->jChipType < SIS_315H) { /* for300 serial*/
    	VCLKIndex=VCLKIndex&0x3F;
  }
  return (VCLKIndex);
}

void
SiS_SetGroup5(USHORT  BaseAddr,ULONG ROMAddr,USHORT ModeNo,
              USHORT ModeIdIndex)
{
  USHORT Pindex,Pdata;

  Pindex=SiS_Part5Port;
  Pdata=SiS_Part5Port+1;
  if(SiS_ModeType==ModeVGA){
    if(!(SiS_VBInfo&(SetInSlaveMode|LoadDACFlag|CRT2DisplayFlag))){
      SiS_EnableCRT2();
/*    LoadDAC2(ROMAddr,SiS_Part5Port,ModeNo,ModeIdIndex);  */
    }
  }
  return;
}

void
SiS_LoadDAC2(ULONG ROMAddr,USHORT SiS_Part5Port,
             USHORT ModeNo,USHORT ModeIdIndex)
{
   USHORT data,data2;
   USHORT time,i,j,k;
   USHORT m,n,o;
   USHORT si,di,bx,dl;
   USHORT al,ah,dh;
   USHORT *table=0;
   USHORT Pindex,Pdata,modeflag;

  if(ModeNo<=0x13) {
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;      /* si+St_ResInfo */
  } else {
    	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;     /* si+Ext_ResInfo */
  }

   Pindex=SiS_Part5Port;
   Pdata=SiS_Part5Port+1;
   data=modeflag&DACInfoFlag;
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

   SiS_SetReg3(Pindex,0x00);

   for(i=0;i<j;i++) {
      data=table[i];
      for(k=0;k<3;k++) {
        data2=0;
        if(data&0x01) data2=0x2A;
        if(data&0x02) data2=data2+0x15;
        SiS_SetReg3(Pdata,data2);
        data=data>>2;
      }
   }

   if(time==256) {
      for(i=16;i<32;i++) {
         data=table[i];
         for(k=0;k<3;k++) SiS_SetReg3(Pdata,data);
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
              SiS_WriteDAC2(Pdata,dl,ah,al,dh);
            }         /* for 5 */
            si=si-2;
            for(o=0;o<3;o++) {
              dh=table[bx];
              ah=table[di];
              al=table[si];
              si--;
              SiS_WriteDAC2(Pdata,dl,ah,al,dh);
            }         /* for 3 */
            dl++;
         }            /* for 3 */
         si=si+5;
      }               /* for 9 */
   }
}

void
SiS_WriteDAC2(USHORT Pdata,USHORT dl, USHORT ah, USHORT al, USHORT dh)
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
    }
    else {
       temp=bl;
       bl=bh;
       bh=temp;
    }
  }
  SiS_SetReg3(Pdata,(USHORT)dh);
  SiS_SetReg3(Pdata,(USHORT)bh);
  SiS_SetReg3(Pdata,(USHORT)bl);
}

/* TW: Start of Chrontel 7005 functions ---------------------------- */

void
SiS_SetCHTVReg(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
               USHORT RefreshRateTableIndex)
{
  USHORT temp,tempbx,tempcl;
/*  USHORT CRT2CRTC; */
  USHORT TVType,resindex;
  SiS_CHTVRegDataStruct *CHTVRegData=NULL;

  if(ModeNo<=0x13){
    	tempcl = SiS_SModeIDTable[ModeIdIndex].St_CRT2CRTC;
  }else{
    	tempcl = SiS_RefIndex[RefreshRateTableIndex].Ext_CRT2CRTC;
  }
  TVType = 0;
  if(SiS_VBInfo&SetPALTV) TVType=TVType+2;
  if(SiS_VBInfo&SetCHTVOverScan) TVType=TVType+1;
  switch(TVType) {
    	case 0: CHTVRegData = SiS_CHTVReg_UNTSC; break;
    	case 1: CHTVRegData = SiS_CHTVReg_ONTSC; break;
    	case 2: CHTVRegData = SiS_CHTVReg_UPAL;  break;
    	case 3: CHTVRegData = SiS_CHTVReg_OPAL;  break;
  }
  resindex=tempcl&0x3F;

  /* TW: We don't support modes >800x600 */
  if (resindex > 5) return;

  if(SiS_VBInfo&SetPALTV) {
    	SiS_SetCH7005(0x4304);  /* TW: 0x40=76uA (PAL); 0x03=15bit non-multi RGB*/
    	SiS_SetCH7005(0x6909);	/* TW: Black level for PAL (105)*/
  } else {
    	SiS_SetCH7005(0x0304);  /* TW: upper nibble=71uA (NTSC), 0x03=15bit non-multi RGB*/
    	SiS_SetCH7005(0x7109);	/* TW: Black level for NTSC (113)*/
  }

  temp = CHTVRegData[resindex].Reg[0];
  tempbx=((temp&0x00FF)<<8)|0x00;	/* TW: Mode register */
  SiS_SetCH7005(tempbx);
  temp = CHTVRegData[resindex].Reg[1];
  tempbx=((temp&0x00FF)<<8)|0x07;	/* TW: Start active video register */
  SiS_SetCH7005(tempbx);
  temp = CHTVRegData[resindex].Reg[2];
  tempbx=((temp&0x00FF)<<8)|0x08;	/* TW: Position overflow register */
  SiS_SetCH7005(tempbx);
  temp = CHTVRegData[resindex].Reg[3];
  tempbx=((temp&0x00FF)<<8)|0x0A;	/* TW: Horiz Position register */
  SiS_SetCH7005(tempbx);
  temp = CHTVRegData[resindex].Reg[4];
  tempbx=((temp&0x00FF)<<8)|0x0B;	/* TW: Vertical Position register */
  SiS_SetCH7005(tempbx);

  /* TW: Set minimum flicker filter for Luma channel (SR1-0=00),
             minimum text enhancement (S3-2=10),
	     maximum flicker filter for Chroma channel (S5-4=10)
	     =00101000=0x28 (When reading, S1-0->S3-2, and S3-2->S1-0!)
   */
  SiS_SetCH7005(0x2801);

  /* TW: Set video bandwidth
         High bandwith Luma composite video filter(S0=1)
         low bandwith Luma S-video filter (S2-1=00)
	 disable peak filter in S-video channel (S3=0)
	 high bandwidth Chroma Filter (S5-4=11)
	 =00110001=0x31
  */
  SiS_SetCH7005(0xb103);       /* old: 3103 */

  /* TW: Register 0x3D does not exist in non-macrovision register map
         (Maybe this is a macrovision register?)
   */
  /* SiS_SetCH7005(0x003D); */

  /* TW: Register 0x10 only contains 1 writable bit (S0) for sensing,
         all other bits a read-only. Macrovision?
   */
  SiS_SetCHTVRegANDOR(0x0010,0x1F);

  /* TW: Register 0x11 only contains 3 writable bits (S0-S2) for
         contrast enhancement (set to 010 -> gain 2 Yout = 9/8*(Yin-57) )
   */
  SiS_SetCHTVRegANDOR(0x0211,0xF8);

  /* TW: Clear DSEN
   */
  SiS_SetCHTVRegANDOR(0x001C,0xEF);

  if(!(SiS_VBInfo&SetPALTV)) {		/* ---- NTSC ---- */
    tempcl=tempcl&0x3F;
    if(SiS_VBInfo&SetCHTVOverScan) {
      if(tempcl==0x04) {   			/* 640x480 overscan: Mode 16 */
      	SiS_SetCHTVRegANDOR(0x0020,0xEF);   	/* loop filter off */
        SiS_SetCHTVRegANDOR(0x0121,0xFE);       /* ACIV on, no need to set FSCI */
      } else {
        if(tempcl==0x05) {    			/* 800x600 overscan: Mode 23 */
          SiS_SetCHTVRegANDOR(0x0118,0xF0);	/* 0x18-0x1f: FSCI 469,762,048 */
          SiS_SetCHTVRegANDOR(0x0C19,0xF0);
          SiS_SetCHTVRegANDOR(0x001A,0xF0);
          SiS_SetCHTVRegANDOR(0x001B,0xF0);
          SiS_SetCHTVRegANDOR(0x001C,0xF0);
          SiS_SetCHTVRegANDOR(0x001D,0xF0);
          SiS_SetCHTVRegANDOR(0x001E,0xF0);
          SiS_SetCHTVRegANDOR(0x001F,0xF0);
          SiS_SetCHTVRegANDOR(0x1020,0xEF);     /* Loop filter on for mode 23 */
          SiS_SetCHTVRegANDOR(0x0021,0xFE);     /* ACIV off, need to set FSCI */
        }
      }
    } else {
      if(tempcl==0x04) {     			/* ----- 640x480 underscan; Mode 17 */
        SiS_SetCHTVRegANDOR(0x0020,0xEF); 	/* loop filter off */
        SiS_SetCHTVRegANDOR(0x0121,0xFE);
      } else {
        if(tempcl==0x05) {   			/* ----- 800x600 underscan: Mode 24 */
          SiS_SetCHTVRegANDOR(0x0118,0xF0);     /* (FSCI was 0x1f1c71c7 - this is for mode 22) */
          SiS_SetCHTVRegANDOR(0x0919,0xF0);	/* FSCI for mode 24 is 428,554,851 */
          SiS_SetCHTVRegANDOR(0x081A,0xF0);
          SiS_SetCHTVRegANDOR(0x0b1B,0xF0);
          SiS_SetCHTVRegANDOR(0x031C,0xF0);
          SiS_SetCHTVRegANDOR(0x0a1D,0xF0);
          SiS_SetCHTVRegANDOR(0x061E,0xF0);
          SiS_SetCHTVRegANDOR(0x031F,0xF0);
          SiS_SetCHTVRegANDOR(0x0020,0xEF);     /* loop filter off for mode 24 */
          SiS_SetCHTVRegANDOR(0x0021,0xFE);	/* ACIV off, need to set FSCI */
        }
      }
    }
  } else {				/* ---- PAL ---- */
        /* TW: We don't play around with FSCI in PAL mode */
      if (tempcl==0x04) {
        SiS_SetCHTVRegANDOR(0x0020,0xEF); 	/* loop filter off */
        SiS_SetCHTVRegANDOR(0x0121,0xFE);       /* ACIV on */
      } else {
        SiS_SetCHTVRegANDOR(0x0020,0xEF); 	/* loop filter off */
        SiS_SetCHTVRegANDOR(0x0121,0xFE);       /* ACIV on */
      }
  }
}

void
SiS_SetCHTVRegANDOR(USHORT tempax,USHORT tempbh)
{
  USHORT tempal,tempah,tempbl;

  tempal=tempax&0x00FF;
  tempah=(tempax>>8)&0x00FF;
  tempbl=SiS_GetCH7005(tempal);
  tempbl=(((tempbl&tempbh)|tempah)<<8|tempal);  
  SiS_SetCH7005(tempbl);
}

/* TW: Write to Chrontel 7005 */
/* Parameter is [Data (S15-S8) | Register no (S7-S0)] */
void
SiS_SetCH7005(USHORT tempbx)
{
  USHORT tempah,temp,i;

  /* TW: This has to be relative to BaseAddr */
  /* SiS_DDC_Port=0x3c4; */
  SiS_DDC_Index=0x11;			/* TW: Bit 0 = SC;  Bit 1 = SD */
  SiS_DDC_DataShift=0x00;
  SiS_DDC_DeviceAddr=0xEA;  		/* TW: DAB (Device Address Byte) */

  for(i=0;i<50;i++) {	/* TW: Do only 50 attempts to write */
    SiS_SetSwitchDDC2();
    SiS_SetStart();			/* TW: Set start condition */
    tempah=SiS_DDC_DeviceAddr;
    temp=SiS_WriteDDC2Data(tempah);	/* TW: Write DAB (S0=0=write) */
    if(temp) continue;			/* TW:    (ERROR: no ack) */
    tempah=tempbx&0x00FF;
    temp=SiS_WriteDDC2Data(tempah);	/* TW: Write RAB */
    if(temp) continue;			/* TW:    (ERROR: no ack) */
    tempah=(tempbx&0xFF00)>>8;
    temp=SiS_WriteDDC2Data(tempah);	/* TW: Write data */
    if(temp) continue;			/* TW:    (ERROR: no ack) */
    SiS_SetStop();			/* TW: Set stop condition */
    return;
  }
}

USHORT
SiS_GetCH7005(USHORT tempbx)
{
  USHORT tempah,temp,i;

  /* TW: This has to be relative to BaseAddr, set by RegInit */
  /* SiS_DDC_Port=0x3c4; */
  SiS_DDC_Index=0x11;			/* TW: Bit 0 = SC;  Bit 1 = SD */
  SiS_DDC_DataShift=0x00;
  SiS_DDC_DeviceAddr=0xEA;		/* TW: DAB */
  SiS_DDC_ReadAddr=tempbx;

  for(i=0;i<50;i++) {	/* TW: Do only 50 attempts to write */
    SiS_SetSwitchDDC2();
    SiS_SetStart();			/* TW: Set start condition */
    tempah=SiS_DDC_DeviceAddr;
    temp=SiS_WriteDDC2Data(tempah);	/* TW: Write DAB (S0=0=write) */
    if(temp) continue;			/* TW:        (ERROR: no ack) */
    tempah=SiS_DDC_ReadAddr;		/* TW: Write RAB */
    temp=SiS_WriteDDC2Data(tempah);
    if(temp) continue;			/* TW:        (ERROR: no ack) */
    SiS_SetStart();			/* TW: Re-start */
    tempah=SiS_DDC_DeviceAddr;
    tempah=tempah|0x01;
    temp=SiS_WriteDDC2Data(tempah);	/* TW: DAB (S0=1=read) */
    if(temp) continue;			/* TW:        (ERROR: no ack) */
    tempah=SiS_ReadDDC2Data(tempah);	/* TW: Read byte */
    SiS_SetStop();			/* TW: Stop condition */
    return(tempah);
  }
  return(tempah);
}

void
SiS_SetSwitchDDC2(void)
{
  USHORT i;

  SiS_SetSCLKHigh();
  for(i=0;i<1000;i++) {
    SiS_GetReg1(SiS_DDC_Port,0x05);
  }
  SiS_SetSCLKLow();
  for(i=0;i<1000;i++) {
    SiS_GetReg1(SiS_DDC_Port,0x05);
  }
}

/* TW: Set Chrontel 7005 start condition */
/* TW: This is done by a SD high-to-low transition while SC is high */
void
SiS_SetStart(void)
{
  SiS_SetSCLKLow();					  /* TW: (SC->low)  */
  SiS_SetRegANDOR(SiS_DDC_Port,SiS_DDC_Index,0xFD,0x02);  /* TW: SD->high */
  SiS_SetSCLKHigh();					  /* TW: SC->high */
  SiS_SetRegANDOR(SiS_DDC_Port,SiS_DDC_Index,0xFD,0x00);  /* TW: SD->low = start condition */
  SiS_SetSCLKHigh();					  /* TW: (SC->low) */
}

/* TW: Set Chrontel 7005 stop condition */
/* TW: This is done by a SD low-to-high transition while SC is high */
void
SiS_SetStop(void)
{
  SiS_SetSCLKLow();					  /* TW: (SC->low) */
  SiS_SetRegANDOR(SiS_DDC_Port,SiS_DDC_Index,0xFD,0x00);  /* TW: SD->low   */
  SiS_SetSCLKHigh();					  /* TW: SC->high  */
  SiS_SetRegANDOR(SiS_DDC_Port,SiS_DDC_Index,0xFD,0x02);  /* TW: SD->high = stop condition */   
  SiS_SetSCLKHigh();					  /* TW: (SC->high) */
}

/* TW: Write 8 bits of data */
USHORT
SiS_WriteDDC2Data(USHORT tempax)
{
  USHORT i,flag,temp;

  flag=0x80;
  for(i=0;i<8;i++) {
    SiS_SetSCLKLow();					     /* TW: SC->low */
    if(tempax&flag) {
      SiS_SetRegANDOR(SiS_DDC_Port,SiS_DDC_Index,0xFD,0x02); /* TW: Write bit (1) to SD */
    } else {
      SiS_SetRegANDOR(SiS_DDC_Port,SiS_DDC_Index,0xFD,0x00); /* TW: Write bit (0) to SD */
    }
    SiS_SetSCLKHigh();					     /* TW: SC->high */
    flag=flag>>1;
  }
  temp=SiS_CheckACK();					     /* TW: Check acknowledge */
  return(temp);
}

USHORT
SiS_ReadDDC2Data(USHORT tempax)
{
  USHORT i,temp,getdata;

  getdata=0;
  for(i=0;i<8;i++) {
    getdata=getdata<<1;
    SiS_SetSCLKLow();
    SiS_SetRegANDOR(SiS_DDC_Port,SiS_DDC_Index,0xFD,0x02);
    SiS_SetSCLKHigh();
    temp=SiS_GetReg1(SiS_DDC_Port,SiS_DDC_Index);
    if(temp&0x02) getdata=getdata|0x01;
  }
  return(getdata);
}

void
SiS_SetSCLKLow(void)
{
    USHORT temp;

    SiS_SetRegANDOR(SiS_DDC_Port,SiS_DDC_Index,0xFE,0x00);      /* SetSCLKLow()  */
    do {
      temp=SiS_GetReg1(SiS_DDC_Port,SiS_DDC_Index);
    } while(temp&0x01);
    SiS_DDC2Delay();
}

void
SiS_SetSCLKHigh(void)
{
  USHORT temp;

  SiS_SetRegANDOR(SiS_DDC_Port,SiS_DDC_Index,0xFE,0x01);      /* SetSCLKHigh()  */
  do {
    temp=SiS_GetReg1(SiS_DDC_Port,SiS_DDC_Index);
  } while(!(temp&0x01));
  SiS_DDC2Delay();
}

void
SiS_DDC2Delay(void)
{
  USHORT i;

   for(i=0;i<DDC2DelayTime;i++) {
    SiS_GetReg1(SiS_P3c4,0x05);
  }
}

/* TW: Check acknowledge */
/* Returns 0 if ack ok, non-0 if ack not ok */
USHORT
SiS_CheckACK(void)
{
  USHORT tempah;

  SiS_SetSCLKLow();					  /* TW: (SC->low) */
  SiS_SetRegANDOR(SiS_DDC_Port,SiS_DDC_Index,0xFD,0x02);  /* TW: (SD->high) */
  SiS_SetSCLKHigh();					  /* TW: SC->high = clock impulse for ack */
  tempah=SiS_GetReg1(SiS_DDC_Port,SiS_DDC_Index);	  /* TW: Read SD */
  SiS_SetSCLKLow();					  /* TW: SC->low = end of clock impulse */
  if(tempah&0x02) return(1);				  /* TW: Ack OK if bit = 0 */
  else return(0);
}

/* TW: End of Chrontel 7005 functions ---------------------------- */

void
SiS_ModCRT1CRTC(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                USHORT RefreshRateTableIndex)
{
  USHORT temp,tempah,i,modeflag,j;
  USHORT ResInfo,DisplayType;
  SiS_LVDSCRT1DataStruct *LVDSCRT1Ptr=NULL;

  if(ModeNo<=0x13) {
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;      /* si+St_ResInfo */
  } else {
    	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;     /* si+Ext_ResInfo */
  }

  temp= SiS_GetLVDSCRT1Ptr(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex,
                       &ResInfo,&DisplayType);
  if(temp==0){
    return;
  }

  switch(DisplayType) {
    case 0 : LVDSCRT1Ptr = SiS_LVDSCRT1800x600_1;           break;
    case 1 : LVDSCRT1Ptr = SiS_LVDSCRT11024x768_1;          break;
    case 2 : LVDSCRT1Ptr = SiS_LVDSCRT11280x1024_1;         break;
    case 3 : LVDSCRT1Ptr = SiS_LVDSCRT1800x600_1_H;         break;
    case 4 : LVDSCRT1Ptr = SiS_LVDSCRT11024x768_1_H;        break;
    case 5 : LVDSCRT1Ptr = SiS_LVDSCRT11280x1024_1_H;       break;
    case 6 : LVDSCRT1Ptr = SiS_LVDSCRT1800x600_2;           break;
    case 7 : LVDSCRT1Ptr = SiS_LVDSCRT11024x768_2;          break;
    case 8 : LVDSCRT1Ptr = SiS_LVDSCRT11280x1024_2;         break;
    case 9 : LVDSCRT1Ptr = SiS_LVDSCRT1800x600_2_H;         break;
    case 10: LVDSCRT1Ptr = SiS_LVDSCRT11024x768_2_H;        break;
    case 11: LVDSCRT1Ptr = SiS_LVDSCRT11280x1024_2_H;       break;
    case 12: LVDSCRT1Ptr = SiS_CHTVCRT1UNTSC;               break;
    case 13: LVDSCRT1Ptr = SiS_CHTVCRT1ONTSC;               break;
    case 14: LVDSCRT1Ptr = SiS_CHTVCRT1UPAL;                break;
    case 15: LVDSCRT1Ptr = SiS_CHTVCRT1OPAL;                break;
    case 16: LVDSCRT1Ptr = SiS_LVDSCRT1320x480_1;           break;
  }

  tempah=(UCHAR)SiS_GetReg1(SiS_P3d4,0x11);                        /*unlock cr0-7  */
  tempah=tempah&0x7F;
  SiS_SetReg1(SiS_P3d4,0x11,tempah);
  tempah = (LVDSCRT1Ptr+ResInfo)->CR[0];
  SiS_SetReg1(SiS_P3d4,0x0,tempah);
  for(i=0x02,j=1;i<=0x05;i++,j++){
    tempah = (LVDSCRT1Ptr+ResInfo)->CR[j];
    SiS_SetReg1(SiS_P3d4,i,tempah);
  }
  for(i=0x06,j=5;i<=0x07;i++,j++){
    tempah = (LVDSCRT1Ptr+ResInfo)->CR[j];
    SiS_SetReg1(SiS_P3d4,i,tempah);
  }
  for(i=0x10,j=7;i<=0x11;i++,j++){
    tempah = (LVDSCRT1Ptr+ResInfo)->CR[j];
    SiS_SetReg1(SiS_P3d4,i,tempah);
  }
  for(i=0x15,j=9;i<=0x16;i++,j++){
    tempah = (LVDSCRT1Ptr+ResInfo)->CR[j];
    SiS_SetReg1(SiS_P3d4,i,tempah);
  }
  for(i=0x0A,j=11;i<=0x0C;i++,j++){
    tempah = (LVDSCRT1Ptr+ResInfo)->CR[j];
    SiS_SetReg1(SiS_P3c4,i,tempah);
  }

  tempah = (LVDSCRT1Ptr+ResInfo)->CR[14];
  tempah=tempah&0x0E0;
  SiS_SetReg1(SiS_P3c4,0x0E,tempah);

  tempah = (LVDSCRT1Ptr+ResInfo)->CR[14];
  tempah=tempah&0x01;
  tempah=tempah<<5;
  if(modeflag&DoubleScanMode){
    	tempah=tempah|0x080;
  }
  SiS_SetRegANDOR(SiS_P3d4,0x09,~0x020,tempah);
  return;
}

/*301b*/
void
SiS_CHACRT1CRTC(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                USHORT RefreshRateTableIndex)
{
  USHORT temp,tempah,i,modeflag,j;
  USHORT ResInfo,DisplayType;
  SiS_LVDSCRT1DataStruct *LVDSCRT1Ptr=NULL;

  if(ModeNo<=0x13) {
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;      /* si+St_ResInfo */
  } else {
    	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;     /* si+Ext_ResInfo */
  }

  temp= SiS_GetLVDSCRT1Ptr(ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex,
                       &ResInfo,&DisplayType);
  if(temp==0){
    return;
  }

  switch(DisplayType) {
    case 0 : LVDSCRT1Ptr = SiS_LVDSCRT1800x600_1;           break;
    case 1 : LVDSCRT1Ptr = SiS_LVDSCRT11024x768_1;          break;
    case 2 : LVDSCRT1Ptr = SiS_LVDSCRT11280x1024_1;         break;
    case 3 : LVDSCRT1Ptr = SiS_LVDSCRT1800x600_1_H;         break;
    case 4 : LVDSCRT1Ptr = SiS_LVDSCRT11024x768_1_H;        break;
    case 5 : LVDSCRT1Ptr = SiS_LVDSCRT11280x1024_1_H;       break;
    case 6 : LVDSCRT1Ptr = SiS_LVDSCRT1800x600_2;           break;
    case 7 : LVDSCRT1Ptr = SiS_LVDSCRT11024x768_2;          break;
    case 8 : LVDSCRT1Ptr = SiS_LVDSCRT11280x1024_2;         break;
    case 9 : LVDSCRT1Ptr = SiS_LVDSCRT1800x600_2_H;         break;
    case 10: LVDSCRT1Ptr = SiS_LVDSCRT11024x768_2_H;        break;
    case 11: LVDSCRT1Ptr = SiS_LVDSCRT11280x1024_2_H;       break;
    case 12: LVDSCRT1Ptr = SiS_CHTVCRT1UNTSC;               break;
    case 13: LVDSCRT1Ptr = SiS_CHTVCRT1ONTSC;               break;
    case 14: LVDSCRT1Ptr = SiS_CHTVCRT1UPAL;                break;
    case 15: LVDSCRT1Ptr = SiS_CHTVCRT1OPAL;                break;
  }

  tempah=(UCHAR)SiS_GetReg1(SiS_P3d4,0x11);                        /*unlock cr0-7  */
  tempah=tempah&0x7F;
  SiS_SetReg1(SiS_P3d4,0x11,tempah);
  tempah = (LVDSCRT1Ptr+ResInfo)->CR[0];
  SiS_SetReg1(SiS_P3d4,0x0,tempah);
  for(i=0x02,j=1;i<=0x05;i++,j++){
    tempah = (LVDSCRT1Ptr+ResInfo)->CR[j];
    SiS_SetReg1(SiS_P3d4,i,tempah);
  }
  for(i=0x06,j=5;i<=0x07;i++,j++){
    tempah = (LVDSCRT1Ptr+ResInfo)->CR[j];
    SiS_SetReg1(SiS_P3d4,i,tempah);
  }
  for(i=0x10,j=7;i<=0x11;i++,j++){
    tempah = (LVDSCRT1Ptr+ResInfo)->CR[j];
    SiS_SetReg1(SiS_P3d4,i,tempah);
  }
  for(i=0x15,j=9;i<=0x16;i++,j++){
    tempah = (LVDSCRT1Ptr+ResInfo)->CR[j];
    SiS_SetReg1(SiS_P3d4,i,tempah);
  }

  for(i=0x0A,j=11;i<=0x0C;i++,j++){
    tempah = (LVDSCRT1Ptr+ResInfo)->CR[j];
    SiS_SetReg1(SiS_P3c4,i,tempah);
  }

  tempah = (LVDSCRT1Ptr+ResInfo)->CR[14];
  tempah=tempah&0x0E0;
  SiS_SetReg1(SiS_P3c4,0x0E,tempah);

  tempah = (LVDSCRT1Ptr+ResInfo)->CR[14];
  tempah=tempah&0x01;
  tempah=tempah<<5;
  if(modeflag&DoubleScanMode){
    	tempah=tempah|0x080;
  }
  SiS_SetRegANDOR(SiS_P3d4,0x09,~0x020,tempah);
  return;
}
/*add for LCDA*/

BOOLEAN
SiS_GetLCDACRT1Ptr(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
		   USHORT RefreshRateTableIndex,USHORT *ResInfo,
		   USHORT *DisplayType)
 {
  USHORT tempbx=0,modeflag=0;
  USHORT CRT2CRTC=0;
 /*301b*/
  if( (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
                    &&(SiS_VBInfo&SetCRT2ToLCDA) ) {
    	tempbx = SiS_LCDResInfo;
    	tempbx -= Panel800x600;
    	if(SiS_LCDInfo&LCDNonExpanding) tempbx +=6;
#if 0   /* TW: That can't work - test modeflag BEFORE reading it? */
    	if(modeflag&HalfDCLK) tempbx +=+3;
#endif
    	if(ModeNo<=0x13) {
    		modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;      /* si+St_ResInfo */
    		CRT2CRTC = SiS_SModeIDTable[ModeIdIndex].St_CRT2CRTC;
  	} else {
    		modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;     /* si+Ext_ResInfo */
    		CRT2CRTC = SiS_RefIndex[RefreshRateTableIndex].Ext_CRT2CRTC;
  	}
	/* TW: This belongs here I assume */
	if(modeflag&HalfDCLK) tempbx +=+3;
  }
  *ResInfo = CRT2CRTC&0x3F;
  *DisplayType = tempbx;
  return 1;
}
/*end for 301b*/

BOOLEAN
SiS_GetLVDSCRT1Ptr(ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
		USHORT RefreshRateTableIndex,USHORT *ResInfo,USHORT *DisplayType)
 {
  USHORT tempbx,modeflag=0;
  USHORT Flag,CRT2CRTC;
 
  if(ModeNo<=0x13) {
    	modeflag = SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;      /* si+St_ResInfo */
    	CRT2CRTC = SiS_SModeIDTable[ModeIdIndex].St_CRT2CRTC;
  } else {
    	modeflag = SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;     /* si+Ext_ResInfo */
    	CRT2CRTC = SiS_RefIndex[RefreshRateTableIndex].Ext_CRT2CRTC;
  }
  if(!(SiS_VBInfo&SetInSlaveMode)){
     return 0;
  }
  Flag=1;
  tempbx=0;
  if(SiS_IF_DEF_CH7005==1) {
    if(!(SiS_VBInfo&SetCRT2ToLCD)) {
      Flag=0;
      tempbx =12;
      if(SiS_VBInfo&SetPALTV) tempbx +=2;
      if(SiS_VBInfo&SetCHTVOverScan) tempbx +=1;
    }
  }
  if(Flag) {
    tempbx =SiS_LCDResInfo;
    tempbx -=Panel800x600;
    if(SiS_LCDInfo&LCDNonExpanding) tempbx +=6;
    if(modeflag&HalfDCLK) tempbx +=+3;
  }
  /*fstn*/
   if(SiS_IF_DEF_FSTN){
     if(SiS_LCDResInfo==Panel320x480){
       tempbx=0x10;         /*not same with asmber code */
     }
   }
  *ResInfo = CRT2CRTC&0x3F;
  *DisplayType = tempbx;
  return 1;
}

void
SiS_SetCRT2ECLK(ULONG ROMAddr, USHORT ModeNo,USHORT ModeIdIndex,
           USHORT RefreshRateTableIndex,PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT tempah,tempal;
  USHORT P3cc=SiS_P3c9+3;
  USHORT vclkindex=0;

  if(SiS_IF_DEF_TRUMPION==0){  /*no trumpion  */
    	tempal=SiS_GetReg2(P3cc);
    	tempal=tempal&0x0C;
    	vclkindex=SiS_GetVCLK2Ptr(ROMAddr,ModeNo,ModeIdIndex,
                               RefreshRateTableIndex,HwDeviceExtension);
  } else {  			/*trumpion  */
    	SiS_SetFlag=SiS_SetFlag&(~ProgrammingCRT2);
/*  	tempal=*((UCHAR *)(ROMAddr+SiS_RefIndex+0x03));     &di+Ext_CRTVCLK  */
    	tempal=tempal&0x03F;
    	if(tempal==0x02){ /*31.5MHz  */
/*      	SiS_RefIndex=SiS_RefIndex-Ext2StructSize;   */
    	}
/*    	SiS_RefIndex=GetVCLKPtr(ROMAddr,ModeNo);  */
    	SiS_SetFlag=SiS_SetFlag|ProgrammingCRT2;
  }
  tempal=0x02B;
  if(!(SiS_VBInfo&SetInSlaveMode)){
    	tempal=tempal+3;
  }
  SiS_SetReg1(SiS_P3c4,0x05,0x86);
  tempah = SiS_VCLKData[vclkindex].SR2B;
  SiS_SetReg1(SiS_P3c4,tempal,tempah);
  tempal++;
  tempah = SiS_VCLKData[vclkindex].SR2C;
  SiS_SetReg1(SiS_P3c4,tempal,tempah);
  tempal++;
  SiS_SetReg1(SiS_P3c4,tempal,0x80);
  return;
}

void
SiS_SetDefCRT2ExtRegs(USHORT BaseAddr)
{
  USHORT  temp;

  if(SiS_IF_DEF_LVDS==0) {
    SiS_SetReg1(SiS_Part1Port,0x02,0x40);
    SiS_SetReg1(SiS_Part4Port,0x10,0x80);
    temp=(UCHAR)SiS_GetReg1(SiS_P3c4,0x16);
    temp=temp&0xC3;
    SiS_SetReg1(SiS_P3d4,0x35,temp);
  }
  else {
    SiS_SetReg1(SiS_P3d4,0x32,0x02);
    SiS_SetReg1(SiS_Part1Port,0x02,0x00);
  }
}

#ifdef SIS315H
/*
    for SIS310 O.E.M.
*/
/*
---------------------------------------------------------
   LCDResInfo 1 : 800x600
              2 : 1024x768
              3 : 1280x1024
              4 : 1280x960
              5 : 640x480
              6 : 1600x1200
              7 : 1920x1440
   VESA
   non-VESA
   non-Expanding
---------------------------------------------------------
*/
USHORT
GetLCDPtrIndex (void)
{
   USHORT index;

   index = (SiS_LCDResInfo & 0x0F)-1;
   index *= 3;
   if (SiS_LCDInfo&LCDNonExpanding)
      index += 2;
   else
   {
     if (!(SiS_LCDInfo&LCDVESATiming))
       index++;
   }

  return index;
 }


/*
---------------------------------------------------------
       GetTVPtrIndex()
          return       0 : NTSC Enhanced/Standard
                       1 : NTSC Standard TVSimuMode
                       2 : PAL Enhanced/Standard
                       3 : PAL Standard TVSimuMode
                       4 : HiVision Enhanced/Standard
                       5 : HiVision Standard TVSimuMode
---------------------------------------------------------
*/
USHORT
GetTVPtrIndex(void)
{
  USHORT index;

  index = 0;
  if (SiS_VBInfo&SetPALTV)
    index++;
  if (SiS_VBInfo&SetCRT2ToHiVisionTV)  /* Hivision TV use PAL */
    index++;
  index *= 2;

  if ((SiS_VBInfo&SetInSlaveMode)&&(SiS_SetFlag&TVSimuMode))
    index++;

  return index;
}

void
SetDelayComp(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT BaseAddr,
             ULONG ROMAddr,USHORT ModeNo)
{
  USHORT Part1Port;
  USHORT delay,index;

  if (SiS_VBInfo&SetCRT2ToRAMDAC)
  {
     delay = SiS310_CRT2DelayCompensation1;
     if (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
       delay = SiS310_CRT2DelayCompensation2;
  }
  else
    if (SiS_VBInfo&SetCRT2ToLCD)
    {
       index = GetLCDPtrIndex();
       delay = SiS310_LCDDelayCompensation1[index];
       if (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
         delay = SiS310_LCDDelayCompensation2[index];
    }
    else
    {
       index = GetTVPtrIndex();
       delay = SiS310_TVDelayCompensation1[index];
       if (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
         delay = SiS310_TVDelayCompensation2[index];
    }
  Part1Port=BaseAddr+SIS_CRT2_PORT_04;
  SiS_SetRegANDOR(Part1Port,0x2D,~0x0F,delay);  /* index 2D D[3:0] */
}

/*
*/
void
SetAntiFlicker(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT BaseAddr,
               ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex)
{
  USHORT Part2Port;
  USHORT index,temp;

  Part2Port=BaseAddr+SIS_CRT2_PORT_10;
  temp = GetTVPtrIndex();
  temp = (temp>>1);  /* 0: NTSC, 1 :PAL, 2:HiTV */
  if (ModeNo<=0x13)
  {
    index = SiS_SModeIDTable[ModeIdIndex].VB_StTVFlickerIndex;
  }
  else
  {
    index = SiS_EModeIDTable[ModeIdIndex].VB_ExtTVFlickerIndex;
  }
  temp = SiS310_TVAntiFlick1[temp][index];
  temp  <<= 4;

  SiS_SetRegANDOR(Part2Port,0x0A,~0x70,temp);  /* index 0A D[6:4] */

}

void
SetEdgeEnhance (PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT BaseAddr,
                ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex)
{
  USHORT Part2Port;
  USHORT index,temp;

  Part2Port=BaseAddr+SIS_CRT2_PORT_10;
  temp = GetTVPtrIndex();
  temp = (temp>>1);   /* 0: NTSC, 1 :PAL, 2:HiTV */
  if (ModeNo<=0x13)
  {
    index = SiS_SModeIDTable[ModeIdIndex].VB_StTVEdgeIndex;   /* si+VB_StTVEdgeIndex */
  }
  else
  {
    index = SiS_EModeIDTable[ModeIdIndex].VB_ExtTVEdgeIndex;  /* si+VB_ExtTVEdgeIndex */
  }
  temp = SiS310_TVEdge1[temp][index];
  temp  <<= 5;

  SiS_SetRegANDOR(Part2Port,0x3A,~0xE0,temp);  /* index 0A D[7:5] */

}

void
SetYFilter(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT BaseAddr,
           ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex)
{
  USHORT Part2Port,temp1,temp2;
  USHORT index,temp,i,index1;
  UCHAR  OutputSelect=*pSiS_OutputSelect;
  Part2Port=BaseAddr+SIS_CRT2_PORT_10;
  temp = GetTVPtrIndex();
  temp >>= 1;  /* 0: NTSC, 1 :PAL, 2:HiTV */

  if (ModeNo<=0x13)
  {
    index =  SiS_SModeIDTable[ModeIdIndex].VB_StTVYFilterIndex;
  }
  else
  {
    index =  SiS_EModeIDTable[ModeIdIndex].VB_ExtTVYFilterIndex;
  }

  if (SiS_VBInfo&SetCRT2ToHiVisionTV)  /* Hivision TV use PAL */
   temp = 0;

  /*301b*/
  if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
    for(i=0x35;i<=0x38;i++)
   {
     SiS_SetReg1(Part2Port,i,SiS310_TVYFilter2[temp][index][i-0x35]);
   }
   for(i=0x48;i<=0x4A;i++)
   {
     SiS_SetReg1(Part2Port,i,SiS310_TVYFilter2[temp][index][(i-0x48)+0x04]);
   }
  }
      /*end 301b*/
  else{
   for(i=0x35;i<=0x38;i++)
   {
     SiS_SetReg1(Part2Port,i,SiS310_TVYFilter1[temp][index][i-0x35]);
   }
  }
/*add PALMN*/
    if(OutputSelect&EnablePALMN) {                  
      index1=SiS_GetReg1(SiS_P3d4,0x31);
      temp1=index1&0x01;
      index1=SiS_GetReg1(SiS_P3d4,0x38);
      temp2=index1&0xC0;
      if(temp1){
         if(temp2==0x40){
              if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
                 for(i=0x35;i<=0x38;i++){
                      SiS_SetReg1(Part2Port,i,SiS310_PALMFilter2[index][i-0x35]);
                     }
                 for(i=0x48;i<=0x4A;i++) {
                       SiS_SetReg1(Part2Port,i,SiS310_PALMFilter2[index][(i-0x48)+0x04]);
                     }
                }
              else{ 
                 for(i=0x35;i<=0x38;i++)
                 SiS_SetReg1(Part2Port,i,SiS310_PALMFilter[index][i-0x35]);
                 } 
             }
         if(temp2==0x80){ 
              if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
                 for(i=0x35;i<=0x38;i++){
                      SiS_SetReg1(Part2Port,i,SiS310_PALNFilter2[index][i-0x35]);
                     }
                 for(i=0x48;i<=0x4A;i++) {
                       SiS_SetReg1(Part2Port,i,SiS310_PALNFilter2[index][(i-0x48)+0x04]);
                     }
             }
              else{
                 for(i=0x35;i<=0x38;i++)
                 SiS_SetReg1(Part2Port,i,SiS310_PALNFilter[index][i-0x35]);
                }
          }
      }
    }
    /*end PALMN*/
}

void
SetPhaseIncr(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT BaseAddr,
             ULONG ROMAddr,USHORT ModeNo)
{
  USHORT Part2Port;
  USHORT index,temp,temp1,i;

  Part2Port=BaseAddr+SIS_CRT2_PORT_10;
  temp = GetTVPtrIndex();
  /* 0: NTSC Graphics, 1: NTSC Text, 2 :PAL Graphics, 3 :PAL Text, 4:HiTV Graphics 5:HiTV Text */
  index = temp % 2;
  temp >>= 1;   /* 0: NTSC, 1 :PAL, 2:HiTV */
  temp1=SiS_GetReg1(SiS_P3d4,0x38);        /*if PALMN Not Set*/
  temp1=temp1&0xC0; 
  if(!temp1){
  	for(i=0x31;i<=0x34;i++)
  	{
    		if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
     			SiS_SetReg1(Part2Port,i,SiS310_TVPhaseIncr2[temp][index][i-0x31]);
    		else
     			SiS_SetReg1(Part2Port,i,SiS310_TVPhaseIncr1[temp][index][i-0x31]);
  	}
  }
}

void
SiS_OEM310Setting(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT BaseAddr,
                  ULONG ROMAddr,USHORT ModeNo,USHORT ModeIdIndex)
{
   SetDelayComp(HwDeviceExtension,BaseAddr,ROMAddr,ModeNo);
   if (SiS_VBInfo&SetCRT2ToTV) {
       SetAntiFlicker(HwDeviceExtension,BaseAddr,ROMAddr,ModeNo,ModeIdIndex);
       SetPhaseIncr(HwDeviceExtension,BaseAddr,ROMAddr,ModeNo);
       SetYFilter(HwDeviceExtension,BaseAddr,ROMAddr,ModeNo,ModeIdIndex);
       SetEdgeEnhance(HwDeviceExtension,BaseAddr,ROMAddr,ModeNo,ModeIdIndex);
   }
}

#endif

#ifdef SIS300
/*
    for SIS300 O.E.M.
*/

USHORT
GetRevisionID(PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
   ULONG temp1,base;
   USHORT temp2=0; 
   
   if((HwDeviceExtension->jChipType==SIS_540)||
      (HwDeviceExtension->jChipType==SIS_630)||
      (HwDeviceExtension->jChipType==SIS_730)) {
     	base=0x80000008;
     	OutPortLong(base,0xcf8);
     	temp1=InPortLong(0xcfc);
     	temp1=temp1&0x000000FF;
     	temp2=(USHORT)(temp1);
    	return temp2;
   }
   return 0;
}

USHORT
GetOEMTVPtr(void)
{
  USHORT index;

  index = 0;
  if (!(SiS_VBInfo&SetInSlaveMode))
      	index=index+4;
  
  if (SiS_VBInfo&SetCRT2ToSCART){
      	index=index+2;
  } else {
     	if (SiS_VBInfo&SetCRT2ToHiVisionTV)
       		index=index+3;
     	else {
          	if(SiS_VBInfo&SetPALTV)
          		index=index+1;
        }
  }
  return index;
}

/* TW: This is not for LVDS/Chrontel */
void
SetOEMTVDelay(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT BaseAddr,
              ULONG ROMAddr,USHORT ModeNo)
{
  USHORT Part1Port;
  USHORT index,temp,ModeIdIndex;

  Part1Port=BaseAddr+SIS_CRT2_PORT_04;
  ModeIdIndex=SiS_SearchVBModeID(ROMAddr,ModeNo);
  temp = GetOEMTVPtr();
  index = SiS_VBModeIDTable[ModeIdIndex].VB_TVDelayIndex;
  temp = SiS300_OEMTVDelay[temp][index];
  temp=temp&0x3c;
  SiS_SetRegANDOR(Part1Port,0x13,~0x3C,temp);  /* index 0A D[6:4] */
}

USHORT
GetOEMLCDPtr(PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  /* USHORT temp,tempax; */
  USHORT tempbx=0;

  if(SiS_IF_DEF_LVDS==0) {
#if 0
  	tempax=tempbx=SiS_LCDResInfo;
      	tempbx=tempbx-Panel1024x768;
     	if(!(SiS_SetFlag&LCDVESATiming)) {
        	tempbx+=4;
        	temp=GetRevisionID(HwDeviceExtension);
        	if((HwDeviceExtension->jChipType == SIS_540)&&(temp<1))
        		tempbx+=4;
        	if((HwDeviceExtension->jChipType == SIS_630)&&(temp<3))
        		tempbx+=4;
       	}
     	if((tempax==Panel1024x768)&&(SiS_LCDInfo&LCDNonExpanding)){
        	tempbx=tempbx+3;
       	}
#endif

	/* TW: This is total bullshit. First, we calculate tempbx, now
	 *     we overwrite it?! (Therefore commented code above)
	 */

     	/*add OEMLCDPanelIDSupport*/
        tempbx=SiS_LCDTypeInfo;

#if 0   /* TW: Commented for DEBUG: */
        tempbx=tempbx<<1;
        if(!(SiS_SetFlag&LCDVESATiming))
        	tempbx=tempbx+1;
	tempbx*=2;
	/* TW: The above code results in eg. 0xc * 2 * 2 = 48, but Delay2 table has
	 *     only 32 entries. Thus, this can't work */
#endif
        /* TW: Don't think that 301(B) bridges need different LCD delay... */
  	tempbx = SiS_LCDTypeInfo;
	if(SiS_LCDInfo&LCDNonExpanding){
      		tempbx=tempbx+16;
	}
  } else {  /* TW: Added LVDS code */
  	tempbx = SiS_LCDTypeInfo;
	if(SiS_LCDInfo&LCDNonExpanding){
      		tempbx=tempbx+16;
	}
  }
  return tempbx;
}

#if 0
/* TW: Original code - can't work (eg. exceeds arrays by re-using temp,
       writes to Part2(!)Port unlike TVDelay, etc...) */
void
SetOEMLCDDelay(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT BaseAddr,
               ULONG ROMAddr,USHORT ModeNo)
{
  USHORT Part2Port;
  USHORT index,temp,ModeIdIndex;
  Part2Port=BaseAddr+SIS_CRT2_PORT_10;
  ModeIdIndex=SiS_SearchVBModeID(ROMAddr,ModeNo);
  temp = GetOEMLCDPtr(HwDeviceExtension);
  index = SiS_VBModeIDTable[ModeIdIndex].VB_LCDDelayIndex;
  temp = SiS300_OEMLCDDelay1[temp][index];
   /*add OEMLCDPanelIDSupport*/
  temp = SiS300_OEMLCDDelay2[temp][index];
  temp=temp&0x3c;
  SiS_SetRegANDOR(Part2Port,0x13,~0x3C,temp);  /* index 0A D[6:4] */
}
#endif

/* TW: Although I assume that the code above does not work correctly,
 *     I won't change it for non-LVDS machines until someone tells me so.
 */
void
SetOEMLCDDelay(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT BaseAddr,
               ULONG ROMAddr,USHORT ModeNo)
{
  USHORT Part1Port, Part2Port;
  USHORT index,temp,ModeIdIndex;

  Part1Port=BaseAddr+SIS_CRT2_PORT_04;
  Part2Port=BaseAddr+SIS_CRT2_PORT_10;

  ModeIdIndex=SiS_SearchVBModeID(ROMAddr,ModeNo);
  temp = GetOEMLCDPtr(HwDeviceExtension);
  index = SiS_VBModeIDTable[ModeIdIndex].VB_LCDDelayIndex; /* TW: Always zero... */
  if (SiS_IF_DEF_LVDS == 1) {
  	/* TW: Added for LVDS: Assuming Part_1!!!_Port to be the correct one */
    	temp = SiS300_OEMLCDDelay3[temp][index];
	temp=temp&0x3c;
  	SiS_SetRegANDOR(Part1Port,0x13,~0x3C,temp);  /* index 0A D[6:4] */
  } else {
        /* temp = SiS300_OEMLCDDelay1[temp][index];  */
	/*  TW: This must be wrong: temp is eg. 48 and
		thus exceeds array size of Delay2 table! */

        /*add OEMLCDPanelIDSupport*/

#if 0  /* TW: Commented for DEBUG */
	temp = SiS300_OEMLCDDelay2[temp][index];
	temp=temp&0x3c;
  	SiS_SetRegANDOR(Part2Port,0x13,~0x3C,temp);  /* index 0A D[6:4] */
#endif

        /* TW: 301B reported not working. Maybe because of the above bullshit? */
	/*     Do it like for LVDS now. 0x13 seems to be the LCD delay anyway */
        temp = SiS300_OEMLCDDelay3[temp][index];
	temp=temp&0x3c;
  	SiS_SetRegANDOR(Part1Port,0x13,~0x3C,temp);  /* index 0A D[6:4] */
  }
}

/*
*/
void
SetOEMAntiFlicker(PSIS_HW_DEVICE_INFO HwDeviceExtension,
                  USHORT BaseAddr,ULONG ROMAddr,USHORT ModeNo)
{
  USHORT Part2Port;
  USHORT index,temp;
  USHORT ModeIdIndex;

  Part2Port=BaseAddr+SIS_CRT2_PORT_10;
  ModeIdIndex=SiS_SearchVBModeID(ROMAddr,ModeNo);
  temp = GetOEMTVPtr();
  index = SiS_VBModeIDTable[ModeIdIndex].VB_TVFlickerIndex;
  temp = SiS300_OEMTVFlicker[temp][index];
  temp=temp&0x70;
  SiS_SetRegANDOR(Part2Port,0x0A,~0x70,temp);  /* index 0A D[6:4] */
}

void
SetOEMPhaseIncr(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT BaseAddr,
                ULONG ROMAddr,USHORT ModeNo)
{
  USHORT Part2Port;
  USHORT index,i,ModeIdIndex;

  Part2Port=BaseAddr+SIS_CRT2_PORT_10;
  /* temp = GetTVPtrIndex();*/
  /* 0: NTSC Graphics, 1: NTSC Text, 2 :PAL Graphics, 3 :PAL Text, 4:HiTV Graphics 5:HiTV Text */
  /* index = temp % 2;*/
  /* temp >>= 1;   0: NTSC, 1 :PAL, 2:HiTV */
  ModeIdIndex=SiS_SearchVBModeID(ROMAddr,ModeNo);
  index = SiS_VBModeIDTable[ModeIdIndex].VB_TVPhaseIndex;
  if (SiS_VBInfo&SetInSlaveMode){
      if(SiS_VBInfo&SetPALTV){  
         for(i=0x31;i<=0x34;i++)
        	SiS_SetReg1(Part2Port,i,SiS300_StPALPhase[index][i-0x31]);
      } else {
          for(i=0x31;i<=0x34;i++)
        	SiS_SetReg1(Part2Port,i,SiS300_StNTSCPhase[index][i-0x31]);
      }
      if(SiS_VBInfo&SetCRT2ToSCART){
          for(i=0x31;i<=0x34;i++)
        	SiS_SetReg1(Part2Port,i,SiS300_StSCARTPhase[index][i-0x31]);
      }
  } else {
       if(SiS_VBInfo&SetPALTV){  
          for(i=0x31;i<=0x34;i++)
        	SiS_SetReg1(Part2Port,i,SiS300_ExtPALPhase[index][i-0x31]);
       } else {
          for(i=0x31;i<=0x34;i++)
        	SiS_SetReg1(Part2Port,i,SiS300_ExtNTSCPhase[index][i-0x31]);
       }
       if(SiS_VBInfo&SetCRT2ToSCART){
          for(i=0x31;i<=0x34;i++)
        	SiS_SetReg1(Part2Port,i,SiS300_ExtSCARTPhase[index][i-0x31]);
       }
  }
}

void
SetOEMYFilter(PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT BaseAddr,
              ULONG ROMAddr,USHORT ModeNo)
{
  USHORT Part2Port;
  USHORT index,temp1,temp2,i,ModeIdIndex,index1;
  Part2Port=BaseAddr+SIS_CRT2_PORT_10;
    /*301b*/
  ModeIdIndex=SiS_SearchVBModeID(ROMAddr,ModeNo);
  index = SiS_VBModeIDTable[ModeIdIndex].VB_TVYFilterIndex;
  if (SiS_VBInfo&SetInSlaveMode){
      if(SiS_VBInfo&SetPALTV){  
        	for(i=0x35;i<=0x38;i++)
        		SiS_SetReg1(Part2Port,i,SiS300_StPALFilter[index][i-0x35]);
      } else {
         	 for(i=0x35;i<=0x38;i++)
        		SiS_SetReg1(Part2Port,i,SiS300_StNTSCFilter[index][i-0x35]);
      }
  } else{
      if(SiS_VBInfo&SetPALTV) {
         for(i=0x35;i<=0x38;i++)
        	SiS_SetReg1(Part2Port,i,SiS300_ExtPALFilter[index][i-0x35]);
      } else{
          for(i=0x35;i<=0x38;i++)
        	SiS_SetReg1(Part2Port,i,SiS300_ExtNTSCFilter[index][i-0x35]);
      }
  }

  if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
    if(SiS_VBInfo&SetPALTV){  
    	for(i=0x35;i<=0x38;i++)
    	{
     		SiS_SetReg1(Part2Port,i,SiS300_PALFilter2[index][i-0x35]);
    	}
   	for(i=0x48;i<=0x4A;i++)
    	{
     		SiS_SetReg1(Part2Port,i,SiS300_PALFilter2[index][(i-0x48)+0x04]);
    	}
    } else {
  	for(i=0x35;i<=0x38;i++)
    	{
     		SiS_SetReg1(Part2Port,i,SiS300_NTSCFilter2[index][i-0x35]);
     	}
   	for(i=0x48;i<=0x4A;i++)
    	{
     		SiS_SetReg1(Part2Port,i,SiS300_NTSCFilter2[index][(i-0x48)+0x04]);
    	}
    }
  }

/*add PALMN*/
  if((HwDeviceExtension->jChipType==SIS_630)||
     (HwDeviceExtension->jChipType==SIS_730)) {
      index1=SiS_GetReg1(SiS_P3d4,0x31);
      temp1=index1&0x01;
      index1=SiS_GetReg1(SiS_P3d4,0x35);
      temp2=index1&0xC0;
      if(temp1){
         if(temp2==0x40){
              if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
                 for(i=0x35;i<=0x38;i++){
                      SiS_SetReg1(Part2Port,i,SiS300_PALMFilter2[index][i-0x35]);
                 }
                 for(i=0x48;i<=0x4A;i++) {
                       SiS_SetReg1(Part2Port,i,SiS300_PALMFilter2[index][(i-0x48)+0x04]);
                 }
              } else {
                 for(i=0x35;i<=0x38;i++)
                 	SiS_SetReg1(Part2Port,i,SiS300_PALMFilter[index][i-0x35]);
              }
         }
         if(temp2==0x80){ 
              if(SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV)) {
                 for(i=0x35;i<=0x38;i++){
                      SiS_SetReg1(Part2Port,i,SiS300_PALNFilter2[index][i-0x35]);
                 }
                 for(i=0x48;i<=0x4A;i++) {
                       SiS_SetReg1(Part2Port,i,SiS300_PALNFilter2[index][(i-0x48)+0x04]);
                 }
             } else {
                 for(i=0x35;i<=0x38;i++)
                 	SiS_SetReg1(Part2Port,i,SiS300_PALNFilter[index][i-0x35]);
             }
          }
      }
   }
  /*end PALMN*/
}

void
SiS_OEM300Setting(PSIS_HW_DEVICE_INFO HwDeviceExtension,
		  USHORT BaseAddr,ULONG ROMAddr,USHORT ModeNo)
{
  if (SiS_VBInfo & SetCRT2ToLCD) {
       SetOEMLCDDelay(HwDeviceExtension,BaseAddr,ROMAddr,ModeNo);
  }
  if (SiS_VBInfo & SetCRT2ToTV) {
       /* TW: (Added LVDS check) - done this way now because LVDS
        * seems to be 1 for some of the SiS bridges as well
	*/
       if( (SiS_VBType&(VB_SIS301B|VB_SIS302B|VB_SIS301LV|VB_SIS302LV))
       						|| (SiS_IF_DEF_LVDS==0))  {
	        SetOEMTVDelay(HwDeviceExtension,BaseAddr,ROMAddr,ModeNo);
       		SetOEMAntiFlicker(HwDeviceExtension,BaseAddr,ROMAddr,ModeNo);
    		/* SetOEMPhaseIncr(HwDeviceExtension,BaseAddr,ROMAddr,ModeNo);*/
       		SetOEMYFilter(HwDeviceExtension,BaseAddr,ROMAddr,ModeNo);
       }
    }
}
#endif
