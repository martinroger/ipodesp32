#define L0x00_RequestExtendedInterfaceMode  0x03
#define L0x00_EnterExtendedInterfaceMode    0x05
#define L0x00_ExitExtendedInterfaceMode     0x06
#define L0x00_RequestiPodName               0x07
#define L0x00_RequestiPodSoftwareVersion    0x09
#define L0x00_RequestiPodSerialNum          0x0B
#define L0x00_RequestiPodModelNum           0x0D
#define L0x00_RequestLingoProtocolVersion   0x0F
#define L0x00_IdentifyDeviceLingoes         0x13
#define L0x00_RetAccessoryInfo              0x28

// Possible values for L0x00 0x02 iPodAck
#define iPodAck_OK                          0x00
#define iPodAck_CmdFailed                   0x02
#define iPodAck_BadParam                    0x04
#define iPodAck_UnknownID                   0x05
#define iPodAck_CmdPending                  0x06
#define iPodAck_TimedOut                    0x0F
#define iPodAck_CmdUnavail                  0x10
#define iPodAck_LingoBusy                   0x14