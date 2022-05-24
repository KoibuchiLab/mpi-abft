typedef unsigned char       uint8;
typedef signed int          int32;
typedef unsigned int        uint32;

//Transfer Rate
#define SET_RATE 1

//Xbar Reg Offset
#define OFFSET_REG_SW_RESET 0x0

#define OFFSET_REG_RATE_SIZE_SL3_0 0x90004
#define OFFSET_REG_RATE_SIZE_SL3_1 0x94004
#define OFFSET_REG_RATE_SIZE_SL3_2 0x98004
#define OFFSET_REG_RATE_SIZE_SL3_3 0x9C004
#define OFFSET_REG_RATE_SIZE_SL3_4 0xA0004
#define OFFSET_REG_RATE_SIZE_SL3_5 0xA4004
#define OFFSET_REG_RATE_SIZE_SL3_6 0xA8004
#define OFFSET_REG_RATE_SIZE_SL3_7 0xAC004

#define OFFSET_REG_RATE_CLK_SL3_0  0x90008
#define OFFSET_REG_RATE_CLK_SL3_1  0x94008
#define OFFSET_REG_RATE_CLK_SL3_2  0x98008
#define OFFSET_REG_RATE_CLK_SL3_3  0x9C008
#define OFFSET_REG_RATE_CLK_SL3_4  0xA0008
#define OFFSET_REG_RATE_CLK_SL3_5  0xA4008
#define OFFSET_REG_RATE_CLK_SL3_6  0xA8008
#define OFFSET_REG_RATE_CLK_SL3_7  0xAC008

//Xbar Reg Value
#define REG_VALUE_SW_RESET_00      0x10
//#define REG_VALUE_SW_RESET_01      0xF
#define REG_VALUE_SW_RESET_01      0x5
#define REG_VALUE_SW_RESET_02      0x0

#define REG_VALUE_RATE_SIZE        0x400
#define REG_VALUE_RATE_CLK         0x10

//SG Offset
#define OFFSET_SG_MYNODE           0x40000
#define OFFSET_SG_XBAR             0x40014
#define OFFSET_SG_READ             0x41044
#define OFFSET_SG_WRITE            0x42044

//#define SG_OFFSET_SG_BASE_0        0x50000
//#define SG_OFFSET_SG_BASE_1        0x50100
#define SG_OFFSET_SG_BASE_2        0x50000

#define OFFSET_SG_MY_NODE_NUM      0x0
#define OFFSET_SG_NODE_CONNECT     0x8
#define OFFSET_SG_NODE_CONNECT2    0xC
#define OFFSET_SG_BARRIER          0x38

#define OFFSET_SG_SW_SIZE           0x10
#define OFFSET_SG_DATASIZE          0x20
#define OFFSET_SG_MPI_TYPE          0x24
#define OFFSET_SG_TO_SEND_NODE      0x28
#define OFFSET_SG_DMA_READ_CH       0x2C
#define OFFSET_SG_FROM_SEND_NODE    0x30
#define OFFSET_SG_DMA_WRITE_CH      0x34
#define OFFSET_SG_BARRIER_VALID     0x38
#define OFFSET_SG_COUNTER           0x3C

#define OFFSET_SG_SOURCE_ADDR_CH0   0x40
#define OFFSET_SG_SOURCE_LENGTH_CH0 0x44
#define OFFSET_SG_SOURCE_ADDR_CH1   0x48
#define OFFSET_SG_SOURCE_LENGTH_CH1 0x4C
#define OFFSET_SG_SOURCE_ADDR_CH2   0x50
#define OFFSET_SG_SOURCE_LENGTH_CH2 0x54
#define OFFSET_SG_SOURCE_ADDR_CH3   0x58
#define OFFSET_SG_SOURCE_LENGTH_CH3 0x5C
#define OFFSET_SG_SOURCE_ADDR_CH4   0x60
#define OFFSET_SG_SOURCE_LENGTH_CH4 0x64
#define OFFSET_SG_SOURCE_ADDR_CH5   0x68
#define OFFSET_SG_SOURCE_LENGTH_CH5 0x6C
#define OFFSET_SG_SOURCE_ADDR_CH6   0x70
#define OFFSET_SG_SOURCE_LENGTH_CH6 0x74
#define OFFSET_SG_SOURCE_ADDR_CH7   0x78
#define OFFSET_SG_SOURCE_LENGTH_CH7 0x7C
#define OFFSET_SG_DEST_ADDR_CH0     0x80
#define OFFSET_SG_DEST_LENGTH_CH0   0x84
#define OFFSET_SG_DEST_ADDR_CH1     0x88
#define OFFSET_SG_DEST_LENGTH_CH1   0x8C
#define OFFSET_SG_DEST_ADDR_CH2     0x90
#define OFFSET_SG_DEST_LENGTH_CH2   0x94
#define OFFSET_SG_DEST_ADDR_CH3     0x98
#define OFFSET_SG_DEST_LENGTH_CH3   0x9C
#define OFFSET_SG_DEST_ADDR_CH4     0xA0
#define OFFSET_SG_DEST_LENGTH_CH4   0xA4
#define OFFSET_SG_DEST_ADDR_CH5     0xA8
#define OFFSET_SG_DEST_LENGTH_CH5   0xAC
#define OFFSET_SG_DEST_ADDR_CH6     0xB0
#define OFFSET_SG_DEST_LENGTH_CH6   0xB4
#define OFFSET_SG_DEST_ADDR_CH7     0xB8
#define OFFSET_SG_DEST_LENGTH_CH7   0xBC

#define OFFSET_SG_DMA_READ_GO       0xC0
#define OFFSET_SG_DMA_WRITE_GO      0xC4
#define OFFSET_SG_END               0xD0
#define OFFSET_SG_COUNT             0x51008
#define OFFSET_SG_KICK              0x51000

//SG Valuei
#define VALUE_SG                    0x1

#define VALUE_NODE_NUM00         0x1
#define VALUE_NODE_NUM01         0x2
#define VALUE_NODE_NUM02         0x4
#define VALUE_NODE_NUM03         0x8
#define VALUE_NODE_NUM04         0x10
#define VALUE_NODE_NUM05         0x20
#define VALUE_NODE_NUM06         0x40
#define VALUE_NODE_NUM07         0x80
#define VALUE_NODE_8NODE         0xFF

#define VALUE_NODE_CONNECT          0x76543210
#define VALUE_NODE_CONNECT2         0x0FEDCBA9

#define VALUE_SW_SIZE               0x3FFF

#define VALUE_MPI_TYPE_SEND_RECV    0x1
#define VALUE_MPI_TYPE_SCATTER      0x2
#define VALUE_MPI_TYPE_GATHER       0x4
#define VALUE_MPI_TYPE_BCAST        0x8
#define VALUE_MPI_TYPE_ALLGATHER    0x10
#define VALUE_MPI_TYPE_ALLTOALL     0x20
#define VALUE_MPI_TYPE_SENDRECV     0x40

#define VALUE_DMA_CH0               0x1
#define VALUE_DMA_CH1               0x2
#define VALUE_DMA_CH2               0x4
#define VALUE_DMA_CH3               0x8
#define VALUE_DMA_CH4               0x10
#define VALUE_DMA_CH5               0x20
#define VALUE_DMA_CH6               0x40
#define VALUE_DMA_CH7               0x80
#define VALUE_DMA_CH_ALL            0xFF

#define VALUE_SG_COUNTER            0x5

#define VALUE_SOURCE_ADDR           0x00000000
#define VALUE_DEST_ADDR0            0x00000000
#define VALUE_DEST_ADDR1            0x00400000

#define VALUE_SG_BARRIER_OFF        0x0

#define VALUE_SG_END_00             0x0
#define VALUE_SG_END_01             0x1
#define VALUE_SG_END_02             0x2
#define VALUE_SG_COUNT01            0x1
#define VALUE_SG_KICK               0x1






