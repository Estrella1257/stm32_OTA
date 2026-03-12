 #ifndef NVS_TYPES_H
 #define NVS_TYPES_H
 
 #include <stdint.h>
 #include "system_memory_map.h"
 
 /* ===================== NVS Const ===================== */
 #define NVS_SECTOR_MAGIC         0x4E565353  /* 'NVSS' */
 #define NVS_RECORD_MAGIC         0x4E565352  /* 'NVSR' */
 #define NVS_LAYOUT_VERSION       0x0001
 #define NVS_RECORD_VERSION       0x0001
 
 #define NVS_COMMIT_ERASED        0xFFFFFFFF
 #define NVS_COMMIT_DONE          0xA5A55A5A
 
 #define NVS_MAX_RESERVED_WORDS   8
 
 #pragma pack(push, 4)
 
 /* 业务“上帝结构体”：只放掉电后必须保留的参数 */
 typedef struct
 {
     uint32_t total_odometer_m;      /* 总里程 (m) */
     uint16_t wheel_circum_mm;       /* 轮周长 */
     uint16_t speed_limit_kmh_x10;   /* 限速 *10 */
     float    pid_kp;
     float    pid_ki;
     float    pid_kd;
     float    kalman_q;
     float    kalman_r;
     int16_t  imu_pitch_zero_x100;   /* 姿态零偏 */
     int16_t  imu_roll_zero_x100;
     uint32_t reserved[NVS_MAX_RESERVED_WORDS];
 } SystemConfig_t;
 
 /* 扇区头：用于区分有效扇区并比较代数 */
 typedef struct
 {
     uint32_t magic;         /* NVS_SECTOR_MAGIC */
     uint16_t layout_ver;    /* NVS_LAYOUT_VERSION */
     uint16_t hdr_len;       /* sizeof(NvsSectorHeader_t) */
     uint32_t generation;    /* GC 切换代数，越大越新 */
     uint32_t reserved0;
     uint32_t hdr_crc32;     /* 本结构 CRC（计算时该字段置0） */
 } NvsSectorHeader_t;
 
 /* 记录头：用于扫描、校验、版本演进 */
 typedef struct
 {
     uint32_t magic;         /* NVS_RECORD_MAGIC */
     uint16_t rec_ver;       /* NVS_RECORD_VERSION */
     uint16_t rec_len;       /* sizeof(NvsRecord_t) */
     uint32_t seq;           /* 单调递增序号 */
     uint32_t payload_len;   /* sizeof(SystemConfig_t) */
     uint32_t payload_crc32; /* payload CRC32 */
 } NvsRecordHeader_t;
 
 /* 完整记录：commit 必须最后写 */
 typedef struct
 {
     NvsRecordHeader_t hdr;
     SystemConfig_t    payload;
     uint32_t          rec_crc32;  /* hdr + payload CRC32 */
     uint32_t          commit;     /* NVS_COMMIT_DONE 才算有效 */
 } NvsRecord_t;
 
 #pragma pack(pop)
 
 /* 扫描结果 */
 typedef struct
 {
     uint8_t  valid;             /* 0=无有效记录, 1=有 */
     uint32_t sector_addr;       /* 记录所在扇区 */
     uint32_t sector_generation; /* 扇区代数 */
     uint32_t last_record_addr;  /* 最新记录地址 */
     uint32_t next_write_addr;   /* 下一条写入地址 */
     uint32_t last_seq;          /* 最新 seq */
 } NvsScanResult_t;
 
 /* 运行时上下文 */
 typedef struct
 {
     uint32_t active_sector_addr;   /* 当前追加写扇区 */
     uint32_t standby_sector_addr;  /* 备用扇区 */
     uint32_t active_generation;    /* 当前扇区代数 */
     uint32_t next_write_addr;      /* active 下一写入地址 */
     uint32_t next_seq;             /* 下次 seq */
 } NvsContext_t;
 
 #endif 
 