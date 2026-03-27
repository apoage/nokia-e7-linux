# Nokia E7 ROM Dump — Hardware Driver Strings

Extracted from romdumpplus.dmp (31,793,152 bytes)
SoC: Broadcom BCM2727B1 (ARM1176JZF-S)
Total strings found: 40985

## SoC (154 matches)
```
569:  cc328 EHwClkMeSSI
2494: 22e540 bcm2727b1
2902: 28589c FSysVchi Segments
3206: 2b16a0 isp_tuner_brcm_dpf_read_bitfield: missing or bad read_info
3207: 2b16dc isp_tuner_brcm_dpf_read_bitfield: illegal read_info->nbytes
3208: 2b17c4 isp_tuner_brcm_dpf_read_enum: missing or bad read_info
3209: 2b195c isp_tuner_brcm_dpf_read_pwl_func: missing or bad read_info
3210: 2b1998 isp_tuner_brcm_dpf_read_pwl_func: missing or bad PWL function
3212: 2b1a88 isp_tuner_brcm_dpf_read_pwl_func_dummy: missing or bad read_info
3214: 2b1d88 isp_tuner_brcm_dpf_read_syntax: Unrecognised keyword.
3215: 2b1dc0 isp_tuner_brcm_dpf_read: unable to read token for no clear reason
3216: 2b1e04 isp_tuner_brcm_dpf_read_syntax: token missing or does not match syntax type
3217: 2b1e50 isp_tuner_brcm_dpf_read: Could not find the zero-length keyword in the handler table.
3218: 2b1ea8 isp_tuner_brcm_dpf_read_syntax: Could not find the zero-length keyword in the syntax table
3219: 2b21d4 isp_tuner_brcm_dpf_test_isp_version: Unable to read comparator.
3220: 2b2214 isp_tuner_brcm_dpf_test_isp_version: Unable to read version number.
3281: 2ba158 mphi_slave
3282: 2ba50c MPHI_BUFFER
3285: 2baa7c MPHI HISR0
3286: 2baa88 MPHI HISR1
3287: 2baa94 MPHI HISR2
3288: 2baaa0 ../../../../interface/vchi/message_drivers/videocore/mphi_ccp2.c
3294: 2bcf38 ../../../../applications/vmcs/vchi/otpburn.c
3313: 2bfb48 ../../../../applications/vmcs/vchi/gce_rpc/resource_handlers.c
3489: 2d70f4 ../../../../applications/vmcs/vchi/tvout.c
3568: 2e480c ../../../../applications/ive/vcam/vcamservice_vchi.c
3602: 2e79e4 vchi:bulk_aux_info
3604: 2e7d7c vchi:control_info
4039: 304b1a logging_vchi.vll
4077: 317448 videocore3
4379: 319601 isp_tuner_brcm_dpf_handle_do_nothing
4380: 319626 isp_tuner_brcm_dpf_handle_include
4381: 319648 isp_tuner_brcm_dpf_handle_mode
4382: 319667 isp_tuner_brcm_dpf_handle_profile
4383: 319689 isp_tuner_brcm_dpf_read_bitfield
4384: 3196aa isp_tuner_brcm_dpf_read_dummy
4385: 3196c8 isp_tuner_brcm_dpf_read_enum
4386: 3196e5 isp_tuner_brcm_dpf_read_float
4387: 319703 isp_tuner_brcm_dpf_read_int32
4388: 319721 isp_tuner_brcm_dpf_read_int8
4389: 31973e isp_tuner_brcm_dpf_read_pwl_func
4390: 31975f isp_tuner_brcm_dpf_read_pwl_func_dummy
4391: 319786 isp_tuner_brcm_dpf_read_sint10p6
4392: 3197a7 isp_tuner_brcm_dpf_read_sint20p12
4393: 3197c9 isp_tuner_brcm_dpf_read_sint24p8
4394: 3197ea isp_tuner_brcm_dpf_read_sint4p12
4395: 31980b isp_tuner_brcm_dpf_read_sint6p10
4396: 31982c isp_tuner_brcm_dpf_read_syntax
4397: 31984b isp_tuner_brcm_dpf_read_token
4398: 319869 isp_tuner_brcm_dpf_read_uint0p16
4399: 31988a isp_tuner_brcm_dpf_read_uint16
4400: 3198a9 isp_tuner_brcm_dpf_read_uint16p16
4401: 3198cb isp_tuner_brcm_dpf_read_uint24p8
4402: 3198ec isp_tuner_brcm_dpf_read_uint32
4403: 31990b isp_tuner_brcm_dpf_read_uint4p12
4404: 31992c isp_tuner_brcm_dpf_read_uint8
4405: 31994a isp_tuner_brcm_dpf_read_uint8p8
4406: 31996a isp_tuner_brcm_dpf_test_isp_version
4539: 31a3a6 vchi_audiopipe_init
4543: 31a3e8 vchi_dispcontrol_server_init
4547: 31a43a fsysvchi_server_init
4549: 31a45b vchi_dispservice_x_init
4551: 31a480 vchi_hostrmi_init
4557: 31a4e1 vchi_egl_gce_server_init
4558: 31a4fa logging_vchi.vll
4559: 31a50b logging_vchi_listener_init
4561: 31a532 vchi_otpburn_init
4567: 31a593 vchi_tvout_server_init
4569: 31a5b3 vchi_vcamservice_server_init
4591: 328a48 /Z:/vlls/bcm2727b1/
4595: 329948 Failed to create Messi DfcQ
4640: 32dbc0 DVCDriver::BootParseMessage Unrecognised VideoCore response
4642: 32de24 DVCDriver VideoCore image ID not found
4643: 32de4c DVCDriver VideoCore image ID found
4671: 330d2c VCDriverMeSSIDfcQ
4676: 330dc4 Y:/ncp_sw/core7.0/IVE3_Engine/IVE3_rapu_drivers/interface/vcdriver/src/messi/controller_messi.cpp
4678: 330e60 DVCDriver::Boot() VideoCore boot ROM not responding (stage 1); HAT=%d, iMessiReadDfc.Queued()=%d
4679: 330ec4 DVCDriver::Boot() VideoCore boot ROM not responding (stage 2); HAT=%d, iMessiReadDfc.Queued()=%d
4687: 331075 15DVCMessiChannel
4688: 331087 16TControllerMessi
4689: 33109a 17DVCMessiRxChannel
4690: 3310ae 17DVCMessiTxChannel
4704: 331c44 BulkRxRequested[Messi]
4710: 331cdc BulkRxDataArrived[Messi]
4724: 333dbc VCHI RX Message too short
4725: 333dd8 VCHI RX Bad sync value
4726: 333df0 VCHI RX Declared message length too big
4729: 334d20 VCHI: Bogus BULX info
4735: 335214 VCHI: MeSSI-16 CRC failure
4736: 335230 VCHI: CCP2 CRC failure
4737: 335ee8 Failed to create VCHI
4743: 336ae4 VCHIDfcQ
4745: 337174 VCHI peer slot size mismatch
4746: 337196 @BDVCHI RX inconsistency (callback for unexpected slot)
4747: 3371d0 DVCHI::CcpCallback iCcpOpened already clear
4748: 3371fc DVCHI data RX inconsistency (callback for unexpected slot)
4749: 3375c0 DVCHI::IssueTxCallbacks: iMsgData=NULL
4750: 3375e8 DVCHI RX inconsistency (truncated control message)
4751: 3379b8 VCHI: VCDriver enable failed (%d)
4755: 33919d N4VCHI12DVCHIServiceE
```
*...truncated, 154 total matches*

## I2C (24 matches)
```
563:  cc2c0 EHwClkI2C0
564:  cc2d0 EHwClkI2C1
565:  cc2e0 EHwClkI2C2
2129: 1cd7c4 Failed to instantiate I2C Power Handler
2130: 1ce3f8 I2CThread_0
2131: 1ce408 I2CRequestLock
2140: 1ce490 I2CPowerHandler0
2141: 1ce4a4 DI2CPowerHandler
2142: 1ce4d8 16DI2CPowerHandler
2238: 1e4a60 18MTestableExtWithIF
2906: 286d20 force_i2c_read
3386: 2c824c force_i2c_read
3388: 2ca1a8 force_i2c_read
4328: 319247 i2c_get_func_table
5190: 36f614 TwIf                    
5915: 40bf28 DCamUseCase::ReturnBuffer() Your code is returning buffers incorrectly,  maybe the same buffer has been given back twice? Consider also the use of RCam::SendInputData which implicity returns the buffer.
12944: 8b1b24 24CAtpsTwistStatusListener
12946: 8b1b5a 24MAtpsTwistStatusObserver
15487: a910bc CREATE TABLE ThumbnailConfig (ImageGridWidth INTEGER,ImageGridHeight INTEGER,VideoGridWidth INTEGER,VideoGridHeight INTEGER,AudioGridWidth INTEGER,AudioGridHeight INTEGER,ContactGridWidth INTEGER,ContactGridHeight INTEGER,ImageListWidth INTEGER,ImageListHeight INTEGER,VideoListWidth INTEGER,VideoListHeight INTEGER,AudioListWidth INTEGER,AudioListHeight INTEGER,ContactListWidth INTEGER,ContactListHeight INTEGER,ImageFullscreenWidth INTEGER,ImageFullscreenHeight INTEGER,VideoFullscreenWidth INTEGER,VideoFullscreenHeight INTEGER,AudioFullscreenWidth INTEGER,AudioFullscreenHeight INTEGER,ContactFullscreenWidth INTEGER,ContactFullscreenHeight INTEGER);
15488: a91758 INSERT INTO ThumbnailConfig (ImageGridWidth,ImageGridHeight,VideoGridWidth,VideoGridHeight,AudioGridWidth,AudioGridHeight,ContactGridWidth,ContactGridHeight,ImageListWidth,ImageListHeight,VideoListWidth,VideoListHeight,AudioListWidth,AudioListHeight,ContactListWidth,ContactListHeight,ImageFullscreenWidth,ImageFullscreenHeight,VideoFullscreenWidth,VideoFullscreenHeight,AudioFullscreenWidth,AudioFullscreenHeight,ContactFullscreenWidth,ContactFullscreenHeight) VALUES (:ImageGridWidth,:ImageGridHeight,:VideoGridWidth,:VideoGridHeight,:AudioGridWidth,:AudioGridHeight,:ContactGridWidth,:ContactGridHeight,:ImageListWidth,:ImageListHeight,:VideoListWidth,:VideoListHeight,:AudioListWidth,:AudioListHeight,:ContactListWidth,:ContactListHeight,:ImageFullscreenWidth,:ImageFullscreenHeight,:VideoFullscreenWidth,:VideoFullscreenHeight,:AudioFullscreenWidth,:AudioFullscreenHeight,:ContactFullscreenWidth,:ContactFullscreenHeight);
16067: acc41b N17CprBindToActivity18CCprBindToActivity41TSendControlClientJoinRequestWithPriorityE
29032:14a29df 24CEditorPlainTextWithUndo
37392:1b38a00 gepmCEDADOCDa4i2ci10cat4c2a4s
37393:1b38d70 a4i2ci10c3as2c3i
```

## SPI (125 matches)
```
574:  cc378 EHwClkMcuSpi
579:  cc3e4 EHwClkDspSpi0
580:  cc3f8 EHwClkDspSpi1
682:  ea724 Spi1SpiDriver Thread
683:  ea740 Spi1Lock
684:  ea750 Spi1Power
685:  ea948 10DSpiCommon
686:  ea955 10DSpiDmaRap
687:  ea962 11TSpiRequest
688:  ea970 12DSpiPowerRap
689:  ea97f 17TSpiChannelConfig
692:  ea9d3 9DSpiHwRap
693:  ea9de 9DSpiPower
694:  ecc84 Spi2SpiDriver Thread
695:  ecca0 Spi2Lock
696:  eccb0 Spi2Power
697:  ecea8 10DSpiCommon
698:  eceb5 10DSpiDmaRap
699:  ecec2 11TSpiRequest
700:  eced0 12DSpiPowerRap
701:  ecedf 17TSpiChannelConfig
704:  ecf33 9DSpiHwRap
705:  ecf3e 9DSpiPower
706:  ef1f8 Spi3SpiDriver Thread
707:  ef214 Spi3Lock
708:  ef224 Spi3Power
709:  ef41c 10DSpiCommon
710:  ef429 10DSpiDmaRap
711:  ef436 11TSpiRequest
712:  ef444 12DSpiPowerRap
713:  ef453 17TSpiChannelConfig
716:  ef4a7 9DSpiHwRap
717:  ef4b2 9DSpiPower
1716: 1903c9 10MUsbAsspIf
1744: 1905b2 18MUsbAsspInternalIf
1767: 1907d1 24MUsbAsspInterruptHandler
4935: 353fc0 Checked by Hal function EDisplayHalGetStateSpinner
5026: 369d90 Y:/ext/adapt/wlan.nokia/bsp_specific/spia/src/am_spia.cpp
5193: 36f668 WspiBusDrv              
5208: 36f810 spi                     
5209: 36f82c spiclient               
5222: 384a50  SpiClient: Request status == SPIA::EFailure
5223: 384a80 SpiClient: Request status unknown
5311: 3c8130  SpiClient::Request iNumPendingRequests  >= MAX_PENDING_REQUESTS 
5318: 3c848b 11TSpiRequest
5328: 3c8553 15MWlanSpiaClient
5331: 3c858c 17TSpiChannelConfig
5343: 3c868f 8WlanSpia
5344: 3c8699 9SpiClient
8054: 5fb906 10CSpiPlugin
10311: 73adcd N9CryptoSpi13MKeyAgreementE
10312: 73ade9 N9CryptoSpi14MSignatureBaseE
10313: 73ae06 N9CryptoSpi16MSymmetricCipherE
10314: 73ae25 N9CryptoSpi17MAsymmetricCipherE
10315: 73ae45 N9CryptoSpi17MKeyPairGeneratorE
10316: 73ae65 N9CryptoSpi20MSymmetricCipherBaseE
10317: 73ae88 N9CryptoSpi21MAsymmetricCipherBaseE
10318: 73aeac N9CryptoSpi4MMacE
10319: 73aebe N9CryptoSpi5MHashE
10320: 73aed1 N9CryptoSpi7MPluginE
10321: 73aee6 N9CryptoSpi7MRandomE
10322: 73aefb N9CryptoSpi7MSignerE
10323: 73af10 N9CryptoSpi9MVerifierE
10493: 756384 N9CryptoSpi10CAsyncHashE
10494: 75639d N9CryptoSpi11CCryptoBaseE
10495: 7563b7 N9CryptoSpi12CAsyncRandomE
10496: 7563d2 N9CryptoSpi12CAsyncSignerE
10497: 7563ed N9CryptoSpi12CCryptoParamE
10498: 756408 N9CryptoSpi13CCryptoParamsE
10499: 756424 N9CryptoSpi13CKeyAgreementE
10500: 756440 N9CryptoSpi13CRuleSelectorE
10501: 75645c N9CryptoSpi14CAsyncVerifierE
10502: 756479 N9CryptoSpi14CSignatureBaseE
10503: 756496 N9CryptoSpi15CCryptoIntParamE
10504: 7564b4 N9CryptoSpi15CLegacySelectorE
10505: 7564d2 N9CryptoSpi15CSelectionRulesE
10506: 7564f0 N9CryptoSpi15MPluginSelectorE
10507: 75650e N9CryptoSpi16CCharacteristicsE
10508: 75652d N9CryptoSpi16CSymmetricCipherE
10509: 75654c N9CryptoSpi17CAsymmetricCipherE
10510: 75656c N9CryptoSpi17CCryptoDesC8ParamE
10511: 75658c N9CryptoSpi17CKeyPairGeneratorE
10512: 7565ac N9CryptoSpi18CAsyncKeyAgreementE
10513: 7565cd N9CryptoSpi18CCryptoBigIntParamE
10514: 7565ee N9CryptoSpi18CCryptoDesC16ParamE
10515: 75660f N9CryptoSpi19CMacCharacteristicsE
10516: 756631 N9CryptoSpi20CHashCharacteristicsE
10517: 756654 N9CryptoSpi20CSymmetricCipherBaseE
10518: 756677 N9CryptoSpi21CAsymmetricCipherBaseE
10519: 75669b N9CryptoSpi21CAsyncSymmetricCipherE
10520: 7566bf N9CryptoSpi21CSelectionRuleContentE
10521: 7566e3 N9CryptoSpi22CAsyncAsymmetricCipherE
10522: 756708 N9CryptoSpi22CAsyncKeyPairGeneratorE
10523: 75672d N9CryptoSpi22CRandomCharacteristicsE
10524: 756752 N9CryptoSpi24CExtendedCharacteristicsE
10525: 756779 N9CryptoSpi28CKeyAgreementCharacteristicsE
10526: 7567a4 N9CryptoSpi29CCharacteristicsAndPluginNameE
10527: 7567d0 N9CryptoSpi31CSymmetricCipherCharacteristicsE
10528: 7567fe N9CryptoSpi32CAsymmetricCipherCharacteristicsE
10529: 75682d N9CryptoSpi32CKeypairGeneratorCharacteristicsE
```
*...truncated, 125 total matches*

## UART (85 matches)
```
7451: 524a84 13CHCTLUartBase
7454: 524abb 21CHCTLUartPowerManager
8552: 656682 7CSerial
9261: 6d3476 N11MeshMachine22TSerializableStateForkINS_12TClientMutexIN8Messages16TTypeMatchPolicyINS2_12TMatchPolicyINS2_18TSubSetMatchPolicyENS2_20TSuperSetMatchPolicyEEENS4_IS6_NS2_15TAnyMatchPolicyEEEEELi256ELi2048EEEN13CoreNetStates25TNoTagOrDataClientsToStopENS_24TStateErrorTransitionTagILi268435451EEEEE
9262: 6d35a1 N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi107EEENS_6TNoTagENS_24TStateErrorTransitionTagILi268435451EEEEE
9263: 6d3620 N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi11EEENS_14TActiveOrNoTagILi7EEENS_24TStateErrorTransitionTagILi268435451EEEEE
9264: 6d36ad N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi34EEEN13SubConnStates21TNoTagOrParamsPresentENS_24TStateErrorTransitionTagILi268435451EEEEE
9265: 6d3748 N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi4EEENS_14TActiveOrNoTagILi8EEENS_24TStateErrorTransitionTagILi268435451EEEEE
9266: 6d37d4 N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi9EEEN13CoreNetStates21TNoTagOrBearerPresentENS_24TStateErrorTransitionTagILi268435451EEEEE
9267: 6d386e N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi9EEENS_6TNoTagENS_24TStateErrorTransitionTagILi268435451EEEEE
9268: 6d38eb N11MeshMachine22TSerializableStateForkINS_18TActivitiesIdMutexILi7ELi99ELi0ELi0ELi0EEEN13CoreNetStates16TNoTagOrNoBearerENS_24TStateErrorTransitionTagILi268435451EEEEE
9269: 6d3993 N11MeshMachine22TSerializableStateForkINS_18TActivitiesIdMutexILi7ELi99ELi0ELi0ELi0EEENS_6TNoTagENS_24TStateErrorTransitionTagILi268435451EEEEE
9270: 6d3a23 N11MeshMachine22TSerializableStateForkINS_18TActivitiesIdMutexILi9ELi11ELi0ELi0ELi0EEEN10ConnStates20TStartOrAttachActiveENS_24TStateErrorTransitionTagILi268435451EEEEE
9271: 6d3acc N11MeshMachine22TSerializableStateForkINS_19TAggregatedMutex_ORINS1_INS_18TActivitiesIdMutexILi9ELi10ELi12ELi11ELi115EEENS_12TClientMutexIN8Messages16TTypeMatchPolicyINS5_12TMatchPolicyINS5_18TSubSetMatchPolicyENS5_20TSuperSetMatchPolicyEEENS7_IS9_NS5_15TAnyMatchPolicyEEEEELi256ELi1EEEEENS_16TActivityIdMutexILi6EEEEENS_6TNoTagENS_24TStateErrorTransitionTagILi268435451EEEEE
9272: 6d3c44 N11MeshMachine23TSerializableTransitionINS_16TActivityIdMutexILi9EEEN10ConnStates14TParseECNStartEEE
9273: 6d3ca9 N11MeshMachine23TSerializableTransitionINS_16TActivityIdMutexILi9EEENS_21TAggregatedTransitionINS3_INS3_IN10ConnStates30TErrorIfAlreadyStartedAttachedENS4_15TParseECNAttachENS_12TNodeContextIN5ESock11CConnectionENS7_INS8_17CMMSockSubSessionENS7_INS8_13ACFMMNodeBaseENS7_INS_11AMMNodeBaseENS_16TNodeContextBaseENS_17CNodeActivityBaseEEESE_EESE_EESE_EEEENS4_19TClearProgressQueueESI_EENS4_19TRequestCSRCreationESI_EEEE
9274: 6d3e4a N11MeshMachine23TSerializableTransitionINS_18TActivitiesIdMutexILi7ELi99ELi0ELi0ELi0EEEN10ConnStates26TRequestIncomingConnectionEEE
11056: 7a7f80 KBcaOptLevelExtSerial
11063: 7a8044 KBcaOptNameSerialPortName
11064: 7a8064 KBcaOptNameSerialSerialConfig
11065: 7a8088 KBcaOptNameSerialSerialSetConfig
11068: 7a80ec KSerialSetTxRxBufferSize
11069: 7a810c KSerialTxRxBufferSize
11070: 7a8128 KSerialMonitorControlLines
11071: 7a8148 KSerialSetControlLines
11129: 7b14c4 Serial::1
11130: 7b14d4 Serial::2
11138: 7b164c #Unable to load the LDD or PDD required for serial. Err code: %d
11139: 7b1694 #Unable to open the serial port. Err code: %d
11140: 7b16c8 #Could not set serial port configuration after opening port. Err code: %d
11146: 7b191a 13CSerialWriter
11755: 7fc964 13CHCTLUartPort
11757: 7fc985 15CHCTLUartSender
11759: 7fc9aa 17CHCTLUartReceiver
11763: 7fc9ff 19MUartSenderObserver
11765: 7fca2b 21CUartSupervisionTimer
11769: 7fca90 29MUartSupervisionTimerObserver
11770: 7fcab0 9CHCTLUart
15110: a5cdd9 11CSerialDesc
15115: a5ce33 12CSerialTimer
15973: aca58b N11MeshMachine22TSerializableStateForkIN12PRActivities20CCommsBinderActivity16TDataClientMutexENS2_19TNoTagOrUseExistingENS_24TStateErrorTransitionTagILi268435451EEEEE
15974: aca633 N11MeshMachine22TSerializableStateForkIN12PRActivities20CCommsBinderActivity23TDefaultDataClientMutexENS2_43TNoTagOrWaitForIncomingOrUseExistingDefaultENS_24TStateErrorTransitionTagILi268435451EEEEE
15975: aca6fa N11MeshMachine22TSerializableStateForkIN12PRActivities9CNoBearer21TServiceProviderMutexENS2_21TNoTagOrBearerPresentENS_24TStateErrorTransitionTagILi268435451EEEEE
15976: aca79d N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi4EEENS_14TActiveOrNoTagILi8EEENS_24TStateErrorTransitionTagILi268435451EEEEE
15977: aca829 N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi9EEEN13CoreNetStates21TNoTagOrBearerPresentENS_24TStateErrorTransitionTagILi268435451EEEEE
16205: ad90d6 N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi7EEEN24PRDataClientStopActivity23TNoTagOrProviderStoppedENS_24TStateErrorTransitionTagILi268435451EEEEE
16206: ad917d N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi9EEENS_6TNoTagENS_24TStateErrorTransitionTagILi268435451EEEEE
16207: ad91fa N11MeshMachine22TSerializableStateForkINS_19TAggregatedMutex_ORINS1_INS_18TActivitiesIdMutexILi9ELi10ELi12ELi11ELi115EEENS_12TClientMutexIN8Messages16TTypeMatchPolicyINS5_12TMatchPolicyINS5_18TSubSetMatchPolicyENS5_20TSuperSetMatchPolicyEEENS7_IS9_NS5_15TAnyMatchPolicyEEEEELi256ELi1EEEEENS_16TActivityIdMutexILi6EEEEENS_6TNoTagENS_24TStateErrorTransitionTagILi268435451EEEEE
17832: beb5b8 CCamImgScaler422QuarterQuality::CCamImgScaler422QuarterQuality
17833: bebb9c CCamImgScaler422QuarterFast::CCamImgScaler422QuarterFast
17841: beee34 CCamImgScalerQuarterQuality::CCamImgScalerQuarterQuality
17842: bef40c CCamImgScalerQuarterFast::CCamImgScalerQuarterFast
17880: bf6661 24CCamImgScalerQuarterFast
17886: bf670f 27CCamImgScaler422QuarterFast
17887: bf672d 27CCamImgScalerQuarterQuality
17900: bf68ce 30CCamImgScaler422QuarterQuality
20013: cb5738 Get production serial number
21044: d6210d CSYList=ECUART
21810: db69eb N11MeshMachine22TSerializableStateForkIN18MobilityMCprStates25THandshakingMobilityMutexENS_6TNoTagENS_24TStateErrorTransitionTagILi268435451EEEEE
21811: db6a7d N11MeshMachine22TSerializableStateForkIN21MobilityCprActivities17CMobilityActivity28TMobilityClientNotReadyMutexENS2_40TNoTagBackwardsOrRecoverableErrorOrErrorENS_24TStateErrorTransitionTagILi268435451EEEEE
21812: db6b4c N11MeshMachine22TSerializableStateForkIN21MobilityCprActivities17CMobilityActivity28TMobilityClientNotReadyMutexENS_11TErrorTagOrINS_4TTagILi10002EEEEENS_24TStateErrorTransitionTagILi268435451EEEEE
21813: db6c12 N11MeshMachine22TSerializableStateForkIN21MobilityCprActivities17CMobilityActivity28TMobilityClientNotReadyMutexENS_16TNoTagOrErrorTagENS_24TStateErrorTransitionTagILi268435451EEEEE
21814: db6cc8 N11MeshMachine22TSerializableStateForkINS_18TActivitiesIdMutexILi38ELi39ELi0ELi0ELi0EEEN22MobilityMCprActivities17CMobilityActivity21TNoTagOrAwaitMobilityENS_24TStateErrorTransitionTagILi268435451EEEEE
21815: db6d92 N11MeshMachine22TSerializableStateForkINS_18TActivitiesIdMutexILi38ELi39ELi0ELi0ELi0EEEN22MobilityMCprActivities17CMobilityActivity47TNoTagOrAwaitMobilityBackwardsOnMobilityTriggerENS_24TStateErrorTransitionTagILi268435451EEEEE
22694: e1ec20 N11MeshMachine22TSerializableStateForkIN12PRActivities20CCommsBinderActivity23TDefaultDataClientMutexENS2_43TNoTagOrWaitForIncomingOrUseExistingDefaultENS_24TStateErrorTransitionTagILi268435451EEEEE
22695: e1ece7 N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi4EEENS_14TActiveOrNoTagILi8EEENS_24TStateErrorTransitionTagILi268435451EEEEE
22696: e1ed73 N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi9EEEN13CoreNetStates21TNoTagOrBearerPresentENS_24TStateErrorTransitionTagILi268435451EEEEE
22928: e34448 N11MeshMachine22TSerializableStateForkIN12PRActivities9CNoBearer21TServiceProviderMutexENS2_21TNoTagOrBearerPresentENS_24TStateErrorTransitionTagILi268435451EEEEE
22929: e344eb N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi1EEENS_6TNoTagENS_24TStateErrorTransitionTagILi268435451EEEEE
22930: e34568 N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi4EEENS0_INS1_ILi9EEEN13CoreNetStates21TNoTagOrBearerPresentENS_24TStateErrorTransitionTagILi268435451EEEEES7_EE
22931: e34617 N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi7EEENS_6TNoTagENS_24TStateErrorTransitionTagILi268435451EEEEE
22932: e34694 N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi9EEEN13CoreNetStates21TNoTagOrBearerPresentENS_24TStateErrorTransitionTagILi268435451EEEEE
23232: e4e195 N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi7EEEN24PRDataClientStopActivity23TNoTagOrProviderStoppedENS_24TStateErrorTransitionTagILi268435451EEEEE
23233: e4e23c N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi9EEEN13CoreNetStates21TNoTagOrBearerPresentENS_24TStateErrorTransitionTagILi268435451EEEEE
23234: e4e2d6 N11MeshMachine22TSerializableStateForkINS_20TAggregatedMutex_ANDINS_16TActivityIdMutexILi4EEENS_14TNoClientMutexIN8Messages16TTypeMatchPolicyINS5_12TMatchPolicyINS5_18TSubSetMatchPolicyENS5_20TSuperSetMatchPolicyEEENS7_IS9_NS5_15TAnyMatchPolicyEEEEELi4864ELi0EEEEENS_6TNoTagENS_24TStateErrorTransitionTagILi268435451EEEEE
23407: e5a585 N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi1EEENS_6TNoTagENS_24TStateErrorTransitionTagILi268435451EEEEE
23408: e5a602 N11MeshMachine22TSerializableStateForkINS_16TActivityIdMutexILi227EEENS_6TNoTagENS_24TStateErrorTransitionTagILi268435451EEEEE
23409: e5a681 N11MeshMachine22TSerializableStateForkINS_18TActivitiesIdMutexILi227ELi127ELi0ELi0ELi0EEEN15QoSIpSCprStates25TNoTagOrSendApplyResponseENS_24TStateErrorTransitionTagILi268435451EEEEE
23410: e5a737 N11MeshMachine22TSerializableStateForkINS_18TActivitiesIdMutexILi9ELi10ELi12ELi4ELi0EEENS_6TNoTagENS_24TStateErrorTransitionTagILi268435451EEEEE
30922:1613e44 Serial Number: %S
32972:16fa938 Z:/PacketVideo_PacketVideoEngine_2011_wk19_9.2/PacketVideoEngine/adapters/common/symbian/comms/serial/Inc/pv_rcomm_adapters.h
35156:1993ca1 20CCMSX509IssuerSerial
35161:1993d1d 25CCMSIssuerAndSerialNumber
37167:1aeb2c0 30CMdEQueryCriteriaSerialization
37171:1aee550 23CMdCSerializationBuffer
```

## USB (555 matches)
```
461:  bce74 UsbExtension
462:  bce88 UsbDriver
477:  bcfcc UsbDataIf
488:  bf20c UsbExtension
489:  bf220 UsbDriver
504:  bf364 UsbDataIf
561:  cc29c EHwClkDomHSUSB
562:  cc2b0 EHwClkHSUSB
875: 107630 UsbExtension
876: 107644 UsbDriver
891: 107788 UsbDataIf
900: 107890 UsbExtension
901: 1078a4 UsbDriver
916: 1079e8 UsbDataIf
924: 107ad4 UsbExtension
925: 107ae8 UsbDriver
940: 107c2c UsbDataIf
947: 107d08 UsbExtension
948: 107d1c UsbDriver
963: 107e60 UsbDataIf
982: 109a5c UsbExtension
983: 109a70 UsbDriver
998: 109bb4 UsbDataIf
1028: 130dc4 UsbExtension
1029: 130dd8 UsbDriver
1044: 130f1c UsbDataIf
1342: 163ecc UsbExtension
1343: 163ee0 UsbDriver
1358: 164024 UsbDataIf
1365: 164100 UsbExtension
1366: 164114 UsbDriver
1381: 164258 UsbDataIf
1388: 164334 UsbExtension
1389: 164348 UsbDriver
1404: 16448c UsbDataIf
1411: 164568 UsbExtension
1412: 16457c UsbDriver
1427: 1646c0 UsbDataIf
1434: 16479c UsbExtension
1435: 1647b0 UsbDriver
1450: 1648f4 UsbDataIf
1457: 1649f8 UsbExtension
1458: 164a0c UsbDriver
1473: 164b50 UsbDataIf
1484: 164ce8 UsbExtension
1485: 164cfc UsbDriver
1500: 164e40 UsbDataIf
1507: 164f2c UsbExtension
1508: 164f40 UsbDriver
1523: 165084 UsbDataIf
1530: 165194 UsbExtension
1531: 1651a8 UsbDriver
1546: 1652ec UsbDataIf
1585: 16b7b8 Checked by USBC.LDD (USB Driver)
1586: 16cad8 USB LDD KILL
1587: 16cb10 USB LDD KILL
1588: 16cb58 14DUsbcLogDevice
1589: 16cb69 15DLddUsbcChannel
1590: 16ecd8 TUsbcScEndpoint::RequestCallback
1591: 16ed0c DLddUsbcScChannel::RequestCallbackEp0
1592: 16f944 TUsbcScStatusList::Destroy
1593: 16ffd0 DLddUsbcScChannel::SetupEp0
1594: 16fff4 DLddUsbcScChannel::RealizeInterface
1595: 1728b0 Checked by USBCSC.LDD (USB Driver)
1596: 172e00 TUsbcScEndpoint::SetBuffer
1597: 172f64 USB LDD KILL
1598: 172f84 UsbcscChunk-
1599: 172fac 16DUsbcScLogDevice
1600: 172fbf 17DLddUsbcScChannel
1601: 1744e4 > DUsbGenericController::DumpRegisters()
1602: 174514 MUSBMHDRC common registers:
1607: 1745c4   KRoIntrUsbE           = 0x%02x
1610: 174630 MUSBMHDRC OTG, DynFIFO + Version registers:
1637: 174ae8 < DUsbGenericController::DumpRegisters()
1638: 17b014 USB PSL: Unexpected test mode selector
1639: 186410 USB PSL: NULL inter-thread env ptr
1640: 186434 USB PSL: Trying to make inter-thread call from target context
1641: 186484 USB PSL: uninitialized inter-thread env
1643: 18f8f4 UsbExtension
1644: 18f908 UsbDriver
1659: 18fa4c UsbDataIf
1665: 18fac8 USB PDD helper
1666: 18fad8 USB PSL: DUsbIdVbusStateMachine::RunStateMachine(), unknown state
1668: 18fb8c UsbExtension
1669: 18fba0 UsbDriver
1684: 18fce4 UsbDataIf
1690: 18fd54 USB PDD Selftest protection semaphore
1691: 18fd80 USB PDD Selftest DFC wait semaphore
1693: 18fe14 UsbExtension
1694: 18fe28 UsbDriver
1709: 18ff6c UsbDataIf
1715: 1903bc 10DUsbRapuIf
1716: 1903c9 10MUsbAsspIf
1717: 1903d6 11MUsbDrcCbIf
1718: 1903e4 11MUsbPilCbIf
1719: 1903f2 12DUsbEndpoint
1720: 190401 12DUsbSelfTest
1721: 190410 12MUsbEndpoint
1722: 19041f 13DUsbEpManager
1723: 19042f 13MUsbEndpoint0
```
*...truncated, 555 total matches*

## MMC_SD (428 matches)
```
588:  cc494 EVoltage_eMMC
589:  cc4a8 EVoltage_eMMC_Core
590:  cc4c0 EVoltage_eMMC_IO
647:  d2d68 18DEMMCPartitionInfo
648:  d2d7d 24DLegacyEMMCPartitionInfo
1242: 14d2d4                    | KMMCErrResponseTimeOut
1243: 14d300                    | KMMCErrDataTimeOut
1244: 14d328                    | KMMCErrBusyTimeOut
1245: 14d350                    | KMMCErrBusTimeOut
1246: 14d378                    | KMMCErrTooManyCards
1247: 14d3a4                    | KMMCErrResponseCRC
1248: 14d3cc                    | KMMCErrDataCRC
1249: 14d3f0                    | KMMCErrCommandCRC
1250: 14d418                    | KMMCErrStatus
1251: 14d43c                    | KMMCErrNoCard
1252: 14d460                    | KMMCErrBrokenLock
1253: 14d488                    | KMMCErrPowerDown
1254: 14d4b0                    | KMMCErrAbort
1255: 14d4d4                    | KMMCErrStackNotReady
1256: 14d500                    | KMMCErrNotSupported
1257: 14d52c                    | KMMCErrHardware
1258: 14d554                    | KMMCErrBusInconsistent
1259: 14d684                    | KMMCErrBypass
1260: 14d6a8                    | KMMCErrInitContext
1261: 14d6d0                    | KMMCErrArgument
1262: 14d6f8                    | KMMCErrSingleBlock
1263: 14d720                    | KMMCErrUpdPswd
1264: 14d744                    | KMMCErrLocked
1265: 14d768                    | KMMCErrNotFound
1266: 14d790                    | KMMCErrAlreadyExists
1267: 14d7bc                    | KMMCErrGeneral
1269: 14d7fc                    | KMMCStatAppCmd
1270: 14d820                    | KMMCStatSwitchError
1271: 14d84c                    | KMMCStatReadyForData
1272: 14d878                    | KMMCStatCurrentStateMask
1275: 14d9f8                    | KMMCStatEraseReset
1276: 14da20                    | KMMCStatCardECCDisabled
1277: 14da50                    | KMMCStatWPEraseSkip
1278: 14da7c                    | KMMCStatErrCSDOverwrite
1279: 14daac                    | KMMCStatErrOverrun
1280: 14dad4                    | KMMCStatErrUnderrun
1281: 14db00                    | KMMCStatErrUnknown
1282: 14db28                    | KMMCStatErrCCError
1283: 14db50                    | KMMCStatErrCardECCFailed
1284: 14db80                    | KMMCStatErrIllegalCommand
1285: 14dbb0                    | KMMCStatErrComCRCError
1286: 14dbdc                    | KMMCStatErrLockUnlock
1287: 14dc08                    | KMMCStatCardIsLocked
1288: 14dc34                    | KMMCStatErrWPViolation
1289: 14dc60                    | KMMCStatErrEraseParam
1290: 14dc8c                    | KMMCStatErrEraseSeqError
1291: 14dcbc                    | KMMCStatErrBlockLenError
1292: 14dcec                    | KMMCStatErrAddressError
1293: 14dd1c                    | KMMCStatErrOutOfRange
1304: 1529b4 Y:/sf/os/kernelhwsrv/kernel/eka/drivers/pbus/mmc/mmccd_init.cpp
1306: 158498 PBUS-MMC
1309: 158f18 10DMMCSocket
1311: 158f32 11DMMCSession
1315: 158f6c 12TMMCardArray
1318: 158f9b 15DMMCMediaChange
1322: 158fe8 17TMMCPasswordStore
1323: 158ffc 26TMMCardControllerInterface
1324: 159020 7DMMCPsu
1325: 159029 7TMMCard
1328: 159045 9DMMCStack
1330: 15905b N9DMMCStack12MAddressCardE
1331: 159076 N9DMMCStack5DBodyE
1332: 15e4f0 Media.MmcF
1333: 15e5c8 20DMmcMediaDriverFlash
1334: 15e5df 28DPhysicalDeviceMediaMmcFlash
1479: 164bc0 MemoryCard_eMMC
1482: 164c2c Rapu_MMCSD_Resource_Manager
1552: 165388 DMemoryCardStackPlatform::SetClockFreq - Invalid MMC_Clk_div iDiv!
1584: 1658cb N9DMMCStack17MDemandPagingInfoE
5191: 36f630 SdioBusDrv              
8549: 656636 19CCommChannelHandler
9162: 6cfdcd N10ConnStates27TRequestCommsBinderFromMcprE
9193: 6d04af N11MeshMachine10TStateForkINS_12TNodeContextIN5ESock20CMMCommsProviderBaseENS1_INS2_13ACFMMNodeBaseENS1_INS_11AMMNodeBaseENS_16TNodeContextBaseENS_17CNodeActivityBaseEEES7_EES7_EEEE
9216: 6d1137 N11MeshMachine16TStateTransitionINS_12TNodeContextIN5ESock20CMMCommsProviderBaseENS1_INS2_13ACFMMNodeBaseENS1_INS_11AMMNodeBaseENS_16TNodeContextBaseENS_17CNodeActivityBaseEEES7_EES7_EEEE
9231: 6d17f9 N11MeshMachine21TAggregatedTransitionIN10CoreStates31TAbortAllActivitiesNodeDeletionEN13CoreNetStates43TSendClientLeavingRequestToServiceProvidersENS_12TNodeContextIN5ESock20CMMCommsProviderBaseENS5_INS6_13ACFMMNodeBaseENS5_INS_11AMMNodeBaseENS_16TNodeContextBaseENS_17CNodeActivityBaseEEESB_EESB_EEEE
9233: 6d19f1 N11MeshMachine21TAggregatedTransitionIN13CoreNetStates14TAddDataClientEN8PRStates14TSendProvisionENS_12TNodeContextIN5ESock20CMMCommsProviderBaseENS5_INS6_13ACFMMNodeBaseENS5_INS_11AMMNodeBaseENS_16TNodeContextBaseENS_17CNodeActivityBaseEEESB_EESB_EEEE
9237: 6d1e43 N11MeshMachine21TAggregatedTransitionIN8PRStates12TStoreParamsEN10CoreStates18TPostToOriginatorsENS_12TNodeContextIN5ESock20CMMCommsProviderBaseENS5_INS6_13ACFMMNodeBaseENS5_INS_11AMMNodeBaseENS_16TNodeContextBaseENS_17CNodeActivityBaseEEESB_EESB_EEEE
9238: 6d1f3f N11MeshMachine21TAggregatedTransitionIN8PRStates12TStoreParamsENS1_25TRespondWithCurrentParamsENS_12TNodeContextIN5ESock20CMMCommsProviderBaseENS4_INS5_13ACFMMNodeBaseENS4_INS_11AMMNodeBaseENS_16TNodeContextBaseENS_17CNodeActivityBaseEEESA_EESA_EEEE
9239: 6d2039 N11MeshMachine21TAggregatedTransitionIN8PRStates29THandleDataClientStatusChangeENS1_27TDestroyOrphanedDataClientsENS_12TNodeContextIN5ESock20CMMCommsProviderBaseENS4_INS5_13ACFMMNodeBaseENS4_INS_11AMMNodeBaseENS_16TNodeContextBaseENS_17CNodeActivityBaseEEESA_EESA_EEEE
9242: 6d23e0 N11MeshMachine21TAggregatedTransitionINS0_IN10CoreStates31TAbortAllActivitiesNodeDeletionEN13CoreNetStates43TSendClientLeavingRequestToServiceProvidersENS_12TNodeContextIN5ESock20CMMCommsProviderBaseENS5_INS6_13ACFMMNodeBaseENS5_INS_11AMMNodeBaseENS_16TNodeContextBaseENS_17CNodeActivityBaseEEESB_EESB_EEEENS3_62TSendClientLeavingAndRemoveControlProviderIfNoServiceProvidersESE_EE
9246: 6d2a0d N11MeshMachine21TAggregatedTransitionINS0_INS0_IN10CoreStates31TAbortAllActivitiesNodeDeletionEN13CoreNetStates43TSendClientLeavingRequestToServiceProvidersENS_12TNodeContextIN5ESock20CMMCommsProviderBaseENS5_INS6_13ACFMMNodeBaseENS5_INS_11AMMNodeBaseENS_16TNodeContextBaseENS_17CNodeActivityBaseEEESB_EESB_EEEENS3_62TSendClientLeavingAndRemoveControlProviderIfNoServiceProvidersESE_EENS3_28TSetIdleIfNoServiceProvidersESE_EE
9247: 6d2bb7 N11MeshMachine21TAggregatedTransitionINS0_INS0_INS_13TRemoveClientEN8PRStates27TDestroyOrphanedDataClientsENS_12TNodeContextIN5ESock20CMMCommsProviderBaseENS4_INS5_13ACFMMNodeBaseENS4_INS_11AMMNodeBaseENS_16TNodeContextBaseENS_17CNodeActivityBaseEEESA_EESA_EEEEN13CoreNetStates27TSendLeaveCompleteIfRequestESD_EENSF_30TSendDataClientIdleIfNoClientsESD_EE
9248: 6d2d1a N11MeshMachine21TAggregatedTransitionINS0_INS_13TRemoveClientEN8PRStates27TDestroyOrphanedDataClientsENS_12TNodeContextIN5ESock20CMMCommsProviderBaseENS4_INS5_13ACFMMNodeBaseENS4_INS_11AMMNodeBaseENS_16TNodeContextBaseENS_17CNodeActivityBaseEEESA_EESA_EEEEN13CoreNetStates27TSendLeaveCompleteIfRequestESD_EE
9249: 6d2e4e N11MeshMachine21TAggregatedTransitionINS0_INS_13TRemoveClientEN8PRStates27TDestroyOrphanedDataClientsENS_12TNodeContextIN5ESock20CMMCommsProviderBaseENS4_INS5_13ACFMMNodeBaseENS4_INS_11AMMNodeBaseENS_16TNodeContextBaseENS_17CNodeActivityBaseEEESA_EESA_EEEEN13CoreNetStates30TSendDataClientIdleIfNoClientsESD_EE
9251: 6d3062 N11MeshMachine21TAggregatedTransitionINS_13TRemoveClientEN8PRStates27TDestroyOrphanedDataClientsENS_12TNodeContextIN5ESock20CMMCommsProviderBaseENS4_INS5_13ACFMMNodeBaseENS4_INS_11AMMNodeBaseENS_16TNodeContextBaseENS_17CNodeActivityBaseEEESA_EESA_EEEE
9299: 6d47d7 N11MeshMachine6TStateINS_12TNodeContextIN5ESock20CMMCommsProviderBaseENS1_INS2_13ACFMMNodeBaseENS1_INS_11AMMNodeBaseENS_16TNodeContextBaseENS_17CNodeActivityBaseEEES7_EES7_EEEE
9447: 6d6c1c N13NetInterfaces24TIfStaticFetcherLinkBaseINS0_INS_16TIfStaticFetcherINS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE
9576: 6d95cb N15NetStateMachine8AContextIN11MeshMachine12TNodeContextIN5ESock20CMMCommsProviderBaseENS2_INS3_13ACFMMNodeBaseENS2_INS1_11AMMNodeBaseENS1_16TNodeContextBaseENS1_17CNodeActivityBaseEEES8_EES8_EEEE
9750: 6db1ff N5ESock20CMMCommsProviderBaseE
11092: 7a9879 N11MeshMachine16TStateTransitionINS_12TNodeContextI25CPppSubConnectionProviderNS1_I27CAgentSubConnectionProviderNS1_I26CCoreSubConnectionProviderNS1_IN5ESock20CMMCommsProviderBaseENS1_INS5_13ACFMMNodeBaseENS1_INS_11AMMNodeBaseENS_16TNodeContextBaseENS_17CNodeActivityBaseEEESA_EESA_EESA_EESA_EESA_EEEE
11093: 7a99a7 N11MeshMachine6TStateINS_12TNodeContextI25CPppSubConnectionProviderNS1_I27CAgentSubConnectionProviderNS1_I26CCoreSubConnectionProviderNS1_IN5ESock20CMMCommsProviderBaseENS1_INS5_13ACFMMNodeBaseENS1_INS_11AMMNodeBaseENS_16TNodeContextBaseENS_17CNodeActivityBaseEEESA_EESA_EESA_EESA_EESA_EEEE
11098: 7a9b6a N15NetStateMachine8AContextIN11MeshMachine12TNodeContextI25CPppSubConnectionProviderNS2_I27CAgentSubConnectionProviderNS2_I26CCoreSubConnectionProviderNS2_IN5ESock20CMMCommsProviderBaseENS2_INS6_13ACFMMNodeBaseENS2_INS1_11AMMNodeBaseENS1_16TNodeContextBaseENS1_17CNodeActivityBaseEEESB_EESB_EESB_EESB_EESB_EEEE
13282: 92a6df 12CRFCommClass
13548: 9793cf 16CRfcommCtrlFrame
13677: 979f7c 22CRfcommCreditDataFrame
```
*...truncated, 428 total matches*

## NAND_Flash (85 matches)
```
441:  b8898 FlashInfo
1016: 1303cc DOneNandIrqGpioPinHandle
1017: 1303e8 DOneNandIrqGpioPinWrapper
1018: 1304d8 DOneNandIrqGpioBindIsr
1026: 130d68 Media.Nand
1054: 130fb4 NandThread
1055: 130ff0 NAND Drive
1056: 131044 NandDMAThread
1057: 131090 14TNandInterrupt
1058: 1310a1 16DMediaDriverNand
1059: 1310b4 16MTestableNandDrv
1060: 1310c7 24DPhysicalDeviceMediaNand
1333: 15e5c8 20DMmcMediaDriverFlash
1334: 15e5df 28DPhysicalDeviceMediaMmcFlash
2243: 1e7000 DAtmelBootFlash::CheckBit bit bigger than 8, bit = %d
2297: 20b56b 15DAtmelBootFlash
2591: 24c254 flash_type
2592: 24c260 test_flash
2594: 24c27c test_flash_high
2670: 2593c4 flashlamp_dummy
2907: 287278 fire_flash
2908: 28732c prepare_flash
3323: 2c13f4 prepare_flash
3389: 2caa68 fire_flash
3443: 2d27e0 flash_type
3444: 2d27ec test_flash
3447: 2d2818 test_flash_high
4268: 318e48 flash_type
4271: 318e6c flash_type
5763: 3fe5cc 14TRawNandAccess
6191: 4227c0 DCamera::TDCamMonitorGroup::Monitoring() unrecognised VCAM_FLASH_STATE_T %d
15324: a73fe2 21CBitmapAnimFlashTimer
17939: bf8824 creating default flash
18157: bff68c CCamDriver::DoSetSupportedFlash
18158: bff6b0 Error setting flash:
18159: bff6c8 CCamDriver::DoSetFlash
18160: bff6e0 Flash mode: 
18162: bff6fc Setting flash off for ALL
18163: bff718 Setting flash mode to: 
18164: bff730 Setting flash mode to: OFF
18165: bff74c None of the modes supports this flash
18331: c04643  pGpGCSettingFlash::Service
18332: c04674 ECamFlashSet
18336: c046d4 ECamFlashGet
18338: c046f4 ECamFlashReady
18467: c07360 EWBFlash supported (not added to supported list though...)
18475: c07858 CSettingFlashImp::ConstructL
18476: c07888 EFlashRedEyeReduce supported
18477: c078ac EFlashAuto supported
18478: c078c4 EFlashForced supported
18479: c078dc EFlashFillIn supported
18480: c078f8 EFlashVideoLight supported
18481: c07914 Supported flash modes: 
18482: c0792c CSettingFlashImp::SetFlash
18483: c07948 Start flash observing
18484: c07960 Flash setting roll back
18485: c07978 Flash roll back error!
18486: c07990 Stop flash observing
18487: c079a8 CSettingFlashImp::IsFlashReady
18488: c079c8 IsFlashReady: 
18489: c07d68 CSettingFlashImp::SendFlashEvent
18490: c07d90 CSettingFlashImp::HandleCamFrame
18492: c07dd0 EFlashCharging supp
18493: c07de4 Flash charged
18494: c07df4 Send flash charged event
18495: c07e14 Flash NOT charged
18496: c07e28 Send flash NOT charged event
18497: c07e48 CSettingFlashImp::HandleCamDriverEvent
18498: c07e80 Flash charging error
18499: c07e98 Unexpected flash event
18607: c0c1fd 13CSettingFlash
18621: c0c2f4 16CSettingFlashImp
19024: c1ced0 EDoSetFlash
19056: c1dd30 Restoring EDoSetFlash
19071: c1e90c CCamDriverIVE3Policy::DoSetFlash
19287: c2b73c CAdvSettingsCustomInterface::SetFlashMode
19312: c2d4dc Flash mode
19324: c2eb98 CAdvSettingsCustomInterface2::IsFlashReady
19498: c3841c CCameraPluginImp::SetFlashL
20127: ccbe3a 16TNandDriveAccess
23630: e7d97d 18MPmmFlashInterface
23631: e7d992 20CPmmNandReaderWriter
26912:134a74d N#N$N(N+N.N/N0N5N@NANDNGNQNZN\NcNhNiNtNuNyN
33135:16fe9e8 Decode_EncryptionAuthenticationAndIntegrity: Unknown extensions (skipped)
38343:1bdbb2c 31CNATBindingSTUNAndCRLFRefresher
```

## GPIO (16 matches)
```
518:  c15b8 GPIO-API:Invalid PinId for Gpio	Pin	!!
519:  c15e8 GPIO-API	DispatchToPinsIsr:Invalid	PinNbr for Gpio	Pin	!!
520:  c181c GPIO-API BindPin:Invalid PinNbr	for	Gpio Pin !!
521:  c184c GPIO-API BindPin:Invalid pin number	!!
522:  c1874 GPIO-API ReleaseBindedPin:Invalid	PinNbr for Gpio	Pin	!!
523:  c18b8 Invalid	Global Id	for	Gpio Pin
617:  cc7d5 20DPowerGenioCtrlClock
1016: 1303cc DOneNandIrqGpioPinHandle
1017: 1303e8 DOneNandIrqGpioPinWrapper
1018: 1304d8 DOneNandIrqGpioBindIsr
2145: 1d13f4 Failed to bind GPIO interrupt handler for SWITCH
2196: 1d1c49 11DGpioSwitch
3005: 29b090 gpio_expander_dummy
3246: 2b5b5c led_gpio
4422: 319aae platform_gpio_control
4423: 319ac4 platform_gpio_read
```

## DMA (530 matches)
```
354:  b4321 15DLogicalChannel
375:  b44be 19DLogicalChannelBase
585:  cc45c EHwClkGDMA
623:  cd17c Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/rebootdriver/rebootdriver_ldd/src/rebootdriverchannel.cpp, line=%d, compiled=Jul 27 2012 17:57:08
627:  cd30e 20DRebootDriverChannel
628:  ceea4 TRapDmac::FreeTransferChannel: Physical transfer channel
629:  ceee4 TRapDmac::FreeTransferChannel: Logical transfer channel
630:  d11ac TRap2DDmac::AppendCommand, KmaxComands exceeded
631:  d199c Rapu_GDMASS
632:  d1a2c DMA Mutex
633:  d1a3c DMA ch Mutex
634:  d1a50 DMA ch Mutex2d
635:  d1ad8 10T2DChannel
636:  d1ae5 10TRap2DDmac
637:  d1af2 11DDmaRequest
638:  d1b00 11TDmaChannel
639:  d1b0e 13TDmaDbChannel
640:  d1b1e 13TDmaSbChannel
641:  d1b2e 13TDmaSgChannel
642:  d1b3e 17TDmaSbQRCCChannel
643:  d1b60 8TRapDmac
656:  dbb6c 10DDmaHelper
679:  ded72 12DChannelComm
686:  ea955 10DSpiDmaRap
689:  ea97f 17TSpiChannelConfig
690:  ea993 20DRapDmaChannelHelper
691:  ea9aa 23DRapSysDmaChannelHelper
698:  eceb5 10DSpiDmaRap
701:  ecedf 17TSpiChannelConfig
702:  ecef3 20DRapDmaChannelHelper
703:  ecf0a 23DRapSysDmaChannelHelper
710:  ef429 10DSpiDmaRap
713:  ef453 17TSpiChannelConfig
714:  ef467 20DRapDmaChannelHelper
715:  ef47e 23DRapSysDmaChannelHelper
722:  f2698 22DSurfaceManagerChannel
732:  f54d8 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessldd_ldd/src/isauserchannel.cpp, line=%d, compiled=Jul 27 2012 17:57:14
733:  f5574 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessldd_ldd/src/isauserchannel.cpp, line=%d, extra info 0x%08x, compiled=Jul 27 2012 17:57:14
734:  f5620 Warning: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessldd_ldd/src/isauserchannel.cpp, line=%d, extra info 0x%x, compiled=Jul 27 2012 17:57:14
736:  f5709 15DISAUserChannel
737:  f571b 15MIAD2ChannelApi
756:  fa074 IAD: iChannelId = %d
783:  fe148 %s: test flags are:     { NCP_COMMON_MINIOS | CASW_ISCISIMULTIPLEXER_MAX_AMOUNT_OF_PARALLEL_DATACHANNELS | ISCTESTSTUB_SUPPORT_FLAG
786:  fe26c %s:     CASW_ISCISIMULTIPLEXER_MAX_AMOUNT_OF_PARALLEL_DATACHANNELS %d
792:  fe3a0 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/isakernelchannel.cpp, line=%d, compiled=Jul 27 2012 17:57:14
793:  fe444 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/isakernelchannel.cpp, line=%d, extra info 0x%08x, compiled=Jul 27 2012 17:57:14
810:  fed7c Unknown message 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x, message routed to channel %d
822:  ff10c 15MChannel2IADApi
823:  ff11e 15MIAD2ChannelApi
828:  ff17d 17DISAKernelChannel
970: 107f66 16DLightPddChannel
980: 108768 Checked by DLightLddChannel::DoCreate
1007: 109caa 16DLightLddChannel
1019: 130570 DMA request failed
1020: 130678 DmaCpyInit(): DfcQ creation failed
1021: 1306a0 DmaCpyInit(): Dma channel open failed
1022: 1306d0 DmaCpyInit(): DMA new request failed
1023: 1307b4 OAM_DmaWrite(): gDmaRequest not set
1024: 1307d8 OAM_DmaWrite(): Request too big
1025: 1307f8 OAM_DmaWrite(): DMA request failed
1056: 131044 NandDMAThread
1070: 1405e7 File: 'Y:/ncp_sw/corecom/security_components/security_drivers/rapu/security_drv/size/common/SecLddChannel.cpp' Line: %d
1072: 14067f File: 'Y:/ncp_sw/corecom/security_components/security_drivers/rapu/security_drv/size/common/SecLddChannel.cpp' Line: %d
1090: 140ad0 SecLddMainDfcq
1101: 140da4 Assertion '!iChannelOpen' failed;
1117: 1416d9 14DSecLddChannel
1138: 146294 Assertion '!iChannelOpen' failed;
1173: 149a04 Checked by DDVFSChannel::DoCreate
1174: 149a30 DDVFSChannel::DoCreate Create error
1179: 149b14 12DDVFSChannel
1186: 14a7ec 13DDmaHwMonitor
1555: 1655c1 14DMemoryCardDma
1566: 16569c 20DRapDmaChannelHelper
1567: 1656b3 22DMemoryCardDmaPlatform
1570: 1656fe 23DRapSysDmaChannelHelper
1573: 16574e 26DMemoryCardDmaHelpPlatform
1589: 16cb69 15DLddUsbcChannel
1591: 16ed0c DLddUsbcScChannel::RequestCallbackEp0
1593: 16ffd0 DLddUsbcScChannel::SetupEp0
1594: 16fff4 DLddUsbcScChannel::RealizeInterface
1600: 172fbf 17DLddUsbcScChannel
1743: 19059d 18MUsbAsspDmaChannel
1748: 190609 19DUsbotghsDmaChannel
1749: 19061f 19MUsbAsspDmaObserver
1770: 190825 26DUsbotghsDmaChannelManager
1818: 1a4930 USB PSL: BaseUsbOtgHostHalHcdManager, hal_isr_bind() called
1819: 1a496c USB PSL: BaseUsbOtgHostHalHcdManager, hal_isr_unbind() called
2105: 1c271c 20DUsbHubDriverChannel
2111: 1c3784 13DUsbdiChannel
2116: 1c40ec 20DUsbOtgDriverChannel
2127: 1cd132 17DLogicalChannelBt
2399: 21b2c0 SetLddChannel
2406: 21b3ca 14DAccPddChannel
2407: 21b3db 18DAccPddChannelImpl
2412: 21c0a0 Checked by DAccelerometerLddChannel
2415: 21e3b0 14DAccLddChannel
2417: 21ef00 Checked by DMagnetometerPddChannel
2460: 227ab8 22DMagnetometerPddDevice
2461: 227ad1 22DMagnetometerPddSensor
2462: 227aea 23DMagnetometerPddChannel
```
*...truncated, 530 total matches*

## IRQ (69 matches)
```
44:  648f8 MODE_IRQ:
1016: 1303cc DOneNandIrqGpioPinHandle
1017: 1303e8 DOneNandIrqGpioPinWrapper
1018: 1304d8 DOneNandIrqGpioBindIsr
1057: 131090 14TNandInterrupt
1223: 14cf24                    | GO_IRQ_STATE
1565: 165685 20DMemoryCardInterrupt
1575: 165788 28DMemoryCardInterruptPlatform
1576: 1657a7 28MMemoryCardInterruptObserver
1766: 1907b6 24DUsbRapuInterruptHandler
1767: 1907d1 24MUsbAsspInterruptHandler
1841: 1b8a40 %s: cannot re-open interrupt pipe after suspend
1853: 1b9808 %s: bad interrupt endpoint
1854: 1b9824 %s: cannot open interrupt pipe
2090: 1c13ee INTERRUPTED
2145: 1d13f4 Failed to bind GPIO interrupt handler for SWITCH
2232: 1e43a0 ConfigureInterrupts
2298: 20b57d 16DAtmelIrqHandler
3064: 2a4498 gl_PointCoord
4121: 317848 IIRQHISR
4359: 3194be intctrl_get_func_table
4626: 32a914   -EInterrupt
4629: 32b28c SyncLostDigit Interrupt Happened
4660: 32ef24 DIspCcp: LastIrq should be Uncertain
4662: 32f1dc DIspCcp: LastIrq shouldn't be End
4663: 32f200 DIspCcp: LastIrq shouldn't be Start
4666: 32f4ec DIspCcp: Could not bind the interrupt!
4675: 330db0 InterruptService
4917: 3485ac   -EInterrupt
5558: 3dbd64 IRQ stack base at %08x, size == %x
5658: 3dd740 MODE_IRQ:
9127: 6cf931 24CUPSAccessPointConfigExt
9447: 6d6c1c N13NetInterfaces24TIfStaticFetcherLinkBaseINS0_INS_16TIfStaticFetcherINS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE
9761: 6db358 N5ESock21MAccessPointConfigApiE
9800: 6db922 N5ESock24CConfigAccessPointConfigE
13765: 97a8c9 27TAVStreamStateINTConfigured
15028: a536a4 Interrupted system call
16214: ad956d N13NetInterfaces16TIfStaticFetcherINS0_INS_24TIfStaticFetcherLinkBaseINS1_INS1_INS0_INS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE24CAgentConnectionProviderNSB_20MLegacyControlApiExtEEESF_NSB_24MQueryConnSettingsApiExtESH_EESF_NSB_39MLinkCprServiceChangeNotificationApiExtESJ_EE
16215: ad9710 N13NetInterfaces16TIfStaticFetcherINS_24TIfStaticFetcherLinkBaseINS1_INS1_INS0_INS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE24CAgentConnectionProviderNSB_20MLegacyControlApiExtEEESF_NSB_24MQueryConnSettingsApiExtESH_EE
16216: ad9878 N13NetInterfaces16TIfStaticFetcherINS_24TIfStaticFetcherLinkBaseINS1_INS1_INS0_INS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE28CAgentMetaConnectionProviderNSB_24MQueryConnSettingsApiExtEEESF_NSB_31MLinkMCprLegacyDataAccessApiExtESH_EE
16218: ad9a6f N13NetInterfaces24TIfStaticFetcherLinkBaseINS0_INS0_INS_16TIfStaticFetcherINS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE24CAgentConnectionProviderNSB_20MLegacyControlApiExtEEE
16219: ad9bab N13NetInterfaces24TIfStaticFetcherLinkBaseINS0_INS0_INS_16TIfStaticFetcherINS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE28CAgentMetaConnectionProviderNSB_24MQueryConnSettingsApiExtEEE
16220: ad9cef N13NetInterfaces24TIfStaticFetcherLinkBaseINS0_INS_16TIfStaticFetcherINS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE
16221: ad9def N13NetInterfaces24TIfStaticFetcherLinkBaseINS_16TIfStaticFetcherINS1_INS0_INS0_INS0_INS1_INS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE24CAgentConnectionProviderNSB_20MLegacyControlApiExtEEESF_NSB_24MQueryConnSettingsApiExtESH_EESF_NSB_39MLinkCprServiceChangeNotificationApiExtESJ_EE30CTunnelAgentConnectionProviderNSB_14MPlatsecApiExtEEE
17151: b89cec interrupted
17265: b95ecc interrupt
18146: bff140 CCamDriver::SetSupportingModes interrupted
19401: c33efc CCameraUseCaseHintCustomInterface::HintDirectVideoCaptureL
19406: c342e4 CCameraUseCaseHintCustomInterface::HintVideoCaptureL
19407: c343bc CCameraUseCaseHintCustomInterface::HintStillCaptureL
19440: c3543b 33CCameraUseCaseHintCustomInterface
20808: d4f90c N3Gce25CVCEndpointClientNotifierE
21826: db721d N13NetInterfaces24TIfStaticFetcherLinkBaseINS0_INS0_INS_16TIfStaticFetcherINS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE27CMobilityConnectionProviderNSB_20AMobilityProtocolReqEEE
21827: db735c N13NetInterfaces24TIfStaticFetcherLinkBaseINS0_INS_16TIfStaticFetcherINS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE
22709: e1f2d3 N13NetInterfaces16TIfStaticFetcherINS0_INS0_INS0_INS_24TIfStaticFetcherLinkBaseINS1_INS1_INS0_INS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE26CIPProtoConnectionProviderNSB_26ADataMonitoringProtocolReqEEESF_NSB_14MLinkCprApiExtESH_EESF_NSB_20MLegacyControlApiExtESJ_EESF_NSB_32ALegacySubConnectionActiveApiExtESL_EESF_NSB_36ALegacyEnumerateSubConnectionsApiExtESN_EE
22710: e1f4cd N13NetInterfaces16TIfStaticFetcherINS0_INS0_INS_24TIfStaticFetcherLinkBaseINS1_INS1_INS0_INS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE26CIPProtoConnectionProviderNSB_26ADataMonitoringProtocolReqEEESF_NSB_14MLinkCprApiExtESH_EESF_NSB_20MLegacyControlApiExtESJ_EESF_NSB_32ALegacySubConnectionActiveApiExtESL_EE
22711: e1f68f N13NetInterfaces16TIfStaticFetcherINS0_INS_24TIfStaticFetcherLinkBaseINS1_INS1_INS0_INS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE26CIPProtoConnectionProviderNSB_26ADataMonitoringProtocolReqEEESF_NSB_14MLinkCprApiExtESH_EESF_NSB_20MLegacyControlApiExtESJ_EE
22714: e1fa19 N13NetInterfaces16TIfStaticFetcherINS_24TIfStaticFetcherLinkBaseINS1_INS1_INS0_INS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE26CIPProtoConnectionProviderNSB_26ADataMonitoringProtocolReqEEESF_NSB_14MLinkCprApiExtESH_EE
22715: e1fb7f N13NetInterfaces16TIfStaticFetcherINS_24TIfStaticFetcherLinkBaseINS1_INS1_INS0_INS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE29CIPProtoSubConnectionProviderNSB_26ADataMonitoringProtocolReqEEESF_NSB_32ALegacySubConnectionActiveApiExtESH_EE
22716: e1fcfa N13NetInterfaces16TIfStaticFetcherINS_24TIfStaticFetcherLinkBaseINS1_INS1_INS0_INS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE33CIPProtoDeftSubConnectionProviderNSB_26ADataMonitoringProtocolReqEEESF_NSB_32ALegacySubConnectionActiveApiExtESH_EE
22721: e20064 N13NetInterfaces24TIfStaticFetcherLinkBaseINS0_INS0_INS_16TIfStaticFetcherINS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE26CIPProtoConnectionProviderNSB_26ADataMonitoringProtocolReqEEE
22722: e201a8 N13NetInterfaces24TIfStaticFetcherLinkBaseINS0_INS0_INS_16TIfStaticFetcherINS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE29CIPProtoSubConnectionProviderNSB_26ADataMonitoringProtocolReqEEE
22723: e202ef N13NetInterfaces24TIfStaticFetcherLinkBaseINS0_INS0_INS_16TIfStaticFetcherINS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE33CIPProtoDeftSubConnectionProviderNSB_26ADataMonitoringProtocolReqEEE
22724: e2043a N13NetInterfaces24TIfStaticFetcherLinkBaseINS0_INS_16TIfStaticFetcherINS_32TIfStaticFetcherFirstInHierarchyINS_17TInterfaceControlEEEN8Messages9ANodeBaseES6_S4_EEN11MeshMachine11AMMNodeBaseES9_EEN5ESock20CMMCommsProviderBaseENSB_21MAccessPointConfigApiEEE
26998:13500e1 qjI qkI+qlI-qmI/qnI0qoI1qpI8qqIAqrIEqsIFqtIGquIJqvIKqwIPqxIRqyIWqzIZq{I\q|I^q}I`q~Ihq!Jyq"J
32794:16d2a80 Decode_MultipointCapability: Unknown extensions (skipped)
36009:1a2fb68 19CDRMConfigIntcfImpl
39171:1c6ae37 19CUsbInterruptReader
39173:1c6ae66 23CUsbInterruptConnection
```

## Display (393 matches)
```
471:  bcf50 DisplayDriver
498:  bf2e8 DisplayDriver
591:  cc4d8 EHwRegHdmi
856: 103c38 DSisaWrapper::SisaResetGenerate() -  ISA uncontrolled reset
858: 103c8c SISA: DSisaExtension not ok => shutdown self
868: 104f50 DSisaExtension::DeallocateBlock, CMT down -> ignore
870: 10500c DSisaExtension::AllocateBlock, CMT down -> allocate block from symbian
871: 105054 WARNING: DSisaExtension::SendMessage. Message NOT sent to modem because connection to modem NOT existing
873: 1050e2 14DSisaExtension
885: 10770c DisplayDriver
910: 10796c DisplayDriver
934: 107bb0 DisplayDriver
957: 107de4 DisplayDriver
972: 107f8c 19DLightDisplayMentor
992: 109b38 DisplayDriver
1038: 130ea0 DisplayDriver
1352: 163fa8 DisplayDriver
1375: 1641dc DisplayDriver
1398: 164410 DisplayDriver
1421: 164644 DisplayDriver
1444: 164878 DisplayDriver
1467: 164ad4 DisplayDriver
1494: 164dc4 DisplayDriver
1517: 165008 DisplayDriver
1540: 165270 DisplayDriver
1653: 18f9d0 DisplayDriver
1678: 18fc68 DisplayDriver
1703: 18fef0 DisplayDriver
1788: 1a3468 DisplayDriver
1890: 1bf950 DisplayDriver
1915: 1bfbec DisplayDriver
1940: 1bfe84 DisplayDriver
1963: 1c00b8 DisplayDriver
1986: 1c02ec DisplayDriver
2009: 1c0524 DisplayDriver
2032: 1c0758 DisplayDriver
2157: 1d175c DisplayDriver
2182: 1d1ae0 DisplayDriver
2215: 1e4168 DisplayDriver
2275: 20b308 DisplayDriver
2315: 20d6b0 DisplayDriver
2341: 20d978 DisplayDriver
2385: 21b1a4 DisplayDriver
2804: 27131c ../../../../middleware/dispmanx/dispmanx.c
2808: 271660 dispmanx_resource
3016: 29cc24 hdmi_control_dummy
3020: 29df48 ../../../../middleware/hdmi/hdmi.c
3223: 2b2a7c ../../../../applications/ive/display/ive_cdp.c
3225: 2b355c ../../../../applications/ive/display/ive_dsi.c
3403: 2cc8b0 GL20_FRAMEBUFFER_T
3488: 2d6e74 HDMI callback
3716: 2fde68 HDMI_VideoFmt_eNTSC
3717: 2fde7c HDMI_VideoFmt_eNTSC_J
3718: 2fde92 HDMI_VideoFmt_ePAL_B
3719: 2fdea7 HDMI_VideoFmt_ePAL_B1
3720: 2fdebd HDMI_VideoFmt_ePAL_D
3721: 2fded2 HDMI_VideoFmt_ePAL_D1
3722: 2fdee8 HDMI_VideoFmt_ePAL_G
3723: 2fdefd HDMI_VideoFmt_ePAL_H
3724: 2fdf12 HDMI_VideoFmt_ePAL_K
3725: 2fdf27 HDMI_VideoFmt_ePAL_I
3726: 2fdf3c HDMI_VideoFmt_ePAL_M
3727: 2fdf51 HDMI_VideoFmt_ePAL_N
3728: 2fdf66 HDMI_VideoFmt_ePAL_NC
3729: 2fdf7c HDMI_VideoFmt_eSECAM
3730: 2fdf91 HDMI_VideoFmt_e1080i
3731: 2fdfa6 HDMI_VideoFmt_e1080p
3732: 2fdfbb HDMI_VideoFmt_e720p
3733: 2fdfcf HDMI_VideoFmt_e480p
3734: 2fdfe3 HDMI_VideoFmt_e1080i_50Hz
3735: 2fdffd HDMI_VideoFmt_e1080p_24Hz
3736: 2fe017 HDMI_VideoFmt_e1080p_25Hz
3737: 2fe031 HDMI_VideoFmt_e1080p_30Hz
3738: 2fe04b HDMI_VideoFmt_e1250i_50Hz
3739: 2fe065 HDMI_VideoFmt_e720p_24Hz
3740: 2fe07e HDMI_VideoFmt_e720p_50Hz
3741: 2fe097 HDMI_VideoFmt_e576p_50Hz
3742: 2fe0b0 HDMI_VideoFmt_eCUSTOM_1440x240p_60Hz
3743: 2fe0d5 HDMI_VideoFmt_eCUSTOM_1440x288p_50Hz
3744: 2fe0fa HDMI_VideoFmt_eCUSTOM_1366x768p
3745: 2fe11a HDMI_VideoFmt_eCUSTOM_1366x768p_50Hz
3746: 2fe13f HDMI_VideoFmt_eDVI_640x480p
3747: 2fe15b HDMI_VideoFmt_eDVI_800x600p
3748: 2fe177 HDMI_VideoFmt_eDVI_1024x768p
3749: 2fe194 HDMI_VideoFmt_eDVI_1280x768p
3750: 2fe1b1 HDMI_VideoFmt_eDVI_1280x720p_50Hz
3751: 2fe1d3 HDMI_VideoFmt_eDVI_1280x720p
3752: 2fe1f0 HDMI_VideoFmt_eDVI_1280x720p_ReducedBlank
3753: 2fe21a HDMI_VideoFmt_eDVI_640x350p_60Hz
3754: 2fe23b HDMI_VideoFmt_eDVI_640x350p_70Hz
3755: 2fe25c HDMI_VideoFmt_eDVI_640x350p_72Hz
3756: 2fe27d HDMI_VideoFmt_eDVI_640x350p_75Hz
3757: 2fe29e HDMI_VideoFmt_eDVI_640x350p_85Hz
3758: 2fe2bf HDMI_VideoFmt_eDVI_640x400p_60Hz
3759: 2fe2e0 HDMI_VideoFmt_eDVI_640x400p_70Hz
3760: 2fe301 HDMI_VideoFmt_eDVI_640x400p_72Hz
3761: 2fe322 HDMI_VideoFmt_eDVI_640x400p_75Hz
3762: 2fe343 HDMI_VideoFmt_eDVI_640x400p_85Hz
3763: 2fe364 HDMI_VideoFmt_eDVI_640x480p_60Hz
3764: 2fe385 HDMI_VideoFmt_eDVI_640x480p_70Hz
```
*...truncated, 393 total matches*

## Camera_ISP (670 matches)
```
468:  bcf18 ThermalSensor
469:  bcf2c CameraHWA
470:  bcf3c CameraDriver
473:  bcf78 CameraStaticData
495:  bf2b0 ThermalSensor
496:  bf2c4 CameraHWA
497:  bf2d4 CameraDriver
500:  bf310 CameraStaticData
559:  cc274 EHwClkCamera
882: 1076d4 ThermalSensor
883: 1076e8 CameraHWA
884: 1076f8 CameraDriver
887: 107734 CameraStaticData
907: 107934 ThermalSensor
908: 107948 CameraHWA
909: 107958 CameraDriver
912: 107994 CameraStaticData
931: 107b78 ThermalSensor
932: 107b8c CameraHWA
933: 107b9c CameraDriver
936: 107bd8 CameraStaticData
954: 107dac ThermalSensor
955: 107dc0 CameraHWA
956: 107dd0 CameraDriver
959: 107e0c CameraStaticData
989: 109b00 ThermalSensor
990: 109b14 CameraHWA
991: 109b24 CameraDriver
994: 109b60 CameraStaticData
1035: 130e68 ThermalSensor
1036: 130e7c CameraHWA
1037: 130e8c CameraDriver
1040: 130ec8 CameraStaticData
1349: 163f70 ThermalSensor
1350: 163f84 CameraHWA
1351: 163f94 CameraDriver
1354: 163fd0 CameraStaticData
1372: 1641a4 ThermalSensor
1373: 1641b8 CameraHWA
1374: 1641c8 CameraDriver
1377: 164204 CameraStaticData
1395: 1643d8 ThermalSensor
1396: 1643ec CameraHWA
1397: 1643fc CameraDriver
1400: 164438 CameraStaticData
1418: 16460c ThermalSensor
1419: 164620 CameraHWA
1420: 164630 CameraDriver
1423: 16466c CameraStaticData
1441: 164840 ThermalSensor
1442: 164854 CameraHWA
1443: 164864 CameraDriver
1446: 1648a0 CameraStaticData
1464: 164a9c ThermalSensor
1465: 164ab0 CameraHWA
1466: 164ac0 CameraDriver
1469: 164afc CameraStaticData
1491: 164d8c ThermalSensor
1492: 164da0 CameraHWA
1493: 164db0 CameraDriver
1496: 164dec CameraStaticData
1514: 164fd0 ThermalSensor
1515: 164fe4 CameraHWA
1516: 164ff4 CameraDriver
1519: 165030 CameraStaticData
1537: 165238 ThermalSensor
1538: 16524c CameraHWA
1539: 16525c CameraDriver
1542: 165298 CameraStaticData
1650: 18f998 ThermalSensor
1651: 18f9ac CameraHWA
1652: 18f9bc CameraDriver
1655: 18f9f8 CameraStaticData
1675: 18fc30 ThermalSensor
1676: 18fc44 CameraHWA
1677: 18fc54 CameraDriver
1680: 18fc90 CameraStaticData
1700: 18feb8 ThermalSensor
1701: 18fecc CameraHWA
1702: 18fedc CameraDriver
1705: 18ff18 CameraStaticData
1785: 1a3430 ThermalSensor
1786: 1a3444 CameraHWA
1787: 1a3454 CameraDriver
1790: 1a3490 CameraStaticData
1887: 1bf918 ThermalSensor
1888: 1bf92c CameraHWA
1889: 1bf93c CameraDriver
1892: 1bf978 CameraStaticData
1912: 1bfbb4 ThermalSensor
1913: 1bfbc8 CameraHWA
1914: 1bfbd8 CameraDriver
1917: 1bfc14 CameraStaticData
1937: 1bfe4c ThermalSensor
1938: 1bfe60 CameraHWA
1939: 1bfe70 CameraDriver
1942: 1bfeac CameraStaticData
1960: 1c0080 ThermalSensor
1961: 1c0094 CameraHWA
1962: 1c00a4 CameraDriver
```
*...truncated, 670 total matches*

## Audio (1049 matches)
```
458:  bc854 Codecheck
581:  cc40c EHwClkACodec
694:  ecc84 Spi2SpiDriver Thread
1269: 14d7fc                    | KMMCStatAppCmd
1814: 1a380a 28TUsbcAudioEndpointDescriptor
3601: 2e78e8 AUDIOPIPE
4033: 304acd audiopipe.vll
4294: 319006 codec_get_func_table
4295: 31901b codec_name
4325: 319210 guess_codec
4473: 319e8d vc_audio_make_stereo16
4474: 319ea4 vc_audio_set_nchannels
4502: 31a0e4 vc_image2codec_type
4538: 31a398 audiopipe.vll
4539: 31a3a6 vchi_audiopipe_init
5985: 412dc4 DCamSharedVid::MaxBitRate() - codec not recognised
6030: 416478 DCamSharedVid::DCamSharedVid() Attempt to set initial QP for invalid codec
6326: 42ae0c Checked by IVE Audio Pipe LDD
6327: 42ae2c audiopipe sync cmd sema
6328: 42ae44 audiopipe conversion sema
6329: 42ae60 audiopipe service sema
6330: 42b324 DAudioPipeChannel::OpenDevice failed to queue message %d
6331: 42b3f0 DAudioPipeChannelDfcQ
6332: 42b420 DAudioPipeChannel::iServiceSemaphore timeout for write - no response from videocore
6333: 42b474 DAudioPipeChannel::iSyncCommandSemaphore timeout for write - no response from videocore
6334: 42b4cc DAudioPipeChannel::ActualWrite - incorrect data length %u returned.
6335: 42b510 DAudioPipeChannel::iSyncCommandSemaphore timeout for progress - no response from videocore
6336: 42b56c DAudioPipeChannel::GetProgress - incorrect data length %u returned.
6337: 42b5b0 DAudioPipeChannel::iSyncCommandSemaphore timeout for OpenDevice - no response from videocore
6338: 42b610 DAudioPipeChannel::OpenDevice - incorrect data length %u returned.
6339: 42b66c 17DAudioPipeChannel
6340: 42b680 17DAudioPipeFactory
6341: 42b704 AudioPipeChannel
6479: 436764 usbaudiodevice.cpp
6480: 437318 usbaudiochannel.cpp
6481: 437da8 usbaudiocontrollerstatedspuser.cpp
6482: 437fbc usbaudiomsghandlerbase.cpp
6484: 438874 usbaudiodspmsghandler.cpp
6485: 43935c usbaudiocontrollerstatekernel.cpp
6487: 43a3c4 usbaudiodatatransferlinkkernel.cpp
6488: 43b184 usbaudiocontrollerstatelinkkernel.cpp
6489: 43b2b8 usbaudiolinkmsghandler.cpp
6490: 43b910 usbaudiocontrollerstatelinkuser.cpp
6491: 43bc98 usbaudiodatatransferlinkkernel.cpp
6492: 43c194 USBAudioDrv
6493: 43c1a4 UsbAudioDriverDfcThread
6541: 43c706 21DUsbAudioDriverDevice
6543: 43c736 22DUsbAudioDriverChannel
6544: 43c74f 22DUsbAudioDspMsgHandler
6545: 43c768 23DUsbAudioLinkMsgHandler
6546: 43c782 23DUsbAudioMsgHandlerBase
6547: 43c79c 28DUsbAudioControllerStateBase
6548: 43c7bb 28DUsbAudioControllerStateIdle
6549: 43c7da 30DUsbAudioControllerStateKernel
6550: 43c7fb 31DUsbAudioControllerStateDspUser
6551: 43c81d 31DUsbAudioDataTransferLinkKernel
6552: 43c83f 32DUsbAudioControllerStateLinkUser
6553: 43c862 34DUsbAudioControllerStateLinkKernel
8275: 62ee92 15CImageReadCodec
8285: 62ef48 16CImageWriteCodec
8318: 62f1f0 19MReadCodecExtension
8329: 62f2eb 21CImageDecodeConstruct
8330: 62f303 21CImageEncodeConstruct
8343: 62f44e 24CImageProcessorReadCodec
8355: 62f5a2 28CImageMaskProcessorReadCodec
8356: 62f5c1 28CImageProcessorReadCodecBody
8364: 62f6c5 32CImageMaskProcessorReadCodecBody
8365: 62f6e8 33CImageProcessorReadCodecExtension
8369: 62f781 37CImageMaskProcessorReadCodecExtension
8446: 642b4b 22MAudioClientThreadInfo
8475: 6480e0 26MAudioContextArbiterClient
8483: 649365 15CAudioSvrLoader
8484: 649377 15CMMFAudioServer
8486: 64939d 19CAudioSvrLoaderImpl
8487: 6493b3 22CMMFAudioServerSession
8489: 6493ed N15CMMFAudioServer25CDelayAudioServerShutDownE
11256: 7c3537 16CDomainNameCodec
11836: 84a1d8 mixer_3d
11838: 84a26c mixer_mono
11842: 84a304 mixer_stereo
11843: 84a33c mixer_multich
11847: 84a3f4 mixer_channel_control
11852: 84f6a0 13EAP_AudioLink
11864: 84f775 16EAP_LogicalMixer
11874: 84f83d 18EAP_ComponentMixer
11878: 84f891 18EAP_LogicalMixer3D
11890: 84f99c 20EAP_ComponentMixer3D
11892: 84f9ca 20EAP_ControlAudioLink
11893: 84f9e1 20EAP_ControlMixerData
11896: 84fa26 20EAP_LogicalMixerMono
11897: 84fa3d 21EAP_AudioLinkObserver
11908: 84fb46 22EAP_AudioLinkDspThread
11909: 84fb5f 22EAP_ComponentMixerMono
11916: 84fc0e 22EAP_LogicalMixerStereo
11917: 84fc27 23EAP_AudioLinkUniProcess
11925: 84fcf7 23EAP_LogicalMixerMultich
11928: 84fd46 24EAP_ComponentMixerStereo
11936: 84fe1e 25EAP_AudioLinkImplObserver
11938: 84fe56 25EAP_ComponentMixerMultich
11939: 84fe72 25EAP_ComponentRoutingMixer
```
*...truncated, 1049 total matches*

## Power (73 matches)
```
352:  b42fd 15DBatteryMonitor
554:  cc210 ERegulatorVaux1
555:  cc224 ERegulatorVaux2
576:  cc3a0 ERegulatorVMEM
588:  cc494 EVoltage_eMMC
589:  cc4a8 EVoltage_eMMC_Core
590:  cc4c0 EVoltage_eMMC_IO
1167: 149160 15DBatteryMonitor
1171: 1491ad 21DBinaryBatteryMonitor
1745: 1905c7 19DUsbChargerDetector
1771: 190842 27DUsbChargerDetectorIsp1702a
2244: 1e9200 DAtmelChargerDectector::DoCreate - Setting aAtmelDfcQ failed
2292: 20b458 DAtmelSensor::DoCreate() - Creation of iAtmelChargerDectector failed
2299: 20b590 22DAtmelChargerDectector
3135: 2a8160 matrixCompMult
3952: 303352 mat2 matrixCompMult(mat2 x, mat2 y){   return mat2(x[0] * y[0], x[1] * y[1]);}
3953: 3033a1 mat3 matrixCompMult(mat3 x, mat3 y){   return mat3(x[0] * y[0], x[1] * y[1], x[2] * y[2]);}
3954: 3033fd mat4 matrixCompMult(mat4 x, mat4 y){   return mat4(x[0] * y[0], x[1] * y[1], x[2] * y[2], x[3] * y[3]);}
4424: 319ad7 pmu_uideck_get_func_table
4606: 32a4f4 Failed to set Vout voltage
4881: 34592c TvOutKernelIveHdmi::CableRemovalHandler() Failed to disable HDMI 5v regulator
4883: 3459c0 TvOutKernelIveHdmi::CableDetectHandler() Failed to enable HDMI 5v regulator
5047: 36cef8 0x%x, /* TXBiPReferencePDvoltage_2_4G */ 
5058: 36d478 /* TXBiPReferencePDvoltage_5G[7*2] */ 
5072: 36df00 regulatoryDomain        
6717: 467f48 12TWlanScanFsm
6758: 46827b 20TWlanScanModeRunning
6759: 468292 20TWlanScanModeStopped
6765: 46831e 21TWlanStopScanningMode
6769: 46837f 22TWlanStartScanningMode
8154: 61162d 18CGetBatteryInfoAct
8165: 611723 21CNotifyBatteryInfoAct
10940: 79b877 24CMobilePhoneEditableListIN12RMobilePhone10TWlanSIDV8EE
11876: 84f867 18EAP_ComponentStwLS
11899: 84fa6d 21EAP_ComponentStwLSMcu
11915: 84fbf5 22EAP_LogicalEffectStwLS
12458: 879c41 21CRadioChargerDetector
12930: 8b1a01 15CStwLsAlgorithm
12933: 8b1a39 16CStwLsParameters
12937: 8b1a88 17CStwLs3DAlgorithm
12939: 8b1ab1 18CStwLs3DParameters
13183: 91331e 28TAvdtpMultiplexingCapability
13554: 979441 16TAvctpMuxerState
13597: 9797b3 19CL2CAPMuxController
13694: 97a125 23CAvctpMuxerStateFactory
13856: 97b417 9CL2CAPMux
13917: 98e231 23CRemConBatteryApiTarget
18043: bfb534 CCamController::DischargeResponsibilities
21665: d98505 N8CommsDat9CMDBFieldI22TCommsDatWlanRegDomainEE
21666: d98535 N8CommsDat9CMDBFieldI23TCommsDatWlanDialogPrefEE
21668: d98598 N8CommsDat9CMDBFieldI24TCommsDatWlanNetworkTypeEE
21669: d985ca N8CommsDat9CMDBFieldI25TCommsDatWlanDesTransRateEE
21670: d985fd N8CommsDat9CMDBFieldI25TCommsDatWlanPreambleTypeEE
21672: d98664 N8CommsDat9CMDBFieldI26TCommsDatWlanEncrytionTypeEE
21673: d98698 N8CommsDat9CMDBFieldI26TCommsDatWlanPowerSaveModeEE
23625: e7c600 19CPowerTrBatteryInfo
26309:129d5db m m!m"m#m$m&m(m)m,m-m/m0m4m6m7m8m:m?m@mBmDmImLmPmUmVmWmXm[m]m_mambmdmemgmhmkmlmmmpmqmrmsmumvmymzm{m}m~m
26972:134e579 W,7 W-7"W.7#W/7$W07%W17)W27*W37,W47.W57/W673W774W87=W97>W:7?W;7EW<7FW=7LW>7MW?7RW@7bWA7eWB7gWC7hWD7kWE7mWF7nWG7oWH7pWI7qWJ7sWK7tWL7uWM7wWN7yWO7zWP7{WQ7|WR7~WS7
30549:15d2443 20CBatteryInfoObserver
30560:15d2540 20MBatteryInfoNotifier
30679:15e6dad 42TPowerVoltageCurrentMeasurementsClientData
30703:15eb75d 26CHWRMBatteryMeasurementsAO
30704:15eb77a 31CHWRMBatteryChargeRateCurrentAO
30705:15eb79c 31CHWRMBatteryPowerMeasurementsAO
30706:15eb7be 34MHWRMBatteryChargingStatusObserver
30707:15eb7e3 35CHWRMBatteryMeasurementsAttributeAOIN10CHWRMPower28TBatteryPowerMeasurementDataEE
30708:15eb837 35CHWRMBatteryMeasurementsAttributeAOIiE
30709:15eb860 36CHWRMBatteryChargeTimeMeasurementsAO
32122:16b3058 Delete_IrpMultiplex: Illegal CHOICE index
32330:16bc670 Encode_IrpMultiplex: Illegal CHOICE index
32517:16c5e30 Decode_IrpMultiplex: Unsupported extension (skipping)
39536:1cafa98 13CXcapEarlyIms
39686:1cc03c2 16MXcapEarlyImsObs
```

## Clock (289 matches)
```
444:  b90f4 MCUClockInfo
534:  ca528 waiting for AccPLLLock timed out
535:  ca550 fatal error: mpll lock is dropped: opp %d
536:  ca580 waiting for ACRC_MPLLLock2 timed out
548:  cc1ac EClockArm
549:  cc1bc EClockMPLL
550:  cc1cc EClockRfClk
551:  cc1dc EClockSdram
611:  cc771 13DPowerHwClock
613:  cc791 13DPowerSwClock
617:  cc7d5 20DPowerGenioCtrlClock
1340: 162318 DMemoryCardStackPlatform::InitClockOff - Invalid System Clock!
1552: 165388 DMemoryCardStackPlatform::SetClockFreq - Invalid MMC_Clk_div iDiv!
2354: 20da7c DigitiserSyncLock
5031: 36ccc0 0x%x, /* ClockValidOnWakeup */ 
6405: 432c90 CUSTOMDRIVER # DCustomRtcHandler::ClockSync
8120: 609008 15MMMFClockSource
8128: 6090a7 18CSystemClockSource
8138: 6091a4 30CMMFClockSourcePeriodicUtility
8155: 611642 18CGetIccLockInfoAct
8166: 61173b 21CNotifyIccLockInfoAct
11036: 7a54fc 20CPsdContextQoSConfig
11040: 7a5564 26CPsdQoSConfigChangeWatcher
11047: 7a55eb N26CPsdQoSConfigChangeWatcher9MObserverE
11380: 7d03a2 16CQoSChannelEvent
11381: 7d03b5 16CQoSConfirmEvent
11416: 7d6f02 18CQoSChannelManager
11418: 7d6f2c 19CInternalQoSChannel
11421: 7d6f75 8CQoSConn
11579: 7f1048 17CReadClockCommand
11603: 7f1276 23CReadClockOffsetCommand
12443: 879b14 13MRadioTxClock
12464: 879cd1 21MRadioTxClockObserver
15269: a6de19 9DClockDll
16536: b10d6a 20CPosContactRequestor
16555: b10f55 26CPosCommonPrivacyResources
17838: bee5bc CCamImgScalerNoScale::CCamImgScalerNoScale
17871: bf657c 20CCamImgScalerNoScale
22654: e1d1fb N11IPProtoSCpr15TStoreProvisionE
22655: e1d21c N11IPProtoSCpr18TProcessAgentEventE
22656: e1d240 N11IPProtoSCpr27TSendDataClientRoutedToFlowE
22657: e1d26d N11IPProtoSCpr31TAwaitingAgentEventNotificationE
22658: e1d29e N11IPProtoSCpr34TProcessDataMonitoringNotificationE
22659: e1d2d2 N11IPProtoSCpr35TAwaitingDataMonitoringNotificationE
22952: e34ee3 N12IpSCprStates22TAddClientToQosChannelE
22956: e34f8c N12IpSCprStates31TPrepareToAddClientToQosChannelE
23148: e4c2be 18CQoSChangeNotifier
23430: e5b0e7 N15QoSIpSCprStates22TAddClientToQoSChannelE
23433: e5b16e N15QoSIpSCprStates25TRemoveClientToQoSChannelE
23434: e5b19d N15QoSIpSCprStates34TRemoveLeavingClientFromQoSChannelE
23435: e5b1d5 N15QoSIpSCprStates43TStoreAddressUpdateAndAddClientToQoSChannelE
23589: e79e28 AT*NSECLOCK
24473: ef7789 19CQoSControlRulebase
26019:11ecbe6 ** Ticks per second (clock resolution): %ld.
29148:14b70be 12RAnalogClock
29149:14b70cd 13RDigitalClock
30856:1603d34 18MClockEComObserver
30857:1603d49 20MClockPluginObserver
30858:1604998 25CClockTimeSourceInterface
30859:1605b8c 19CClockMCCTzIdMapper
30860:1605ba2 22CClockTimeZoneResolver
30861:1605bbb 26CClockTimeZoneResolverImpl
30862:16063e4 18CClockEComListener
30863:1606d14 16CClockNitzPlugin
30864:1606d27 18CClockNitzListener
30865:1606d3c 20CClockNitzPluginImpl
30897:1610363 28CVtImageRotatorImplClockwise
31862:1667dac N15CMusManagerImpl23CMusManagerImplListenerE
32125:16b30e0 Delete_IndClockRecovery: Illegal CHOICE index
32135:16b34c8 Delete_CmdClockRecovery: Illegal CHOICE index
32333:16bcb04 Encode_IndClockRecovery: Illegal CHOICE index
32342:16bd01c Encode_CmdClockRecovery: Illegal CHOICE index
32522:16c6084 Decode_IndClockRecovery: Unsupported extension (skipping)
32536:16c6a6c Decode_QOSCapability: Unknown extensions (skipped)
32550:16c72c4 Decode_CmdClockRecovery: Unsupported extension (skipping)
33133:16fe960 Decode_CustomPictureClockFrequency: Unknown extensions (skipped)
33175:17011fb 10Oscl_Alloc
33184:1701276 11OSCL_String
33185:1701284 11Oscl_TAllocI10MuxSduData16OsclMemAllocatorE
33186:17012b2 11Oscl_TAllocI11CPVProxyMsg16OsclMemAllocatorE
33187:17012e1 11Oscl_TAllocI11PVMFCmdResp16OsclMemAllocatorE
33188:1701310 11Oscl_TAllocI13OlcFormatInfo16OsclMemAllocatorE
33189:1701341 11Oscl_TAllocI13OsclSharedPtrI12PVMFMediaMsgE16OsclMemAllocatorE
33190:1701382 11Oscl_TAllocI13OsclSharedPtrI13PVMFMediaDataE16OsclMemAllocatorE
33191:17013c4 11Oscl_TAllocI13OsclSharedPtrI13PVMFMediaDataE23PVDevVideoPlayNodeAllocE
33192:170140d 11Oscl_TAllocI13OsclSharedPtrI14PVLoggerFilterE16OsclMemAllocatorE
33193:1701450 11Oscl_TAllocI13OsclSharedPtrI16PVLoggerAppenderE16OsclMemAllocatorE
33194:1701495 11Oscl_TAllocI13OsclSharedPtrI17PVMFMediaDataImplE16OsclMemAllocatorE
33195:17014db 11Oscl_TAllocI13OsclSharedPtrI19H223IncomingChannelE16OsclMemAllocatorE
33196:1701523 11Oscl_TAllocI13OsclSharedPtrI19H223OutgoingChannelE16OsclMemAllocatorE
33197:170156b 11Oscl_TAllocI14CPVDevSoundCmd16CPVDevSoundAllocE
33198:170159d 11Oscl_TAllocI14PVMFAsyncEvent16OsclMemAllocatorE
33199:17015cf 11Oscl_TAllocI15AdaptationLayer10BasicAllocE
33200:17015fc 11Oscl_TAllocI15CPV2WayPortPair10BasicAllocE
33201:1701629 11Oscl_TAllocI15CPVDatapathNode10BasicAllocE
33202:1701656 11Oscl_TAllocI16PVLoggerRegistry16OsclMemAllocatorE
33203:170168a 11Oscl_TAllocI17CPVProxyInterface16OsclMemAllocatorE
33204:17016bf 11Oscl_TAllocI17Oscl_Rb_Tree_NodeI9Oscl_PairIK13PVCodecType_tjEE16OsclMemAllocatorE
33205:1701713 11Oscl_TAllocI17Oscl_Rb_Tree_NodeI9Oscl_PairIK14TPVMediaType_tP11Oscl_VectorIj16OsclMemAllocatorEEES5_E
33206:170177b 11Oscl_TAllocI17Oscl_Rb_Tree_NodeI9Oscl_PairIK6OlcKeyP8OlcParamEE16OsclMemAllocatorE
```
*...truncated, 289 total matches*

## Memory (16 matches)
```
338:  b4212 13DRamAllocator
551:  cc1dc EClockSdram
586:  cc46c EHwClkMemCard0
587:  cc480 EHwClkMemCard1
1338: 161c7c Rapu_Memcard_PSU_HW_
1339: 1622f4 Rapu_Memcard_Stack_HW_
1481: 164c18 MEMCARD_DFCQ
3390: 2cae54 sdram_test
4301: 31907d dma_memcpy2d
4523: 31a274 vclib_memcpy
6483: 4381e0 userdspsdramhandler.cpp
6486: 439b04 usbdspsdramhandler.cpp
6540: 43c6f0 19DUsbDspSdramHandler
12861: 8a7755 13CArrayPtrFlatI12TSdramConfigE
12871: 8a7826 9CArrayFixIP12TSdramConfigE
12872: 8a7842 9CArrayPtrI12TSdramConfigE
```

## Security (374 matches)
```
189:  a5d9c Secure Id Check 
190:  a5db0  attempted an operation requiring the secure id: 
196:  a5e74 the SecureId: 
289:  b3638 SecureRNGMutex
314:  b40b2 10DSecureRNG
1077: 14082c PA_RSAOBC
1086: 1409f4 PA_RSAOBC
1113: 14147c Assertion 'iSecLddAesMutex != NULL' failed;
1114: 1414a8 File: 'Y:/ncp_sw/corecom/security_components/security_drivers/rapu/security_drv/size/rapu/SecCrypto.cpp' Line: %d
1131: 145d40 SecAesResource
2355: 20da94 DigitiserSafeLock
3293: 2bcf2c OTP hash
3294: 2bcf38 ../../../../applications/vmcs/vchi/otpburn.c
4037: 304b01 otpburn.vll
4560: 31a526 otpburn.vll
4561: 31a532 vchi_otpburn_init
4640: 32dbc0 DVCDriver::BootParseMessage Unrecognised VideoCore response
4645: 32de90 DVCDriver::Boot() BootParseMessage failed %d
4648: 32e1bc DVCDriver::Boot() BootParseMessage returned an error (stage 2)
4941: 354170 Checked by Hal function EDisplayHalSetSecure
5098: 36e1d8 keyDeriveAes            
5583: 3dc2ac          SecureId: %08x, VendorId: %08x
5613: 3dc92c       SecureId: %08x, VendorId: %08x
6342: 42bb8c OTP burning sem
6350: 42be3c OTPBDataServiceIveBc::GetService() - Error opening VCHI service %d
7650: 547fc5 12CSecureStore
7663: 5480be 13TSecureFilter
7694: 548309 20RSecureStorePagePool
8059: 5fb94d 13CSecurePlugin
8265: 62ede5 14CErrorDiffuser
8344: 62f469 24CMonochromeErrorDiffuser
10133: 71bf88 10MNifIfUser
10137: 71bfbf 11CIp6NifUser
10274: 73a8e8 N14SoftwareCrypto11CRandomImplE
10275: 73a908 N14SoftwareCrypto11CSignerImplE
10276: 73a928 N14SoftwareCrypto13CRijndaelImplE
10277: 73a94a N14SoftwareCrypto13CSoftwareHashE
10278: 73a96c N14SoftwareCrypto13CVerifierImplE
10279: 73a98e N14SoftwareCrypto13MSoftwareHashE
10280: 73a9b0 N14SoftwareCrypto14CDSASignerImplE
10281: 73a9d3 N14SoftwareCrypto14CRSASignerImplE
10282: 73a9f6 N14SoftwareCrypto15CKeyPairGenImplE
10283: 73aa1a N14SoftwareCrypto16CDSAVerifierImplE
10284: 73aa3f N14SoftwareCrypto16CRSAVerifierImplE
10285: 73aa64 N14SoftwareCrypto17CDHKeyPairGenImplE
10286: 73aa8a N14SoftwareCrypto17CKeyAgreementImplE
10287: 73aab0 N14SoftwareCrypto17CSHA224And256ImplE
10288: 73aad6 N14SoftwareCrypto17CSHA384And512ImplE
10289: 73aafc N14SoftwareCrypto18CDSAKeyPairGenImplE
10290: 73ab23 N14SoftwareCrypto18CRSAKeyPairGenImplE
10291: 73ab4a N14SoftwareCrypto20CDSAPrimeCertificateE
10292: 73ab73 N14SoftwareCrypto20CSymmetricCipherImplE
10293: 73ab9c N14SoftwareCrypto21CAsymmetricCipherImplE
10294: 73abc6 N14SoftwareCrypto25CSymmetricBlockCipherImplE
10295: 73abf4 N14SoftwareCrypto26CSymmetricStreamCipherImplE
10296: 73ac23 N14SoftwareCrypto7CDHImplE
10297: 73ac3e N14SoftwareCrypto8CDesImplE
10298: 73ac5a N14SoftwareCrypto8CMD2ImplE
10299: 73ac76 N14SoftwareCrypto8CMD4ImplE
10300: 73ac92 N14SoftwareCrypto8CMD5ImplE
10301: 73acae N14SoftwareCrypto8CMacImplE
10302: 73acca N14SoftwareCrypto8CRSAImplE
10303: 73ace6 N14SoftwareCrypto8CRc2ImplE
10304: 73ad02 N14SoftwareCrypto9C3DesImplE
10305: 73ad1f N14SoftwareCrypto9CArc4ImplE
10306: 73ad3c N14SoftwareCrypto9CCMacImplE
10307: 73ad59 N14SoftwareCrypto9CHMacImplE
10308: 73ad76 N14SoftwareCrypto9CSHA1ImplE
10309: 73ad93 N14SoftwareCrypto9CSHA2ImplE
10310: 73adb0 N14SoftwareCrypto9MSHA2ImplE
10311: 73adcd N9CryptoSpi13MKeyAgreementE
10312: 73ade9 N9CryptoSpi14MSignatureBaseE
10313: 73ae06 N9CryptoSpi16MSymmetricCipherE
10314: 73ae25 N9CryptoSpi17MAsymmetricCipherE
10315: 73ae45 N9CryptoSpi17MKeyPairGeneratorE
10316: 73ae65 N9CryptoSpi20MSymmetricCipherBaseE
10317: 73ae88 N9CryptoSpi21MAsymmetricCipherBaseE
10318: 73aeac N9CryptoSpi4MMacE
10319: 73aebe N9CryptoSpi5MHashE
10320: 73aed1 N9CryptoSpi7MPluginE
10321: 73aee6 N9CryptoSpi7MRandomE
10322: 73aefb N9CryptoSpi7MSignerE
10323: 73af10 N9CryptoSpi9MVerifierE
10391: 74c01a 10CDecryptor
10392: 74c027 10CEncryptor
10393: 74c034 10CRSASigner
10396: 74c05d 11CRSAKeyPair
10400: 74c098 12CRSAVerifier
10401: 74c0a7 13CAESDecryptor
10402: 74c0b7 13CAESEncryptor
10403: 74c0c7 13CDESDecryptor
10404: 74c0d7 13CDESEncryptor
10411: 74c147 13CRC2Decryptor
10412: 74c157 13CRC2Encryptor
10413: 74c167 13CRSAPublicKey
10414: 74c177 13CRSASignature
10416: 74c197 13MCryptoSystem
10417: 74c1a7 14C3DESDecryptor
10418: 74c1b8 14C3DESEncryptor
10423: 74c20d 14CRSAParameters
```
*...truncated, 374 total matches*

## Modem (288 matches)
```
623:  cd17c Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/rebootdriver/rebootdriver_ldd/src/rebootdriverchannel.cpp, line=%d, compiled=Jul 27 2012 17:57:08
625:  cd238 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/rebootdriver/rebootdriver_ldd/src/rebootdriverdevice.cpp, line=%d, compiled=Jul 27 2012 17:57:08
730:  f5430 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessldd_ldd/src/isauserdevice.cpp, line=%d, compiled=Jul 27 2012 17:57:15
732:  f54d8 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessldd_ldd/src/isauserchannel.cpp, line=%d, compiled=Jul 27 2012 17:57:14
733:  f5574 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessldd_ldd/src/isauserchannel.cpp, line=%d, extra info 0x%08x, compiled=Jul 27 2012 17:57:14
734:  f5620 Warning: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessldd_ldd/src/isauserchannel.cpp, line=%d, extra info 0x%x, compiled=Jul 27 2012 17:57:14
782:  fe0a8 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/isakernelapi.cpp, line=%d, compiled=Jul 27 2012 17:57:14
783:  fe148 %s: test flags are:     { NCP_COMMON_MINIOS | CASW_ISCISIMULTIPLEXER_MAX_AMOUNT_OF_PARALLEL_DATACHANNELS | ISCTESTSTUB_SUPPORT_FLAG
786:  fe26c %s:     CASW_ISCISIMULTIPLEXER_MAX_AMOUNT_OF_PARALLEL_DATACHANNELS %d
790:  fe2e8 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/isaaccessextension.cpp, line=%d, compiled=Jul 27 2012 17:57:15
792:  fe3a0 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/isakernelchannel.cpp, line=%d, compiled=Jul 27 2012 17:57:14
793:  fe444 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/isakernelchannel.cpp, line=%d, extra info 0x%08x, compiled=Jul 27 2012 17:57:14
794:  fe4f8 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/queue.cpp, line=%d, compiled=Jul 27 2012 17:57:15
795:  fe590 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/queue.cpp, line=%d, extra info 0x%08x, compiled=Jul 27 2012 17:57:15
796:  fe63c Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/pep.cpp, line=%d, compiled=Jul 27 2012 17:57:15
797:  fe6d0 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/inc/pep.h, line=%d, compiled=Jul 27 2012 17:57:15
799:  fe778 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/peptransceiver.cpp, line=%d, compiled=Jul 27 2012 17:57:14
800:  fe818 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/peptransceiver.cpp, line=%d, extra info 0x%08x, compiled=Jul 27 2012 17:57:14
801:  fe8cc Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/inc/peptransceiver.h, line=%d, compiled=Jul 27 2012 17:57:14
802:  fe96c Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/pipehandler.cpp, line=%d, compiled=Jul 27 2012 17:57:14
803:  fea08 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/pipehandler.cpp, line=%d, extra info 0x%08x, compiled=Jul 27 2012 17:57:14
804:  feab8 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/indicationhandler.cpp, line=%d, compiled=Jul 27 2012 17:57:14
805:  feb5c Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/indicationhandler.cpp, line=%d, extra info 0x%08x, compiled=Jul 27 2012 17:57:14
808:  fec38 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/router.cpp, line=%d, compiled=Jul 27 2012 17:57:14
809:  fecd0 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/router.cpp, line=%d, extra info 0x%08x, compiled=Jul 27 2012 17:57:14
812:  fee1c Warning: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/router.cpp, line=%d, extra info 0x%x, compiled=Jul 27 2012 17:57:14
868: 104f50 DSisaExtension::DeallocateBlock, CMT down -> ignore
870: 10500c DSisaExtension::AllocateBlock, CMT down -> allocate block from symbian
871: 105054 WARNING: DSisaExtension::SendMessage. Message NOT sent to modem because connection to modem NOT existing
1067: 138cd0 PA_MODEM
1123: 144434 PA_MODEM
1827: 1af7a8 : USB revision %s
2356: 20daa8 DDigitiserHandler::CalculateCalibration(): Division by zero!!! %d
2483: 22d174 KernelIsiMessage: OFFSET Invalid
2484: 22d49c KernelIsiMessage: Invalid buffer length
2485: 22d4c4 KernelIsiMessage: Invalid buffer
2486: 22d854 KernelIsiMessage: invalid offset in constructor
2487: 22dc2c KernelIsiMessage: Invalid Receiverobject
2488: 22dc58 KernelIsiMessage: Invalid SenderObject 
2489: 22dc80 KernelIsiMessage: Invalid ExtendedResourceId 
2490: 22dcb0 KernelIsiMessage: Invalid Pefix 
2491: 22df44 KernelIsiMessage: Invalid subblock type
3710: 2fcfbc GL_FRAGMENT_PRECISION_HIGH
4161: 317f05 High Precision not supported
4196: 318424 Precision used with type other than integer, floating point or sampler type
4200: 3184ee Use of float or int without a precision qualifier where the default precision is not defined
4201: 31854b Expression that does not have an intrinsic precision where the default precision is not defined
4222: 318971 Global variables must have the same type (including the same names for structure and field names) and precision
4245: 318c26 GL_FRAGMENT_PRECISION_HIGH
4452: 319ce9 sw_stage_parse_revision
4856: 343748 TVOPTvOutKext: Error initialising VCHI
5615: 3dc962    iSize: %x
6348: 42bdc4 DataServiceIveBc::GetService() - Error initialising VCHI %d
6681: 466c84 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/umacdynamicpowermodemgmtcntx.cpp
6761: 4682c0 20WlanPowerModeMgrBase
6790: 4685b2 26WlanActiveModePowerModeMgr
6792: 4685ec 26WlanDeepPsModePowerModeMgr
6798: 46869d 27WlanLightPsModePowerModeMgr
7456: 524e7c 26CHCICmdQueueDecisionPlugin
7552: 52eaac N3Den23TInitFallibleMsgModeMsgE
7553: 52eacc N3Den24TCheckFallibleMsgModeMsgE
7555: 52eb0f N3Den25TEnableFallibleMsgModeMsgE
7617: 538b74 Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/iscapi_dll/src/iscapi.cpp
9233: 6d19f1 N11MeshMachine21TAggregatedTransitionIN13CoreNetStates14TAddDataClientEN8PRStates14TSendProvisionENS_12TNodeContextIN5ESock20CMMCommsProviderBaseENS5_INS6_13ACFMMNodeBaseENS5_INS_11AMMNodeBaseENS_16TNodeContextBaseENS_17CNodeActivityBaseEEESB_EESB_EEEE
9326: 6d4e64 N12PRActivities20CCommsBinderActivity24TSendCustomFlowProvisionE
9367: 6d556b N13CoreNetStates18TAwaitingProvisionE
9673: 6da91f N4Meta5TMetaIPN5ESock35TDataMonitoringProvisioningInfoBaseEEE
9874: 6dc52b N5ESock35TDataMonitoringConnProvisioningInfoE
9875: 6dc559 N5ESock35TDataMonitoringProvisioningInfoBaseE
9880: 6dc668 N5ESock38TDataMonitoringSubConnProvisioningInfoE
10030: 6de6aa N8PRStates14TSendProvisionE
10032: 6de6e3 N8PRStates15TStoreProvisionE
11073: 7a821c N25BasebandChannelAdaptation11MBcaFactoryE
11074: 7a8247 N25BasebandChannelAdaptation4MBcaE
11075: 7a826a N25BasebandChannelAdaptation6C32Bca11CCommReaderE
11076: 7a829c N25BasebandChannelAdaptation6C32Bca11CCommWriterE
11077: 7a82ce N25BasebandChannelAdaptation6C32Bca14CC32BcaFactoryE
11078: 7a8303 N25BasebandChannelAdaptation6C32Bca16CCommLinkMonitorE
11079: 7a833a N25BasebandChannelAdaptation6C32Bca7CC32BcaE
11080: 7a8367 N25BasebandChannelAdaptation6C32Bca8MC32UserE
11081: 7a8395 N25BasebandChannelAdaptation6C32Bca9CCommBaseE
11283: 7c378b 23CDHCPAddressAcquisition
11676: 7f1b44 34CReadLinkSupervisionTimeoutCommand
11687: 7f1ce0 35CWriteLinkSupervisionTimeoutCommand
11737: 7fa10f 24CBluetoothDutModeManager
11751: 7fb049 29MHCICmdQueueDecisionInterface
11765: 7fca2b 21CUartSupervisionTimer
11769: 7fca90 29MUartSupervisionTimerObserver
11788: 807366 17CSupervisionTimer
11802: 808ba0 Y:/ncp_sw/corecom/modemadaptation.nokia/common/bcaiscadapter/bcatoisc_dll/src/bcatoisc.cpp
11803: 808cdc N25BasebandChannelAdaptation11MBcaFactoryE
11804: 808d07 N25BasebandChannelAdaptation16CBcaToIscFactoryE
11805: 808d37 N25BasebandChannelAdaptation25CNotifyFlowControlMonitorE
11806: 808d70 N25BasebandChannelAdaptation25CNotifyWriteStatusMonitorE
11807: 808da9 N25BasebandChannelAdaptation4MBcaE
11808: 808dcc N25BasebandChannelAdaptation9CBcaToIscE
13054: 904ecd 18CBTBasebandManager
13055: 904ee2 18MBTBasebandHandler
13057: 904f0e 20CBTBasebandConnecter
13065: 904fe3 26CBTSynchronousLinkBaseband
```
*...truncated, 288 total matches*

## Boot (13 matches)
```
4646: 32dec0 DVCDriver::Boot() Unexpected response from boot ROM %u %u
4678: 330e60 DVCDriver::Boot() VideoCore boot ROM not responding (stage 1); HAT=%d, iMessiReadDfc.Queued()=%d
4679: 330ec4 DVCDriver::Boot() VideoCore boot ROM not responding (stage 2); HAT=%d, iMessiReadDfc.Queued()=%d
7203: 4efca6 14CSessionLoader
8541: 6565aa 12CLibUnloader
9091: 6cf604 12CLibUnloader
10879: 78f004 12CLibUnloader
27598:13af630 N4DBSC19CPolicyDomainLoaderE
27599:13af64d N4DBSC19MPolicyDomainLoaderE
28346:142462d 14CApaIconLoader
28364:142480d N11CApaAppList18CApaIdleIconLoaderE
29365:14cdf97 26CSusAdaptationPluginLoader
40603:1e0b0c2 21CUsbAudioPluginLoader
```

## Timer_WDT (401 matches)
```
31:  638d4 Y:/sf/os/kernelhwsrv/kernel/eka/nkern/nk_timer.cpp
291:  b3660 TimerThread
292:  b3670 TimerMutex
445:  b910c BindMsTick
446:  b9b7c EnbMsTick
464:  bceb4 Joystick
482:  bd02c CustomRtc
484:  bea70 !T51.02040816 ns XTI tick
485:  bea90 !T62.5 ns XTI tick
491:  bf24c Joystick
509:  bf3c4 CustomRtc
878: 107670 Joystick
896: 1077e8 CustomRtc
903: 1078d0 Joystick
921: 107a48 CustomRtc
927: 107b14 Joystick
945: 107c8c CustomRtc
950: 107d48 Joystick
968: 107ec0 CustomRtc
985: 109a9c Joystick
1003: 109c14 CustomRtc
1031: 130e04 Joystick
1049: 130f7c CustomRtc
1345: 163f0c Joystick
1363: 164084 CustomRtc
1368: 164140 Joystick
1386: 1642b8 CustomRtc
1391: 164374 Joystick
1409: 1644ec CustomRtc
1414: 1645a8 Joystick
1432: 164720 CustomRtc
1437: 1647dc Joystick
1455: 164954 CustomRtc
1460: 164a38 Joystick
1478: 164bb0 CustomRtc
1487: 164d28 Joystick
1505: 164ea0 CustomRtc
1510: 164f6c Joystick
1528: 1650e4 CustomRtc
1533: 1651d4 Joystick
1551: 16534c CustomRtc
1646: 18f934 Joystick
1664: 18faac CustomRtc
1671: 18fbcc Joystick
1689: 18fd44 CustomRtc
1696: 18fe54 Joystick
1714: 18ffcc CustomRtc
1781: 1a33cc Joystick
1799: 1a3544 CustomRtc
1883: 1bf8b4 Joystick
1901: 1bfa2c CustomRtc
1908: 1bfb50 Joystick
1926: 1bfcc8 CustomRtc
1933: 1bfde8 Joystick
1951: 1bff60 CustomRtc
1956: 1c001c Joystick
1974: 1c0194 CustomRtc
1979: 1c0250 Joystick
1997: 1c03c8 CustomRtc
2002: 1c0488 Joystick
2020: 1c0600 CustomRtc
2025: 1c06bc Joystick
2043: 1c0834 CustomRtc
2135: 1ce442 OperationTimer
2150: 1d16c0 Joystick
2168: 1d1838 CustomRtc
2175: 1d1a44 Joystick
2193: 1d1bbc CustomRtc
2208: 1e40cc Joystick
2226: 1e4244 CustomRtc
2268: 20b26c Joystick
2286: 20b3e4 CustomRtc
2308: 20d614 Joystick
2326: 20d78c CustomRtc
2334: 20d8dc Joystick
2352: 20da54 CustomRtc
2378: 21b108 Joystick
2396: 21b280 CustomRtc
3441: 2d2288 systimer
4445: 319c61 rtos_timer_cancel
4446: 319c73 rtos_timer_init
4447: 319c83 rtos_timer_reset
4456: 319d49 systimer_get_func_table
4991: 3655dc Joystick
5009: 365754 CustomRtc
5069: 36deac timer                   
5207: 36f7f4 timerclient             
5307: 3c7f50 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_symbian/osa_symbian/src/osatimer.cpp
5314: 3c8455 10MWlanTimer
5319: 3c8499 11TimerClient
5329: 3c8565 16MWlanTimerClient
5346: 3c86af 9WlanTimer
5734: 3fda54 Joystick
5752: 3fdbcc CustomRtc
6405: 432c90 CUSTOMDRIVER # DCustomRtcHandler::ClockSync
6411: 4332cc RTC Handler
6421: 433b48 Joystick
6439: 433cc0 CustomRtc
6442: 433dd8 17DCustomRtcHandler
6498: 43c284 Joystick
```
*...truncated, 401 total matches*

## NFC (1 matches)
```
26863:1343da1 Ng>!N>N&NBJ*N$P-NfC1N%P2Nz66N&P8N]49N0C;Ng<<N'P?N(PBN)PCN5GENW5KN7GMNcFNNC8ON3KUNIiVN*PWNh>XN+PYN52]Ne6^Np8_NiLbN&VqNpMsN}F~N%4
```

## WLAN (334 matches)
```
5025: 3694a0 wlan.phys
5026: 369d90 Y:/ext/adapt/wlan.nokia/bsp_specific/spia/src/am_spia.cpp
5203: 36f784 wlanhwinit              
5204: 36f7a0 wlanhwinitmain          
5205: 36f7bc TIWlanHpaCB             
5210: 36f848 TIWlanTestIfac          
5211: 36fa2c wlantrace               
5306: 3c7f04 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_symbian/osa_symbian/src/osadfc.cpp
5307: 3c7f50 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_symbian/osa_symbian/src/osatimer.cpp
5308: 3c7f9c Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/osa_common/inc/osahandle.inl
5309: 3c7fe8 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/osa_common/src/osamemorypool.cpp
5310: 3c8038 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/osa_common/inc/osalist.inl
5312: 3c8174 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_symbian/osa_symbian/src/osaplatformhwchunk.cpp
5313: 3c8448 10MWlanHpaCb
5314: 3c8455 10MWlanTimer
5315: 3c8462 10WlanObject
5316: 3c846f 11MWlanOsaExt
5317: 3c847d 11TIWlanHpaCB
5320: 3c84a7 11WlanDfcImpl
5321: 3c84b5 11WlanOsaImpl
5322: 3c84c3 11WlanPddInit
5323: 3c84d1 12WlanPddShell
5324: 3c84e0 13MWlanPddIface
5325: 3c84f0 13list_iteratorIN4listI6HandleI19WlanPlatformHwChunkEE4NodeES3_E
5326: 3c8531 14MWlanDfcClient
5327: 3c8542 14WlanMemoryPool
5328: 3c8553 15MWlanSpiaClient
5329: 3c8565 16MWlanTimerClient
5330: 3c8578 17MWlanOsaChunkBase
5334: 3c85ca 19WlanPhysicalChannel
5335: 3c85e0 19WlanPlatformHwChunk
5336: 3c85f6 23WlanPlatformHwChunkImpl
5337: 3c8616 4listI6HandleI19WlanPlatformHwChunkEE
5338: 3c864a 6HandleI19WlanPlatformHwChunkE
5339: 3c8669 7WlanDfc
5340: 3c8672 7WlanOsa
5341: 3c867b 8MWlanDfc
5342: 3c8685 8MWlanOsa
5343: 3c868f 8WlanSpia
5345: 3c86a4 9WlanChunk
5346: 3c86af 9WlanTimer
5347: 3c86ba N10WlanHalApi3WhaE
5348: 3c86cd N4listI6HandleI19WlanPlatformHwChunkEE4NodeE
5379: 3c8fec Y:/ext/adapt/wlan.nokia/bsp_specific/hpa/src/am_hpa.cpp
5380: 3c93a4 WLANDriver
5381: 3c940c 10WlanObject
5382: 3c9419 14MWlanDfcClient
5383: 3c9436 7WlanHpa
6568: 446f50 DWlanLogicalChannelBase::Request
6569: 447be4 Checked by wlan.ldd
6570: 447eec WLANLDD: panic
6571: 449420 WLANLDD: panic
6572: 44a4ac WLANLDD: panic
6573: 44a7d8 WLANLDD: panic
6608: 45a680 UMAC: WlanDot11Synchronize::OnTxCompleteEvent(): panic
6623: 45efa0 UMAC: WlanDot11DeauthPending::Fsm: panic
6642: 465f98 WlanDdThread
6643: 465fa8 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_symbian/wlanldd_symbian/src/WlanLogicalChannel.cpp
6644: 466008 WLANLDD 
6645: 466014 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_symbian/wlanldd_symbian/src/EthernetFrameMemMngr.cpp
6646: 466070 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_symbian/wlanldd_symbian/src/MgmtFrameMemMngr.cpp
6647: 4660cc WLAN power handler
6648: 4660e0 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_symbian/wlanldd_symbian/src/wlldddmausablememory.cpp
6649: 466144 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/Umac.cpp
6650: 46618c Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/inc/umaceventdispatcher.inl
6651: 4661e4 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/UmacManagementSideUmacCb.cpp
6652: 466270 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/UmacContextImpl.cpp
6653: 4662c8 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/umacpacketscheduler.cpp
6654: 466320 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_symbian/osa_symbian/src/osadfc.cpp
6655: 46636c Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_symbian/osa_symbian/src/osatimer.cpp
6656: 4663b8 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/osa_common/inc/osahandle.inl
6657: 466404 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/osa_common/src/osamemorypool.cpp
6658: 466454 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/osa_common/inc/osalist.inl
6659: 4664a0 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/UmacMacActionState.cpp
6660: 466574 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/UmacDot11State.cpp
6661: 4665c8 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/umacaddbroadcastwepkey.cpp
6662: 466624 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/umacconfiguretxqueueparams.cpp
6663: 466684 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/umacconfiguretxautoratepolicy.cpp
6664: 4666e4 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/UmacWsaInitiliaze.cpp
6665: 466738 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/UmacWsaConfigure.cpp
6666: 466794 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/UmacWsaJoin.cpp
6667: 4667e4 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/UmacWsaScan.cpp
6668: 466834 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/umacwhastopscan.cpp
6669: 466888 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/UmacWsaDisconnect.cpp
6670: 4668dc Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/UmacWsaReadMib.cpp
6671: 466930 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/UmacWsaWriteMib.cpp
6672: 466984 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/UmacWsaSetBssParameters.cpp
6673: 4669e0 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/UmacWsaAddKey.cpp
6674: 466a30 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/UmacWsaSetPsMode.cpp
6675: 466a84 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/umacwhaconfigurequeue.cpp
6676: 466ae4 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/umacwhaconfigureac.cpp
6677: 466b3c Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/umacwharelease.cpp
6678: 466b90 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/UmacTxRateAdaptation.cpp
6679: 466be8 UMAC: WlanTxRateAdaptation::RatePolicy(): panic: no rates defined
6680: 466c2c Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/umaceventdispatcher.cpp
6681: 466c84 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/umacdynamicpowermodemgmtcntx.cpp
6682: 466d04 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/UmacDot11Associated.cpp
6683: 466d60 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/UmacDot11InfrastructureMode.cpp
6684: 466e6c UMAC: UMAC: WlanDot11MibDefaultConfigure::OnTxCompleteEvent(): panic
6685: 466eb4 Y:/sf/os/wlan/wlan_bearer/wlanldd/wlan_common/umac_common/src/UmacDot11MibDefaultConfigure.cpp
```
*...truncated, 334 total matches*

## Bluetooth (97 matches)
```
2120: 1c8d0c BT_driver
2125: 1cd0f0 bt_driver
7445: 523e18 14CCoreHCIPlugin
7456: 524e7c 26CHCICmdQueueDecisionPlugin
7459: 525d97 8CHciUtil
7464: 527d98 13CHCICmdQTimer
7465: 527da8 15MHCITimerClient
7466: 527dba 16CHCICommandQItem
7467: 527dcd 16MHCICommandQueue
7468: 527de0 18CHCICmdQController
7469: 527df5 21MHCICmdQueueUtilities
7470: 527e0d 23CHCICmdQCompletionTimer
7471: 527e27 23CHCICmdQStarvationTimer
7472: 527e41 24MHCICommandEventObserver
11573: 7f0fd8 15CHCICommandBase
11610: 7f132e 24CHCICommandBaseExtension
11617: 7f13eb 24MHCICompletingEventQuery
11650: 7f17bf 30CHCICompletingEventQueryHelper
11714: 7f9f20 10CHCIServer
11715: 7f9f2d 11CHCISession
11716: 7f9f3b 14MHCIDataFramer
11718: 7f9f5d 16CHCIPowerManager
11719: 7f9f70 17CHCIAccessManager
11722: 7f9fac 18CCoreHCIPluginImpl
11725: 7f9fec 20MHCICommandAllocator
11730: 7fa061 21CHCIPowerClientHandle
11733: 7fa0aa 22MHCICommandQueueClient
11734: 7fa0c3 22MHCIPowerStateObserver
11735: 7fa0dc 22THCIServerAsyncRequest
11736: 7fa0f5 23MHCIClientUsageCallback
11737: 7fa10f 24CBluetoothDutModeManager
11738: 7fa12a 24CHCIServerRequestManager
11740: 7fa160 24MHCICommandEventObserver
11741: 7fa17b 25CBluetoothStateController
11742: 7fa197 25MExtendedCoreHciInterface
11744: 7fa1cf 29THCIServerAsyncCommandRequest
11745: 7fa1ef 34CHCIAccessManagerRadioLegacyKludge
11746: 7fa214 38THCIServerAsyncLocalVersionInfoRequest
11747: 7fa23d 39THCIServerAsyncGetSelftestResultRequest
11748: 7fa267 8MCoreHci
11751: 7fb049 29MHCICmdQueueDecisionInterface
11752: 7fb069 7CHCIQdp
11792: 8073bd 22MHCICommandQueueClient
11799: 808154 21CHCIHardResetListener
11800: 80816c 23CHCIEventReceiveHandler
12381: 869d1e 17TLacMonoBluetooth
12584: 87d1fc 13MHciFunctions
12586: 87d21e 17MHCIEventObserver
12589: 87d263 22MHciChipStatusObserver
12590: 87d27c 9CRadioHci
13052: 904ea6 16CBluetoothSocket
13059: 904f3d 23CBluetoothPhysicalLinks
13061: 904f72 24MBluetoothSocketNotifier
13062: 904f8d 25CBluetoothSynchronousLink
13068: 90503a 29CBluetoothPhysicalLinkMetrics
13071: 90509c 31MBluetoothPhysicalLinksNotifier
13072: 9050be 33MBluetoothSynchronousLinkNotifier
13074: 90510a 37MBluetoothPhysicalLinkMetricsObserver
13075: 905132 38CBluetoothPhysicalLinkMetricSubscriber
13462: 978e4c 10CHCIFacade
13490: 978fd8 13CBluetoothSAP
13535: 9792dc 15MHCIClientUsage
13551: 979408 16MHCIDataObserver
13652: 979d14 21MHCIDataEventObserver
13665: 979e50 22CBluetoothProtocolBase
13679: 979fae 22MHCICommandQueueClient
13733: 97a530 25CBluetoothPrefetchManager
13754: 97a782 26MBluetoothPrefetchNotifier
13762: 97a86f 27MBluetoothControlPlaneToken
13780: 97aa8f 28CBluetoothSecurityResultList
13824: 97b02b 32MBluetoothSecurityResultObserver
13863: 981cd8 Bluetooth SAP
13864: 981ce6 Bluetooth Host Resolver
13865: 981cfe Bluetooth Proxy SAP
13868: 981d27 Bluetooth SDP Net Database
14019: 9a1bb8 20CHciExtensionConduit
18520: c08903 FEJFHciR
18524: c08ae7 FDJEHciR
23113: e3db9b hjFAV0hCi0
24393: ee70e3 FCLTHCI$
27091:135f18f hci<nDn	ws|
27170:137bcc8 # Comms Configurator config file for the ESock Bluetooth module
27226:137c60d filename= bt_v2.prt
27228:137c637 filename= bt_v2.prt
27232:137c679 filename= bt_v2.prt
27235:137c6b0 filename= bt_v2.prt
27238:137c6e9 filename= bt_v2.prt
27241:137c722 filename= bt_v2.prt
28947:1485937 GcJeHcI+
29542:14f185e fJfHcII:
29759:1518b13 G hciAj"i 
30866:16072b9 ``` hCi 
34754:1962c4a 18CBTHciExtensionCmd
34755:1962c5f 18CBTHciExtensionMan
34763:1962d13 23CBTHciExtensionCmdEvent
34765:1962d48 25MVendorSpecificHciConduit
38419:1beb08e "i hciah
```

## GPS (25 matches)
```
16325: ae5820 25CLbsLocationSourceGpsBase
16326: ae583c 29MLbsLocationSourceGpsObserver
16419: afdfa8 12CAgpsChannel
16421: afdfc6 13MAgpsObserver
16454: b04be6 20CLbsLocMonitorGpsBus
16469: b04d77 28MLbsLocMonitorGpsBusObserver
16540: b10dc6 21CAgpsInterfaceHandler
16563: b11047 29MAgpsInterfaceHandlerObserver
16621: b18bfc 15CLbsBtGpsConfig
16622: b18c0e 19CLbsBtGpsConfigImpl
16623: b18c24 23CLbsBtGpsConfigInternal
16624: b18c3e 23MLbsBtGpsConfigObserver
23587: e79df4 AT*NMEASBIASING
30416:159c700 17CGpsDataLinkLayer
30417:159c714 21CGpsSirfMsgCompressor
30418:159c72c 21CGpsSirfProtocolLayer
30423:159c7a7 34CLbsLocationSourceGpsMainLogicBase
30424:159c7cc 34CLbsLocationSourceGpsMainLogicSirf
34538:193544e 13CArrayPtrFlatI24CBlinkingPSKeySubscriberE
34539:193547a 24CBlinkingPSKeySubscriber
34542:19354d3 33MBlinkingPSKeyValueChangeObserver
34543:19354f7 9CArrayFixIP24CBlinkingPSKeySubscriberE
34544:193551f 9CArrayPtrI24CBlinkingPSKeySubscriberE
36867:1ac5c30 iMinUpdateIntervalOnGpsFailure = %d
40098:1d17bbc 11CExifIfdGps
```

## Accelerometer (58 matches)
```
474:  bcf90 Accelerometer
501:  bf328 Accelerometer
888: 10774c Accelerometer
913: 1079ac Accelerometer
937: 107bf0 Accelerometer
960: 107e24 Accelerometer
995: 109b78 Accelerometer
1041: 130ee0 Accelerometer
1355: 163fe8 Accelerometer
1378: 16421c Accelerometer
1401: 164450 Accelerometer
1424: 164684 Accelerometer
1447: 1648b8 Accelerometer
1470: 164b14 Accelerometer
1497: 164e04 Accelerometer
1520: 165048 Accelerometer
1543: 1652b0 Accelerometer
1656: 18fa10 Accelerometer
1681: 18fca8 Accelerometer
1706: 18ff30 Accelerometer
1791: 1a34a8 Accelerometer
1893: 1bf990 Accelerometer
1918: 1bfc2c Accelerometer
1943: 1bfec4 Accelerometer
1966: 1c00f8 Accelerometer
1989: 1c032c Accelerometer
2012: 1c0564 Accelerometer
2035: 1c0798 Accelerometer
2160: 1d179c Accelerometer
2185: 1d1b20 Accelerometer
2218: 1e41a8 Accelerometer
2278: 20b348 Accelerometer
2318: 20d6f0 Accelerometer
2344: 20d9b8 Accelerometer
2370: 212fb0 AccelerometerPDD
2373: 21b044 accelerometer.pdd
2388: 21b1e4 Accelerometer
2397: 21b290 Accelerometer DFC queue
2403: 21b31c Rapido_Accelerometer_Pdd
2410: 21b41d 21DAccPddSensorLIS302DL
2412: 21c0a0 Checked by DAccelerometerLddChannel
2413: 21e340 accelerometer
5001: 3656b8 Accelerometer
5744: 3fdb30 Accelerometer
6431: 433c24 Accelerometer
6508: 43c360 Accelerometer
6531: 43c594 Accelerometer
16625: b24840 20CGraphicsAccelerator
16626: b24857 27CGenericGraphicsAccelerator
16627: b24875 28CHardwareGraphicsAccelerator
16628: b24894 28CSoftwareGraphicsAccelerator
16629: b248b3 29CGraphicsAcceleratorEColor64K
16630: b248d3 30CGraphicsAcceleratorEColor16MA
16631: b248f4 30CGraphicsAcceleratorEColor16MU
16632: b24915 35CGenericHardwareGraphicsAccelerator
16633: b2493b 35CGenericSoftwareGraphicsAccelerator
16757: b3ac70 Accelerometer
16768: b3aed4 24CAccelerometerSsyControl
```

## Magnetometer (23 matches)
```
2417: 21ef00 Checked by DMagnetometerPddChannel
2453: 2277e4 magnetometer.pdd
2454: 2277fc MagnetometerPwrHandler
2455: 227818 MagnetometerDfcQueue
2458: 2279f4 MagnetometerPwrHandler
2459: 227a10 MagnetometerRequestReadySemaphore
2460: 227ab8 22DMagnetometerPddDevice
2461: 227ad1 22DMagnetometerPddSensor
2462: 227aea 23DMagnetometerPddChannel
2463: 227b04 25DMagnetometerPddChannelIf
2464: 227b20 25DMagnetometerPddSensorBus
2465: 227b3c 28DMagnetometerPddPowerHandler
2466: 227b5b 28DMagnetometerPddSensorAK8974
2467: 227b7a 28DMagnetometerPddSensorAMI305
2468: 227b99 33DMagnetometerPddSensorBusPlatform
2469: 2290c4 magnetometer
2470: 2290fc 22DMagnetometerLddDevice
2471: 229115 23DMagnetometerLddChannel
2472: 22912f 25DMagnetometerLddChannelIf
16778: b3d118 Magnetometer
16783: b3d1e8 23CMagnetometerSsyControl
16785: b3d21c 24CMagnetometerSsyProperty
40399:1de13ae 46CSensorDataCompensatorMagneticNorthDataHandler
```

## Proximity (384 matches)
```
74:  7c8bc Checked by VMHalFunction(EVMHalSetCacheSize)
75:  7c8f4 Checked by VMHalFunction(EVMHalSetThrashThresholds)
87:  80c08 Checked by VMHalFunction(EVMHalHalSetSwapThresholds)
88:  80c40 Checked by VMHalFunction(EVMHalSetUsePhysicalAccess)
89:  80c78 Checked by VMHalFunction(EVMHalSetDataWriteSize)
436:  b605c LocalServices
468:  bcf18 ThermalSensor
495:  bf2b0 ThermalSensor
721:  f214c DFlatVirtualSurface::SetBuffer ERROR - flatBuffSize %d > iFlatBuffer.MaxSize() %d
882: 1076d4 ThermalSensor
907: 107934 ThermalSensor
931: 107b78 ThermalSensor
954: 107dac ThermalSensor
989: 109b00 ThermalSensor
1035: 130e68 ThermalSensor
1162: 148de4 Checked by Hal function EPowerHalSwitchOff
1163: 148e10 Checked by Hal Function EPowerHalSetPointerSwitchesOn
1164: 148e48 Checked by Hal function EPowerHalSetCaseOpenSwitchesOn
1165: 148e80 Checked by Hal function EPowerHalSetCaseCloseSwitchesOff
1349: 163f70 ThermalSensor
1372: 1641a4 ThermalSensor
1395: 1643d8 ThermalSensor
1418: 16460c ThermalSensor
1441: 164840 ThermalSensor
1464: 164a9c ThermalSensor
1491: 164d8c ThermalSensor
1514: 164fd0 ThermalSensor
1537: 165238 ThermalSensor
1650: 18f998 ThermalSensor
1675: 18fc30 ThermalSensor
1700: 18feb8 ThermalSensor
1785: 1a3430 ThermalSensor
1887: 1bf918 ThermalSensor
1912: 1bfbb4 ThermalSensor
1937: 1bfe4c ThermalSensor
1960: 1c0080 ThermalSensor
1983: 1c02b4 ThermalSensor
2006: 1c04ec ThermalSensor
2029: 1c0720 ThermalSensor
2154: 1d1724 ThermalSensor
2179: 1d1aa8 ThermalSensor
2212: 1e4130 ThermalSensor
2272: 20b2d0 ThermalSensor
2300: 20c8c4 Checked by Hal function EDigitiserHalSetXYInputCalibration
2301: 20c904 Checked by Hal function EDigitiserHalSetXYState
2312: 20d678 ThermalSensor
2338: 20d940 ThermalSensor
2382: 21b16c ThermalSensor
2915: 288728 proxima_osram
2920: 288778 proxima_citizen
2924: 2887c4 proxima_everlight
3338: 2c229c proxima_osram
3343: 2c22ec proxima_citizen
3347: 2c2338 proxima_everlight
4933: 353f50 Checked by Hal function EDisplayHalSpecificScreenInfo
4936: 353ff4 Checked by Hal function EDisplayHalSetDisplayContrast
4937: 3540ac Checked by Hal function EDisplayHalSetDisplayBrightness
4938: 3540e4 Checked by Hal function EDisplayHalSetBacklightOn
4939: 354118 Checked by Hal function EDisplayHalSetMode
4940: 354144 Checked by Hal function EDisplayHalSetState
4941: 354170 Checked by Hal function EDisplayHalSetSecure
4942: 3541a0 Checked by Hal function EDisplayHalSetBacklightBehavior
4943: 3543c4 Checked by Hal function EDisplayHalSetBacklightOnTime
4944: 3543fc Checked by Hal function EDisplayHalSetDisplayState
4980: 363ff0 Failed to create ThermalSensingCb_DfcQ
4981: 3647f4 ThermalSensingCb_DfcQ
4982: 364810 ThermalSensingHWAdaptationBTempDFCThread
4983: 364840 ThermalSensorSemaphore
4984: 364858 ThermalSensingHWAdaptationBTemp FAULT >Y:/ext/adapt/devicesrv.nokia.adapt/thermal/thermalsensinghwadaptation/BTemp/ThermalSensingHWAdaptationBTemp.cpp
4985: 364910 16DThermalSensorHW
4986: 364923 21DThermalSensorHWBTemp
4995: 365640 ThermalSensor
5010: 365764 ThermalSensorLock
5011: 36577c ThermalSensorDFCThread
5012: 365794 ThermalSensor FAULT >Y:/ext/os/devicesrv.os/thermal/thermalsensor/ThermalSensor.cpp
5013: 3657f0 13ThermalSensor
5414: 3d2480 DTraceCoreRouter::iProcessSubscriptionEventsDfc.Enque() returns EFalse
5738: 3fdab8 ThermalSensor
5915: 40bf28 DCamUseCase::ReturnBuffer() Your code is returning buffers incorrectly,  maybe the same buffer has been given back twice? Consider also the use of RCam::SendInputData which implicity returns the buffer.
6023: 416220 DCamSharedVid::EndOfNAL() too many NALs have come through,  increase iMaxNumberNALs!
6376: 42e030 fsys_proxy
6425: 433bac ThermalSensor
6502: 43c2e8 ThermalSensor
6525: 43c51c ThermalSensor
7007: 4bc6b3 13CProxyConsole
7068: 4ce524 Add Proxy Drive
7106: 4d3dd4 Mount Proxy Drive
7125: 4dbc04 Add File System Proxy Drive
7126: 4dbc28 Remove Proxy Drive
7127: 4dbc3c Dismount Proxy Drive
7185: 4efb86 11CProxyDrive
7199: 4efc62 14CExtProxyDrive
7206: 4efcda 15CProxyDriveBody
7210: 4efd25 16CLocalProxyDrive
7214: 4efd75 18CBaseExtProxyDrive
7219: 4efdde 18CProxyDriveFactory
7228: 4efea8 21CExtProxyDriveFactory
7428: 5228a5 24TAllowMediumRemovalState
7433: 52292f 26TPreventMediumRemovalState
7441: 522a1f 31TPreventMediumRemovalSenseState
```
*...truncated, 384 total matches*

## Touch (56 matches)
```
479:  bcff8 TouchIC2
506:  bf390 TouchIC2
893: 1077b4 TouchIC2
918: 107a14 TouchIC2
942: 107c58 TouchIC2
965: 107e8c TouchIC2
1000: 109be0 TouchIC2
1046: 130f48 TouchIC2
1360: 164050 TouchIC2
1383: 164284 TouchIC2
1406: 1644b8 TouchIC2
1429: 1646ec TouchIC2
1452: 164920 TouchIC2
1475: 164b7c TouchIC2
1502: 164e6c TouchIC2
1525: 1650b0 TouchIC2
1548: 165318 TouchIC2
1661: 18fa78 TouchIC2
1686: 18fd10 TouchIC2
1711: 18ff98 TouchIC2
1796: 1a3510 TouchIC2
1898: 1bf9f8 TouchIC2
1923: 1bfc94 TouchIC2
1948: 1bff2c TouchIC2
1971: 1c0160 TouchIC2
1994: 1c0394 TouchIC2
2017: 1c05cc TouchIC2
2040: 1c0800 TouchIC2
2165: 1d1804 TouchIC2
2190: 1d1b88 TouchIC2
2223: 1e4210 TouchIC2
2243: 1e7000 DAtmelBootFlash::CheckBit bit bigger than 8, bit = %d
2244: 1e9200 DAtmelChargerDectector::DoCreate - Setting aAtmelDfcQ failed
2283: 20b3b0 TouchIC2
2287: 20b3f4 AtmelLock
2288: 20b404 AtmelControllerReadyLock
2289: 20b424 AtmelPowerLock
2290: 20b438 AtmelThread
2291: 20b448 TouchIcAtmel
2292: 20b458 DAtmelSensor::DoCreate() - Creation of iAtmelChargerDectector failed
2295: 20b54e 11MTouchIcApi
2296: 20b55c 12DAtmelSensor
2297: 20b56b 15DAtmelBootFlash
2298: 20b57d 16DAtmelIrqHandler
2299: 20b590 22DAtmelChargerDectector
2323: 20d758 TouchIC2
2349: 20da20 TouchIC2
2353: 20da64 TouchDriverThread
2360: 20db86 17DInterfaceTouchIc
2393: 21b24c TouchIC2
5006: 365720 TouchIC2
5749: 3fdb98 TouchIC2
6436: 433c8c TouchIC2
6513: 43c3c8 TouchIC2
6536: 43c5fc TouchIC2
24026: ec3885 EpicTouch
```

## Keyboard (86 matches)
```
475:  bcfa4 KeypadExpander
480:  bd008 KeypadExpander2
502:  bf33c KeypadExpander
507:  bf3a0 KeypadExpander2
889: 107760 KeypadExpander
894: 1077c4 KeypadExpander2
914: 1079c0 KeypadExpander
919: 107a24 KeypadExpander2
938: 107c04 KeypadExpander
943: 107c68 KeypadExpander2
961: 107e38 KeypadExpander
966: 107e9c KeypadExpander2
996: 109b8c KeypadExpander
1001: 109bf0 KeypadExpander2
1042: 130ef4 KeypadExpander
1047: 130f58 KeypadExpander2
1356: 163ffc KeypadExpander
1361: 164060 KeypadExpander2
1379: 164230 KeypadExpander
1384: 164294 KeypadExpander2
1402: 164464 KeypadExpander
1407: 1644c8 KeypadExpander2
1425: 164698 KeypadExpander
1430: 1646fc KeypadExpander2
1448: 1648cc KeypadExpander
1453: 164930 KeypadExpander2
1471: 164b28 KeypadExpander
1476: 164b8c KeypadExpander2
1498: 164e18 KeypadExpander
1503: 164e7c KeypadExpander2
1521: 16505c KeypadExpander
1526: 1650c0 KeypadExpander2
1544: 1652c4 KeypadExpander
1549: 165328 KeypadExpander2
1657: 18fa24 KeypadExpander
1662: 18fa88 KeypadExpander2
1682: 18fcbc KeypadExpander
1687: 18fd20 KeypadExpander2
1707: 18ff44 KeypadExpander
1712: 18ffa8 KeypadExpander2
1792: 1a34bc KeypadExpander
1797: 1a3520 KeypadExpander2
1894: 1bf9a4 KeypadExpander
1899: 1bfa08 KeypadExpander2
1919: 1bfc40 KeypadExpander
1924: 1bfca4 KeypadExpander2
1944: 1bfed8 KeypadExpander
1949: 1bff3c KeypadExpander2
1967: 1c010c KeypadExpander
1972: 1c0170 KeypadExpander2
1990: 1c0340 KeypadExpander
1995: 1c03a4 KeypadExpander2
2013: 1c0578 KeypadExpander
2018: 1c05dc KeypadExpander2
2036: 1c07ac KeypadExpander
2041: 1c0810 KeypadExpander2
2161: 1d17b0 KeypadExpander
2166: 1d1814 KeypadExpander2
2186: 1d1b34 KeypadExpander
2191: 1d1b98 KeypadExpander2
2219: 1e41bc KeypadExpander
2224: 1e4220 KeypadExpander2
2227: 1e4288 KeypadExpanderThread
2279: 20b35c KeypadExpander
2284: 20b3c0 KeypadExpander2
2319: 20d704 KeypadExpander
2324: 20d768 KeypadExpander2
2345: 20d9cc KeypadExpander
2350: 20da30 KeypadExpander2
2389: 21b1f8 KeypadExpander
2394: 21b25c KeypadExpander2
5002: 3656cc KeypadExpander
5007: 365730 KeypadExpander2
5745: 3fdb44 KeypadExpander
5750: 3fdba8 KeypadExpander2
6432: 433c38 KeypadExpander
6437: 433c9c KeypadExpander2
6509: 43c374 KeypadExpander
6514: 43c3d8 KeypadExpander2
6532: 43c5a8 KeypadExpander
6537: 43c60c KeypadExpander2
12859: 8a7735 13CAammKeyboard
26188:128f881 21CKeypadToneObserverAO
34666:194d90c 16CKeyboardDecoder
34713:195b543 15CKeyboardLayout
34723:195b61d 23CStandardKeyboardLayout
```

## LED (535 matches)
```
132:  8c838 Checked by RThread::IsExceptionHandled
165:  9da48   queue callback failed, queueing for cleanup
223:  b1be0 would have failed - 
224:  b1bf8 failed - 
606:  cc64c  (disabled)
622:  cd0e0 Kern::DynamicDfcQCreate() failed
623:  cd17c Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/rebootdriver/rebootdriver_ldd/src/rebootdriverchannel.cpp, line=%d, compiled=Jul 27 2012 17:57:08
625:  cd238 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/rebootdriver/rebootdriver_ldd/src/rebootdriverdevice.cpp, line=%d, compiled=Jul 27 2012 17:57:08
718:  efa74 Failed to create IVE Surface Manager
730:  f5430 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessldd_ldd/src/isauserdevice.cpp, line=%d, compiled=Jul 27 2012 17:57:15
732:  f54d8 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessldd_ldd/src/isauserchannel.cpp, line=%d, compiled=Jul 27 2012 17:57:14
733:  f5574 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessldd_ldd/src/isauserchannel.cpp, line=%d, extra info 0x%08x, compiled=Jul 27 2012 17:57:14
734:  f5620 Warning: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessldd_ldd/src/isauserchannel.cpp, line=%d, extra info 0x%x, compiled=Jul 27 2012 17:57:14
741:  f6460 %s: Compiled=%s - %s
782:  fe0a8 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/isakernelapi.cpp, line=%d, compiled=Jul 27 2012 17:57:14
790:  fe2e8 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/isaaccessextension.cpp, line=%d, compiled=Jul 27 2012 17:57:15
792:  fe3a0 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/isakernelchannel.cpp, line=%d, compiled=Jul 27 2012 17:57:14
793:  fe444 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/isakernelchannel.cpp, line=%d, extra info 0x%08x, compiled=Jul 27 2012 17:57:14
794:  fe4f8 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/queue.cpp, line=%d, compiled=Jul 27 2012 17:57:15
795:  fe590 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/queue.cpp, line=%d, extra info 0x%08x, compiled=Jul 27 2012 17:57:15
796:  fe63c Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/pep.cpp, line=%d, compiled=Jul 27 2012 17:57:15
797:  fe6d0 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/inc/pep.h, line=%d, compiled=Jul 27 2012 17:57:15
799:  fe778 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/peptransceiver.cpp, line=%d, compiled=Jul 27 2012 17:57:14
800:  fe818 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/peptransceiver.cpp, line=%d, extra info 0x%08x, compiled=Jul 27 2012 17:57:14
801:  fe8cc Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/inc/peptransceiver.h, line=%d, compiled=Jul 27 2012 17:57:14
802:  fe96c Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/pipehandler.cpp, line=%d, compiled=Jul 27 2012 17:57:14
803:  fea08 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/pipehandler.cpp, line=%d, extra info 0x%08x, compiled=Jul 27 2012 17:57:14
804:  feab8 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/indicationhandler.cpp, line=%d, compiled=Jul 27 2012 17:57:14
805:  feb5c Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/indicationhandler.cpp, line=%d, extra info 0x%08x, compiled=Jul 27 2012 17:57:14
808:  fec38 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/router.cpp, line=%d, compiled=Jul 27 2012 17:57:14
809:  fecd0 Assertion failed: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/router.cpp, line=%d, extra info 0x%08x, compiled=Jul 27 2012 17:57:14
812:  fee1c Warning: file=Y:/ncp_sw/corecom/modemadaptation.nokia/hw79/isce/isaaccessextension_dll/src/router.cpp, line=%d, extra info 0x%x, compiled=Jul 27 2012 17:57:14
819:  ff0d7 14DEnableEnabled
820:  ff0e8 15DEnabledEnabled
824:  ff130 16DCreatingEnabled
825:  ff143 16DDisabledEnabled
826:  ff156 16DEnabledDisabled
827:  ff169 17DDisabledDisabled
833:  ff20f 8DEnabled
837:  ff238 9DDisabled
856: 103c38 DSisaWrapper::SisaResetGenerate() -  ISA uncontrolled reset
1019: 130570 DMA request failed
1020: 130678 DmaCpyInit(): DfcQ creation failed
1021: 1306a0 DmaCpyInit(): Dma channel open failed
1022: 1306d0 DmaCpyInit(): DMA new request failed
1025: 1307f8 OAM_DmaWrite(): DMA request failed
1069: 1405c0 Assertion 'm.Client() != NULL' failed;
1071: 140660 Assertion 'pS != NULL' failed;
1078: 140844 Assertion 'iIsa != NULL' failed;
1080: 1408e0 Assertion 'iMainDfcq != NULL' failed;
1087: 140a00 Assertion 'currentThread != NULL' failed;
1091: 140ae0 Assertion 'e != NULL' failed;
1093: 140b6c Assertion 'iIsa' failed;
1095: 140bf0 Assertion 'iRequest == NULL' failed;
1097: 140c80 Assertion 'iServiceFn == NULL' failed;
1099: 140d14 Assertion 'iSema != NULL' failed;
1101: 140da4 Assertion '!iChannelOpen' failed;
1103: 140e34 Assertion 'iRequest != NULL' failed;
1105: 140ec4 Assertion 'iServiceFn != NULL' failed;
1107: 140f58 Assertion 'iEngine != NULL' failed;
1111: 1413f4 Assertion 'NULL' failed;
1113: 14147c Assertion 'iSecLddAesMutex != NULL' failed;
1127: 145c10 Assertion 'gSecurityKext' failed;
1129: 145ca0 Assertion 'e != NULL' failed;
1132: 146144 Assertion 'NULL' failed;
1136: 146208 Assertion 'e != NULL' failed;
1138: 146294 Assertion '!iChannelOpen' failed;
1140: 146324 Assertion 'iRequest != NULL' failed;
1142: 1463b4 Assertion 'iServiceFn != NULL' failed;
1144: 146448 Assertion 'iSema != NULL' failed;
1146: 1464d8 Assertion 'iEngine != NULL' failed;
1148: 146568 Assertion 'iIsa' failed;
1276: 14da20                    | KMMCStatCardECCDisabled
1283: 14db50                    | KMMCStatErrCardECCFailed
1737: 190524 17DUsbCableDetector
1752: 190661 20DUsbCableDetectorISA
1818: 1a4930 USB PSL: BaseUsbOtgHostHalHcdManager, hal_isr_bind() called
1819: 1a496c USB PSL: BaseUsbOtgHostHalHcdManager, hal_isr_unbind() called
1835: 1b2ccc Failed to register to OTG Feature Change notification - %d
1836: 1b2d08 Failed to register to USB Status Change notification - %d
1846: 1b9044 %s: port %d reset failed
1855: 1b9844 %s: port %d power on failed, %s
1864: 1bae04 usbd_setup_pipe: failed to start endpoint, %s
1873: 1bba0c Device creation failed
2087: 1c13bd SET_ADDR_FAILED
2088: 1c13d5 CANCELLED
2129: 1cd7c4 Failed to instantiate I2C Power Handler
2145: 1d13f4 Failed to bind GPIO interrupt handler for SWITCH
2201: 1d2a14 Failed to create DFC queue ( %d )
2244: 1e9200 DAtmelChargerDectector::DoCreate - Setting aAtmelDfcQ failed
2292: 20b458 DAtmelSensor::DoCreate() - Creation of iAtmelChargerDectector failed
2303: 20cd9c Failed to create DFC queue ( %d )
2818: 2726a4 %s: signature check failed
2831: 273634 blemish_test_downscaled_image
2911: 287888 generic_led
2917: 288748 gemini3_lumiled
2921: 288788 polaris3_lumiled
3246: 2b5b5c led_gpio
3253: 2b6bb8 %s: signature verification failed.
3256: 2b6c28 %s: lazy_read failed, possible truncated VLL
```
*...truncated, 535 total matches*

## CCP2 (8 matches)
```
2905: 2869e4 ftCCP2HIST
3288: 2baaa0 ../../../../interface/vchi/message_drivers/videocore/mphi_ccp2.c
4291: 318fd4 ccp2_get_func_table
4597: 329988 Failed to allocate CCP2 driver
4598: 3299a8 Failed to initialise CCP2 driver
4707: 331c8c BulkRxRequested[Ccp2]
4713: 331d28 BulkRxDataArrived[Ccp2]
4736: 335230 VCHI: CCP2 CRC failure
```

