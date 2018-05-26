#ifndef PIM_H
#define PIM_H

#define PIM_V1_VERSION         0x10000000

#define PIM_V1_QUERY           0
#define PIM_V1_REGISTER        1
#define PIM_V1_REGISTER_STOP   2
#define PIM_V1_JOIN_PRUNE      3
#define PIM_V1_RP_REACHABLE    4
#define PIM_V1_ASSERT          5
#define PIM_V1_GRAFT           6
#define PIM_V1_GRAFT_ACK       7
#define PIM_V1_MODE            8

#define PIM_MSG_TYPE_HELLO      (0)
#define PIM_MSG_TYPE_REGISTER   (1)
#define PIM_MSG_TYPE_REG_STOP   (2)
#define PIM_MSG_TYPE_JOIN_PRUNE (3)
#define PIM_MSG_TYPE_BOOTSTRAP  (4)
#define PIM_MSG_TYPE_ASSERT     (5)
#define PIM_MSG_TYPE_GRAFT      (6)
#define PIM_MSG_TYPE_GRAFT_ACK  (7)
#define PIM_MSG_TYPE_CANDIDATE  (8)

#define PIM_MSG_HDR_OFFSET_VERSION(pim_msg) (pim_msg)
#define PIM_MSG_HDR_OFFSET_TYPE(pim_msg) (pim_msg)
#define PIM_MSG_HDR_OFFSET_RESERVED(pim_msg) (((char *)(pim_msg)) + 1)
#define PIM_MSG_HDR_OFFSET_CHECKSUM(pim_msg) (((char *)(pim_msg)) + 2)

#define PIM_MSG_HDR_GET_VERSION(pim_msg) ((*(uint8_t*) PIM_MSG_HDR_OFFSET_VERSION(pim_msg)) >> 4)
#define PIM_MSG_HDR_GET_TYPE(pim_msg) ((*(uint8_t*) PIM_MSG_HDR_OFFSET_TYPE(pim_msg)) & 0xF)
#define PIM_MSG_HDR_GET_CHECKSUM(pim_msg) (*(uint16_t*) PIM_MSG_HDR_OFFSET_CHECKSUM(pim_msg))

#endif